/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2019 Hubert Figuière
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

/*
 * Port to Maemo Development Platform
 * Author: INdT - Renato Araujo <renato.filho@indt.org.br>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <glib/gstdio.h>
#include <gsf/gsf.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/stat.h>

#include <fontconfig/fontconfig.h>

#include "ut_debugmsg.h"
#include "ut_path.h"
#include "ut_string.h"
#include "ut_uuid.h"

#include "xap_UnixDialogHelper.h"
#include "xap_Strings.h"
#include "xap_UnixApp.h"
#include "xap_FakeClipboard.h"
#include "gr_UnixImage.h"
#include "xap_Unix_TB_CFactory.h"
#include "xap_Prefs.h"
#include "xap_UnixEncodingManager.h"

#include "gr_UnixCairoGraphics.h"

#include "gr_CairoNullGraphics.h"
static CairoNull_Graphics * nullgraphics = nullptr;

/*****************************************************************/

XAP_UnixApp::XAP_UnixApp(const char * szAppName, const char* app_id)
	: XAP_App(szAppName),
	  m_dialogFactory(new AP_UnixDialogFactory(this)),
	  m_controlFactory(new AP_UnixToolbar_ControlFactory()),
	  m_szTmpFile(nullptr),
	  // XXX maybe we need better flags as we handle command line
	  m_gtkApp(gtk_application_new(app_id, G_APPLICATION_DEFAULT_FLAGS))
{
	int fc_inited = FcInit();
	UT_UNUSED(fc_inited); // TODO actually deal with the error here
	UT_ASSERT(fc_inited);

	_setAbiSuiteLibDir();

	// Make the bundled font collection (installed under
	// <AbiSuiteLibDir>/fonts) visible to fontconfig so documents
	// render identically even without system-installed fonts.
	{
		FcConfig * config = FcConfigGetCurrent();
		std::string fontDir = getAbiSuiteLibDir();
		fontDir += "/fonts";
		// the fonts dir only exists after install; skip quietly when
		// running from the build tree instead of making fontconfig
		// log an error for a file we know is absent
		if (g_access(fontDir.c_str(), F_OK) == 0)
		{
			if (!FcConfigAppFontAddDir(config,
					reinterpret_cast<const FcChar8*>(fontDir.c_str())))
			{
				UT_DEBUGMSG(("Failed to add bundled font directory %s\n",
							 fontDir.c_str()));
			}

			// Load substitution rules mapping common document font
			// names onto the bundled metric-compatible fonts.
			std::string fontConf = fontDir + "/abinova-fonts.conf";
			if (g_access(fontConf.c_str(), F_OK) == 0 &&
				!FcConfigParseAndLoad(config,
					reinterpret_cast<const FcChar8*>(fontConf.c_str()),
					FcTrue))
			{
				UT_DEBUGMSG(("Failed to load bundled font config %s\n",
							 fontConf.c_str()));
			}
		}
	}

	memset(&m_geometry, 0, sizeof(m_geometry));

	// create an instance of UT_UUIDGenerator or appropriate derrived class
	_setUUIDGenerator(new UT_UUIDGenerator());

	// register graphics allocator
	GR_GraphicsFactory * pGF = getGraphicsFactory();
	UT_ASSERT( pGF );

	if(pGF)
	{
		bool bSuccess;
		bSuccess = pGF->registerClass(GR_UnixCairoGraphics::graphicsAllocator,
									  GR_UnixCairoGraphics::graphicsDescriptor,
									  GR_UnixCairoGraphics::s_getClassId());

		UT_ASSERT( bSuccess );

		if(bSuccess)
		{
			pGF->registerAsDefault(GR_UnixCairoGraphics::s_getClassId(), true);
		}

		bSuccess = pGF->registerClass(CairoNull_Graphics::graphicsAllocator,
									  CairoNull_Graphics::graphicsDescriptor,
									  CairoNull_Graphics::s_getClassId());
		UT_ASSERT( bSuccess );

		/* We need to link CairoNull_Graphics because the AbiCommand
		 * plugin uses it.
		 *
		 * We do not need to keep an instance of it around though, as the
		 * plugin constructs its own (I wonder if there is a more elegant way
		 * to force the linker into including it).
		 */
		{
			GR_CairoNullGraphicsAllocInfo ai;
			nullgraphics =
				static_cast<CairoNull_Graphics*>( XAP_App::getApp()->newGraphics(static_cast<UT_uint32>(GRID_CAIRO_NULL), ai));

			delete nullgraphics;
			nullgraphics = nullptr;
		}
	}
}

XAP_UnixApp::~XAP_UnixApp()
{
	delete m_dialogFactory;
	delete m_controlFactory;
	removeTmpFile();
//	FcFini();
}

void XAP_UnixApp::removeTmpFile(void)
{
	if(m_szTmpFile)
	{
		//
		// Remove the tempfile if it exists
		//
		g_unlink(m_szTmpFile);
		g_free(m_szTmpFile);
	}
	m_szTmpFile = nullptr;
}

//
// Write an image buffer to a secure temp file and start a GTK drag
// offering it as a file copy. Shared by the frame-edit and
// inline-image drag-out paths.
//
bool XAP_UnixApp::dragImageToFile(GtkWidget * window,
								  const UT_ConstByteBufPtr & pBuf,
								  gint x, gint y)
{
	//
	// g_file_open_tmp creates the file securely (unpredictable name,
	// O_EXCL) - a predictable name here would be a symlink-attack
	// vector.
	//
	removeTmpFile();
	gchar *pszTmpPath = nullptr;
	int iTmpFd = g_file_open_tmp("abinova-XXXXXX.png", &pszTmpPath, nullptr);
	if (iTmpFd == -1)
	{
		return false;
	}
	UT_UTF8String sTmpF = pszTmpPath;
	g_free(pszTmpPath);
	FILE * fd = fdopen(iTmpFd,"w");
	if (fd)
	{
		fwrite(pBuf->getPointer(0),sizeof(UT_Byte),pBuf->getLength(),fd);
		fclose(fd);
	}
	else
	{
		close(iTmpFd);
		g_unlink(sTmpF.utf8_str());
		return false;
	}

	//
	// GTK4: drag a GFile; the content provider offers text/uri-list
	//
	GdkSurface * surface = gtk_native_get_surface(GTK_NATIVE(window));
	GdkSeat * seat = gdk_display_get_default_seat(gtk_widget_get_display(window));
	GdkDevice * device = seat ? gdk_seat_get_pointer(seat) : nullptr;
	GFile * tmpFile = g_file_new_for_path(sTmpF.utf8_str());
	GdkContentProvider * content =
		gdk_content_provider_new_typed(G_TYPE_FILE, tmpFile);
	g_object_unref(tmpFile);
	if (surface && device)
		gdk_drag_begin(surface, device, content, GDK_ACTION_COPY, x, y);
	g_object_unref(content);
	m_szTmpFile = g_strdup(sTmpF.utf8_str());
	UT_DEBUGMSG(("Created Tmp File %s XApp %s \n",sTmpF.utf8_str(),m_szTmpFile));
	return true;
}

bool XAP_UnixApp::initialize(const char * szKeyBindingsKey, const char * szKeyBindingsDefaultValue)
{
	// let our base class do it's thing.
	
	XAP_App::initialize(szKeyBindingsKey, szKeyBindingsDefaultValue);

	// do any thing we need here...

	return true;
}

void XAP_UnixApp::shutdown()
{
}

void XAP_UnixApp::reallyExit()
{
	g_application_quit(G_APPLICATION(m_gtkApp));
}

XAP_DialogFactory* XAP_UnixApp::getDialogFactory() const
{
	return m_dialogFactory;
}

XAP_Toolbar_ControlFactory * XAP_UnixApp::getControlFactory() const
{
	return m_controlFactory;
}

void XAP_UnixApp::setWinGeometry(int x, int y, UT_uint32 width, UT_uint32 height,
								 UT_uint32 flags)
{
	// TODO : do some range checking?
	m_geometry.x = x;
	m_geometry.y = y;
	m_geometry.width = width;
	m_geometry.height = height;
	m_geometry.flags = flags;
}

void XAP_UnixApp::getWinGeometry(int * x, int * y, UT_uint32 * width,
								 UT_uint32 * height, UT_uint32 * flags)
{
	UT_return_if_fail(x && y && width && height);
	*x = m_geometry.x;
	*y = m_geometry.y;
	*width = m_geometry.width;
	*height = m_geometry.height;
	*flags = m_geometry.flags;
}

// This should be removed at some time.
void XAP_UnixApp::migrate(const char *oldName,
                          const char *newName, const char *path) const
{
    if (path && newName && oldName && (*oldName == '/')) {

        const char* end = strrchr(path, '/');
#ifdef G_OS_WIN32
        const char* endBS = strrchr(path, '\\');
        if (endBS && (!end || endBS > end))
            end = endBS;
#endif
        if (!end) {
            UT_WARNINGMSG(("invalid path '%s', directory separator not found", path));
            return;
        }

        std::string old(path, end);
        old += oldName;

        if (g_access(old.c_str(), F_OK) == 0) {
            UT_WARNINGMSG(("Renaming: %s -> %s\n", old.c_str(), path));
            g_rename(old.c_str(), path);
        }
    }
}

const char * XAP_UnixApp::getUserPrivateDirectory() const
{
    /* return a pointer to a static buffer */
    static std::string private_dir;

    if (private_dir.empty()) {
        const char * szAbiDir = "abinova";

        // g_get_user_config_dir() honours XDG_CONFIG_HOME / ~/.config on
        // POSIX and returns the Roaming profile dir on Windows.
        private_dir = g_get_user_config_dir();

        private_dir += '/';
        private_dir += szAbiDir;

        // migration / legacy
        // XXX shouldn't that be /.AbiSuite ?
        migrate("/AbiSuite", szAbiDir, private_dir.c_str());
        migrate("/abiword", szAbiDir, private_dir.c_str());
    }

    return private_dir.c_str();
}


void XAP_UnixApp::_setAbiSuiteLibDir()
{
	// FIXME: this code sucks hard

	char * buf = nullptr;
	
	// see if ABINOVA_DATADIR was set in the environment
	const char * sz = getenv("ABINOVA_DATADIR");
	if (sz && *sz)
	{
		int len = strlen(sz);
		buf = static_cast<gchar *>(g_malloc(len+1));
		strcpy(buf,sz);
		char * p = buf;
		if ( (len > 1) && (p[0]=='"') && (p[len-1]=='"') )
		{
			// trim leading and trailing DQUOTES
			p[len-1]=0;
			p++;
			len -= 2;
		}
		// trim trailing separator ('\\' matters on Windows too)
		if ( (len > 0) && G_IS_DIR_SEPARATOR(p[len-1]) )
			p[len-1] = 0;
		XAP_App::_setAbiSuiteLibDir(p);
		g_free(buf);
		return;
	}

	// otherwise, use the hard-coded value
	XAP_App::_setAbiSuiteLibDir(getAbiSuiteHome());

	// Running from the build tree: the install prefix points at an
	// empty (or not-yet-installed) datadir, so bundled content like
	// the artwork galleries would be unreachable. Walk up from the
	// executable path looking for the source root (identified by its
	// artwork/ directory) and prefer it when the configured dir has
	// no artwork of its own.
	{
		std::string artdir = getAbiSuiteLibDir();
		artdir += "/artwork";
		if (g_access(artdir.c_str(), F_OK) != 0)
		{
			gchar * exe = g_file_read_link("/proc/self/exe", nullptr);
			if (exe)
			{
				gchar * dir = g_path_get_dirname(exe);
				for (int up = 0; up < 5 && dir; up++)
				{
					gchar * cand = g_build_filename(dir, "artwork", nullptr);
					if (g_file_test(cand, G_FILE_TEST_IS_DIR))
					{
						XAP_App::_setAbiSuiteLibDir(dir);
						g_free(cand);
						break;
					}
					g_free(cand);
					gchar * parent = g_path_get_dirname(dir);
					if (0 == strcmp(parent, dir))
					{
						g_free(parent);
						break;
					}
					g_free(dir);
					dir = parent;
				}
				g_free(dir);
				g_free(exe);
			}
		}
	}

	return;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

void XAP_UnixApp::setTimeOfLastEvent(UT_uint32 eventTime)
{
	m_eventTime = eventTime;
}

