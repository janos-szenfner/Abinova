/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* Abinova
 * Copyright (C) 2002 Dom Lachowicz and others
 * Copyright (C) 2004, 2009, 2019 Hubert Figuière
 * Copyright (C) 2025-2026 Abinova contributors
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

#ifdef HAVE_CONFIG_H
#include <string>
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <vector>
#include <glib.h>
#include <glib/gstdio.h>

#include "ut_debugmsg.h"
#include "ev_EditMethod.h"
#include "ap_Features.h"
#include "ap_App.h"
#include "ap_Args.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_Strings.h"
#include "xap_Frame.h"
#include "xap_Prefs_SchemeIds.h"
#include "pd_Document.h"
#include "ie_imp.h"
#include "ut_go_file.h"
#include "ut_path.h"

AP_App::AP_App (const char * szAppName)
  : XAP_App_BaseClass(szAppName, "io.github.janos_szenfner.Abinova")
{
}

AP_App::~AP_App ()
{
}

XAP_Frame* AP_App::openFile(const char* uri, const char* file)
{
	XAP_Frame*  pFrame = newFrame();

	UT_Error error = pFrame->loadDocument(uri, IEFT_Unknown, true);

	if (UT_IS_IE_SUCCESS(error)) {
		if (error == UT_IE_TRY_RECOVER) {
			pFrame->showMessageBox(AP_STRING_ID_MSG_OpenRecovered,
								   XAP_Dialog_MessageBox::b_O,
								   XAP_Dialog_MessageBox::a_OK);
		}
	} else {
		// TODO we crash if we just delete this without putting something
		// TODO in it, so let's go ahead and open an untitled document
		// TODO for now.  this would cause us to get 2 untitled documents
		// TODO if the user gave us 2 bogus pathnames....

		// Because of the incremental loader, we should not crash anymore;
		// I've got other things to do now though.
		pFrame->loadDocument(static_cast<const char *>(nullptr), IEFT_Unknown);
		pFrame->raise();

		errorMsgBadFile (pFrame, file ? file : uri, error);
	}
	return pFrame;
}

/*!
 *  Open windows requested on commandline.
 * 
 * \return False if an unknown command line option was used, true
 * otherwise.  
 */
bool AP_App::openCmdLineFiles(const AP_Args * /*args*/)
{
	int kWindowsOpened = 0;
	const char *file = nullptr;

	if (AP_Args::m_sFiles == nullptr) {
		// no files to open, this is ok
		XAP_Frame * pFrame = newFrame();
		pFrame->loadDocument(static_cast<const char *>(nullptr), IEFT_Unknown);
		return true;
	}

	int i = 0;
	while ((file = AP_Args::m_sFiles[i++]) != nullptr) {
		char * uri = nullptr;

		uri = UT_go_shell_arg_to_uri (file);
		openFile(uri);
		g_free(uri);

		kWindowsOpened++;
	}

	if (kWindowsOpened == 0)
	{
		// no documents specified or openable, open an untitled one
		
		XAP_Frame * pFrame = newFrame();
		pFrame->loadDocument(static_cast<const char *>(nullptr), IEFT_Unknown);
	}

	return true;
}



bool	AP_App::initialize(void)
{
	return XAP_App_BaseClass::initialize(AP_PREF_KEY_KeyBindings,AP_PREF_DEFAULT_KeyBindings);
}

void AP_App::errorMsgBadFile(XAP_Frame *, const char *, UT_Error)
{
	UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
}

bool AP_App::doWindowlessArgs (const AP_Args *, bool & /*bSuccess*/)
{
	return false;
}

/*!
 * A directory entry counts as a recovery candidate only if it carries a
 * suffix the autosave machinery itself writes: ".saved" (the
 * crash-recovery copies written by saveRecoveryFiles) or the configured
 * periodic-autosave extension (default ".bak~"). ".part"/".info"
 * sidecars and any other file dropped into the directory are never
 * imported unprompted.
 */
bool AP_App::isAutosaveCandidateName(const char * name,
								   const char * configuredExt)
{
	if (!name || !*name)
		return false;
	if (g_str_has_suffix(name, ".part") || g_str_has_suffix(name, ".info"))
		return false;
	if (g_str_has_suffix(name, ".saved"))
		return true;
	return configuredExt && *configuredExt
		&& g_str_has_suffix(name, configuredExt);
}

/*!
 * The ".info" sidecar's first line is applied verbatim as the recovered
 * document's filename, so it is only trusted when it names a local
 * file: a "file://" URI or an absolute path. A planted remote URI
 * (sftp://, davs://, ...) would silently redirect the user's next Save
 * of the recovered document to a remote share.
 */
bool AP_App::isAutosaveSidecarUriSafe(const char * uri)
{
	return uri
		&& (g_str_has_prefix(uri, "file://")
			|| g_path_is_absolute(uri));
}

/*!
 * Scans the autosave directory for recovery files left behind by an
 * unclean shutdown and opens each one for review. A ".info" sidecar
 * written next to each backup carries the document's original URI, so
 * recovered documents get their filename back and only need a normal
 * Save to be restored in place. Successfully recovered backups are
 * removed; files that fail to load are left alone.
 *
 * The candidates go through the exact same pinned-importer load path
 * (loadDocument + readFromFile) a user-opened .abwn gets, so a hostile
 * recovery file gets no more trust than a hostile document the user
 * opened. The gates in front — filename suffix, regular-file check,
 * sidecar URI validation — just limit WHICH local bytes get fed to
 * that importer without a user gesture.
 */
void AP_App::recoverAutosavedDocs()
{
	std::string dir = XAP_Frame::getAutosaveDirectory();
	GDir *d = g_dir_open(dir.c_str(), 0, nullptr);
	if (!d)
		return;

	std::string cfgExt;
	if (!getPrefsValue(XAP_PREF_KEY_AutoSaveFileExt, cfgExt)
		|| cfgExt.empty())
		cfgExt = XAP_PREF_DEFAULT_AutoSaveFileExt;

	std::vector<std::string> files;
	const gchar *name;
	while ((name = g_dir_read_name(d)) != nullptr) {
		if (isAutosaveCandidateName(name, cfgExt.c_str()))
			files.push_back(name);
	}
	g_dir_close(d);
	if (files.empty())
		return;

	IEFileType abiType = IE_Imp::fileTypeForSuffix(".abwn");
	int recovered = 0;
	for (const std::string &nm : files) {
		std::string path = dir + nm;

		// a planted FIFO/device would block the open() below forever
		if (!UT_isRegularFile(path.c_str()))
			continue;

		// the sidecar holds the original URI, if any
		std::string orig;
		std::string infoPath = path + ".info";
		if (UT_isRegularFile(infoPath.c_str()))
		{
			FILE *info = g_fopen(infoPath.c_str(), "r");
			if (info) {
				char buf[4096];
				if (fgets(buf, sizeof(buf), info)) {
					buf[strcspn(buf, "\r\n")] = 0;
					orig = buf;
				}
				fclose(info);
			}
		}
		if (!isAutosaveSidecarUriSafe(orig.c_str()))
			orig.clear();

		gchar *uri = UT_go_filename_to_uri(path.c_str());
		if (!uri)
			continue;
		XAP_Frame *f = newFrame();
		UT_Error error = f->loadDocument(uri, abiType, true);
		g_free(uri);

		if (UT_IS_IE_SUCCESS(error)) {
			AD_Document *pDoc = f->getCurrentDoc();
			if (pDoc) {
				if (!orig.empty())
					pDoc->setFilename(orig.c_str());
				else
					pDoc->clearFilename();
				pDoc->forceDirty(); // an explicit Save is required to keep the recovery
			}
			f->updateTitle();
			g_unlink(path.c_str());
			g_unlink((path + ".info").c_str());
			recovered++;
		}
		else {
			// give the frame an empty document so it isn't left unusable
			f->loadDocument(static_cast<const char *>(nullptr), IEFT_Unknown);
		}
	}

	if (recovered > 0) {
		XAP_Frame *f = m_vecFrames.empty() ? nullptr : m_vecFrames[0];
		if (f) {
			XAP_Dialog_MessageBox *dlg = f->createMessageBox(
				AP_STRING_ID_MSG_RecoveredDocuments,
				XAP_Dialog_MessageBox::b_O,
				XAP_Dialog_MessageBox::a_OK,
				recovered);
			f->showMessageBox(dlg);
		}
	}
}

void AP_App::saveRecoveryFiles()
{
	IEFileType abiType = IE_Imp::fileTypeForSuffix(".abwn");

	for(UT_sint32 i = 0; i < static_cast<UT_sint32>(m_vecFrames.size()); i++) {
		XAP_Frame * curFrame = m_vecFrames[i];
		if(!curFrame) {
			continue;
		}
		try {
			AD_Document *pDoc = curFrame->getCurrentDoc();
			if (!pDoc || pDoc->isPieceTableChanging()) {
				/* crashed mid-mutation: a snapshot export could be
				 * corrupt — keep the last consistent autosave instead
				 * of overwriting it */
				continue;
			}
			curFrame->clearBackupInProgress();
			if (nullptr == curFrame->getFilename()) {
				curFrame->backup(".abw.saved",abiType);
			}
			else {
				curFrame->backup(".saved",abiType);
			}
		}
		catch(...) {
			UT_DEBUGMSG(("saveRecoveryFiles: backup() threw for frame %d, its recovery file was not written\n", i));
		}
	}
}

