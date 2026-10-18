/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 2026 Abinova contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include "ut_asyncwrite.h"

#include <string>

#include <gsf/gsf.h>

#include "ut_debugmsg.h"
#include "ut_go_file.h"

namespace
{

struct AsyncWriteJob
{
	std::string				target;
	GBytes *				bytes = nullptr;
	UT_AsyncWriteDoneFn		fnDone = nullptr;
	void *					userData = nullptr;
	GDestroyNotify			fnDestroy = nullptr;
	UT_Error				result = UT_OK;
};

GMutex		s_mx;
GCond		s_cond;
GQueue		s_queue = G_QUEUE_INIT;		// writes not yet started (FIFO)
GQueue		s_done = G_QUEUE_INIT;		// finished jobs awaiting delivery
bool		s_workerRunning = false;
bool		s_activeSet = false;
std::string	s_activeTarget;				// target the worker is writing now
bool		s_flushScheduled = false;	// an idle flush is on the context

void job_free(AsyncWriteJob * job)
{
	if (job->fnDestroy)
		job->fnDestroy(job->userData);
	if (job->bytes)
		g_bytes_unref(job->bytes);
	delete job;
}

/* Deliver every finished job's completion, in write order.  Runs on the
 * main thread — either as an idle handler or inline from waitAll(). */
void flush_done(void)
{
	GQueue deliver;

	g_mutex_lock(&s_mx);
	/* move the whole node list — copying data pointers into a second
	 * queue then reiniting s_done would orphan its GList nodes */
	deliver = s_done;
	s_done = G_QUEUE_INIT;
	s_flushScheduled = false;
	g_mutex_unlock(&s_mx);

	AsyncWriteJob * job;
	while ((job = static_cast<AsyncWriteJob *>(g_queue_pop_head(&deliver))))
	{
		if (job->fnDone)
			job->fnDone(job->userData, job->result);
		job_free(job);
	}
}

gboolean flush_done_idle(gpointer /*data*/)
{
	flush_done();
	return G_SOURCE_REMOVE;
}

/* Completions are delivered on the default main context — the UI
 * thread when the main loop runs — so callbacks may safely touch
 * documents and widgets.  Scheduling an idle source is thread-safe;
 * this may be called from the worker or the main thread. */
void post_done(AsyncWriteJob * job)
{
	bool bNeedIdle;
	g_mutex_lock(&s_mx);
	g_queue_push_tail(&s_done, job);
	bNeedIdle = !s_flushScheduled;
	s_flushScheduled = true;
	g_mutex_unlock(&s_mx);

	if (bNeedIdle)
	{
		/* the idle flag is cleared by flush_done() once it has run, or
		 * by this notify if the source is dropped before it can run —
		 * a dropped flush loses nothing: waitAll() delivers s_done
		 * inline during shutdown */
		g_idle_add_full(G_PRIORITY_HIGH_IDLE, flush_done_idle, nullptr,
						+[](gpointer) {
							g_mutex_lock(&s_mx);
							s_flushScheduled = false;
							g_mutex_unlock(&s_mx);
						});
	}
}

/* Write the payload to its target, mirroring IE_Exp::writeFile():
 * local files go through a sibling ".part" temp + atomic rename so an
 * interrupted write never truncates the previous copy; remote URIs are
 * written directly.  Runs on the worker thread — only self-contained
 * data is touched, and all file helpers used are plain POSIX/GIO. */
UT_Error write_job(AsyncWriteJob * job)
{
	const char * szTarget = job->target.c_str();
	const guint8 * pBytes = static_cast<const guint8 *>(
		g_bytes_get_data(job->bytes, nullptr));
	const gsize iLen = g_bytes_get_size(job->bytes);

	char * localFinal = nullptr;
	char * localTmp = nullptr;
	const char * writeName = szTarget;

	const bool bLocal = !UT_go_path_is_uri(szTarget) ||
						g_str_has_prefix(szTarget, "file://");
	if (bLocal)
	{
		localFinal = UT_go_path_is_uri(szTarget)
					   ? UT_go_filename_from_uri(szTarget)
					   : g_strdup(szTarget);
		if (localFinal)
		{
			localTmp = UT_go_file_atomic_temp_name(localFinal);
			writeName = localTmp;
		}
	}

	GError * gerr = nullptr;
	GsfOutput * out = UT_go_file_create(writeName, &gerr);
	if (!out)
	{
		UT_DEBUGMSG(("UT_AsyncWrite: cannot open '%s': %s\n",
					 writeName, gerr ? gerr->message : "?"));
		if (gerr)
			g_error_free(gerr);
		g_free(localFinal);
		g_free(localTmp);
		return UT_IE_COULDNOTWRITE;
	}

	UT_Error error = UT_OK;
	if (iLen && !gsf_output_write(out, iLen, pBytes))
		error = UT_IE_COULDNOTWRITE;
	if (error == UT_OK && !gsf_output_close(out))
		error = UT_IE_COULDNOTWRITE;
	g_object_unref(G_OBJECT(out));

	if (localTmp)
	{
		if (error == UT_OK)
		{
			if (!UT_go_file_atomic_replace(localTmp, localFinal, &gerr))
			{
				UT_DEBUGMSG(("UT_AsyncWrite: replace '%s' failed: %s\n",
							 localFinal, gerr ? gerr->message : "?"));
				if (gerr)
					g_error_free(gerr);
				error = UT_IE_COULDNOTWRITE;
			}
		}
		else
		{
			UT_go_file_atomic_abort(localTmp);
		}
	}
	g_free(localFinal);
	g_free(localTmp);

	return error;
}

gpointer worker_main(gpointer /*data*/)
{
	for (;;)
	{
		g_mutex_lock(&s_mx);
		AsyncWriteJob * job =
			static_cast<AsyncWriteJob *>(g_queue_pop_head(&s_queue));
		if (!job)
		{
			s_workerRunning = false;
			g_cond_broadcast(&s_cond);
			g_mutex_unlock(&s_mx);
			return nullptr;
		}
		s_activeTarget = job->target;
		s_activeSet = true;
		g_mutex_unlock(&s_mx);

		job->result = write_job(job);

		g_mutex_lock(&s_mx);
		s_activeSet = false;
		s_activeTarget.clear();
		g_cond_broadcast(&s_cond);
		g_mutex_unlock(&s_mx);

		post_done(job);
	}
}

} // namespace

/* Direct synchronous write using the same atomic staging as the
 * worker, but touching none of the queue's shared state — safe to
 * call from a forked crash-recovery child where the mutex may be
 * frozen mid-operation and no main context will ever run. */
UT_Error UT_AsyncWrite_syncWrite(const char * szTarget, GBytes * payload)
{
	g_return_val_if_fail(szTarget && *szTarget, UT_IE_COULDNOTWRITE);
	g_return_val_if_fail(payload, UT_IE_COULDNOTWRITE);

	AsyncWriteJob job;
	job.target = szTarget;
	job.bytes = payload;
	return write_job(&job);
}

void UT_AsyncWrite_enqueue(const char * szTarget, GBytes * payload,
						   UT_AsyncWriteDoneFn fnDone, void * pUserData,
						   GDestroyNotify fnDestroyUserData)
{
	g_return_if_fail(szTarget && *szTarget);
	g_return_if_fail(payload);

	AsyncWriteJob * job = new AsyncWriteJob;
	job->target = szTarget;
	job->bytes = g_bytes_ref(payload);
	job->fnDone = fnDone;
	job->userData = pUserData;
	job->fnDestroy = fnDestroyUserData;

	GList * superseded = nullptr;

	g_mutex_lock(&s_mx);
	/* a queued (not yet started) job for the same target is superseded
	 * by this newer snapshot — writing it would only be overwritten
	 * again, and its completion still fires normally */
	for (GList * l = s_queue.head; l;)
	{
		GList * next = l->next;
		AsyncWriteJob * pending = static_cast<AsyncWriteJob *>(l->data);
		if (pending->target == job->target)
		{
			pending->result = UT_OK;
			g_queue_remove(&s_queue, pending);
			superseded = g_list_prepend(superseded, pending);
		}
		l = next;
	}
	g_queue_push_tail(&s_queue, job);
	if (!s_workerRunning)
	{
		s_workerRunning = true;
		GThread * th = g_thread_new("abn-asyncwrite", worker_main, nullptr);
		g_thread_unref(th);
	}
	g_cond_broadcast(&s_cond);
	g_mutex_unlock(&s_mx);

	/* post outside the lock so callbacks can never run under s_mx */
	for (GList * l = superseded; l; l = l->next)
		post_done(static_cast<AsyncWriteJob *>(l->data));
	g_list_free(superseded);
}

void UT_AsyncWrite_waitAll(void)
{
	g_mutex_lock(&s_mx);
	while (s_workerRunning)
		g_cond_wait(&s_cond, &s_mx);
	g_mutex_unlock(&s_mx);

	/* deliveries due on the main context: if we are the main thread
	 * (context owned, or unowned and acquirable) flush them inline so
	 * callers see the effects — clean flags, save results — before
	 * they return; otherwise leave them for the main loop */
	GMainContext * ctx = g_main_context_default();
	if (g_main_context_is_owner(ctx) || g_main_context_acquire(ctx))
	{
		flush_done();
		g_main_context_release(ctx);
	}
}

void UT_AsyncWrite_waitForTarget(const char * szTarget)
{
	g_mutex_lock(&s_mx);
	for (;;)
	{
		bool found = szTarget == nullptr ? s_activeSet
					 : (s_activeSet && s_activeTarget == szTarget);
		if (!found)
		{
			for (GList * l = s_queue.head; l; l = l->next)
			{
				AsyncWriteJob * j = static_cast<AsyncWriteJob *>(l->data);
				if (!szTarget || j->target == szTarget)
				{
					found = true;
					break;
				}
			}
		}
		if (!found)
			break;
		g_cond_wait(&s_cond, &s_mx);
	}
	g_mutex_unlock(&s_mx);
}

UT_uint32 UT_AsyncWrite_pendingJobs(void)
{
	g_mutex_lock(&s_mx);
	UT_uint32 n = s_queue.length + (s_activeSet ? 1 : 0);
	g_mutex_unlock(&s_mx);
	return n;
}
