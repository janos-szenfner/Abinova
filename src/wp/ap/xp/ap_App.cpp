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
#include "config.h"
#endif

#include <stdio.h>

#include "ev_EditMethod.h"
#include "ap_Features.h"
#include "ap_App.h"
#include "ap_Args.h"
#include "ap_Prefs_SchemeIds.h"
#include "ap_Strings.h"
#include "xap_Frame.h"
#include "pd_Document.h"
#include "ie_imp.h"

AP_App::AP_App (const char * szAppName)
  : XAP_App_BaseClass(szAppName, "com.abisource.AbiWord")
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
		pFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
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
bool AP_App::openCmdLineFiles(const AP_Args * args)
{
	int kWindowsOpened = 0;
	const char *file = nullptr;

	if (AP_Args::m_sFiles == nullptr) {
		// no files to open, this is ok
		XAP_Frame * pFrame = newFrame();
		pFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
		return true;
	}

	int i = 0;
	while ((file = AP_Args::m_sFiles[i++]) != nullptr) {
		char * uri = nullptr;

		uri = UT_go_shell_arg_to_uri (file);
		XAP_Frame* pFrame = openFile(uri);
		g_free(uri);

		kWindowsOpened++;

		if (args->m_sMerge) {
			PD_Document * pDoc = static_cast<PD_Document*>(pFrame->getCurrentDoc());
			pDoc->setMailMergeLink(args->m_sMerge);
		}
	}

	if (kWindowsOpened == 0)
	{
		// no documents specified or openable, open an untitled one
		
		XAP_Frame * pFrame = newFrame();
		pFrame->loadDocument((const char *)nullptr, IEFT_Unknown);
		if (args->m_sMerge) {
			PD_Document * pDoc = static_cast<PD_Document*>(pFrame->getCurrentDoc());
			pDoc->setMailMergeLink(args->m_sMerge);
		}
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

void AP_App::saveRecoveryFiles()
{
	IEFileType abiType = IE_Imp::fileTypeForSuffix(".abwn");

	for(UT_sint32 i = 0; i < m_vecFrames.getItemCount(); i++) {
		XAP_Frame * curFrame = m_vecFrames[i];
		if(!curFrame) {
			continue;
		}
		try {
			if (nullptr == curFrame->getFilename()) {
				curFrame->backup(".abw.saved",abiType);
			}
			else {
				curFrame->backup(".saved",abiType);
			}
		}
		catch(...) {
			// just continue
		}
	}
}

