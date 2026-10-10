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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/* unit tests for text/fmt/gtk: the widget-side selection handles,
 * paste tag, frame-edit / inline-image / visual-drag subclasses, and
 * the FvTextHandle GObject.
 *
 * Everything that can run frame-less runs headless against a
 * GR_UnixCairoGraphics view (the null-handle / null-button early
 * returns and the drag-mode dispatch ladders).  The cases that need
 * a realized widget tree are guarded by gtk_init_check() like
 * xap_UnixWidget.t.cpp and run only when a display exists: they
 * build a real frame via XAP_App::newFrame() so the GtkOverlay the
 * handles/button live in actually exists.
 *
 * Left uncovered on purpose (documented in COVERAGE.md): the
 * drag-out-of-window paths that call gdk_drag_begin / pFrameImpl->
 * getTopLevelWindow() (they start a real DnD operation on the
 * session) and the GtkGestureDrag handlers (synthetic pointer input
 * on a realized window is ui-drive territory). */

#include "tf_test.h"

#include <string>

#include <gtk/gtk.h>

#include "ie_types.h"
#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "fv_Selection.h"
#include "gr_UnixCairoGraphics.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"

#include "fv_UnixFrameEdit.h"
#include "fv_UnixInlineImage.h"
#include "fv_UnixPasteTag.h"
#include "fv_UnixSelectionHandles.h"
#include "fv_UnixVisualDrag.h"
#include "gtktexthandleprivate.h"

#define TFSUITE "core.text.fmt.gtk"

namespace {

struct WidgetDoc
{
	WidgetDoc() = default;
	WidgetDoc(const WidgetDoc &) = delete;
	WidgetDoc &operator=(const WidgetDoc &) = delete;

	bool load(const char *text)
	{
		doc = new PD_Document;
		if (doc->createRawDocument() != UT_OK)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		pt_PieceTable *pt = doc->getPieceTable();
		bool ok = pt->appendStrux(PTX_Section, PP_NOPROPS);
		const char *p = text;
		while (p)
		{
			const char *nl = strchr(p, '\n');
			ok = ok && pt->appendStrux(PTX_Block, PP_NOPROPS);
			UT_UCS4String s(p, nl ? static_cast<size_t>(nl - p)
								  : strlen(p));
			if (s.length())
			{
				ok = ok && pt->appendSpan(s.ucs4_str(),
							static_cast<UT_uint32>(s.length()));
			}
			p = nl ? nl + 1 : nullptr;
		}
		if (!ok)
		{
			doc->unref();
			doc = nullptr;
			return false;
		}
		doc->finishRawCreation();
		return finish();
	}

	bool finish()
	{
		GR_UnixCairoAllocInfo ai(nullptr);
		graphics = XAP_App::getApp()->newGraphics(GRID_UNIX_PANGO, ai);
		if (!graphics)
			return false;
		layout = new FL_DocLayout(doc, graphics);
		view = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->fillLayouts();
		layout->formatAll();
		view->setWindowSize(800, 600);
		return layout->countPages() > 0;
	}

	~WidgetDoc()
	{
		delete view;
		delete layout;
		delete graphics;
		if (doc)
			doc->unref();
	}

	PD_Document *doc = nullptr;
	GR_Graphics *graphics = nullptr;
	FL_DocLayout *layout = nullptr;
	FV_View *view = nullptr;
};

/* xap_UnixWidget.t.cpp explains why gtk init is deferred to the
 * tests that need it */
static bool tf_fmt_gtk_ready()
{
	static int ready = -1;
	if (ready < 0)
		ready = gtk_init_check() ? 1 : 0;
	return ready != 0;
}

} // namespace

TFTEST_MAIN("selection_handles_no_frame")
{
	WidgetDoc d;
	TFPASS(d.load("hello world"));
	if (!d.view)
		return;

	FV_Selection sel(d.view);
	FV_UnixSelectionHandles handles(d.view, sel);

	/* no frame -> no text handle: every public call must early-out */
	handles.setCursorCoords(10, 10, 12, true);
	handles.setCursorCoords(10, 10, 12, false);
	handles.setSelectionCoords(5, 5, 12, true, 50, 5, 12, true);
	handles.setSelectionCoords(5, 5, 12, false, 50, 5, 12, false);
	handles.hide();
	TFPASS(true);
}

TFTEST_MAIN("paste_tag_no_frame")
{
	WidgetDoc d;
	TFPASS(d.load("hello world"));
	if (!d.view)
		return;

	FV_UnixPasteTag tag(d.view);
	TFPASS(!tag.isVisible());
	tag.setPosition(3);
	/* no frame -> no overlay -> never visible */
	TFPASS(!tag.isVisible());
	tag.hide();
	tag.popup();
	TFPASS(true);
}

TFTEST_MAIN("frame_edit_drag_modes")
{
	WidgetDoc d;
	TFPASS(d.load("hello world"));
	if (!d.view)
		return;

	FV_UnixFrameEdit fe(d.view);

	/* drag inside the window -> plain _mouseDrag */
	fe.mouseDrag(100, 100);

	/* drag outside the window, nothing dragged whole yet */
	fe.mouseDrag(900, 100);
	fe.mouseDrag(-10, 100);

	/* whole-frame drag, wrong edit mode -> early path */
	fe.setDragWhat(FV_DragWhole);
	fe.mouseDrag(900, 100);

	/* correct mode, but nothing selected as image wrapper -> the
	 * next early path (the frame-dependent drag-out block stays
	 * unreached) */
	fe.setMode(FV_FrameEdit_DRAG_EXISTING);
	fe.mouseDrag(900, 100);
	fe.mouseDrag(900, 100);
	TFPASS(true);
}

TFTEST_MAIN("inline_image_drag_modes")
{
	WidgetDoc d;
	TFPASS(d.load("hello world"));
	if (!d.view)
		return;

	FV_UnixVisualInlineImage ii(d.view);

	ii.mouseDrag(100, 100);
	ii.mouseDrag(900, 100);

	/* whole-drag + dragging mode, no image bound -> getPNGImage
	 * yields an empty buffer and the frame-dependent block is
	 * skipped, ending the drag */
	ii.setDragWhat(FV_DragWhole);
	ii.setMode(FV_InlineDrag_DRAGGING);
	ii.mouseDrag(900, 100);
	TFPASS(true);
}

TFTEST_MAIN("visual_drag_no_localbuf")
{
	WidgetDoc d;
	TFPASS(d.load("hello world"));
	if (!d.view)
		return;

	FV_UnixVisualDrag vd(d.view);

	vd.mouseDrag(100, 100);
	/* outside the window with no drag buffer -> early return */
	vd.mouseDrag(900, 100);
	TFPASS(true);
}

TFTEST_MAIN("fv_text_handle_object")
{
	if (!tf_fmt_gtk_ready())
		return;

	GtkWidget *overlay = gtk_overlay_new();
	FvTextHandle *th = _fv_text_handle_new(overlay);
	TFPASS(th != nullptr);
	if (!th)
		return;

	GdkRectangle rect = { 10, 10, 1, 20 };

	_fv_text_handle_set_mode(th, FV_TEXT_HANDLE_MODE_CURSOR);
	TFPASS(_fv_text_handle_get_mode(th) == FV_TEXT_HANDLE_MODE_CURSOR);
	_fv_text_handle_set_position(th, FV_TEXT_HANDLE_POSITION_CURSOR, &rect);
	_fv_text_handle_set_visible(th, FV_TEXT_HANDLE_POSITION_CURSOR, TRUE);
	_fv_text_handle_set_visible(th, FV_TEXT_HANDLE_POSITION_CURSOR, FALSE);

	_fv_text_handle_set_mode(th, FV_TEXT_HANDLE_MODE_SELECTION);
	_fv_text_handle_set_position(th, FV_TEXT_HANDLE_POSITION_SELECTION_START, &rect);
	_fv_text_handle_set_visible(th, FV_TEXT_HANDLE_POSITION_SELECTION_START, TRUE);
	TFPASS(_fv_text_handle_get_mode(th) == FV_TEXT_HANDLE_MODE_SELECTION);

	_fv_text_handle_set_mode(th, FV_TEXT_HANDLE_MODE_NONE);

	/* kill the child widgets out from under the handle: the weak
	 * refs must null them, and the next update takes the
	 * no-widget early return */
	for (GtkWidget *c = gtk_widget_get_first_child(overlay); c; )
	{
		GtkWidget *next = gtk_widget_get_next_sibling(c);
		gtk_overlay_remove_overlay(GTK_OVERLAY(overlay), c);
		c = next;
	}
	_fv_text_handle_set_mode(th, FV_TEXT_HANDLE_MODE_CURSOR);
	_fv_text_handle_set_mode(th, FV_TEXT_HANDLE_MODE_NONE);

	g_object_unref(th);
	g_object_unref(g_object_ref_sink(G_OBJECT(overlay)));
	TFPASS(true);
}

TFTEST_MAIN("frame widget paths")
{
	if (!tf_fmt_gtk_ready())
		return;

	XAP_App *app = XAP_App::getApp();
	XAP_Frame *frame = app->newFrame();
	TFPASS(frame != nullptr);
	if (!frame)
		return;

	/* newFrame leaves the frame document-less (no view) until a
	 * document is loaded — same newFrame/loadDocument/show order
	 * as ui_drive.cpp */
	std::string uri;
	TFPASS(TF_Test::ensure_test_data("test/wp/accents.abw", uri));
	UT_Error err = frame->loadDocument(uri.c_str(), IEFT_Unknown, true);
	TFPASS(err == UT_OK);
	frame->show();
	while (g_main_context_iteration(nullptr, FALSE))
		;

	FV_View *view = static_cast<FV_View *>(frame->getCurrentView());
	TFPASS(view != nullptr);
	if (!view)
	{
		app->forgetFrame(frame);
		delete frame;
		return;
	}

	/* a real frame puts the view inside a GtkOverlay, so the
	 * selection handles now get a live FvTextHandle */
	FV_Selection sel(view);
	FV_UnixSelectionHandles handles(view, sel);
	handles.setCursorCoords(10, 10, 12, true);
	handles.setCursorCoords(10, 10, 12, false);
	handles.setSelectionCoords(5, 5, 12, true, 60, 5, 12, true);
	handles.setSelectionCoords(5, 5, 12, false, 60, 5, 12, false);
	handles.hide();

	/* the paste tag builds its GtkMenuButton + popover lazily in
	 * the same overlay */
	FV_UnixPasteTag tag(view);
	tag.setPosition(2);
	tag.popup();
	tag.hide();
	TFPASS(true);

	/* destroying the frame's widget tree first exercises the
	 * weak-ref nulling path in the still-alive helpers */
	app->forgetFrame(frame);
	delete frame;
}
