/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiWord
 * Copyright (C) 2025 AbiWord contributors
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

#include "config.h"

#include <cstring>

#include "ap_UnixCommentsPane.h"
#include "ap_UnixFrameImpl.h"
#include "xap_Frame.h"
#include "fv_View.h"
#include "fl_FootnoteLayout.h"
#include "fp_AnnotationRun.h"
#include "fp_Line.h"
#include "fp_Page.h"
#include "ut_debugmsg.h"

/* per-card context handed to the button callbacks; the pane owns and
 * deletes them on every rebuild */
struct _CardCtx
{
	AP_UnixCommentsPane *	self;
	UT_uint32				pid;
};

/* matches FV_View::m_colorAnnotations - the per-author colours also
 * used for the anchored text in the document */
static const UT_uint8 s_colors[10][3] =
{
	{171,4,254}, {171,20,119}, {255,151,8}, {158,179,69}, {15,179,5},
	{8,179,248}, {4,206,195}, {4,133,195}, {7,18,195}, {255,0,0}
};

AP_UnixCommentsPane::AP_UnixCommentsPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wList(nullptr)
	, m_iRefreshIdle(0)
{
}

AP_UnixCommentsPane::~AP_UnixCommentsPane()
{
	if (m_iRefreshIdle)
		g_source_remove(m_iRefreshIdle);
	for(gpointer p : m_ctx)
		delete static_cast<_CardCtx *>(p);
}

GtkWidget * AP_UnixCommentsPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(box, 240, -1);

	/* header: title + close */
	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(header, 8);
	gtk_widget_set_margin_end(header, 4);
	gtk_widget_set_margin_top(header, 4);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Comments</b>");
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(header), title);
	GtkWidget * close =
		gtk_button_new_from_icon_name("window-close-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(header), close);
	gtk_box_append(GTK_BOX(box), header);
	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* the comment list */
	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	m_wList = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_wList),
									GTK_SELECTION_SINGLE);
	gtk_list_box_set_placeholder(GTK_LIST_BOX(m_wList),
		gtk_label_new("No comments in the document"));
	g_signal_connect(m_wList, "row-selected",
					 G_CALLBACK(_s_row_selected), this);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
								  m_wList);
	gtk_box_append(GTK_BOX(box), scroll);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* bottom bar: new comment */
	GtkWidget * bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(bar, 8);
	gtk_widget_set_margin_end(bar, 8);
	gtk_widget_set_margin_top(bar, 4);
	gtk_widget_set_margin_bottom(bar, 4);
	GtkWidget * btn = gtk_button_new_with_label("New Comment");
	gtk_widget_set_tooltip_text(btn, "Insert a comment at the insertion point");
	gtk_widget_set_hexpand(btn, TRUE);
	g_signal_connect(btn, "clicked",
					 G_CALLBACK(_s_new_clicked), this);
	gtk_box_append(GTK_BOX(bar), btn);
	gtk_box_append(GTK_BOX(box), bar);

	return box;
}

/* the view listener calls refresh() on every document change, which
 * fires once per typed character when the caret is inside a comment.
 * Rebuilding mid-keystroke churns focus and eats the rest of the key
 * sequence, so coalesce refreshes through a short idle delay. */
void AP_UnixCommentsPane::refresh()
{
	if (m_iRefreshIdle)
		return;
	m_iRefreshIdle = g_timeout_add(150, _s_refresh_idle, this);
}

gboolean AP_UnixCommentsPane::_s_refresh_idle(gpointer data)
{
	AP_UnixCommentsPane * self =
		static_cast<AP_UnixCommentsPane *>(data);
	self->m_iRefreshIdle = 0;
	self->_refreshNow();
	return G_SOURCE_REMOVE;
}

/* rebuild the cards only when the comment set actually changed */
void AP_UnixCommentsPane::_refreshNow()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!m_wList || !pView)
		return;

	std::string sig;
	UT_sint32 n = static_cast<UT_sint32>(pView->countAnnotations());
	for (UT_sint32 i = 0; i < n; ++i)
	{
		fl_AnnotationLayout * pAL = pView->getNthAnnotation(i);
		if (!pAL)
			continue;
		UT_uint32 pid = pAL->getAnnotationPID();
		std::string sText;
		pView->getAnnotationText(pid, sText);
		sig += std::to_string(pid);
		sig += pView->isAnnotationResolved(pid) ? "R" : "-";
		sig += std::to_string(sText.size());
		sig += '|';
	}
	if (sig == m_sig)
		return;
	m_sig = sig;
	_rebuild();
}

void AP_UnixCommentsPane::_rebuild()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!m_wList || !pView)
		return;

	GtkListBoxRow * old;
	while ((old = gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(m_wList), 0)))
		gtk_list_box_remove(GTK_LIST_BOX(m_wList), GTK_WIDGET(old));
	for(gpointer p : m_ctx)
		delete static_cast<_CardCtx *>(p);
	m_ctx.clear();

	UT_sint32 n = static_cast<UT_sint32>(pView->countAnnotations());
	for (UT_sint32 i = 0; i < n; ++i)
	{
		fl_AnnotationLayout * pAL = pView->getNthAnnotation(i);
		if (!pAL)
			continue;
		GtkWidget * card = _makeCard(pAL->getAnnotationPID(), i);
		gtk_list_box_append(GTK_LIST_BOX(m_wList), card);
	}
}

void AP_UnixCommentsPane::_s_color_draw(GtkDrawingArea * /*area*/,
										cairo_t * cr, int w, int h,
										gpointer data)
{
	const UT_uint8 * c = static_cast<const UT_uint8 *>(data);
	cairo_set_source_rgb(cr, c[0] / 255.0, c[1] / 255.0, c[2] / 255.0);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_fill(cr);
}

GtkWidget * AP_UnixCommentsPane::_makeCard(UT_uint32 pid, UT_sint32 iIndex)
{
	FV_View * pView = static_cast<FV_View *>(m_pFrame->getCurrentView());
	fl_AnnotationLayout * pAL = pView->getAnnotationLayout(pid);
	UT_return_val_if_fail(pAL, gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

	bool bResolved = pView->isAnnotationResolved(pid);

	/* author colour: the same palette index the page uses for the
	 * anchor, falling back to document order */
	UT_uint8 * pc = m_cardColors[iIndex % 64];
	pc[0] = s_colors[iIndex % 10][0];
	pc[1] = s_colors[iIndex % 10][1];
	pc[2] = s_colors[iIndex % 10][2];
	fp_AnnotationRun * pRun = pAL->getAnnotationRun();
	if (pRun && pRun->getLine() && pRun->getLine()->getPage())
	{
		UT_RGBColor c = pView->getColorAnnotation(
			pRun->getLine()->getPage(), pid);
		pc[0] = c.m_red; pc[1] = c.m_grn; pc[2] = c.m_blu;
	}

	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set_data(G_OBJECT(row), "pid", GUINT_TO_POINTER(pid));

	/* colour bar on the left edge, like Word's author colour coding */
	GtkWidget * bar = gtk_drawing_area_new();
	gtk_widget_set_size_request(bar, 4, -1);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bar),
								   _s_color_draw, pc, nullptr);
	gtk_widget_set_margin_top(bar, 4);
	gtk_widget_set_margin_bottom(bar, 4);
	gtk_box_append(GTK_BOX(row), bar);

	GtkWidget * body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_start(body, 8);
	gtk_widget_set_margin_end(body, 8);
	gtk_widget_set_margin_top(body, 6);
	gtk_widget_set_margin_bottom(body, 6);
	gtk_widget_set_hexpand(body, TRUE);
	gtk_box_append(GTK_BOX(row), body);

	/* author + date */
	const char * szAuthor = pAL->getAuthor();
	GtkWidget * author = gtk_label_new(nullptr);
	gchar * esc = g_markup_escape_text(
		(szAuthor && *szAuthor) ? szAuthor : "Unknown author", -1);
	gchar * markup = g_strdup_printf(
		"<b>%s</b>%s", esc,
		bResolved ? "  <span foreground=\"gray\">(Resolved)</span>" : "");
	g_free(esc);
	gtk_label_set_markup(GTK_LABEL(author), markup);
	g_free(markup);
	gtk_label_set_xalign(GTK_LABEL(author), 0.0);
	gtk_box_append(GTK_BOX(body), author);

	const char * szDate = pAL->getDate();
	if (szDate && *szDate)
	{
		GtkWidget * date = gtk_label_new(szDate);
		gtk_widget_add_css_class(date, "dim-label");
		gtk_label_set_xalign(GTK_LABEL(date), 0.0);
		gtk_box_append(GTK_BOX(body), date);
	}

	/* comment text */
	std::string sText;
	pView->getAnnotationText(pid, sText);
	GtkWidget * text = gtk_label_new(
		sText.empty() ? "(no text)" : sText.c_str());
	gtk_label_set_xalign(GTK_LABEL(text), 0.0);
	gtk_label_set_wrap(GTK_LABEL(text), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(text), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable(GTK_LABEL(text), TRUE);
	if (sText.empty() || bResolved)
		gtk_widget_add_css_class(text, "dim-label");
	gtk_box_append(GTK_BOX(body), text);

	/* actions */
	GtkWidget * actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_top(actions, 4);
	gtk_box_append(GTK_BOX(body), actions);

	_CardCtx * ctx = new _CardCtx{this, pid};
	m_ctx.push_back(ctx);

	GtkWidget * rep = gtk_button_new_with_label("Reply");
	gtk_button_set_has_frame(GTK_BUTTON(rep), FALSE);
	gtk_widget_set_tooltip_text(rep, "Add a reply to this comment");
	g_signal_connect(rep, "clicked",
					 G_CALLBACK(_s_reply_clicked), ctx);
	gtk_box_append(GTK_BOX(actions), rep);

	GtkWidget * res = gtk_button_new_with_label(
		bResolved ? "Unresolve" : "Resolve");
	gtk_button_set_has_frame(GTK_BUTTON(res), FALSE);
	gtk_widget_set_tooltip_text(res,
		bResolved ? "Reopen this comment" : "Mark this comment as resolved");
	g_signal_connect(res, "clicked",
					 G_CALLBACK(_s_resolve_clicked), ctx);
	gtk_box_append(GTK_BOX(actions), res);

	GtkWidget * del = gtk_button_new_with_label("Delete");
	gtk_button_set_has_frame(GTK_BUTTON(del), FALSE);
	gtk_widget_set_tooltip_text(del, "Delete this comment");
	g_signal_connect(del, "clicked",
					 G_CALLBACK(_s_delete_clicked), ctx);
	gtk_box_append(GTK_BOX(actions), del);

	return row;
}

/* pane buttons take keyboard focus on click; hand it back to the
 * document canvas so the next keystroke reaches the view */
static void s_focusDoc(XAP_Frame * pFrame)
{
	AP_UnixFrameImpl * pImpl = pFrame
		? static_cast<AP_UnixFrameImpl *>(pFrame->getFrameImpl())
		: nullptr;
	if (pImpl)
		pImpl->focusDocument();
}

void AP_UnixCommentsPane::_s_row_selected(GtkListBox * /*box*/,
										  GtkListBoxRow * row,
										  gpointer data)
{
	AP_UnixCommentsPane * self =
		static_cast<AP_UnixCommentsPane *>(data);
	if (!self || !row)
		return;
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	/* the pid lives on the card box, which gtk_list_box_append wrapped
	 * in this GtkListBoxRow */
	GtkWidget * child = gtk_list_box_row_get_child(row);
	UT_uint32 pid = child ? GPOINTER_TO_UINT(
		g_object_get_data(G_OBJECT(child), "pid")) : 0;
	if (!pView || !pid)
		return;
	fl_AnnotationLayout * pAL = pView->getAnnotationLayout(pid);
	if (pAL)
		pView->selectAnnotation(pAL);
}

void AP_UnixCommentsPane::_s_reply_clicked(GtkButton * /*btn*/,
										  gpointer data)
{
	_CardCtx * ctx = static_cast<_CardCtx *>(data);
	AP_UnixCommentsPane * self = ctx->self;
	UT_uint32 pid = ctx->pid;
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (!pView)
		return;
	fl_AnnotationLayout * pAL = pView->getAnnotationLayout(pid);
	if (pAL)
		pView->replyAnnotation(pAL);
	self->refresh();
	/* the caret sits in the new reply paragraph - typing must go to
	 * the canvas, not stay on the Reply button */
	s_focusDoc(self->m_pFrame);
}

void AP_UnixCommentsPane::_s_resolve_clicked(GtkButton * /*btn*/,
											 gpointer data)
{
	_CardCtx * ctx = static_cast<_CardCtx *>(data);
	/* the view notify fired by resolveAnnotation() rebuilds the cards
	 * and deletes every _CardCtx - including this one. Keep only the
	 * pane pointer (owned by the frame) across the call. */
	AP_UnixCommentsPane * self = ctx->self;
	UT_uint32 pid = ctx->pid;
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (!pView)
		return;
	fl_AnnotationLayout * pAL = pView->getAnnotationLayout(pid);
	if (pAL)
		pView->resolveAnnotation(pAL);
	self->refresh();
}

void AP_UnixCommentsPane::_s_delete_clicked(GtkButton * /*btn*/,
											gpointer data)
{
	_CardCtx * ctx = static_cast<_CardCtx *>(data);
	AP_UnixCommentsPane * self = ctx->self;
	UT_uint32 pid = ctx->pid;
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (!pView)
		return;
	fl_AnnotationLayout * pAL = pView->getAnnotationLayout(pid);
	if (pAL)
		pView->delAnnotationLayout(pAL);
	self->refresh();
}

void AP_UnixCommentsPane::_s_new_clicked(GtkButton * /*btn*/,
										 gpointer data)
{
	AP_UnixCommentsPane * self =
		static_cast<AP_UnixCommentsPane *>(data);
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (pView)
	{
		pView->cmdInsertComment();
		/* caret is inside the new comment; return focus to the
		 * canvas so typing lands there */
		s_focusDoc(self->m_pFrame);
	}
}

void AP_UnixCommentsPane::_s_close_clicked(GtkButton * /*btn*/,
										   gpointer data)
{
	AP_UnixCommentsPane * self =
		static_cast<AP_UnixCommentsPane *>(data);
	if (!self || !self->m_pFrame)
		return;
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		self->m_pFrame->getFrameImpl());
	if (pImpl)
		pImpl->setCommentsPaneVisible(false);
}
