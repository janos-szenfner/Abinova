/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* AbiWord
 * Copyright (C) 2024 AbiWord contributors
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

#include "ap_UnixStylesPane.h"
#include "ap_UnixFrameImpl.h"
#include "xap_Frame.h"
#include "xap_App.h"
#include "ev_EditMethod.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "pd_Style.h"
#include "pt_PieceTable.h"
#include "ut_debugmsg.h"
#include "ut_string.h"

AP_UnixStylesPane::AP_UnixStylesPane(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wCurrent(nullptr)
	, m_wList(nullptr)
	, m_wFilter(nullptr)
	, m_szCurrent(nullptr)
{
}

AP_UnixStylesPane::~AP_UnixStylesPane()
{
	g_free(m_szCurrent);
}

/* pango markup rendering szText in the style's own formatting */
std::string AP_UnixStylesPane::styleMarkup(const PD_Style * pStyle,
											const char * szText,
											double minPt, double maxPt)
{
	std::string open, close;
	const gchar * szVal = nullptr;

	if (pStyle->getPropertyExpand("font-family", szVal) && szVal)
	{
		gchar * ef = g_markup_escape_text(szVal, -1);
		open += "<span font_family='";
		open += ef;
		open += "'>";
		close = "</span>" + close;
		g_free(ef);
	}
	if (pStyle->getPropertyExpand("font-weight", szVal) &&
		szVal && strcmp(szVal, "bold") == 0)
	{
		open += "<b>";
		close = "</b>" + close;
	}
	if (pStyle->getPropertyExpand("font-style", szVal) &&
		szVal && strcmp(szVal, "italic") == 0)
	{
		open += "<i>";
		close = "</i>" + close;
	}
	if (pStyle->getPropertyExpand("text-decoration", szVal) &&
		szVal && strstr(szVal, "underline"))
	{
		open += "<u>";
		close = "</u>" + close;
	}
	if (pStyle->getPropertyExpand("font-size", szVal) && szVal)
	{
		double pt = g_ascii_strtod(szVal, nullptr);
		if (pt > 0)
		{
			pt = CLAMP(pt, minPt, maxPt);
			char buf[64];
			g_snprintf(buf, sizeof(buf), "<span size='%d'>",
					   static_cast<int>(pt * PANGO_SCALE));
			open += buf;
			close = "</span>" + close;
		}
	}
	if (pStyle->getPropertyExpand("color", szVal) && szVal &&
		strlen(szVal) == 6)
	{
		open += "<span foreground='#";
		open += szVal;
		open += "'>";
		close = "</span>" + close;
	}

	gchar * esc = g_markup_escape_text(szText ? szText : "", -1);
	std::string markup = open + esc + close;
	g_free(esc);
	return markup;
}

bool AP_UnixStylesPane::isListPseudoStyle(const char * szInternalName)
{
	if (!szInternalName)
		return false;
	const size_t len = strlen(szInternalName);
	return len > 5 && !strcmp(szInternalName + len - 5, " List");
}

void AP_UnixStylesPane::_invokeMethod(const char * szMethod)
{
	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);
	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethod);
	UT_return_if_fail(pEM);
	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	EV_EditMethodCallData emcd("", 0);
	pEM->Fn(pView, &emcd);
}

void AP_UnixStylesPane::_applyStyle(const char * szInternalName)
{
	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	UT_return_if_fail(pView && szInternalName);
	pView->setStyle(szInternalName);
}

void AP_UnixStylesPane::_s_row_activated(GtkListBox * /*box*/,
										 GtkListBoxRow * row,
										 gpointer data)
{
	AP_UnixStylesPane * self = static_cast<AP_UnixStylesPane *>(data);
	UT_return_if_fail(self && row);
	const char * szName = static_cast<const char *>(
		g_object_get_data(G_OBJECT(row), "abi-style-name"));
	UT_return_if_fail(szName);
	if (!strcmp(szName, "@@clear@@"))
		self->_invokeMethod("clearFormatting");
	else
		self->_applyStyle(szName);
}

void AP_UnixStylesPane::_s_close_clicked(GtkButton * /*btn*/, gpointer data)
{
	AP_UnixStylesPane * self = static_cast<AP_UnixStylesPane *>(data);
	UT_return_if_fail(self && self->m_pFrame);
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		self->m_pFrame->getFrameImpl());
	if (pImpl)
		pImpl->setStylesPaneVisible(false);
}

void AP_UnixStylesPane::_s_new_style_clicked(GtkButton * /*btn*/,
											 gpointer data)
{
	AP_UnixStylesPane * self = static_cast<AP_UnixStylesPane *>(data);
	UT_return_if_fail(self);
	self->_invokeMethod("dlgStyle");
}

void AP_UnixStylesPane::_s_filter_changed(GtkDropDown * dd,
										  GParamSpec * /*pspec*/,
										  gpointer data)
{
	AP_UnixStylesPane * self = static_cast<AP_UnixStylesPane *>(data);
	UT_return_if_fail(self);
	self->_populate(gtk_drop_down_get_selected(dd) == 1);
}

/* (re)build the "Apply a style" list - displayed styles for
 * Recommended, every style for All Styles */
void AP_UnixStylesPane::_populate(bool bAll)
{
	if (!m_wList)
		return;

	GtkWidget * child = gtk_widget_get_first_child(m_wList);
	while (child)
	{
		GtkWidget * next = gtk_widget_get_next_sibling(child);
		gtk_list_box_remove(GTK_LIST_BOX(m_wList), child);
		child = next;
	}

	FV_View * pView = m_pFrame
		? static_cast<FV_View *>(m_pFrame->getCurrentView()) : nullptr;
	PD_Document * pdoc = pView ? pView->getDocument() : nullptr;
	if (!pdoc)
		return;

	/* Clear Formatting row first, like the reference pane */
	{
		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * label = gtk_label_new("Clear Formatting");
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_widget_set_margin_start(label, 6);
		gtk_widget_set_margin_end(label, 6);
		gtk_widget_set_margin_top(label, 4);
		gtk_widget_set_margin_bottom(label, 4);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
		g_object_set_data_full(G_OBJECT(row), "abi-style-name",
							   g_strdup("@@clear@@"), g_free);
		gtk_list_box_append(GTK_LIST_BOX(m_wList), row);
	}

	for (UT_uint32 k = 0;; ++k)
	{
		const char * szName = nullptr;
		const PD_Style * pStyle = nullptr;
		if (!pdoc->enumStyles(k, &szName, &pStyle))
			break;
		if (!pStyle || !szName || !*szName ||
			isListPseudoStyle(szName))
			continue;
		if (!bAll && !pStyle->isDisplayed())
			continue;

		std::string sLoc;
		pt_PieceTable::s_getLocalisedStyleName(szName, sLoc);
		const char * szDisp = sLoc.empty() ? szName : sLoc.c_str();

		GtkWidget * row = gtk_list_box_row_new();
		GtkWidget * label = gtk_label_new(nullptr);
		gtk_label_set_markup(GTK_LABEL(label),
							 styleMarkup(pStyle, szDisp).c_str());
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_widget_set_margin_start(label, 6);
		gtk_widget_set_margin_end(label, 6);
		gtk_widget_set_margin_top(label, 4);
		gtk_widget_set_margin_bottom(label, 4);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
		g_object_set_data_full(G_OBJECT(row), "abi-style-name",
							   g_strdup(szName), g_free);
		gtk_list_box_append(GTK_LIST_BOX(m_wList), row);
	}

	refresh(m_szCurrent);
}

void AP_UnixStylesPane::rebuildList()
{
	_populate(m_wFilter && gtk_drop_down_get_selected(
		GTK_DROP_DOWN(m_wFilter)) == 1);
}

/* update the current-style readout and the selected row */
void AP_UnixStylesPane::refresh(const char * szCurrentStyle)
{
	if (szCurrentStyle)
	{
		g_free(m_szCurrent);
		m_szCurrent = g_strdup(szCurrentStyle);
	}
	if (m_wCurrent)
	{
		std::string sLoc;
		if (m_szCurrent)
			pt_PieceTable::s_getLocalisedStyleName(m_szCurrent, sLoc);
		gtk_label_set_text(GTK_LABEL(m_wCurrent),
						   sLoc.empty()
						   ? (m_szCurrent ? m_szCurrent : "")
						   : sLoc.c_str());
	}
	if (m_wList && m_szCurrent)
	{
		GtkWidget * row = gtk_widget_get_first_child(m_wList);
		while (row)
		{
			const char * szName = static_cast<const char *>(
				g_object_get_data(G_OBJECT(row), "abi-style-name"));
			if (szName && strcmp(szName, m_szCurrent) == 0)
			{
				gtk_list_box_select_row(GTK_LIST_BOX(m_wList),
										GTK_LIST_BOX_ROW(row));
				break;
			}
			row = gtk_widget_get_next_sibling(row);
		}
	}
}

GtkWidget * AP_UnixStylesPane::createWidget()
{
	GtkWidget * root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(root, 260, -1);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_widget_set_margin_start(root, 8);
	gtk_widget_set_margin_end(root, 8);
	gtk_widget_add_css_class(root, "abiword-styles-pane");

	/* header: title + close */
	GtkWidget * head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title), "<b>Styles</b>");
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_widget_set_hexpand(title, TRUE);
	gtk_box_append(GTK_BOX(head), title);
	GtkWidget * close = gtk_button_new_from_icon_name(
		"window-close-symbolic");
	gtk_widget_add_css_class(close, "flat");
	g_signal_connect(close, "clicked",
					 G_CALLBACK(_s_close_clicked), this);
	gtk_box_append(GTK_BOX(head), close);
	gtk_box_append(GTK_BOX(root), head);

	/* current style */
	GtkWidget * curlbl = gtk_label_new("Current style:");
	gtk_widget_set_halign(curlbl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(root), curlbl);

	GtkWidget * curbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_add_css_class(curbox, "view");
	gtk_widget_add_css_class(curbox, "frame");
	m_wCurrent = gtk_label_new("");
	gtk_widget_set_halign(m_wCurrent, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_wCurrent, TRUE);
	gtk_widget_set_margin_start(m_wCurrent, 8);
	gtk_widget_set_margin_top(m_wCurrent, 6);
	gtk_widget_set_margin_bottom(m_wCurrent, 6);
	gtk_box_append(GTK_BOX(curbox), m_wCurrent);
	GtkWidget * pilcrow = gtk_label_new("\xC2\xB6");
	gtk_widget_set_margin_end(pilcrow, 8);
	gtk_box_append(GTK_BOX(curbox), pilcrow);
	gtk_box_append(GTK_BOX(root), curbox);

	/* New Style / Select All buttons */
	GtkWidget * btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(btns), TRUE);
	GtkWidget * bnew = gtk_button_new_with_label("New Style\xE2\x80\xA6");
	g_signal_connect(bnew, "clicked",
					 G_CALLBACK(_s_new_style_clicked), this);
	gtk_box_append(GTK_BOX(btns), bnew);
	GtkWidget * ball = gtk_button_new_with_label("Select All");
	gtk_widget_set_sensitive(ball, FALSE);
	gtk_box_append(GTK_BOX(btns), ball);
	gtk_box_append(GTK_BOX(root), btns);

	/* apply-a-style list */
	GtkWidget * applylbl = gtk_label_new("Apply a style:");
	gtk_widget_set_halign(applylbl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(root), applylbl);

	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	m_wList = gtk_list_box_new();
	gtk_widget_add_css_class(m_wList, "frame");
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_wList),
									GTK_SELECTION_SINGLE);
	g_signal_connect(m_wList, "row-activated",
					 G_CALLBACK(_s_row_activated), this);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), m_wList);
	gtk_box_append(GTK_BOX(root), scroll);

	/* list filter */
	GtkWidget * fbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget * flbl = gtk_label_new("List:");
	gtk_box_append(GTK_BOX(fbox), flbl);
	const char * opts[] = {"Recommended", "All Styles", nullptr};
	m_wFilter = gtk_drop_down_new_from_strings(opts);
	gtk_widget_set_hexpand(m_wFilter, TRUE);
	g_signal_connect(m_wFilter, "notify::selected",
					 G_CALLBACK(_s_filter_changed), this);
	gtk_box_append(GTK_BOX(fbox), m_wFilter);
	gtk_box_append(GTK_BOX(root), fbox);

	/* guides - no backend support yet */
	GtkWidget * g1 = gtk_check_button_new_with_label(
		"Show styles guides");
	gtk_widget_set_sensitive(g1, FALSE);
	gtk_box_append(GTK_BOX(root), g1);
	GtkWidget * g2 = gtk_check_button_new_with_label(
		"Show direct formatting guides");
	gtk_widget_set_sensitive(g2, FALSE);
	gtk_box_append(GTK_BOX(root), g2);

	_populate(false);
	refresh(nullptr);
	return root;
}
