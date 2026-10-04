/* Abinova - unix impl of the post-paste options smart tag
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

#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "xap_GtkUtils.h"
#include "ap_UnixFrameImpl.h"
#include "ap_UnixRibbon.h"
#include "fv_UnixPasteTag.h"
#include "fv_View.h"

/* weak-ref notify: nulls a GtkWidget* slot through its own type
 * (avoids the gpointer* alias pun g_object_add_weak_pointer requires) */
static void
s_widget_notify(gpointer data, GObject * /*where_dead*/)
{
	*static_cast<GtkWidget **>(data) = nullptr;
}

FV_UnixPasteTag::FV_UnixPasteTag(FV_View * pView)
	: m_pView(pView)
	, m_pOverlay(nullptr)
	, m_pButton(nullptr)
	, m_pPopover(nullptr)
	, m_bVisible(false)
{
}

FV_UnixPasteTag::~FV_UnixPasteTag()
{
	/* if the overlay child still lives, detach it; if the widget
	 * tree died first the weak ref already nulled the members */
	if (m_pButton)
	{
		g_object_weak_unref(G_OBJECT(m_pButton), s_widget_notify, &m_pButton);
		if (m_pPopover)
		{
			g_object_weak_unref(G_OBJECT(m_pPopover), s_widget_notify, &m_pPopover);
		}
		gtk_overlay_remove_overlay(GTK_OVERLAY(m_pOverlay), m_pButton);
	}
}

bool FV_UnixPasteTag::_getPositionCoords(PT_DocPosition pos, UT_sint32& x,
										 UT_sint32& y, UT_uint32& height) const
{
	UT_sint32 x1, y1, x2, y2;
	UT_uint32 h;
	bool bPos, visible = true;

	m_pView->_findPositionCoords(pos, false, x1, y1,
								 x2, y2, h,
								 bPos, nullptr, nullptr);

	if (x1 < 0 || y1 < 0 ||
		x1 > m_pView->getWindowWidth() ||
		y1 > m_pView->getWindowHeight() - static_cast<UT_sint32>(h))
		visible = false;

	x = m_pView->getGraphics()->tdu(x1);
	y = m_pView->getGraphics()->tdu(y1);
	height = m_pView->getGraphics()->tdu(h);

	return visible;
}

/* The tag lives in the GtkOverlay that wraps the document drawing
 * area (see AP_UnixFrameImpl::_createDocumentWindow).  Created lazily
 * on first use: the view can be created before the frame widget tree
 * exists, or with no frame at all (headless convert, abiwidget). */
void FV_UnixPasteTag::_ensureButton()
{
	if (m_pButton || !m_pView)
		return;

	XAP_Frame * pFrame = static_cast<XAP_Frame*>(m_pView->getParentData());
	if (!pFrame)
		return;
	XAP_UnixFrameImpl * pFrameImpl =
		static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
	if (!pFrameImpl)
		return;
	GtkWidget * pWidget = pFrameImpl->getViewWidget();
	GtkWidget * pOverlay = pWidget ? gtk_widget_get_parent(pWidget) : nullptr;

	if (!pOverlay || !GTK_IS_OVERLAY(pOverlay))
		return;

	m_pOverlay = pOverlay;
	m_pButton = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(m_pButton),
								  "edit-paste");
	gtk_menu_button_set_always_show_arrow(GTK_MENU_BUTTON(m_pButton),
										  TRUE);
	gtk_widget_set_tooltip_text(m_pButton, "Paste Options (Ctrl)");
	XAP_gtk_a11y_name_from_tooltip(m_pButton);
	gtk_widget_set_halign(m_pButton, GTK_ALIGN_START);
	gtk_widget_set_valign(m_pButton, GTK_ALIGN_START);
	/* force LTR so that margin-start always means the left edge */
	gtk_widget_set_direction(m_pButton, GTK_TEXT_DIR_LTR);
	/* keep keyboard focus on the document canvas */
	gtk_widget_set_focusable(m_pButton, FALSE);
	gtk_widget_add_css_class(m_pButton, "paste-tag");
	gtk_widget_set_visible(m_pButton, FALSE);

	m_pPopover = gtk_popover_new();
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_top(box, 4);
	gtk_widget_set_margin_bottom(box, 4);
	gtk_widget_set_margin_start(box, 4);
	gtk_widget_set_margin_end(box, 4);

	static const struct {
		const char * label;
		int          opt;
	} s_rows[] = {
		{ "Keep Source Formatting", FV_View::PASTETAG_KEEP_SOURCE },
		{ "Merge Formatting",       FV_View::PASTETAG_MERGE },
		{ "Keep Text Only",         FV_View::PASTETAG_TEXT_ONLY },
	};
	for (const auto & row : s_rows)
	{
		GtkWidget * w = gtk_button_new_with_label(row.label);
		gtk_widget_add_css_class(w, "flat");
		g_object_set_data(G_OBJECT(w), "abi-opt",
						  GINT_TO_POINTER(row.opt));
		g_signal_connect(w, "clicked",
						 G_CALLBACK(s_option_clicked), this);
		gtk_box_append(GTK_BOX(box), w);
	}

	/* Paste Special opens the frame-level dialog; it only exists
	 * when the view sits in a real AP_UnixFrameImpl with a ribbon */
	AP_UnixFrameImpl * pApImpl =
		dynamic_cast<AP_UnixFrameImpl *>(pFrameImpl);
	if (pApImpl && pApImpl->hasPasteSpecialDialog())
	{
		GtkWidget * sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_append(GTK_BOX(box), sep);
		GtkWidget * w = gtk_button_new_with_label("Paste Special\xe2\x80\xa6");
		gtk_widget_add_css_class(w, "flat");
		g_object_set_data(G_OBJECT(w), "abi-opt",
						  GINT_TO_POINTER(static_cast<int>(FV_View::PASTETAG_SPECIAL)));
		g_signal_connect(w, "clicked",
						 G_CALLBACK(s_option_clicked), this);
		gtk_box_append(GTK_BOX(box), w);
	}

	gtk_popover_set_child(GTK_POPOVER(m_pPopover), box);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(m_pButton), m_pPopover);

	gtk_overlay_add_overlay(GTK_OVERLAY(m_pOverlay), m_pButton);
	g_object_weak_ref(G_OBJECT(m_pButton), s_widget_notify, &m_pButton);
	g_object_weak_ref(G_OBJECT(m_pPopover), s_widget_notify, &m_pPopover);
}

void FV_UnixPasteTag::setPosition(PT_DocPosition pos)
{
	UT_sint32 x, y;
	UT_uint32 height;
	bool visible = _getPositionCoords(pos, x, y, height);

	_ensureButton();
	m_bVisible = visible && (m_pButton != nullptr);
	if (!m_pButton)
		return;

	if (m_bVisible)
	{
		/* anchor just below-right of the caret at the end of the
		 * pasted range, Word-style */
		gtk_widget_set_margin_start(m_pButton, x + 4);
		gtk_widget_set_margin_top(m_pButton,
								  y + static_cast<UT_sint32>(height) + 4);
	}
	gtk_widget_set_visible(m_pButton, m_bVisible);
}

void FV_UnixPasteTag::hide()
{
	m_bVisible = false;
	if (m_pPopover)
		gtk_popover_popdown(GTK_POPOVER(m_pPopover));
	if (m_pButton)
		gtk_widget_set_visible(m_pButton, FALSE);
}

void FV_UnixPasteTag::popup()
{
	if (m_bVisible && m_pButton)
		gtk_menu_button_set_active(GTK_MENU_BUTTON(m_pButton), TRUE);
}

void FV_UnixPasteTag::_optionPicked(int opt)
{
	if (m_pPopover)
		gtk_popover_popdown(GTK_POPOVER(m_pPopover));
	FV_View * pView = m_pView;
	if (!pView)
		return;

	switch (static_cast<FV_View::PasteTagOption>(opt))
	{
	case FV_View::PASTETAG_SPECIAL:
		pView->dismissPasteTag();
		_showPasteSpecial();
		break;
	default:
		pView->applyPasteTagOption(
			static_cast<FV_View::PasteTagOption>(opt));
		break;
	}
}

void FV_UnixPasteTag::s_option_clicked(GtkWidget * w, gpointer data)
{
	FV_UnixPasteTag * self = static_cast<FV_UnixPasteTag *>(data);
	if (!self)
		return;
	self->_optionPicked(GPOINTER_TO_INT(
		g_object_get_data(G_OBJECT(w), "abi-opt")));
}

void FV_UnixPasteTag::_showPasteSpecial()
{
	XAP_Frame * pFrame = m_pView
		? static_cast<XAP_Frame*>(m_pView->getParentData()) : nullptr;
	AP_UnixFrameImpl * pImpl = pFrame
		? dynamic_cast<AP_UnixFrameImpl*>(pFrame->getFrameImpl()) : nullptr;
	if (pImpl)
		pImpl->showPasteSpecialDialog();
}
