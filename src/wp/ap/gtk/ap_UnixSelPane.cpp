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
#include <vector>
#include <algorithm>

#include "ap_UnixSelPane.h"
#include "ap_UnixFrameImpl.h"
#include "xap_Frame.h"
#include "xap_GtkUtils.h"
#include "fv_View.h"
#include "fl_FrameLayout.h"
#include "fp_FrameContainer.h"
#include "fp_Page.h"
#include "pp_AttrProp.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "ut_vector.h"

AP_UnixSelPane::AP_UnixSelPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wList(nullptr)
	, m_wUp(nullptr)
	, m_wDown(nullptr)
	, m_wGroup(nullptr)
	, m_wUngroup(nullptr)
	, m_pSelected(nullptr)
	, m_wRenamePopover(nullptr)
	, m_wRenameEntry(nullptr)
	, m_wRenameRow(nullptr)
{
}

AP_UnixSelPane::~AP_UnixSelPane()
{
	if (m_wRenamePopover)
		gtk_widget_unparent(m_wRenamePopover);
}

/* true while pFL still refers to a live frame layout - the rows
 * keep raw pointers, so every action re-enumerates and validates
 * before dereferencing (frames can die when text around them is
 * edited while the pane is open) */
bool AP_UnixSelPane::_isLive(fl_FrameLayout * pFL)
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView || !pFL)
		return false;
	UT_GenericVector<fl_FrameLayout *> vec;
	pView->getFrameLayouts(vec);
	return vec.findItem(pFL) >= 0;
}

struct _ObjRow
{
	fl_FrameLayout *	pFL;
	bool				bAbove;
	double				order;
};

static const char * s_type_icon(FL_FrameType t)
{
	switch (t)
	{
	case FL_FRAME_WRAPPER_IMAGE:	return "image-x-generic-symbolic";
	case FL_FRAME_WRAPPER_TABLE:	return "x-office-spreadsheet-symbolic";
	case FL_FRAME_WRAPPER_EMBED:	return "package-x-generic-symbolic";
	default:						return "text-x-generic-symbolic";
	}
}

static const char * s_type_name(FL_FrameType t)
{
	switch (t)
	{
	case FL_FRAME_WRAPPER_IMAGE:	return "Picture";
	case FL_FRAME_WRAPPER_TABLE:	return "Table";
	case FL_FRAME_WRAPPER_EMBED:	return "Object";
	default:						return "Text Box";
	}
}

static void s_prop(fl_FrameLayout * pFL, const char * name,
				   const gchar ** psz)
{
	*psz = nullptr;
	if (!pFL)
		return;
	const PP_AttrProp * pAP = nullptr;
	pFL->getAP(pAP);
	if (pAP)
		pAP->getProperty(name, *psz);
}

static bool s_hidden(fl_FrameLayout * pFL)
{
	const gchar * sz = nullptr;
	s_prop(pFL, "frame-hidden", &sz);
	return sz && *sz && strcmp(sz, "0") != 0 && strcmp(sz, "false") != 0;
}

/* the pane lists objects front-to-back like Word: the above-text
 * layer first (highest rank first), then the below-text layer */
static bool s_front_first(const _ObjRow & a, const _ObjRow & b)
{
	if (a.bAbove != b.bAbove)
		return a.bAbove;
	return a.order > b.order;
}

void AP_UnixSelPane::rebuildList()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!m_wList || !pView)
		return;

	if (m_wRenamePopover)
	{
		gtk_widget_unparent(m_wRenamePopover);
		m_wRenamePopover = nullptr;
		m_wRenameEntry = nullptr;
		m_wRenameRow = nullptr;
	}

	GtkListBoxRow * old;
	while ((old = gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(m_wList), 0)))
		gtk_list_box_remove(GTK_LIST_BOX(m_wList), GTK_WIDGET(old));

	UT_GenericVector<fl_FrameLayout *> vec;
	pView->getFrameLayouts(vec);

	m_lastFrames.clear();
	m_lastFrames.reserve(vec.getItemCount());
	for (UT_sint32 i = 0; i < vec.getItemCount(); ++i)
		m_lastFrames.push_back(vec.getNthItem(i));

	std::vector<_ObjRow> rows;
	rows.reserve(vec.getItemCount());
	for (UT_sint32 i = 0; i < vec.getItemCount(); ++i)
	{
		fl_FrameLayout * pFL = vec.getNthItem(i);
		fp_FrameContainer * pFC =
			static_cast<fp_FrameContainer *>(pFL->getFirstContainer());
		_ObjRow r;
		r.pFL = pFL;
		r.bAbove = pFC ? pFC->isAbove() : true;
		r.order = pFC ? pFC->getStackOrder() : 0.0;
		rows.push_back(r);
	}
	std::sort(rows.begin(), rows.end(), s_front_first);

	/* per-type numbering for the fallback names, done in document
	 * order so numbering matches how the objects appear in the file */
	std::vector<fl_FrameLayout *> docOrder;
	for (UT_sint32 i = 0; i < vec.getItemCount(); ++i)
		docOrder.push_back(vec.getNthItem(i));
	std::sort(docOrder.begin(), docOrder.end(),
			  [](fl_FrameLayout * a, fl_FrameLayout * b)
			  { return a->getPosition(true) < b->getPosition(true); });

	fl_FrameLayout * curSel = pView->isFrameSelected()
		? pView->getFrameLayout() : nullptr;

	for (const _ObjRow & r : rows)
	{
		fl_FrameLayout * pFL = r.pFL;
		FL_FrameType t = pFL->getFrameType();

		const gchar * szName = nullptr;
		s_prop(pFL, "frame-name", &szName);
		std::string disp;
		if (szName && *szName)
			disp = szName;
		else
		{
			/* index of this frame among same-type frames in doc order */
			int idx = 0;
			for (auto q : docOrder)
			{
				if (q->getFrameType() == t)
					++idx;
				if (q == pFL)
					break;
			}
			char buf[64];
			g_snprintf(buf, sizeof(buf), "%s %d", s_type_name(t), idx);
			disp = buf;
		}

		GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_margin_start(hbox, 6);
		gtk_widget_set_margin_end(hbox, 6);
		gtk_widget_set_margin_top(hbox, 3);
		gtk_widget_set_margin_bottom(hbox, 3);

		/* group tick box - AbiWord's canvas only selects one frame,
		 * so multi-select for the Group command lives here */
		GtkWidget * chk = gtk_check_button_new();
		gtk_widget_set_tooltip_text(chk, "Include in the Group command");
		g_object_set_data(G_OBJECT(chk), "pfl", pFL);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(chk),
									pView->isInGroupSel(pFL));
		g_signal_connect(chk, "toggled",
						 G_CALLBACK(_s_check_toggled), this);
		gtk_box_append(GTK_BOX(hbox), chk);

		GtkWidget * icon =
			gtk_image_new_from_icon_name(s_type_icon(t));
		gtk_box_append(GTK_BOX(hbox), icon);

		GtkWidget * lbl = gtk_label_new(disp.c_str());
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
		gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_box_append(GTK_BOX(hbox), lbl);

		/* grouped objects show their shared id, like Word's pane
		 * indents group members */
		const gchar * szGrp = nullptr;
		s_prop(pFL, "frame-group", &szGrp);
		if (szGrp && *szGrp)
		{
			char gbuf[32];
			g_snprintf(gbuf, sizeof(gbuf), "[%s]", szGrp);
			GtkWidget * gtag = gtk_label_new(nullptr);
			char * mk = g_markup_printf_escaped(
				"<span size='small' alpha='60%%'>%s</span>", gbuf);
			gtk_label_set_markup(GTK_LABEL(gtag), mk);
			g_free(mk);
			gtk_box_append(GTK_BOX(hbox), gtag);
		}

		GtkWidget * eye = gtk_button_new_from_icon_name(
			s_hidden(pFL) ? "view-conceal-symbolic"
						  : "view-reveal-symbolic");
		gtk_button_set_has_frame(GTK_BUTTON(eye), FALSE);
		gtk_widget_set_tooltip_text(eye, "Show or hide this object");
		g_object_set_data(G_OBJECT(eye), "pfl", pFL);
		g_signal_connect(eye, "clicked",
						 G_CALLBACK(_s_eye_clicked), this);
		gtk_box_append(GTK_BOX(hbox), eye);

		GtkWidget * row = gtk_list_box_row_new();
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
		g_object_set_data(G_OBJECT(row), "pfl", pFL);
		g_object_set_data(G_OBJECT(row), "lbl", lbl);

		GtkGesture * click = gtk_gesture_click_new();
		gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
		g_signal_connect(click, "pressed",
						 G_CALLBACK(_s_row_gesture), this);
		gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));

		gtk_list_box_append(GTK_LIST_BOX(m_wList), row);
		if (pFL == curSel)
		{
			gtk_list_box_select_row(GTK_LIST_BOX(m_wList),
									GTK_LIST_BOX_ROW(row));
			m_pSelected = pFL;
		}
	}

	gtk_widget_set_sensitive(m_wUp, m_pSelected != nullptr);
	gtk_widget_set_sensitive(m_wDown, m_pSelected != nullptr);
	_updateGroupButtons();
}

/* view-listener hook - only rebuilds when the document's frame
 * set changed (an object was inserted or deleted); property
 * writes from the pane itself rebuild directly */
void AP_UnixSelPane::refresh()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!m_wList || !pView)
		return;
	UT_GenericVector<fl_FrameLayout *> vec;
	pView->getFrameLayouts(vec);
	if (vec.getItemCount() ==
		static_cast<UT_sint32>(m_lastFrames.size()))
	{
		bool bSame = true;
		for (UT_sint32 i = 0; i < vec.getItemCount(); ++i)
			bSame &= (vec.getNthItem(i) == m_lastFrames[i]);
		if (bSame)
			return;
	}
	rebuildList();
}

void AP_UnixSelPane::_selectRow(fl_FrameLayout * pFL)
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView || !_isLive(pFL))
		return;
	pView->selectFrameObject(pFL);
	m_pSelected = pFL;
	gtk_widget_set_sensitive(m_wUp, TRUE);
	gtk_widget_set_sensitive(m_wDown, TRUE);
}

void AP_UnixSelPane::_toggleHidden(GtkWidget * eyeBtn)
{
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
		g_object_get_data(G_OBJECT(eyeBtn), "pfl"));
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView || !_isLive(pFL))
		return;
	pView->setFrameProp(pFL, "frame-hidden",
						s_hidden(pFL) ? "0" : "1");
	rebuildList();
}

void AP_UnixSelPane::_move(int iDir)
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView || !_isLive(m_pSelected))
		return;
	pView->restackFrame(m_pSelected, iDir);
	rebuildList();
}

void AP_UnixSelPane::_s_row_selected(GtkListBox * /*box*/,
								   GtkListBoxRow * row, gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	if (!row)
		return;
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
		g_object_get_data(G_OBJECT(row), "pfl"));
	self->_selectRow(pFL);
}

void AP_UnixSelPane::_s_eye_clicked(GtkButton * btn, gpointer data)
{
	static_cast<AP_UnixSelPane *>(data)->_toggleHidden(GTK_WIDGET(btn));
}

void AP_UnixSelPane::_s_move_clicked(GtkButton * btn, gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	int dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "dir"));
	self->_move(dir);
}

void AP_UnixSelPane::_s_check_toggled(GtkCheckButton * chk,
									gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
		g_object_get_data(G_OBJECT(chk), "pfl"));
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (!pView || !self->_isLive(pFL))
		return;
	pView->toggleGroupSel(pFL,
						  gtk_check_button_get_active(chk));
	self->_updateGroupButtons();
}

void AP_UnixSelPane::_s_group_clicked(GtkButton * btn, gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	if (btn == GTK_BUTTON(self->m_wUngroup))
		self->_ungroup();
	else
		self->_group();
}

/* Group: combine every ticked object into one shared group id -
 * moves and restacks as a unit afterwards */
void AP_UnixSelPane::_group()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView)
		return;
	UT_GenericVector<fl_FrameLayout *> sel;
	pView->getGroupSel(sel);
	if (pView->groupFrames(sel))
		pView->clearGroupSel();
	rebuildList();
}

/* Ungroup: clears the group id off every ticked object plus the
 * currently selected frame */
void AP_UnixSelPane::_ungroup()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	if (!pView)
		return;
	UT_GenericVector<fl_FrameLayout *> sel;
	pView->getGroupSel(sel);
	fl_FrameLayout * pCur = pView->getFrameLayout();
	if (pCur && sel.findItem(pCur) < 0)
		sel.addItem(pCur);
	pView->ungroupFrames(sel);
	rebuildList();
}

void AP_UnixSelPane::_updateGroupButtons()
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	UT_sint32 nTicked = pView ? pView->groupSelCount() : 0;
	if (m_wGroup)
		gtk_widget_set_sensitive(m_wGroup, nTicked >= 2);
	bool bCanUngroup = nTicked > 0;
	if (!bCanUngroup && pView && pView->getFrameLayout())
	{
		const gchar * sz = nullptr;
		s_prop(pView->getFrameLayout(), "frame-group", &sz);
		bCanUngroup = sz && *sz;
	}
	if (m_wUngroup)
		gtk_widget_set_sensitive(m_wUngroup, bCanUngroup);
}

void AP_UnixSelPane::_s_close_clicked(GtkButton * /*btn*/, gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	if (!self->m_pFrame)
		return;
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		self->m_pFrame->getFrameImpl());
	if (pImpl)
		pImpl->setSelPaneVisible(false);
}

void AP_UnixSelPane::_s_name_activated(GtkEntry * entry, gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	FV_View * pView = self->m_pFrame
		? static_cast<FV_View *>(self->m_pFrame->getCurrentView())
		: nullptr;
	if (self->m_wRenameRow && pView && entry)
	{
		fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
			g_object_get_data(G_OBJECT(self->m_wRenameRow), "pfl"));
		const char * text = gtk_editable_get_text(GTK_EDITABLE(entry));
		if (self->_isLive(pFL) && text)
			pView->setFrameProp(pFL, "frame-name", text);
	}
	if (self->m_wRenamePopover)
		gtk_popover_popdown(GTK_POPOVER(self->m_wRenamePopover));
	self->m_wRenameRow = nullptr;
	self->rebuildList();
}

/* Return/KP_Enter commit, Escape cancels - a key controller is used
 * rather than relying on GtkEntry::activate alone so cancellation
 * works too */
gboolean AP_UnixSelPane::_s_rename_key(GtkEventControllerKey * /*c*/,
									 guint keyval, guint /*keycode*/,
									 GdkModifierType /*mods*/,
									 gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
	{
		_s_name_activated(GTK_ENTRY(self->m_wRenameEntry), self);
		return TRUE;
	}
	if (keyval == GDK_KEY_Escape)
	{
		if (self->m_wRenamePopover)
			gtk_popover_popdown(GTK_POPOVER(self->m_wRenamePopover));
		self->m_wRenameRow = nullptr;
		return TRUE;
	}
	return FALSE;
}

void AP_UnixSelPane::_s_row_gesture(GtkGestureClick * g, int n_press,
									double /*x*/, double /*y*/,
									gpointer data)
{
	AP_UnixSelPane * self = static_cast<AP_UnixSelPane *>(data);
	if (n_press != 2)
		return;
	GtkWidget * row = gtk_event_controller_get_widget(
		GTK_EVENT_CONTROLLER(g));
	UT_return_if_fail(row);
	self->_rename(GTK_LIST_BOX_ROW(row));
}

void AP_UnixSelPane::_rename(GtkListBoxRow * row)
{
	if (m_wRenamePopover)
		gtk_widget_unparent(m_wRenamePopover);
	m_wRenamePopover = xap_gtk_popover_new();
	gtk_widget_set_parent(m_wRenamePopover, GTK_WIDGET(row));
	m_wRenameRow = row;

	GtkWidget * entry = gtk_entry_new();
	m_wRenameEntry = entry;
	fl_FrameLayout * pFL = static_cast<fl_FrameLayout *>(
		g_object_get_data(G_OBJECT(row), "pfl"));
	const gchar * szName = nullptr;
	s_prop(pFL, "frame-name", &szName);
	if (szName && *szName)
		gtk_editable_set_text(GTK_EDITABLE(entry), szName);
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Object name");
	g_signal_connect(entry, "activate",
					 G_CALLBACK(_s_name_activated), this);
	GtkEventController * keys = gtk_event_controller_key_new();
	g_signal_connect(keys, "key-pressed",
					 G_CALLBACK(_s_rename_key), this);
	gtk_widget_add_controller(entry, keys);
	gtk_popover_set_child(GTK_POPOVER(m_wRenamePopover), entry);
	gtk_popover_popup(GTK_POPOVER(m_wRenamePopover));
	gtk_widget_grab_focus(entry);
}

GtkWidget * AP_UnixSelPane::createWidget()
{
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(box, 220, -1);

	/* header: title + close */
	GtkWidget * header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(header, 8);
	gtk_widget_set_margin_end(header, 4);
	gtk_widget_set_margin_top(header, 4);
	gtk_widget_set_margin_bottom(header, 4);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Selection</b>");
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

	/* the object list */
	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	m_wList = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_wList),
									GTK_SELECTION_SINGLE);
	gtk_list_box_set_placeholder(GTK_LIST_BOX(m_wList),
		gtk_label_new("No objects in the document"));
	g_signal_connect(m_wList, "row-selected",
					 G_CALLBACK(_s_row_selected), this);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
								  m_wList);
	gtk_box_append(GTK_BOX(box), scroll);

	gtk_box_append(GTK_BOX(box),
				   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* bottom bar: reorder the selected object */
	GtkWidget * bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(bar, 8);
	gtk_widget_set_margin_end(bar, 8);
	gtk_widget_set_margin_top(bar, 4);
	gtk_widget_set_margin_bottom(bar, 4);

	m_wUp = gtk_button_new_from_icon_name("go-up-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(m_wUp), FALSE);
	gtk_widget_set_tooltip_text(m_wUp, "Bring forward");
	g_object_set_data(G_OBJECT(m_wUp), "dir", GINT_TO_POINTER(1));
	g_signal_connect(m_wUp, "clicked",
					 G_CALLBACK(_s_move_clicked), this);
	gtk_widget_set_sensitive(m_wUp, FALSE);
	gtk_box_append(GTK_BOX(bar), m_wUp);

	m_wDown = gtk_button_new_from_icon_name("go-down-symbolic");
	gtk_button_set_has_frame(GTK_BUTTON(m_wDown), FALSE);
	gtk_widget_set_tooltip_text(m_wDown, "Send backward");
	g_object_set_data(G_OBJECT(m_wDown), "dir", GINT_TO_POINTER(-1));
	g_signal_connect(m_wDown, "clicked",
					 G_CALLBACK(_s_move_clicked), this);
	gtk_widget_set_sensitive(m_wDown, FALSE);
	gtk_box_append(GTK_BOX(bar), m_wDown);

	GtkWidget * hint = gtk_label_new("Double-click to rename");
	gtk_widget_add_css_class(hint, "dim-label");
	gtk_label_set_ellipsize(GTK_LABEL(hint), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(hint, TRUE);
	gtk_label_set_xalign(GTK_LABEL(hint), 1.0);
	gtk_box_append(GTK_BOX(bar), hint);

	gtk_box_append(GTK_BOX(box), bar);

	/* second bar: grouping - the tick boxes mark the objects the
	 * Group command combines */
	GtkWidget * gbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(gbar, 8);
	gtk_widget_set_margin_end(gbar, 8);
	gtk_widget_set_margin_bottom(gbar, 4);

	m_wGroup = gtk_button_new_with_label("Group");
	gtk_widget_set_tooltip_text(m_wGroup,
		"Combine the ticked objects into one unit");
	g_object_set_data(G_OBJECT(m_wGroup), "grp", GINT_TO_POINTER(1));
	g_signal_connect(m_wGroup, "clicked",
					 G_CALLBACK(_s_group_clicked), this);
	gtk_widget_set_sensitive(m_wGroup, FALSE);
	gtk_box_append(GTK_BOX(gbar), m_wGroup);

	m_wUngroup = gtk_button_new_with_label("Ungroup");
	gtk_widget_set_tooltip_text(m_wUngroup,
		"Split the ticked or selected group into objects");
	g_signal_connect(m_wUngroup, "clicked",
					 G_CALLBACK(_s_group_clicked), this);
	gtk_widget_set_sensitive(m_wUngroup, FALSE);
	gtk_box_append(GTK_BOX(gbar), m_wUngroup);

	GtkWidget * ghint = gtk_label_new("Tick to multi-select");
	gtk_widget_add_css_class(ghint, "dim-label");
	gtk_label_set_ellipsize(GTK_LABEL(ghint), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(ghint, TRUE);
	gtk_label_set_xalign(GTK_LABEL(ghint), 1.0);
	gtk_box_append(GTK_BOX(gbar), ghint);

	gtk_box_append(GTK_BOX(box), gbar);
	return box;
}
