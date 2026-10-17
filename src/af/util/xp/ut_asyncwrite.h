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

#pragma once

#include <glib.h>

#include "ut_export.h"
#include "ut_types.h"

/**
 * Serialized background file writes.
 *
 * A caller hands an already-encoded payload to UT_AsyncWrite_enqueue();
 * the bytes are written to disk on a single worker thread so the UI is
 * never stalled on file I/O.  All writes funnel through one FIFO queue:
 * two writes to the same target can never interleave, and a queued job
 * is dropped when a newer job for the same target is enqueued behind it
 * (the dropped job's completion still fires, with UT_OK, since its
 * snapshot was superseded by an accepted newer save).
 *
 * Local targets are staged through a sibling ".part" file and committed
 * with UT_go_file_atomic_replace() — identical crash-safety semantics
 * to the synchronous IE_Exp::writeFile() path.  URI targets (gvfs) are
 * written directly, matching the synchronous behaviour.
 *
 * The completion callback is delivered on the main thread: either via
 * an idle handler on the default main context, or inline inside
 * UT_AsyncWrite_waitAll() when that is called from a thread owning the
 * context — so callers can rely on @fnDone having run by the time
 * waitAll() returns on the UI thread.  @fnDestroyUserData runs exactly
 * once, right after @fnDone is delivered.
 */
typedef void (*UT_AsyncWriteDoneFn)(void * userData, UT_Error result);

void      UT_AsyncWrite_enqueue (const char * szTarget,
								 GBytes * payload,
								 UT_AsyncWriteDoneFn fnDone,
								 void * pUserData,
								 GDestroyNotify fnDestroyUserData);

/* Write @payload to @szTarget on the calling thread with the same
 * ".part"-staging semantics as queued writes, without touching the
 * queue or its locks — for contexts where the worker machinery cannot
 * run (the forked crash-recovery child). */
UT_Error  UT_AsyncWrite_syncWrite (const char * szTarget,
								   GBytes * payload);

/* Block the calling thread until every queued and in-flight write has
 * finished; when called from the thread that owns the default main
 * context the pending completions are also delivered inline.
 * Intended for app shutdown and for save-then-close flows so a save
 * in flight always reaches the disk before teardown proceeds. */
void      UT_AsyncWrite_waitAll (void);

/* Block until no queued or in-flight write remains for @szTarget
 * (nullptr: any target).  Synchronous save paths call this before
 * staging their own ".part" file so they cannot collide with an
 * in-flight async write to the same target. */
void      UT_AsyncWrite_waitForTarget (const char * szTarget);

/* Number of jobs queued or in flight — for tests and status UIs. */
UT_uint32 UT_AsyncWrite_pendingJobs (void);
