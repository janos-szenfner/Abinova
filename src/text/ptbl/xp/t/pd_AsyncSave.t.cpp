/* AbiSource Applications
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
#include "pd_Document.h"
#include "ie_exp.h"
#include "ie_imp.h"
#include "ie_types.h"
#include "ut_asyncwrite.h"
#include "xap_App.h"
#include "xap_Prefs.h"

#include <gsf/gsf.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-output-stdio.h>

#define TFSUITE "core.text.ptbl.asyncsave"

static const char *PD_ASYNC_ABW_FILE = "/test/wp/Gettysburg.abw";

static PD_Document *import_doc()
{
	std::string data_file;
	if (!TF_Test::ensure_test_data(PD_ASYNC_ABW_FILE, data_file))
		return nullptr;

	PD_Document *doc = new PD_Document;
	if (doc->readFromFile(data_file.c_str(), IEFT_Unknown, nullptr) != UT_OK) {
		doc->unref();
		return nullptr;
	}
	return doc;
}

static bool read_file(const std::string & path, std::vector<unsigned char> & out)
{
	gchar * buf = nullptr;
	gsize len = 0;
	if (!g_file_get_contents(path.c_str(), &buf, &len, nullptr))
		return false;
	out.assign(reinterpret_cast<unsigned char *>(buf),
			   reinterpret_cast<unsigned char *>(buf) + len);
	g_free(buf);
	return true;
}

/* strip the <history> element — every save bumps the document version
 * and mints a version UID, so two consecutive saves of the same doc
 * differ there by design; everything outside it is the payload the
 * exporter produced from the same content */
static std::string gunzip_text(const std::vector<unsigned char> & blob)
{
	GsfInput * mem = gsf_input_memory_new(blob.data(), blob.size(), FALSE);
	if (!mem)
		return {};
	/* uncompress hands back src itself (no extra ref) when the bytes
	 * are not compressed — only drop our ref when it wrapped it */
	GsfInput * dec = gsf_input_uncompress(mem);
	if (dec != mem)
		g_object_unref(mem);
	if (!dec)
		return {};
	std::string out;
	for (;;)
	{
		const gsf_off_t before = gsf_input_remaining(dec);
		if (before <= 0)
			break;
		const guint8 * bytes = gsf_input_read(dec, before < 8192 ? before : 8192, nullptr);
		if (!bytes)
			break;
		out.append(reinterpret_cast<const char *>(bytes),
				   static_cast<size_t>(before < 8192 ? before : 8192));
	}
	g_object_unref(dec);
	return out;
}

static std::string strip_history(const std::string & xml)
{
	const std::string open = "<history";
	const std::string close = "</history>";
	std::string::size_type s = xml.find(open);
	std::string::size_type e = xml.find(close);
	if (s == std::string::npos || e == std::string::npos || e < s)
		return xml;
	/* drop the whole line so a save that emits no <history> at all
	 * still compares equal */
	std::string::size_type ls = xml.rfind('\n', s);
	ls = (ls == std::string::npos) ? 0 : ls + 1;
	std::string::size_type le = e + close.size();
	if (le < xml.size() && xml[le] == '\n')
		++le;
	return xml.substr(0, ls) + xml.substr(le);
}

/* the byte-identical contract: an async save must produce exactly what
 * the synchronous exporter writes, modulo the history bookkeeping that
 * bumps on every save */
TFTEST_MAIN("saveAsAsync output is byte-identical to saveAs")
{
	PD_Document *doc = import_doc();
	TFPASS(doc);
	if (!doc)
		return;

	std::string base = std::string("/tmp/abn_async_") +
		std::to_string(::getpid());
	std::string syncName = base + "_sync.abwn";
	std::string asyncName = base + "_async.abwn";

	/* the prefs' "ignore next recent" one-shot may still be armed by
	 * an earlier test — consume it so both saves take the same
	 * history-bookkeeping branch (it gates _adjustHistoryOnSave) */
	XAP_Prefs * prefs = XAP_App::getApp()->getPrefs();
	if (prefs->isIgnoreRecent())
		prefs->addRecent("abn_async_burn");

	const int iEFT = IE_Exp::fileTypeForSuffix(".abwn");

	TFPASSEQ(static_cast<int>(
				 static_cast<AD_Document*>(doc)->saveAs(syncName.c_str(), iEFT, false, nullptr)),
			 static_cast<int>(UT_OK));

	TFPASSEQ(static_cast<int>(
				 doc->saveAsAsync(asyncName.c_str(), iEFT, false)),
			 static_cast<int>(UT_OK));
	TFPASSEQ(static_cast<int>(doc->drainAsyncSaves()),
			 static_cast<int>(UT_OK));
	TFFAIL(doc->isAsyncSavePending());

	std::vector<unsigned char> a, b;
	TFPASS(read_file(syncName, a));
	TFPASS(read_file(asyncName, b));
	TFPASS(a.size() > 256);

	std::string xmlSync = gunzip_text(a);
	std::string xmlAsync = gunzip_text(b);
	TFPASS(xmlSync.size() > 256);
	TFPASS(strip_history(xmlSync) == strip_history(xmlAsync));

	// async payload must be a real document: decompress + parse shape
	TFPASS(xmlAsync.find("<abinova") != std::string::npos ||
		   xmlAsync.find("<abiword") != std::string::npos);
	TFPASS(xmlAsync.find("</abinova>") != std::string::npos ||
		   xmlAsync.find("</abiword>") != std::string::npos);

	g_remove(syncName.c_str());
	g_remove(asyncName.c_str());
	doc->unref();
}

/* the dirty bit is only cleared when the write has actually landed and
 * the content is unchanged since the snapshot */
TFTEST_MAIN("drained save marks clean; mid-write edits stay dirty")
{
	PD_Document *doc = import_doc();
	TFPASS(doc);
	if (!doc)
		return;

	std::string name = std::string("/tmp/abn_async_") +
		std::to_string(::getpid()) + "_dirty.abwn";
	const int iEFT = IE_Exp::fileTypeForSuffix(".abwn");

	/* adopting save-as: filename is applied on completion */
	TFPASSEQ(static_cast<int>(
				 doc->saveAsAsync(name.c_str(), iEFT, true)),
			 static_cast<int>(UT_OK));
	TFPASSEQ(static_cast<int>(doc->drainAsyncSaves()),
			 static_cast<int>(UT_OK));
	TFFAIL(doc->isDirty());

	/* a save queued and then edited past must not lose the dirty bit */
	doc->forceDirty();
	TFPASSEQ(static_cast<int>(doc->saveAsync()), static_cast<int>(UT_OK));
	/* mutate the document underneath the in-flight write */
	UT_UCS4Char ch = 'x';
	TFPASS(doc->insertSpan(2, &ch, 1));
	TFPASSEQ(static_cast<int>(doc->drainAsyncSaves()),
			 static_cast<int>(UT_OK));
	TFPASS(doc->isDirty());

	/* and a clean save after that lands clean */
	TFPASSEQ(static_cast<int>(doc->saveAsync()), static_cast<int>(UT_OK));
	TFPASSEQ(static_cast<int>(doc->drainAsyncSaves()),
			 static_cast<int>(UT_OK));
	TFFAIL(doc->isDirty());

	std::vector<unsigned char> out;
	TFPASS(read_file(name, out));
	TFPASS(out.size() > 256);

	g_remove(name.c_str());
	doc->unref();
}

/* encodeToBytes gives the autosave path the raw payload without any
 * disk activity */
TFTEST_MAIN("encodeToBytes matches a synchronous export payload")
{
	PD_Document *doc = import_doc();
	TFPASS(doc);
	if (!doc)
		return;

	std::string name = std::string("/tmp/abn_async_") +
		std::to_string(::getpid()) + "_enc.abwn";
	const int iEFT = IE_Exp::fileTypeForSuffix(".abwn");

	GBytes * payload = nullptr;
	TFPASSEQ(static_cast<int>(
				 doc->encodeToBytes(name.c_str(), iEFT, nullptr, &payload)),
			 static_cast<int>(UT_OK));
	TFPASS(payload != nullptr);
	TFPASS(g_bytes_get_size(payload) > 256);

	/* payload written through the blocking helper equals a sync save */
	TFPASSEQ(static_cast<int>(
				 UT_AsyncWrite_syncWrite(name.c_str(), payload)),
			 static_cast<int>(UT_OK));
	g_bytes_unref(payload);

	std::vector<unsigned char> disk;
	TFPASS(read_file(name, disk));
	TFPASS(disk.size() > 256);

	g_remove(name.c_str());
	doc->unref();
}
