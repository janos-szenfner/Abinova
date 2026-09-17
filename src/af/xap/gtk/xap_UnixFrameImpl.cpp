/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiSource Application Framework
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2002 William Lachance
 * Copyright (C) 2019-2026 Hubert Figuière
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <vector>

#include "ap_Features.h"
#include "ut_string.h"
#include "ut_types.h"
#include "ut_assert.h"
#include "ut_files.h"
#include "ut_misc.h"
#include "ut_sleep.h"
#include "xap_ViewListener.h"
#include "xap_UnixApp.h"
#include "xap_UnixFrameImpl.h"
#include "xap_Frame.h"
#include "ev_UnixKeyboard.h"
#include "ev_UnixMouse.h"
#include "ev_UnixMenuBar.h"
#include "ev_UnixMenuPopup.h"
#include "ev_UnixToolbar.h"
#include "ev_EditMethod.h"
#include "xav_View.h"
#include "fv_View.h"
#include "fv_FrameEdit.h"
#include "fl_DocLayout.h"
#include "xad_Document.h"
#include "gr_CairoGraphics.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_UnixDialogHelper.h"
#include "xap_UnixClipboard.h"
#include "xap_Strings.h"
#include "xap_Prefs.h"
#include "ap_FrameData.h"
#include "ap_UnixFrame.h"
#include "ev_Mouse.h"

#include "ie_types.h"
#include "ie_imp.h"
#include "ie_impGraphic.h"
#include "fg_Graphic.h"

enum: uint8_t {
	TARGET_DOCUMENT, // 0, to sync with gtk_drag_dest_add_text_target's default info value
 	TARGET_IMAGE,
	TARGET_URI_LIST,
	TARGET_URL,
	TARGET_UNKNOWN
};

static const struct {
	const char * mime;
	int          target;
} XAP_UnixFrameImpl__knownDragTypes[] = {
	{"text/uri-list", 	TARGET_URI_LIST},
	{"_NETSCAPE_URL", 	TARGET_URL},
	{"image/gif", 		TARGET_IMAGE},
	{"image/jpeg", 		TARGET_IMAGE},
	{"image/png", 		TARGET_IMAGE},
	{"image/tiff", 		TARGET_IMAGE},
	{"image/vnd", 		TARGET_IMAGE},
	{"image/bmp", 		TARGET_IMAGE},
	{"image/x-xpixmap", TARGET_IMAGE},

    // RDF types
    {"text/x-vcard", TARGET_DOCUMENT}
};

struct DragInfo {
	DragInfo() = default;
	std::vector<std::string> mimes;
	std::vector<int>         targets;

	void addEntry(const char * target, int info)
	{
		mimes.push_back(target);
		targets.push_back(info);
	}

	guint count() const { return static_cast<guint>(mimes.size()); }

private:
	DragInfo& operator=(const DragInfo & rhs);
	DragInfo(const DragInfo & rhs);
};

/*!
 * Build targets table from supported mime types.
 */
static DragInfo * s_getDragInfo ()
{
	static DragInfo dragInfo;
	static bool		isInitialized = FALSE;

	if (isInitialized) {
		return &dragInfo;
	}

	std::vector<std::string>::const_iterator iter;
	std::vector<std::string>::const_iterator end;

	// static types
	for (gsize idx = 0; idx < G_N_ELEMENTS(XAP_UnixFrameImpl__knownDragTypes); idx++) {
		dragInfo.addEntry(XAP_UnixFrameImpl__knownDragTypes[idx].mime,
						   XAP_UnixFrameImpl__knownDragTypes[idx].target);
	}

	// document types
    {
        const std::vector<std::string> &mimeTypes = IE_Imp::getSupportedMimeTypes();
        iter = mimeTypes.begin();
        end = mimeTypes.end();
        while (iter != end) {
            dragInfo.addEntry((*iter).c_str(), TARGET_DOCUMENT);
            iter++;
        }
    }

	// image types
    {
        const std::vector<std::string> &mimeTypes = IE_ImpGraphic::getSupportedMimeTypes();
        iter = mimeTypes.begin();
        end = mimeTypes.end();
        while (iter != end) {
            dragInfo.addEntry((*iter).c_str(), TARGET_IMAGE);
            iter++;
        }
    }

	isInitialized = TRUE;

	return &dragInfo;
}

/*!
 * Mime types we accept on the drop target: the drag-info table plus the
 * plain-text types gtk_drag_dest_add_text_targets() used to add.
 */
static GdkContentFormats * s_getDropFormats()
{
	static GdkContentFormats * formats = nullptr;
	if (formats)
		return formats;

	DragInfo * dragInfo = s_getDragInfo();
	std::vector<const char*> mimes;
	mimes.reserve(dragInfo->mimes.size());
	for (const std::string & m : dragInfo->mimes)
		mimes.push_back(m.c_str());
	static const char * textMimes[] = {
		"text/plain;charset=utf-8",
		"text/plain",
		"UTF8_STRING",
		"STRING",
		"TEXT",
		"COMPOUND_TEXT",
	};
	for (gsize i = 0; i < G_N_ELEMENTS(textMimes); i++)
		mimes.push_back(textMimes[i]);

	formats = gdk_content_formats_new(mimes.data(), mimes.size());
	return formats;
}

static int s_targetForMime(const char * mime)
{
	DragInfo * dragInfo = s_getDragInfo();
	for (size_t i = 0; i < dragInfo->mimes.size(); i++)
		if (!g_ascii_strcasecmp(mime, dragInfo->mimes[i].c_str()))
			return dragInfo->targets[i];
	return TARGET_UNKNOWN;
}

static int s_mapMimeToUriType (const char * uri)
{
	if (!uri || !strlen(uri))
		return TARGET_UNKNOWN;

	int target = TARGET_UNKNOWN;

	gchar *mimeType;

	mimeType = UT_go_get_mime_type (uri);

	if (g_ascii_strcasecmp (mimeType, "application/octet-stream") == 0) {
		FREEP (mimeType);
		std::string suffix = UT_pathSuffix(uri);
		if (!suffix.empty()) {
			const gchar *mt = IE_Imp::getMimeTypeForSuffix(suffix.c_str());
			if (!mt) {
				mt = IE_ImpGraphic::getMimeTypeForSuffix(suffix.c_str());
			}
			if (mt) {
				/* we to g_free that later */
				mimeType = g_strdup(mt);
			}
			else {
				return target;
			}
		}
		else {
			return target;
		}
	}

	UT_DEBUGMSG(("DOM: mimeType %s dropped into AbiWord(%s)\n", mimeType, uri));

	DragInfo * dragInfo = s_getDragInfo();
	for (size_t i = 0; i < dragInfo->mimes.size(); i++)
		if (!g_ascii_strcasecmp (mimeType, dragInfo->mimes[i].c_str())) {
			target = dragInfo->targets[i];
			break;
		}

	g_free (mimeType);
	return target;
}

static void
s_loadImage (const UT_UTF8String & file, FV_View * pView, XAP_Frame * pF, gint x, gint y)
{
	FG_ConstGraphicPtr pFG;
	UT_Error error = IE_ImpGraphic::loadGraphic(file.utf8_str(), 0, pFG);
	if (error != UT_OK || !pFG)
		{
			UT_DEBUGMSG(("Dom: could not import graphic (%s)\n", file.utf8_str()));
			return;
		}
	UT_sint32 xoff = static_cast<AP_UnixFrame*>(pF)->getDocumentAreaXoff();
	UT_sint32 yoff = static_cast<AP_UnixFrame*>(pF)->getDocumentAreaYoff();
	UT_sint32 mouseX = x - xoff;
	UT_sint32 mouseY = y - yoff;
	UT_DEBUGMSG(("x %d xoff %d y %d yoff %d ",y,xoff,y,yoff));
	if(pView && pView->getGraphics())
		mouseX = pView->getGraphics()->tlu(mouseX);
	if(pView && pView->getGraphics())
		mouseY = pView->getGraphics()->tlu(mouseY);
#ifdef DEBUG
	double xInch = (double) mouseX/1440.;
	double yInch = (double) mouseY/1440.;
#endif

	UT_DEBUGMSG(("Insert Image at logical (x,y) %d %d \n",mouseX,mouseY));

	UT_DEBUGMSG(("Insert Image at x %f in y %f in \n",xInch,yInch));
	pView->cmdInsertPositionedGraphic(pFG,mouseX,mouseY);
}

static void
s_loadImage (const UT_ConstByteBufPtr & bytes, FV_View * pView, XAP_Frame * pF, gint x, gint y)
{
	FG_ConstGraphicPtr pFG;
	UT_Error error = IE_ImpGraphic::loadGraphic(bytes, 0, pFG);
	if (error != UT_OK || !pFG)
		{
			UT_DEBUGMSG(("JK: could not import graphic from data buffer\n"));
			return;
		}
	UT_sint32 xoff = static_cast<AP_UnixFrame*>(pF)->getDocumentAreaXoff();
	UT_sint32 yoff = static_cast<AP_UnixFrame*>(pF)->getDocumentAreaYoff();
	UT_sint32 mouseX = x - xoff;
	UT_sint32 mouseY = y - yoff;
	UT_DEBUGMSG(("x %d newX %d y %d newY %d ",y,xoff,y,yoff));
	if(pView && pView->getGraphics())
		mouseX = pView->getGraphics()->tlu(mouseX);
	if(pView && pView->getGraphics())
		mouseY = pView->getGraphics()->tlu(mouseY);

	pView->cmdInsertPositionedGraphic(pFG,mouseX,mouseY);
}

static void
s_loadDocument (const UT_UTF8String & file, XAP_Frame * pFrame)
{
	XAP_Frame * pNewFrame = nullptr;
	if (pFrame->isDirty() || pFrame->getFilename() ||
		(pFrame->getViewNumber() > 0))
		pNewFrame = XAP_App::getApp()->newFrame ();
	else
		pNewFrame = pFrame;


	UT_Error error = pNewFrame->loadDocument(file.utf8_str(), 0 /* IEFT_Unknown */);
	if (error)
		{
			// TODO: warn user that we couldn't open that file
			// TODO: we crash if we just delete this without putting something
			// TODO: in it, so let's go ahead and open an untitled document
			// TODO: for now.
			UT_DEBUGMSG(("DOM: couldn't load document %s\n", file.utf8_str()));
			pNewFrame->loadDocument((const char *)nullptr, 0 /* IEFT_Unknown */);
		}
}

static void s_pasteFile(const UT_UTF8String & file, XAP_Frame * pFrame)
{
	    if(!pFrame)
			return;
	    XAP_App * pApp = XAP_App::getApp();
	    PD_Document * newDoc = new PD_Document();
	    UT_Error err = newDoc->readFromFile(file.utf8_str(), IEFT_Unknown);
	    if ( err != UT_OK )
		{
			UNREFP(newDoc);
			return;
		}
		FV_View * pView = static_cast<FV_View *>(pFrame->getCurrentView());
		// we'll share the same graphics context, which won't matter because
		// we only use it to get font metrics and stuff and not actually draw
		GR_Graphics *pGraphics = pView->getGraphics();
	    // create a new layout and view object for the doc
	    FL_DocLayout *pDocLayout = new FL_DocLayout(newDoc,pGraphics);
	    FV_View copyView(pApp, nullptr, pDocLayout);

	    pDocLayout->setView (&copyView);
	    pDocLayout->fillLayouts();

	    copyView.cmdSelect(0, 0, FV_DOCPOS_BOD, FV_DOCPOS_EOD); // select all the contents of the new doc
	    copyView.cmdCopy(); // copy the contents of the new document
	    pView->cmdPaste ( true ); // paste the contents into the existing document honoring the formatting

	    DELETEP(pDocLayout);
	    UNREFP(newDoc);
	    return ;
}

static void
s_loadUri (XAP_Frame * pFrame, const char * uri,gint x, gint y)
{
	FV_View * pView  = static_cast<FV_View*>(pFrame->getCurrentView ());

	int type = s_mapMimeToUriType (uri);
	if (type == TARGET_UNKNOWN)
		{
			UT_DEBUGMSG(("DOM: unknown uri type: %s\n", uri));
			return;
		}

	if (type == TARGET_IMAGE)
		{
			s_loadImage (uri, pView,pFrame,x,y);
			return;
		}
	else
		{
			if(pFrame)
			{
				AP_FrameData *pFrameData = static_cast<AP_FrameData *>(pFrame->getFrameData());
				if(pFrameData && pFrameData->m_bIsWidget)
				{
					s_pasteFile(uri,pFrame);
				}
				else if(pFrame->isDirty() || pFrame->getFilename())
				{
					s_pasteFile(uri,pFrame);
				}
				else
				{
					s_loadDocument (uri, pFrame);
				}
			}
		}
}

static void
s_loadUriList (XAP_Frame * pFrame,  const char * uriList,gint x, gint y)
{
	gchar ** uris = g_uri_list_extract_uris(uriList);
	gchar ** uriIter = uris;

	while (uriIter && *uriIter) {
		s_loadUri(pFrame,*uriIter,x,y);
		uriIter++;
	}
	g_strfreev(uris);
}

static void
s_pasteText (XAP_Frame * pFrame, const char * target_name,
			 const unsigned char * data, UT_uint32 data_length)
{
	FV_View   * pView  = static_cast<FV_View*>(pFrame->getCurrentView ());
	PD_Document * pDoc = pView->getDocument ();

	IEFileType file_type = IEFT_Unknown;

	file_type = IE_Imp::fileTypeForMimetype (target_name);
	if (file_type == IEFT_Unknown)
		file_type = IE_Imp::fileTypeForContents (reinterpret_cast<const char *>(data), data_length);

	if (file_type != IEFT_Unknown)
		{
			IE_Imp * importer = nullptr;

			if (UT_OK == IE_Imp::constructImporter (pDoc, file_type, &importer) && importer)
				{
					PD_DocumentRange dr(pDoc, pView->getPoint(), pView->getPoint());
					importer->pasteFromBuffer(&dr, data, data_length);

					delete importer;
				}
		}
}

/* GTK4 drop handling: a GtkDropTargetAsync is attached to the top-level
 * window; on "drop" we pick the best offered mime type and read it. */

static void
s_dropDispatch(XAP_Frame * pFrame, const char * targetName, int target,
			   const guchar * data, gsize len, gdouble x, gdouble y)
{
	FV_View   * pView  = static_cast<FV_View*>(pFrame->getCurrentView ());

	UT_DEBUGMSG(("JK: target in drop = %s \n", targetName));

	if (target == TARGET_URI_LIST)
	{
		const char * rawChar = reinterpret_cast<const char *>(data);
		UT_DEBUGMSG(("DOM: text in selection = %s \n", rawChar));
		s_loadUriList (pFrame,rawChar,static_cast<gint>(x),static_cast<gint>(y));
	}
	else if (target == TARGET_DOCUMENT)
	{
		UT_DEBUGMSG(("JK: Document target as data buffer\n"));
		s_pasteText (pFrame, targetName, data, len);
	}
	else if (target == TARGET_IMAGE)
	{
		UT_ByteBufPtr bytes(new UT_ByteBuf(len));

		UT_DEBUGMSG(("JK: Image target\n"));
		bytes->append (data, len);
		s_loadImage (bytes, pView,pFrame,static_cast<gint>(x),static_cast<gint>(y));
	}
	else if (target == TARGET_URL)
	{
		// NUL-terminate; the stream data may not be
		UT_UTF8String sUriRaw(reinterpret_cast<const char *>(data), len);
		const char * uri = sUriRaw.utf8_str();
		UT_DEBUGMSG(("DOM: hyperlink: %s\n", uri));
		//
		// Look to see if this is actually an image.
		//
		std::string suffix = UT_pathSuffix(uri);
		if (!suffix.empty())
		{
			UT_DEBUGMSG(("Suffix of uri is %s \n",suffix.c_str()));
			if ((suffix.substr(1,3) == "jpg") ||
				  (suffix.substr(1,4) == "jpeg") ||
				  (suffix.substr(1,3) == "png") ||
				  (suffix.substr(1,3) == "svg") ||
				  (suffix.substr(1,3) == "gif"))
			{

				UT_UTF8String sUri = uri;
				UT_uint32 i = 0;
				if(sUri.length())
				{
					for(i=0;i<sUri.length()-1;i++)
					{
						if((sUri.substr(i,1) == "\n") ||
						   (sUri.substr(i,1) == " ")  )
						{
							sUri = sUri.substr(0,i);
							break;
						}
					}
				}
				UT_DEBUGMSG(("trimmed Uri is (%s) \n",sUri.utf8_str()));
				s_loadImage(sUri,pView,pFrame,static_cast<gint>(x),static_cast<gint>(y));
				return;
			}
		}
		if (pView)
			pView->cmdInsertHyperlink(uri);
	}
}

struct DropReadCtx
{
	GMainLoop * loop;
	GInputStream * stream;
};

static void s_drop_read_cb(GObject *src, GAsyncResult *res, gpointer data)
{
	DropReadCtx * ctx = static_cast<DropReadCtx*>(data);
	const char * mime = nullptr;
	ctx->stream = gdk_drop_read_finish(GDK_DROP(src), res, &mime, nullptr);
	g_main_loop_quit(ctx->loop);
}

static void
s_drop_cb(GtkDropTargetAsync * /*target*/, GdkDrop *drop,
		  gdouble x, gdouble y, gpointer data)
{
	UT_DEBUGMSG(("DOM: dnd_drop_event being handled\n"));

	XAP_UnixFrameImpl * pFrameImpl = static_cast<XAP_UnixFrameImpl*>(data);
	XAP_Frame * pFrame = pFrameImpl->getFrame ();
	GdkContentFormats * formats = gdk_drop_get_formats(drop);
	DragInfo * dragInfo = s_getDragInfo();

	// pick the preferred offered mime type; dragInfo order is priority order
	const char * mime = nullptr;
	for (size_t i = 0; i < dragInfo->mimes.size() && !mime; i++)
	{
		if (gdk_content_formats_contain_mime_type(formats, dragInfo->mimes[i].c_str()))
			mime = dragInfo->mimes[i].c_str();
	}
	if (!mime)
	{
		static const char * textMimes[] = {
			"text/plain;charset=utf-8", "text/plain", "UTF8_STRING",
			"STRING", "TEXT", "COMPOUND_TEXT",
		};
		for (gsize i = 0; i < G_N_ELEMENTS(textMimes) && !mime; i++)
			if (gdk_content_formats_contain_mime_type(formats, textMimes[i]))
				mime = textMimes[i];
	}
	if (!mime)
		return;

	// read the drop data through a nested main loop, like the GTK3
	// synchronous gtk_selection_data path did
	DropReadCtx ctx;
	ctx.loop = g_main_loop_new(nullptr, FALSE);
	ctx.stream = nullptr;

	const char * mimes[] = { mime, nullptr };
	gdk_drop_read_async(drop, mimes, G_PRIORITY_DEFAULT, nullptr,
						s_drop_read_cb, &ctx);
	g_main_loop_run(ctx.loop);
	g_main_loop_unref(ctx.loop);

	if (ctx.stream)
	{
		UT_ByteBuf buf;
		guchar chunk[8192];
		gssize n;
		while ((n = g_input_stream_read(ctx.stream, chunk, sizeof(chunk),
										nullptr, nullptr)) > 0)
		{
			buf.append(chunk, n);
		}
		g_object_unref(ctx.stream);

		if (buf.getLength() > 0)
			s_dropDispatch(pFrame, mime, s_targetForMime(mime),
						   buf.getPointer(0), buf.getLength(), x, y);
	}
}

XAP_UnixFrameImpl::XAP_UnixFrameImpl(XAP_Frame *pFrame) :
	XAP_FrameImpl(pFrame),
	m_imContext(nullptr),
	m_wTopLevelWindow(nullptr),
	m_pUnixMenu(nullptr),
	need_im_reset (false),
	m_bDoZoomUpdate(false),
	m_iNewX(0),
	m_iNewY(0),
	m_iNewWidth(0),
	m_iNewHeight(0),
	m_iZoomUpdateID(0),
	m_iAbiRepaintID(0),
	m_iScrollIdleID(0),
	m_bScrollWait(false),
	m_pUnixPopup(nullptr),
	m_dialogFactory(XAP_App::getApp(), pFrame),
	m_iPreeditLen (0),
	m_iPreeditStart (0)
{
}

XAP_UnixFrameImpl::~XAP_UnixFrameImpl()
{
	if(m_iZoomUpdateID) {
		g_source_remove(m_iZoomUpdateID);
	}

	// only delete the things we created...
	if(m_iAbiRepaintID)
	{
		g_source_remove(m_iAbiRepaintID);
	}
	if(m_iScrollIdleID)
	{
		g_source_remove(m_iScrollIdleID);
	}

	DELETEP(m_pUnixMenu);
	DELETEP(m_pUnixPopup);

	// unref the input method context
	if (m_imContext) {
		g_object_unref (G_OBJECT (m_imContext));
	}
}


void XAP_UnixFrameImpl::focusIMIn ()
{
	need_im_reset = true;
	gtk_im_context_focus_in(getIMContext());
	gtk_im_context_reset (getIMContext());
}

void XAP_UnixFrameImpl::focusIMOut ()
{
	need_im_reset = true;
	gtk_im_context_focus_out(getIMContext());
}

void XAP_UnixFrameImpl::resetIMContext()
{
  if (need_im_reset)
    {
      need_im_reset = false;
      gtk_im_context_reset (getIMContext());
    }
}

void XAP_UnixFrameImpl::_fe::focus_in_event(GtkEventControllerFocus * /*c*/, GtkWidget *w)
{
	XAP_UnixFrameImpl * pFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	UT_return_if_fail(pFrameImpl);

	XAP_Frame* pFrame = pFrameImpl->getFrame();
	g_object_set_data(G_OBJECT(w), "toplevelWindowFocus",
						GINT_TO_POINTER(TRUE));
	if (pFrame->getCurrentView())
	{
		pFrame->getCurrentView()->focusChange(AV_FOCUS_HERE);
	}
	pFrameImpl->focusIMIn ();
}

void XAP_UnixFrameImpl::_fe::focus_out_event(GtkEventControllerFocus * /*c*/, GtkWidget *w)
{
	XAP_UnixFrameImpl * pFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	UT_return_if_fail(pFrameImpl);

	XAP_Frame* pFrame = pFrameImpl->getFrame();
	g_object_set_data(G_OBJECT(w), "toplevelWindowFocus",
						GINT_TO_POINTER(FALSE));
	if (pFrame->getCurrentView())
		pFrame->getCurrentView()->focusChange(AV_FOCUS_NONE);
	pFrameImpl->focusIMOut();
}

void XAP_UnixFrameImpl::_fe::button_press_event(GtkGestureClick * g, gint n_press,
											  gdouble /*x*/, gdouble /*y*/, GtkWidget * w)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	GtkEventController * controller = GTK_EVENT_CONTROLLER(g);
	pUnixFrameImpl->setTimeOfLastEvent(gtk_event_controller_get_current_event_time(controller));
	AV_View * pView = pFrame->getCurrentView();
	EV_UnixMouse * pUnixMouse = static_cast<EV_UnixMouse *>(pFrame->getMouse());

	pUnixFrameImpl->resetIMContext ();

	if (pView)
		pUnixMouse->mouseClick(pView,
							   gtk_event_controller_get_current_event(controller),
							   n_press);
}

void XAP_UnixFrameImpl::_fe::button_release_event(GtkGestureClick * g, gint /*n_press*/,
												gdouble /*x*/, gdouble /*y*/, GtkWidget * w)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	GtkEventController * controller = GTK_EVENT_CONTROLLER(g);
	pUnixFrameImpl->setTimeOfLastEvent(gtk_event_controller_get_current_event_time(controller));
	AV_View * pView = pFrame->getCurrentView();

	EV_UnixMouse * pUnixMouse = static_cast<EV_UnixMouse *>(pFrame->getMouse());

	if (pView)
		pUnixMouse->mouseUp(pView,
							gtk_event_controller_get_current_event(controller));
}

/*!
 * Background zoom updater. It updates the view zoom level after all configure
 * events have been processed. This is
 */
gint XAP_UnixFrameImpl::_fe::do_ZoomUpdate(gpointer /* XAP_UnixFrameImpl * */ p)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(p);
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	FV_View * pView = static_cast<FV_View *>(pFrame->getCurrentView());
	UT_sint32 prevWidth = 0;
	UT_sint32 prevHeight = 0;
	UT_sint32 iNewWidth = 0;
	UT_sint32 iNewHeight = 0;
	if(pView)
	{
		prevWidth = pView->getGraphics()->tdu(pView->getWindowWidth());
		prevHeight = pView->getGraphics()->tdu(pView->getWindowHeight());
		iNewWidth = pUnixFrameImpl->m_iNewWidth;
		iNewHeight = pUnixFrameImpl->m_iNewHeight;
		xxx_UT_DEBUGMSG(("OldWidth %d NewWidth %d \n",prevWidth,iNewWidth));
	}
	if(!pView || pFrame->isFrameLocked() ||
	   ((pUnixFrameImpl->m_bDoZoomUpdate) && (prevWidth == iNewWidth) && (prevHeight == iNewHeight)))
	{
		if(!pView)
		{
			// source stays alive — keep m_iZoomUpdateID so the dtor can remove it
			return TRUE;
		}
		pUnixFrameImpl->m_iZoomUpdateID = 0;
		pUnixFrameImpl->m_bDoZoomUpdate = false;
		if(pView && !pFrame->isFrameLocked())
		{
				GR_Graphics * pGr = pView->getGraphics ();
				UT_Rect rClip;
				rClip.left = pGr->tlu(0);
				UT_sint32 iHeight = abs(iNewHeight - prevHeight);
				rClip.top = pGr->tlu(iNewHeight - iHeight);
				rClip.width = pGr->tlu(iNewWidth)+1;
				rClip.height = pGr->tlu(iHeight)+1;
				xxx_UT_DEBUGMSG(("Drawing in zoom at x %d y %d height %d width %d \n",rClip.left,rClip.top,rClip.height,rClip.width));
				pView->setWindowSize(iNewWidth, iNewHeight);
				if(!pView->isConfigureChanged())
				{
						pView->queueDraw(&rClip);
				}
				else
				{
						pView->queueDraw();
						pView->setConfigure(false);
				}
		}
		if(pView)
		{
			pView->setWindowSize(iNewWidth, iNewHeight);
		}
		return FALSE;
	}
	if(!pView || pFrame->isFrameLocked() ||
	   ((prevWidth == iNewWidth) && (pFrame->getZoomType() != XAP_Frame::z_WHOLEPAGE)))
	{
		xxx_UT_DEBUGMSG(("Abandoning zoom widths are equal \n"));
		if(!pView)
		{
			// source stays alive — keep m_iZoomUpdateID so the dtor can remove it
			return TRUE;
		}
		pUnixFrameImpl->m_iZoomUpdateID = 0;
		pUnixFrameImpl->m_bDoZoomUpdate = false;
		if(pView && !pFrame->isFrameLocked())
		{
				GR_Graphics * pGr = pView->getGraphics ();
				UT_Rect rClip;
				rClip.left = pGr->tlu(0);
				UT_sint32 iHeight = abs(iNewHeight - prevHeight);
				rClip.top = pGr->tlu(iNewHeight - iHeight);
				rClip.width = pGr->tlu(iNewWidth)+1;
				rClip.height = pGr->tlu(iHeight)+1;
				xxx_UT_DEBUGMSG(("Drawing in zoom at x %d y %d height %d width %d \n",rClip.left,rClip.top,rClip.height,rClip.width));
				pView->setWindowSize(iNewWidth, iNewHeight);
				if(!pView->isConfigureChanged())
				{
						pView->queueDraw(&rClip);
				}
				else
				{
						pView->queueDraw();
						pView->setConfigure(false);
				}
		}
		if(pView)
			pView->setWindowSize(iNewWidth, iNewHeight);
		return FALSE;
	}

	pUnixFrameImpl->m_bDoZoomUpdate = true;
	UT_sint32 iLoop = 0;
	do
	{
		// currently, we blow away the old view.  This will change, rendering
		// the loop superfluous.
		pView = static_cast<FV_View *>(pFrame->getCurrentView());

		if(!pView)
		{
			pUnixFrameImpl->m_iZoomUpdateID = 0;
			pUnixFrameImpl->m_bDoZoomUpdate = false;
			return FALSE;
		}

		// oops, we're not ready yet.
		if (pView->isLayoutFilling())
		{
			pUnixFrameImpl->m_iZoomUpdateID = 0;
			pUnixFrameImpl->m_bDoZoomUpdate = false;
			return FALSE;
		}

		iNewWidth = pUnixFrameImpl->m_iNewWidth;
		iNewHeight = pUnixFrameImpl->m_iNewHeight;
		//
		// In web mode we reflow the text to changed page set at the
		// current zoom.
		//
		if((pView->getViewMode() == VIEW_WEB) && (abs(iNewWidth -prevWidth) > 2) && (prevWidth > 10) && (iNewWidth > 10))
		{
			pView->setWindowSize(iNewWidth, iNewHeight);
			UT_sint32 iAdjustZoom = pView->calculateZoomPercentForPageWidth();
			FL_DocLayout * pLayout = pView->getLayout();
			PD_Document * pDoc = pLayout->getDocument();
			UT_Dimension orig_ut = DIM_IN;
			orig_ut = pLayout->m_docViewPageSize.getDims();
			double orig_width = pDoc->m_docPageSize.Width(orig_ut);
			double orig_height = pDoc->m_docPageSize.Height(orig_ut);
			double rat = static_cast<double>(iAdjustZoom)/static_cast<double>(pView->getGraphics()->getZoomPercentage()) ;
			double new_width = orig_width*rat;
			UT_DEBUGMSG(("VIEW_WEB old width %f new width %f old height %f \n",orig_width,new_width,orig_height));
			bool isPortrait = pLayout->m_docViewPageSize.isPortrait();
			pLayout->m_docViewPageSize.Set(new_width,orig_height,orig_ut);
			pLayout->m_docViewPageSize.Set(fp_PageSize::psCustom,orig_ut);
			if(isPortrait)
			{
				pLayout->m_docViewPageSize.setPortrait();
			}
			else
			{
				pLayout->m_docViewPageSize.setLandscape();
			}
			pView->rebuildLayout();
			pView->updateScreen(false);
			//
			// We're done. No more calls needed
			//
			return TRUE;
		}
		//
		// If we are here in view_web we just return
		//
		pView->setWindowSize(iNewWidth, iNewHeight);
		if(pView->getViewMode() == VIEW_WEB)
		{
			return TRUE;
		}
		pFrame->quickZoom(); // was update zoom
		iLoop++;
	}
	while(((iNewWidth != pUnixFrameImpl->m_iNewWidth) || (iNewHeight != pUnixFrameImpl->m_iNewHeight)) 
		&& (iLoop < 10));

	pUnixFrameImpl->m_iZoomUpdateID = 0;
	pUnixFrameImpl->m_bDoZoomUpdate = false;
	return FALSE;
}

void XAP_UnixFrameImpl::_fe::resize_event(GtkDrawingArea * /*area*/, gint width,
										gint height, GtkWidget * w)
{
	// This is basically a resize event.

	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	AV_View * pView = pFrame->getCurrentView();
	if (pView)
	{
		if (pUnixFrameImpl->m_iNewWidth == width &&
		    pUnixFrameImpl->m_iNewHeight == height)
			return;
		pUnixFrameImpl->m_iNewWidth = width;
		pUnixFrameImpl->m_iNewHeight = height;
		pUnixFrameImpl->m_iNewY = 0;
		pUnixFrameImpl->m_iNewX = 0;
		xxx_UT_DEBUGMSG(("Drawing in zoom at height %d width %d \n", height, width));
		XAP_App * pApp = XAP_App::getApp();
		UT_sint32 x,y;
		UT_uint32 savedWidth,savedHeight,flags;
		pApp->getGeometry(&x,&y,&savedWidth,&savedHeight,&flags);
//
// Who ever wants to change this code in the future. The height and widths you
// get from the event struct are the height and widths of the drawable area of
// the screen. We want the height and width of the entire widget which we get
// from the m_wTopLevelWindow widget.
// -- MES
//

        GtkWindow * pWin = nullptr;
		if(pFrame->getFrameMode() == XAP_NormalFrame) {
			pWin = GTK_WINDOW(pUnixFrameImpl->m_wTopLevelWindow);
			// worth remembering size?
			GdkSurface * surface = gtk_native_get_surface(GTK_NATIVE(pWin));
			GdkToplevelState state = surface ?
				gdk_toplevel_get_state(GDK_TOPLEVEL(surface)) :
				static_cast<GdkToplevelState>(0);
			if (!(state & (GDK_TOPLEVEL_STATE_MINIMIZED |
				  GDK_TOPLEVEL_STATE_MAXIMIZED |
				  GDK_TOPLEVEL_STATE_FULLSCREEN))) {

				pApp->setGeometry(0, 0,
								  gtk_widget_get_width(GTK_WIDGET(pWin)),
								  gtk_widget_get_height(GTK_WIDGET(pWin)),
								  flags);
			}
		}

		// Dynamic Zoom Implementation

		if(!pUnixFrameImpl->m_bDoZoomUpdate && (pUnixFrameImpl->m_iZoomUpdateID == 0))
		{
			pUnixFrameImpl->m_iZoomUpdateID = g_idle_add(reinterpret_cast<GSourceFunc>(do_ZoomUpdate), static_cast<gpointer>(pUnixFrameImpl));
		}
			
	}
	gtk_widget_grab_focus(w);
}

void XAP_UnixFrameImpl::_fe::motion_notify_event(GtkEventControllerMotion * c,
												 gdouble /*x*/, gdouble /*y*/,
												 GtkWidget * w)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	GtkEventController * controller = GTK_EVENT_CONTROLLER(c);
	GdkEvent * e = gtk_event_controller_get_current_event(controller);
	if (!e)
		return;

	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	pUnixFrameImpl->setTimeOfLastEvent(gtk_event_controller_get_current_event_time(controller));
	AV_View * pView = pFrame->getCurrentView();
	EV_UnixMouse * pUnixMouse = static_cast<EV_UnixMouse *>(pFrame->getMouse());

	if (pView)
		pUnixMouse->mouseMotion(pView, e);
}

gboolean XAP_UnixFrameImpl::_fe::scroll_notify_event(GtkEventControllerScroll * c,
												   gdouble /*dx*/, gdouble /*dy*/,
												   GtkWidget * w)
{
	xxx_UT_DEBUGMSG(("Scroll event \n"));
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	GtkEventController * controller = GTK_EVENT_CONTROLLER(c);
	GdkEvent * e = gtk_event_controller_get_current_event(controller);
	if (!e)
		return FALSE;

	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	pUnixFrameImpl->setTimeOfLastEvent(gtk_event_controller_get_current_event_time(controller));
	AV_View * pView = pFrame->getCurrentView();
	EV_UnixMouse * pUnixMouse = static_cast<EV_UnixMouse *>(pFrame->getMouse());

	if (pView)
		pUnixMouse->mouseScroll(pView, e);

	return TRUE;
}

gboolean XAP_UnixFrameImpl::_fe::key_release_event(GtkEventControllerKey * c,
												 guint keyval, guint /*keycode*/,
												 GdkModifierType /*state*/,
												 GtkWidget * w)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	GdkEvent * e = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(c));

	// Let IM handle the event first.
	if (e && gtk_im_context_filter_keypress(pUnixFrameImpl->getIMContext(), e)) {
		UT_DEBUGMSG(("IMCONTEXT keyevent swallow: %u\n", keyval));
		pUnixFrameImpl->queueIMReset ();
	    return FALSE;
	}
	return TRUE;
}

gboolean XAP_UnixFrameImpl::_fe::key_press_event(GtkEventControllerKey * c,
											   guint keyval, guint /*keycode*/,
											   GdkModifierType state,
											   GtkWidget * w)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	GtkEventController * controller = GTK_EVENT_CONTROLLER(c);
	GdkEvent * e = gtk_event_controller_get_current_event(controller);

	// Let IM handle the event first.
	if (e && gtk_im_context_filter_keypress(pUnixFrameImpl->getIMContext(), e)) {
		pUnixFrameImpl->queueIMReset ();

		if ((state & GDK_ALT_MASK) ||
			(state & GDK_SUPER_MASK) ||
			(state & GDK_HYPER_MASK) ||
			(state & GDK_META_MASK))
			return FALSE;

		// ... else, stop this event
		return TRUE;
	}

	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	pUnixFrameImpl->setTimeOfLastEvent(gtk_event_controller_get_current_event_time(controller));
	AV_View * pView = pFrame->getCurrentView();
	ev_UnixKeyboard * pUnixKeyboard = static_cast<ev_UnixKeyboard *>(pFrame->getKeyboard());

	if (pView && e)
		pUnixKeyboard->keyPressEvent(pView, e);

	// claim keys that would take the focus away from the document widget
	switch (keyval) {
	case GDK_KEY_Tab:
	case GDK_KEY_ISO_Left_Tab:
	case GDK_KEY_Left:
	case GDK_KEY_Up:
	case GDK_KEY_Right:
	case GDK_KEY_Down:
		return TRUE;
		break;
	}

	return FALSE;
}

gboolean XAP_UnixFrameImpl::_fe::close_request(GtkWindow * w, gpointer /*data*/)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	XAP_App * pApp = XAP_App::getApp();
	UT_return_val_if_fail(pApp,FALSE);

	UT_ASSERT(pApp->getMenuActionSet());

	const EV_EditMethodContainer * pEMC = pApp->getEditMethodContainer();
	UT_return_val_if_fail(pEMC,FALSE);

	// was "closeWindow", TRUE, FALSE
	const EV_EditMethod * pEM = pEMC->findEditMethodByName("closeWindowX");
	UT_ASSERT_HARMLESS(pEM);

	if (pEM)
	{
		if (pEM->Fn(pFrame->getCurrentView(),nullptr))
		{
			// returning FALSE means destroy the window, continue along the
			// chain of Gtk destroy events

			return FALSE;
		}
	}

	// returning TRUE means do NOT destroy the window; halt the message
	// chain so it doesn't see destroy
	return TRUE;
}

void XAP_UnixFrameImpl::_fe::draw(GtkDrawingArea * /*area*/, cairo_t *cr,
								  int /*width*/, int /*height*/, gpointer w)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	FV_View * pView = static_cast<FV_View *>(pUnixFrameImpl->getFrame()->getCurrentView());
	double x, y, width, height;
	cairo_clip_extents (cr, &x, &y, &width, &height);
	width -= x;
	height -= y;
/* Jean: commenting out next lines since the zoom update code does draw only
 * part of what needs to be updated. */
//	if((pUnixFrameImpl->m_bDoZoomUpdate) || (pUnixFrameImpl->m_iZoomUpdateID != 0))
//	{
//		return TRUE;
//	}
	if(pView)
	{
		GR_CairoGraphics * pGr = static_cast<GR_CairoGraphics*>(pView->getGraphics());
		UT_Rect rClip;
		if (pGr->getPaintCount () > 0)
			return;
		xxx_UT_DEBUGMSG(("Expose area: x %g y %g width %g  height %g \n",x,y,width,height));
		rClip.left = pGr->tlu(x);
		rClip.top = pGr->tlu(y);
		rClip.width = pGr->tlu(width);
		rClip.height = pGr->tlu(height);
		GR_UnixCairoGraphics *pUGr = static_cast<GR_UnixCairoGraphics*>(pGr);
		pUGr->beginFrame();
		pView->drawImmediate(&rClip);
		pUGr->endFrame(cr);
	}
}

class _ViewScroll
{
public:
	_ViewScroll(XAP_UnixFrameImpl * pImpl, AV_View * pView, UT_sint32 amount):
		m_pImpl(pImpl),m_pView(pView),m_amount(amount)
	{
	}
	XAP_UnixFrameImpl * m_pImpl;
	AV_View * m_pView;
	UT_sint32 m_amount;
};

gboolean XAP_UnixFrameImpl::_fe::_actualScroll(gpointer data)
{
	_ViewScroll * pVS = reinterpret_cast<_ViewScroll *>(data);
	AV_View * pView = pVS->m_pView;
	xxx_UT_DEBUGMSG(("vScrollSchanged callback\n"));
	pVS->m_pImpl->m_iScrollIdleID = 0;
	pVS->m_pImpl->m_bScrollWait = false;
	XAP_Frame * pFrame = pVS->m_pImpl->getFrame();
	if (pView && pFrame && pFrame->getCurrentView() == pView)
		pView->sendVerticalScrollEvent(pVS->m_amount);
	return FALSE;
}

static void _scrollDataFree(gpointer data)
{
	delete reinterpret_cast<_ViewScroll *>(data);
}

void XAP_UnixFrameImpl::_fe::vScrollChanged(GtkAdjustment * w, gpointer /*data*/)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	if(pUnixFrameImpl->m_bScrollWait)
	{
		xxx_UT_DEBUGMSG(("VScroll dropped!!! \n"));
		return;
	}
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	AV_View * pView = pFrame->getCurrentView();
	_ViewScroll * pVS = new  _ViewScroll(pUnixFrameImpl,pView,static_cast<UT_sint32>(gtk_adjustment_get_value(w)));
	pUnixFrameImpl->m_bScrollWait = true;
	pUnixFrameImpl->m_iScrollIdleID =
		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, _actualScroll, pVS, _scrollDataFree);
}

void XAP_UnixFrameImpl::_fe::hScrollChanged(GtkAdjustment * w, gpointer /*data*/)
{
	XAP_UnixFrameImpl * pUnixFrameImpl = static_cast<XAP_UnixFrameImpl *>(g_object_get_data(G_OBJECT(w), "user_data"));
	XAP_Frame* pFrame = pUnixFrameImpl->getFrame();
	AV_View * pView = pFrame->getCurrentView();

	if (pView)
		pView->sendHorizontalScrollEvent(static_cast<UT_sint32>(gtk_adjustment_get_value(w)));
}

void XAP_UnixFrameImpl::_fe::destroy(GtkWidget * /*widget*/, gpointer /*data*/)
{
}

/*****************************************************************/

void XAP_UnixFrameImpl::_nullUpdate() const
{
//   	for (UT_uint32 i = 0; (i < 5) && gtk_events_pending(); i++)
//		gtk_main_iteration ();
}

void XAP_UnixFrameImpl::_initialize()
{
	UT_DEBUGMSG (("XAP_UnixFrameImpl::_initialize()\n"));

    	// get a handle to our keyboard binding mechanism
 	// and to our mouse binding mechanism.
 	EV_EditEventMapper * pEEM = XAP_App::getApp()->getEditEventMapper();
 	UT_ASSERT(pEEM);

	m_pKeyboard = new ev_UnixKeyboard(pEEM);
	UT_ASSERT(m_pKeyboard);

	m_pMouse = new EV_UnixMouse(pEEM);
	UT_ASSERT(m_pMouse);
}

void XAP_UnixFrameImpl::_setCursor(GR_Graphics::Cursor c)
{
//	if (m_cursor == c)
//		return;
//	m_cursor = c;
	FV_View * pView = static_cast<FV_View *>(getFrame()->getCurrentView());
	if(pView)
	{
		GR_Graphics * pG = pView->getGraphics();
		if(pG && pG->queryProperties( GR_Graphics::DGP_PAPER))
			return;
	}
	if(getTopLevelWindow() == nullptr || (m_iFrameMode != XAP_NormalFrame))
		return;

	const char* cursor_name = GR_UnixCairoGraphics::_getCursor(c);
	xxx_UT_DEBUGMSG(("Set cursor number in Frame %d to %s\n", c, cursor_name));
	gtk_widget_set_cursor_from_name(getTopLevelWindow(), cursor_name);
	gtk_widget_set_cursor_from_name(getVBoxWidget(), cursor_name);

	gtk_widget_set_cursor_from_name(m_wSunkenBox, cursor_name);

	if (m_wStatusBar)
		gtk_widget_set_cursor_from_name(m_wStatusBar, cursor_name);
}

UT_sint32 XAP_UnixFrameImpl::_setInputMode(const char * szName)
{
	UT_sint32 result = XAP_App::getApp()->setInputMode(szName);
	if (result == 1)
	{
		// if it actually changed we need to update keyboard and mouse

		EV_EditEventMapper * pEEM = XAP_App::getApp()->getEditEventMapper();
		UT_ASSERT(pEEM);

		m_pKeyboard->setEditEventMap(pEEM);
		m_pMouse->setEditEventMap(pEEM);
	}

	return result;
}

GtkWidget * XAP_UnixFrameImpl::getTopLevelWindow(void) const
{
	return m_wTopLevelWindow;
}

GtkWidget * XAP_UnixFrameImpl::getVBoxWidget(void) const
{
	return m_wVBox;
}

XAP_DialogFactory * XAP_UnixFrameImpl::_getDialogFactory(void)
{
	return &m_dialogFactory;
}

GtkWidget *  XAP_UnixFrameImpl::_createInternalWindow (void)
{
	return gtk_application_window_new(static_cast<XAP_UnixApp*>(XAP_App::getApp())->getGtkApp());
}

// TODO: split me up into smaller pieces/subfunctions
void XAP_UnixFrameImpl::_createTopLevelWindow(void)
{
	// create a top-level window for us.

	if(m_iFrameMode == XAP_NormalFrame)
	{
		m_wTopLevelWindow = _createInternalWindow ();
		gtk_window_set_title(GTK_WINDOW(m_wTopLevelWindow),
				     XAP_App::getApp()->getApplicationTitleForTitleBar());
		gtk_window_set_resizable(GTK_WINDOW(m_wTopLevelWindow), TRUE);

		g_object_set_data(G_OBJECT(m_wTopLevelWindow), "ic_attr", nullptr);
		g_object_set_data(G_OBJECT(m_wTopLevelWindow), "ic", nullptr);

	}
	g_object_set_data(G_OBJECT(m_wTopLevelWindow), "toplevelWindow",
						m_wTopLevelWindow);
	g_object_set_data(G_OBJECT(m_wTopLevelWindow), "toplevelWindowFocus",
						GINT_TO_POINTER(FALSE));
	g_object_set_data(G_OBJECT(m_wTopLevelWindow), "user_data", this);

	_setGeometry ();

	// GTK4: drop target replaces gtk_drag_dest_set() and the drag_data_*
	// signals. (No GtkDragSource was ever configured, so only the drop
	// side is ported.)
	GtkEventController * dropTarget = GTK_EVENT_CONTROLLER(
		gtk_drop_target_async_new(s_getDropFormats(), GDK_ACTION_COPY));
	g_signal_connect(dropTarget, "drop",
					 G_CALLBACK(s_drop_cb), this);
	gtk_widget_add_controller(m_wTopLevelWindow, dropTarget);

	g_signal_connect(G_OBJECT(m_wTopLevelWindow), "close-request",
					   G_CALLBACK(_fe::close_request), nullptr);

	// create a VBox inside it.

	m_wVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	g_object_set_data(G_OBJECT(m_wTopLevelWindow), "vbox", m_wVBox);
	g_object_set_data(G_OBJECT(m_wVBox),"user_data", this);
	gtk_window_set_child(GTK_WINDOW(m_wTopLevelWindow), m_wVBox);

	if (m_iFrameMode != XAP_NoMenusWindowLess) {
		// synthesize a menu from the info in our base class.
		m_pUnixMenu = new EV_UnixMenuBar(static_cast<XAP_UnixApp*>(XAP_App::getApp()), getFrame(), m_szMenuLayoutName,
										 m_szMenuLabelSetName);
		UT_return_if_fail(m_pUnixMenu);
		UT_DebugOnly<bool> bResult;
		bResult = m_pUnixMenu->synthesizeMenuBar();
		UT_ASSERT(bResult);
	}

	// create a toolbar instance for each toolbar listed in our base class.

	if(m_iFrameMode == XAP_NormalFrame)
		gtk_widget_realize(m_wTopLevelWindow);

	_createIMContext(m_wTopLevelWindow);

	/* If refactoring the toolbars code, please make sure that toolbars
	 * are created AFTER the main menu bar has been synthesized, otherwise
	 * the embedded build will stop working
	 */
	if(m_iFrameMode == XAP_NormalFrame)
		_createToolbars();

	// Let the app-specific frame code create the contents of
	// the child area of the window (between the toolbars and
	// the status bar).
	m_wSunkenBox = _createDocumentWindow();
	gtk_box_append(GTK_BOX(m_wVBox), m_wSunkenBox);
	gtk_widget_show(m_wSunkenBox);

	// Create statusLet the app-specific frame code create the status bar
	// if it wants to.  we will put it below the document
	// window (a peer with toolbars and the overall sunkenbox)
	// so that it will appear outside of the scrollbars.
	m_wStatusBar = nullptr;

#ifdef ENABLE_STATUSBAR
	if(m_iFrameMode == XAP_NormalFrame)
		m_wStatusBar = _createStatusBarWindow();
#endif

	if (m_wStatusBar)
	{
		gtk_widget_show(m_wStatusBar);
		gtk_box_append(GTK_BOX(m_wVBox), m_wStatusBar);
	}

	gtk_widget_show(m_wVBox);

	// set the icon
	if(m_iFrameMode == XAP_NormalFrame)
		_setWindowIcon();
}

void XAP_UnixFrameImpl::_createIMContext(GtkWidget *w)
{
	m_imContext = gtk_im_multicontext_new();

	gtk_im_context_set_use_preedit (m_imContext, FALSE);

	gtk_im_context_set_client_widget(m_imContext, w);

	g_signal_connect(G_OBJECT(m_imContext), "commit",
					 G_CALLBACK(_imCommit_cb), this);
	g_signal_connect (m_imContext, "preedit_start",
					  G_CALLBACK (_imPreeditStart_cb), this);
	g_signal_connect (m_imContext, "preedit_changed",
					  G_CALLBACK (_imPreeditChanged_cb), this);
	g_signal_connect (m_imContext, "preedit_end",
					  G_CALLBACK (_imPreeditEnd_cb), this);
	g_signal_connect (m_imContext, "retrieve_surrounding",
					  G_CALLBACK (_imRetrieveSurrounding_cb), this);
	g_signal_connect (m_imContext, "delete_surrounding",
					  G_CALLBACK (_imDeleteSurrounding_cb), this);
}

void XAP_UnixFrameImpl::_imPreeditStart_cb (GtkIMContext * /*context*/,
											gpointer data)
{
	XAP_UnixFrameImpl * pImpl = static_cast<XAP_UnixFrameImpl*>(data);
	FV_View * pView =
		static_cast<FV_View*>(pImpl->getFrame()->getCurrentView ());

	pImpl->m_iPreeditStart = pView->getInsPoint ();
	pImpl->m_iPreeditLen = 0;

	UT_DEBUGMSG(("@@@@ Preedit Started, pos %d\n", pView->getInsPoint ()));
}

void XAP_UnixFrameImpl::_imPreeditEnd_cb (GtkIMContext * /*context*/,
										  gpointer data)
{
	XAP_UnixFrameImpl * pImpl = static_cast<XAP_UnixFrameImpl*>(data);
	FV_View * pView =
		static_cast<FV_View*>(pImpl->getFrame()->getCurrentView ());

	UT_DEBUGMSG(("@@@@ Preedit Ended\n"));

	if (pImpl->m_iPreeditLen)
		{
			// Anything that might have been entered as part of pre-edit
			// needs to be nuked.
			UT_DEBUGMSG(("@@@@@ deleting preedit from %d, len %d\n",
						 pImpl->m_iPreeditStart,
						 pImpl->m_iPreeditLen));
			pView->moveInsPtTo (pImpl->m_iPreeditStart);
			pView->cmdCharDelete (true, pImpl->m_iPreeditLen);

			pImpl->m_iPreeditLen = 0;
		}

	pImpl->m_iPreeditStart = 0;
}

void XAP_UnixFrameImpl::_imPreeditChanged_cb (GtkIMContext *context,
											  gpointer data)
{
	gchar *text = nullptr;
	size_t len = 0;
	gint   pos;

	XAP_UnixFrameImpl * pImpl = static_cast<XAP_UnixFrameImpl*>(data);
	XAP_Frame* pFrame = pImpl->getFrame();
	FV_View * pView = static_cast<FV_View*>(pFrame->getCurrentView ());
	ev_UnixKeyboard * pUnixKeyboard =
		static_cast<ev_UnixKeyboard *>(pFrame->getKeyboard());

	// delete previous pre-edit, if there is one
	if (pImpl->m_iPreeditLen)
		{
			UT_DEBUGMSG(("deleting preedit from %d, len %d\n",
						 pImpl->m_iPreeditStart,
						 pImpl->m_iPreeditLen));
			pView->moveInsPtTo (pImpl->m_iPreeditStart);
			pView->cmdCharDelete (true, pImpl->m_iPreeditLen);

			pImpl->m_iPreeditLen = 0;
			pImpl->m_iPreeditStart = 0;
		}

	// fetch the updated pre-edit string.
	gtk_im_context_get_preedit_string (context, &text, nullptr, &pos);

	if (!text)
		return;

	len = strlen(text);
	if (len > 0) {
		pImpl->m_iPreeditStart = pView->getInsPoint ();
		pImpl->m_iPreeditLen   = g_utf8_strlen (text, -1);

		pUnixKeyboard->charDataEvent(pView, static_cast<EV_EditBits>(0),
									 text, len);
		UT_DEBUGMSG(("@@@@ Preedit Changed, text %s, len %ld (utf8 chars %d)\n",
					 text, len, pImpl->m_iPreeditLen));
	}
	g_free(text);
}

gint XAP_UnixFrameImpl::_imRetrieveSurrounding_cb (GtkIMContext *context,
												   gpointer data)
{
	XAP_UnixFrameImpl * pImpl = static_cast<XAP_UnixFrameImpl*>(data);
	FV_View * pView =
		static_cast<FV_View*>(pImpl->getFrame()->getCurrentView ());

	PT_DocPosition begin_p, end_p, here;

	begin_p = pView->mapDocPosSimple (FV_DOCPOS_BOB);
	end_p = pView->mapDocPosSimple (FV_DOCPOS_EOB);
	here = pView->getInsPoint ();

	// here can be 0, in that case it's likely out of bounds
	// so we tree it as nothing to do, otherwise it's likely to crash.
	if (here < begin_p) {
		return TRUE;
	}

	UT_UCS4Char * text = nullptr;
	if (end_p > begin_p)
		text = pView->getTextBetweenPos (begin_p, end_p);

	if (!text)
		return TRUE;

	UT_UTF8String utf (text);
	DELETEPV(text);

	gtk_im_context_set_surrounding (context,
									utf.utf8_str(),
									utf.byteLength (),
									g_utf8_offset_to_pointer(utf.utf8_str(), here - begin_p) - utf.utf8_str());

	return TRUE;
}

gint XAP_UnixFrameImpl::_imDeleteSurrounding_cb (GtkIMContext * /*slave*/,
												 gint offset, gint n_chars,
												 gpointer data)
{
	xxx_UT_DEBUGMSG(("Delete Surrounding\n"));
	XAP_UnixFrameImpl * pImpl = static_cast<XAP_UnixFrameImpl*>(data);
	FV_View * pView =
		static_cast<FV_View*>(pImpl->getFrame()->getCurrentView ());

	PT_DocPosition insPt = pView->getInsPoint ();
	if ((gint) insPt + offset < 0)
		return TRUE;

	pView->moveInsPtTo (insPt + offset);
	pView->cmdCharDelete (true, n_chars);

	return TRUE;
}

// Actual keyboard commit should be done here.
void XAP_UnixFrameImpl::_imCommit_cb(GtkIMContext *imc,
									 const gchar *text, gpointer data)
{
	XAP_UnixFrameImpl * impl = static_cast<XAP_UnixFrameImpl*>(data);
	impl->_imCommit (imc, text);
}

// Actual keyboard commit should be done here.
void XAP_UnixFrameImpl::_imCommit(GtkIMContext * /*imc*/, const gchar * text)
{
	XAP_Frame* pFrame = getFrame();
	FV_View * pView = static_cast<FV_View*>(getFrame()->getCurrentView ());
	ev_UnixKeyboard * pUnixKeyboard =
		static_cast<ev_UnixKeyboard *>(pFrame->getKeyboard());

	if (m_iPreeditLen)
		{
			/* delete previous pre-edit */
			UT_DEBUGMSG(("deleting preedit from %d, len %d\n",
						 m_iPreeditStart,
						 m_iPreeditLen));
			pView->moveInsPtTo (m_iPreeditStart);
			pView->cmdCharDelete (true, m_iPreeditLen);

			m_iPreeditLen = 0;
			m_iPreeditStart = 0;
		}

	pUnixKeyboard->charDataEvent(pView, static_cast<EV_EditBits>(0),
								 text, strlen(text));

	xxx_UT_DEBUGMSG(("<<<<<<<<_imCommit: text %s, len %d\n", text, strlen(text)));
}

GtkIMContext * XAP_UnixFrameImpl::getIMContext()
{
	return m_imContext;
}

void XAP_UnixFrameImpl::_setGeometry ()
{
	UT_sint32 app_x = 0;
	UT_sint32 app_y = 0;
	UT_uint32 app_w = 0;
	UT_uint32 app_h = 0;
	UT_uint32 app_f = 0;

	XAP_UnixApp * pApp = static_cast<XAP_UnixApp*>(XAP_App::getApp ());
	pApp->getGeometry (&app_x, &app_y, &app_w, &app_h, &app_f);
	// (ignore app_x, app_y & app_f since the WM will set them for us just fine)

	// This is now done with --geometry parsing.
	if (app_w == 0 || app_w > USHRT_MAX) app_w = 760;
        if (app_h == 0 || app_h > USHRT_MAX) app_h = 520;

	UT_DEBUGMSG(("xap_UnixFrameImpl: app-width=%lu, app-height=%lu\n",
				 static_cast<unsigned long>(app_w),static_cast<unsigned long>(app_h)));

	// set geometry hints as the user requested
	gint user_x = 0;
	gint user_y = 0;
	UT_uint32 uuser_w = static_cast<UT_uint32>(app_w);
	UT_uint32 uuser_h = static_cast<UT_uint32>(app_h);
	UT_uint32 user_f = 0;

	pApp->getWinGeometry (&user_x, &user_y, &uuser_w, &uuser_h, &user_f);
	// to avoid bad signedess warnings
	gint user_w = static_cast<gint>(uuser_w);
	gint user_h = static_cast<gint>(uuser_h);

	UT_DEBUGMSG(("xap_UnixFrameImpl: user-width=%u, user-height=%u\n",
				 static_cast<unsigned>(user_w),static_cast<unsigned>(user_h)));

	// Get fall-back defaults from preferences
	UT_sint32 pref_x = 0;
	UT_sint32 pref_y = 0;
	UT_uint32 pref_w = static_cast<UT_uint32>(app_w);
	UT_uint32 pref_h = static_cast<UT_uint32>(app_h);
	UT_uint32 pref_f = 0;

	pApp->getPrefs()->getGeometry (&pref_x, &pref_y, &pref_w, &pref_h, &pref_f);

	UT_DEBUGMSG(("xap_UnixFrameImpl: pref-width=%lu, pref-height=%lu\n",
				 static_cast<unsigned long>(pref_w),static_cast<unsigned long>(pref_h)));

	if (!(user_f & XAP_UnixApp::GEOMETRY_FLAG_SIZE))
		if (pref_f & PREF_FLAG_GEOMETRY_SIZE)
			{
				user_w = static_cast<guint>(pref_w);
				user_h = static_cast<guint>(pref_h);
				user_f |= XAP_UnixApp::GEOMETRY_FLAG_SIZE;
			}
	if (!(user_f & XAP_UnixApp::GEOMETRY_FLAG_POS))
		if (pref_f & PREF_FLAG_GEOMETRY_POS)
			{
				user_x = static_cast<gint>(pref_x);
				user_y = static_cast<gint>(pref_y);
				user_f |= XAP_UnixApp::GEOMETRY_FLAG_POS;
			}

	UT_DEBUGMSG(("xap_UnixFrameImpl: user-x=%d, user-y=%d\n",
				 static_cast<int>(user_x),static_cast<int>(user_y)));

	if (!(user_f & XAP_UnixApp::GEOMETRY_FLAG_SIZE))
		{
			user_w = static_cast<guint>(app_w);
			user_h = static_cast<guint>(app_h);
		}

	if (user_w > USHRT_MAX)
		user_w = app_w;
        if (user_h > USHRT_MAX)
		user_h = app_h;

	if(getFrame()->getFrameMode() == XAP_NormalFrame)
	{
		// GTK4: no gtk_window_set_geometry_hints(); the minimum size is
		// expressed as a widget size request
		gtk_widget_set_size_request(GTK_WIDGET(m_wTopLevelWindow), 100, 100);

		GdkDisplay* display = gdk_display_get_default();
		GListModel * monitors = gdk_display_get_monitors(display);
		GdkMonitor* monitor = monitors && g_list_model_get_n_items(monitors) ?
			GDK_MONITOR(g_list_model_get_item(monitors, 0)) : nullptr;
		if (monitor)
		{
			GdkRectangle geometry;
			gdk_monitor_get_geometry(monitor, &geometry);
			user_w = (user_w < geometry.width ? user_w : geometry.width);
			user_h = (user_h < geometry.height ? user_h : geometry.height);
			g_object_unref(monitor);
		}
		gtk_window_set_default_size (GTK_WINDOW(m_wTopLevelWindow), user_w, user_h);
	}

	// GTK4: gtk_window_move() is gone; positioning is up to the compositor
	// (and unsupported on Wayland anyway). The saved position is still
	// remembered in the prefs below.

	// Remember geometry settings for next time
	pApp->getPrefs()->setGeometry (user_x, user_y, user_w, user_h, user_f);

}

/*!
 * This code is used by the dynamic menu API to rebuild the menus after a
 * a change in the menu structure.
 */
void XAP_UnixFrameImpl::_rebuildMenus(void)
{
	// no menu? then nothing to rebuild!
	if (!m_pUnixMenu) return;

	// destroy old menu
	m_pUnixMenu->destroy();
	DELETEP(m_pUnixMenu);

	// build new one.
	m_pUnixMenu = new EV_UnixMenuBar(static_cast<XAP_UnixApp*>(XAP_App::getApp()), getFrame(),
					 m_szMenuLayoutName,
					 m_szMenuLabelSetName);
	UT_return_if_fail(m_pUnixMenu);
	UT_DebugOnly<bool> bResult = m_pUnixMenu->rebuildMenuBar();
	UT_ASSERT_HARMLESS(bResult);
}

/*!
 * This code is used by the dynamic toolbar API to rebuild a toolbar after a
 * a change in the toolbar structure.
 */
void XAP_UnixFrameImpl::_rebuildToolbar(UT_uint32 ibar)
{
	XAP_Frame*	pFrame = getFrame();
	// Destroy the old toolbar
	EV_Toolbar * pToolbar = static_cast<EV_Toolbar *>(m_vecToolbars.getNthItem(ibar));
	const char * szTBName = reinterpret_cast<const char *>(m_vecToolbarLayoutNames.getNthItem(ibar));
	EV_UnixToolbar * pUTB = static_cast<EV_UnixToolbar *>(pToolbar);
	UT_sint32 oldpos = pUTB->destroy();

	// Delete the old class
	delete pToolbar;
	if(oldpos < 0) {
		return;
	}

	// Build a new one.
	pToolbar = _newToolbar(pFrame, szTBName,
			       static_cast<const char *>(m_szToolbarLabelSetName));
	static_cast<EV_UnixToolbar *>(pToolbar)->rebuildToolbar(oldpos);
	m_vecToolbars.setNthItem(ibar, pToolbar, nullptr);
	// Refill the framedata pointers

	pFrame->refillToolbarsInFrameData();
	pFrame->repopulateCombos();
}

bool XAP_UnixFrameImpl::_close()
{
	gtk_window_destroy(GTK_WINDOW(m_wTopLevelWindow)); // TOPLEVEL
	m_wTopLevelWindow = nullptr;
	return true;
}

bool XAP_UnixFrameImpl::_raise()
{
	UT_ASSERT(m_wTopLevelWindow);
	if (GTK_IS_WINDOW (m_wTopLevelWindow))
		gtk_window_present(GTK_WINDOW (m_wTopLevelWindow));
	return true;
}

bool XAP_UnixFrameImpl::_show()
{
	if(m_wTopLevelWindow) {
		gtk_widget_show(m_wTopLevelWindow);
	}

	return true;
}

bool XAP_UnixFrameImpl::_updateTitle()
{
	if (!XAP_FrameImpl::_updateTitle() || (m_wTopLevelWindow== nullptr) || (m_iFrameMode != XAP_NormalFrame))
	{
		// no relevant change, so skip it
		return false;
	}

	if(getFrame()->getFrameMode() == XAP_NormalFrame)
	{
		if (GTK_IS_WINDOW (m_wTopLevelWindow))
			{
				gtk_window_set_title(GTK_WINDOW(m_wTopLevelWindow), getFrame()->getTitle().c_str());
			}
	}
	return true;
}

bool XAP_UnixFrameImpl::_runModalContextMenu(AV_View * /* pView */, const char * szMenuName,
											 UT_sint32 x, UT_sint32 y)
{
	XAP_Frame*	pFrame = getFrame();
	bool bResult = true;

	UT_ASSERT_HARMLESS(!m_pUnixPopup);

	// WL_REFACTOR: we DON'T want to do this
	m_pUnixPopup = new EV_UnixMenuPopup(static_cast<XAP_UnixApp*>(XAP_App::getApp()),
										pFrame, szMenuName, m_szMenuLabelSetName);

	if (m_pUnixPopup && m_pUnixPopup->synthesizeMenuPopup())
	{
		// GTK4: the popup menu is a GtkPopoverMenu.  Parent it to the
		// toplevel window and point it at the current pointer
		// position, like gtk_menu_popup_at_pointer() did.
		GtkWidget * menu = m_pUnixPopup->getMenuHandle();
		if (GTK_IS_POPOVER(menu))
		{
			GtkWidget * toplevel = m_wTopLevelWindow;
			gtk_widget_set_parent(menu, toplevel);

			GdkRectangle rect = { x, y, 1, 1 };
			GdkSurface * surface = gtk_native_get_surface(GTK_NATIVE(toplevel));
			GdkDisplay * display = gtk_widget_get_display(toplevel);
			GdkSeat * seat = gdk_display_get_default_seat(display);
			GdkDevice * pointer = seat ? gdk_seat_get_pointer(seat) : nullptr;
			double px = x, py = y;
			if (surface && pointer &&
				gdk_surface_get_device_position(surface, pointer, &px, &py, nullptr))
			{
				rect.x = static_cast<int>(px);
				rect.y = static_cast<int>(py);
			}
			gtk_popover_set_pointing_to(GTK_POPOVER(menu), &rect);

			GMainLoop * loop = g_main_loop_new(nullptr, FALSE);
			g_signal_connect_swapped(G_OBJECT(menu), "closed",
							 G_CALLBACK(g_main_loop_quit), loop);
			gtk_popover_popup(GTK_POPOVER(menu));

			// We run this menu synchronously, since GTK doesn't.
			// The "closed" handler above quits the nested loop.
			g_main_loop_run(loop);
			g_main_loop_unref(loop);

			gtk_widget_unparent(menu);
		}
	}

	if (pFrame && pFrame->getCurrentView())
		pFrame->getCurrentView()->focusChange( AV_FOCUS_HERE);

	DELETEP(m_pUnixPopup);
	return bResult;
}

void XAP_UnixFrameImpl::setTimeOfLastEvent(guint32 eventTime)
{
	static_cast<XAP_UnixApp*>(XAP_App::getApp())->setTimeOfLastEvent(eventTime);
}

void XAP_UnixFrameImpl::_queue_resize()
{
	gtk_widget_queue_resize(m_wTopLevelWindow);
}

EV_Menu* XAP_UnixFrameImpl::_getMainMenu()
{
	return m_pUnixMenu;
}

void XAP_UnixFrameImpl::_setFullScreen(bool changeToFullScreen)
{
	if (!GTK_IS_WINDOW(m_wTopLevelWindow)) return;

	if (changeToFullScreen)
		gtk_window_fullscreen (GTK_WINDOW(m_wTopLevelWindow));
	else
		gtk_window_unfullscreen (GTK_WINDOW(m_wTopLevelWindow));
}

EV_Toolbar * XAP_UnixFrameImpl::_newToolbar(XAP_Frame *pFrame,
											const char *szLayout,
											const char *szLanguage)
{
	EV_UnixToolbar *pToolbar = nullptr;
#ifdef HAVE_GCONF
	pToolbar = new EV_GnomeToolbar(static_cast<XAP_UnixApp *>(XAP_App::getApp()), pFrame, szLayout, szLanguage);
#else
	pToolbar = new EV_UnixToolbar(static_cast<XAP_UnixApp *>(XAP_App::getApp()), pFrame, szLayout, szLanguage);
#endif
	return pToolbar;
}
