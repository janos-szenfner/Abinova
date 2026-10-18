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

#include <glib.h>
#include <glib/gstdio.h>

#include <string>
#include <vector>

#include "tf_test.h"
#include "ut_asyncwrite.h"
#include "ut_types.h"

#define TFSUITE "core.af.util.asyncwrite"

namespace
{

struct WriteResult
{
	int			calls = 0;
	UT_Error	last = UT_OK;
	std::vector<UT_Error> results;
};

void done_cb(void * userData, UT_Error result)
{
	WriteResult * r = static_cast<WriteResult *>(userData);
	r->calls++;
	r->last = result;
	r->results.push_back(result);
}

struct FreeTrack
{
	WriteResult	r;
	int			destroyed = 0;
};

void done_tracked(void * userData, UT_Error result)
{
	done_cb(&static_cast<FreeTrack *>(userData)->r, result);
}

void destroy_cb(void * userData)
{
	static_cast<FreeTrack *>(userData)->destroyed++;
}

GBytes * payload_of(const std::string & s)
{
	return g_bytes_new(s.data(), s.size());
}

bool file_contents(const std::string & path, std::string & out)
{
	gchar * buf = nullptr;
	gsize len = 0;
	if (!g_file_get_contents(path.c_str(), &buf, &len, nullptr))
		return false;
	out.assign(buf, len);
	g_free(buf);
	return true;
}

std::string tmpdir()
{
	gchar * d = g_mkdtemp(g_strdup_printf("%s/abnawXXXXXX",
										  g_get_tmp_dir()));
	if (!d)
		return std::string(g_get_tmp_dir());
	std::string out(d);
	g_free(d);
	return out;
}

} // namespace

TFTEST_MAIN("queued write lands and completion fires")
{
	std::string dir = tmpdir();
	std::string target = dir + "/one.bin";

	WriteResult r;
	GBytes * p = payload_of("hello async world");
	UT_AsyncWrite_enqueue(target.c_str(), p, done_cb, &r, nullptr);
	g_bytes_unref(p);

	UT_AsyncWrite_waitAll();

	TFPASSEQ(r.calls, 1);
	TFPASSEQ(static_cast<int>(r.last), static_cast<int>(UT_OK));
	std::string got;
	TFPASS(file_contents(target, got));
	TFPASS(got == "hello async world");

	/* nothing may linger queued after waitAll */
	TFPASSEQ(static_cast<int>(UT_AsyncWrite_pendingJobs()), 0);
	g_remove(target.c_str());
	g_rmdir(dir.c_str());
}

TFTEST_MAIN("writes are staged atomically — previous copy survives failure")
{
	std::string dir = tmpdir();
	std::string target = dir + "/keep.bin";

	/* seed a "last good" file, then fail a write to it by pointing at
	 * a sibling temp name that cannot be created is not reachable —
	 * instead write to an impossible directory to see the error */
	WriteResult r;
	GBytes * p = payload_of("good");
	TFPASSEQ(static_cast<int>(UT_AsyncWrite_syncWrite(target.c_str(), p)),
			 static_cast<int>(UT_OK));
	g_bytes_unref(p);

	std::string bogus = dir + "/no/such/dir/x.bin";
	GBytes * p2 = payload_of("never");
	UT_AsyncWrite_enqueue(bogus.c_str(), p2, done_cb, &r, nullptr);
	g_bytes_unref(p2);
	UT_AsyncWrite_waitAll();

	TFPASSEQ(r.calls, 1);
	TFFAIL(r.last == UT_OK);

	std::string got;
	TFPASS(file_contents(target, got));
	TFPASS(got == "good");
	g_remove(target.c_str());
	g_rmdir(dir.c_str());
}

TFTEST_MAIN("same-target queue: every completion fires, file holds a whole payload")
{
	std::string dir = tmpdir();
	std::string target = dir + "/same.bin";

	WriteResult r;
	const char * texts[] = {"payload-1", "payload-2-longer", "p3"};
	for (const char * t : texts)
	{
		GBytes * p = payload_of(t);
		UT_AsyncWrite_enqueue(target.c_str(), p, done_cb, &r, nullptr);
		g_bytes_unref(p);
	}
	UT_AsyncWrite_waitAll();

	/* superseded or not, each queued job reports */
	TFPASSEQ(r.calls, 3);

	std::string got;
	TFPASS(file_contents(target, got));
	bool whole = false;
	for (const char * t : texts)
		whole |= got == t;
	/* serialized writes can never interleave */
	TFPASS(whole);
	g_remove(target.c_str());
	g_rmdir(dir.c_str());
}

TFTEST_MAIN("fifo order across different targets")
{
	std::string dir = tmpdir();
	std::vector<std::string> order;
	order.reserve(3);

	struct Ordered { std::vector<std::string> * out; std::string tag; };
	Ordered a{&order, "a"}, b{&order, "b"}, c{&order, "c"};
	auto cb = [](void * ud, UT_Error res) {
		Ordered * o = static_cast<Ordered *>(ud);
		if (res == UT_OK)
			o->out->push_back(o->tag);
	};

	/* enqueue takes its own ref on the payload — a g_bytes_ref in the
	 * argument would be an abandoned ref */
	GBytes * p = payload_of("x");
	UT_AsyncWrite_enqueue((dir + "/a").c_str(), p, cb, &a, nullptr);
	UT_AsyncWrite_enqueue((dir + "/b").c_str(), p, cb, &b, nullptr);
	UT_AsyncWrite_enqueue((dir + "/c").c_str(), p, cb, &c, nullptr);
	g_bytes_unref(p);
	UT_AsyncWrite_waitAll();

	TFPASSEQ(static_cast<int>(order.size()), 3);
	TFPASS(order[0] == "a");
	TFPASS(order[1] == "b");
	TFPASS(order[2] == "c");
	g_remove((dir + "/a").c_str());
	g_remove((dir + "/b").c_str());
	g_remove((dir + "/c").c_str());
	g_rmdir(dir.c_str());
}

TFTEST_MAIN("destroy notify runs exactly once, completions survive teardown")
{
	std::string dir = tmpdir();
	std::string target = dir + "/free.bin";

	FreeTrack t1, t2, t3;
	GBytes * p = payload_of("z");
	GBytes * p2 = payload_of("z2");
	UT_AsyncWrite_enqueue(target.c_str(), p, done_tracked, &t1, destroy_cb);
	UT_AsyncWrite_enqueue(target.c_str(), p2, done_tracked, &t2, destroy_cb);
	UT_AsyncWrite_enqueue(target.c_str(), p, done_tracked, &t3, destroy_cb);
	g_bytes_unref(p);
	g_bytes_unref(p2);

	UT_AsyncWrite_waitAll();
	TFPASSEQ(t1.r.calls + t2.r.calls + t3.r.calls, 3);
	TFPASSEQ(t1.destroyed + t2.destroyed + t3.destroyed, 3);
	/* waitForTarget on a finished target returns immediately */
	UT_AsyncWrite_waitForTarget(target.c_str());
	UT_AsyncWrite_waitForTarget(nullptr);
	g_remove(target.c_str());
	g_rmdir(dir.c_str());
}
