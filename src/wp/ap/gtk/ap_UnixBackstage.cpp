/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "ap_UnixBackstage.h"
#include "ap_UnixFrameImpl.h"
#include "ap_DocInspect.h"

#include "fv_View.h"
#include "pd_Document.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_Prefs.h"
#include "ut_go_file.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ev_EditMethod.h"

/*****************************************************************/

struct _NavSpec
{
	const char * page;	/* content stack page name, nullptr for
						 * action rows */
	const char * label;
	const char * method;	/* action rows: edit method to run */
	bool		 action;	/* true: run method instead of
							 * switching pages */
};

/* the left column, Word order: Info first since it doubles as the
 * landing page */
static const _NavSpec s_nav[] =
{
	{ "info",	"Info",		nullptr,		false },
	{ "new",	"New",		nullptr,		false },
	{ "open",	"Open",		nullptr,		false },
	{ nullptr,	"Save",		"fileSave",		true },
	{ "saveas",	"Save As",	nullptr,		false },
	{ "print",	"Print",	nullptr,		false },
	{ "share",	"Share",	nullptr,		false },
	{ nullptr,	"Options",	"dlgOptions",	true }
};

/*****************************************************************/

AP_UnixBackstage::AP_UnixBackstage(XAP_Frame * pFrame)
	: m_pFrame(pFrame)
	, m_wRoot(nullptr)
	, m_wNav(nullptr)
	, m_wStack(nullptr)
	, m_wBackBtn(nullptr)
	, m_wPropTitle(nullptr)
	, m_wPropSubject(nullptr)
	, m_wPropAuthor(nullptr)
	, m_wPropKeywords(nullptr)
	, m_wPropCategory(nullptr)
	, m_wStatGrid(nullptr)
	, m_wLocation(nullptr)
	, m_wProtectStatus(nullptr)
	, m_wRecentBox(nullptr)
{
}

AP_UnixBackstage::~AP_UnixBackstage()
{
}

FV_View * AP_UnixBackstage::_view() const
{
	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	return static_cast<FV_View *>(pView);
}

void AP_UnixBackstage::_invokeMethod(const char * szMethod,
									 const char * szData)
{
	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);
	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethod);
	UT_return_if_fail(pEM);
	AV_View * pView = m_pFrame ? m_pFrame->getCurrentView() : nullptr;
	UT_return_if_fail(pView);
	EV_EditMethodCallData emcd(szData ? szData : "",
							   szData ? strlen(szData) : 0);
	pEM->Fn(pView, &emcd);
}

void AP_UnixBackstage::_close()
{
	AP_UnixFrameImpl * impl = m_pFrame
		? static_cast<AP_UnixFrameImpl *>(m_pFrame->getFrameImpl())
		: nullptr;
	if (impl)
		impl->hideBackstage();
}

/* action rows and document-changing commands run the edit method,
 * then dismiss the backstage (Word returns to the document once a
 * File command completes).  Commands that open a modal dialog on
 * the backstage itself — Options, Document Properties, Restrict
 * Editing, Inspect — keep the view behind them (bStayOpen). */
void AP_UnixBackstage::_runAndClose(const char * szMethod,
									const char * szData, bool bStayOpen)
{
	_invokeMethod(szMethod, szData);
	if (!bStayOpen)
		_close();
}

/*****************************************************************/

GtkWidget * AP_UnixBackstage::_navRow(const char * szLabel,
									  const char * szPage, bool bAction)
{
	GtkWidget * w = gtk_button_new_with_label(szLabel);
	GtkWidget * lab = gtk_button_get_child(GTK_BUTTON(w));
	if (lab && GTK_IS_LABEL(lab))
	{
		gtk_label_set_xalign(GTK_LABEL(lab), 0.0f);
		gtk_widget_set_margin_start(lab, 18);
		gtk_widget_set_margin_end(lab, 12);
	}
	gtk_widget_add_css_class(w, "flat");
	gtk_widget_add_css_class(w, "abinova-backstage-row");
	gtk_widget_set_hexpand(w, TRUE);
	if (!bAction && szPage)
	{
		g_object_set_data(G_OBJECT(w), "abi-bs-page",
						  const_cast<gpointer>(static_cast<const void *>(szPage)));
	}
	else
		g_object_set_data(G_OBJECT(w), "abi-bs-action",
						  GINT_TO_POINTER(1));
	g_signal_connect(w, "clicked", G_CALLBACK(_s_nav_clicked), this);
	return w;
}

void AP_UnixBackstage::_s_nav_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixBackstage * self = static_cast<AP_UnixBackstage *>(data);
	UT_return_if_fail(self);
	const char * page = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-bs-page"));
	if (page)
	{
		self->setPage(page);
		return;
	}
	/* action row: find the method for this label from the spec */
	const char * label = nullptr;
	GtkWidget * lab = gtk_button_get_child(GTK_BUTTON(w));
	if (lab && GTK_IS_LABEL(lab))
		label = gtk_label_get_text(GTK_LABEL(lab));
	for (const _NavSpec & spec : s_nav)
	{
		if (spec.action && label && !strcmp(label, spec.label))
		{
			/* Options keeps the backstage behind the dialog; Save
			 * dismisses to the document when it completes */
			self->_runAndClose(spec.method, nullptr,
							   !strcmp(spec.label, "Options"));
			return;
		}
	}
}

void AP_UnixBackstage::_s_back_clicked(GtkWidget * /*w*/, gpointer data)
{
	AP_UnixBackstage * self = static_cast<AP_UnixBackstage *>(data);
	if (self)
		self->_close();
}

/* Escape returns to the document, same as Word's Esc on the File
 * page; every other key falls through to the focused widget */
gboolean AP_UnixBackstage::_s_key(GtkEventControllerKey * /*c*/,
								  guint keyval, guint /*keycode*/,
								  GdkModifierType /*state*/,
								  gpointer data)
{
	if (keyval != GDK_KEY_Escape)
		return FALSE;
	AP_UnixBackstage * self = static_cast<AP_UnixBackstage *>(data);
	if (self)
		self->_close();
	return TRUE;
}

/* a wide page row: bold label + dimmed detail underneath, dispatches
 * an edit method on click */
GtkWidget * AP_UnixBackstage::_bigRow(const char * szLabel,
									  const char * szDetail,
									  const char * szMethod,
									  const char * szData,
									  bool bStayOpen)
{
	GtkWidget * w = gtk_button_new();
	gtk_widget_add_css_class(w, "flat");
	gtk_widget_set_hexpand(w, TRUE);
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_margin_start(box, 8);
	gtk_widget_set_margin_end(box, 8);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	GtkWidget * lab = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(lab),
						 (std::string("<b>") + szLabel + "</b>").c_str());
	gtk_label_set_xalign(GTK_LABEL(lab), 0.0f);
	gtk_box_append(GTK_BOX(box), lab);
	if (szDetail && *szDetail)
	{
		GtkWidget * det = gtk_label_new(szDetail);
		gtk_label_set_xalign(GTK_LABEL(det), 0.0f);
		gtk_widget_add_css_class(det, "dim-label");
		gtk_box_append(GTK_BOX(box), det);
	}
	gtk_button_set_child(GTK_BUTTON(w), box);
	if (szMethod)
		g_object_set_data(G_OBJECT(w), "abi-bs-method",
						  const_cast<gpointer>(
							  static_cast<const void *>(szMethod)));
	if (szData)
		g_object_set_data_full(G_OBJECT(w), "abi-bs-data",
							   g_strdup(szData), g_free);
	g_object_set_data(G_OBJECT(w), "abi-bs-stay",
					  GINT_TO_POINTER(bStayOpen ? 1 : 0));
	g_signal_connect(w, "clicked", G_CALLBACK(_s_row_clicked), this);
	return w;
}

void AP_UnixBackstage::_s_row_clicked(GtkWidget * w, gpointer data)
{
	AP_UnixBackstage * self = static_cast<AP_UnixBackstage *>(data);
	UT_return_if_fail(self);
	const char * method = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-bs-method"));
	const char * arg = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-bs-data"));
	const bool stay = GPOINTER_TO_INT(
		g_object_get_data(G_OBJECT(w), "abi-bs-stay")) != 0;
	if (method)
		self->_runAndClose(method, arg, stay);
}

/* a page scaffold: scrolled vertical box with a large heading */
GtkWidget * AP_UnixBackstage::_page(const char * szTitle)
{
	GtkWidget * scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_hexpand(scroll, TRUE);
	gtk_widget_set_vexpand(scroll, TRUE);
	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(box, 32);
	gtk_widget_set_margin_end(box, 32);
	gtk_widget_set_margin_top(box, 24);
	gtk_widget_set_margin_bottom(box, 24);
	gtk_widget_set_halign(box, GTK_ALIGN_FILL);
	gtk_widget_set_valign(box, GTK_ALIGN_START);
	GtkWidget * title = gtk_label_new(nullptr);
	gtk_label_set_markup(
		GTK_LABEL(title),
		(std::string("<span size=\"xx-large\" weight=\"bold\">") +
		 szTitle + "</span>").c_str());
	gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
	gtk_widget_set_margin_bottom(title, 12);
	gtk_box_append(GTK_BOX(box), title);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
	g_object_set_data(G_OBJECT(scroll), "abi-bs-body", box);
	return scroll;
}

/* a "Name:" property row inside the Info properties grid; returns
 * the value label so _refreshInfo can fill it */
GtkWidget * AP_UnixBackstage::_propRow(GtkWidget * grid, int row,
									   const char * szLabel)
{
	GtkWidget * lab = gtk_label_new(szLabel);
	gtk_label_set_xalign(GTK_LABEL(lab), 0.0f);
	gtk_widget_add_css_class(lab, "dim-label");
	gtk_grid_attach(GTK_GRID(grid), lab, 0, row, 1, 1);
	GtkWidget * val = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(val), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(val), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(val), 40);
	gtk_grid_attach(GTK_GRID(grid), val, 1, row, 1, 1);
	return val;
}

void AP_UnixBackstage::_statRow(const char * szLabel,
								const char * szValue, int row)
{
	GtkWidget * lab = gtk_label_new(szLabel);
	gtk_label_set_xalign(GTK_LABEL(lab), 0.0f);
	gtk_widget_add_css_class(lab, "dim-label");
	gtk_grid_attach(GTK_GRID(m_wStatGrid), lab, 0, row, 1, 1);
	GtkWidget * val = gtk_label_new(szValue);
	gtk_label_set_xalign(GTK_LABEL(val), 0.0f);
	gtk_grid_attach(GTK_GRID(m_wStatGrid), val, 1, row, 1, 1);
	gtk_widget_set_visible(lab, TRUE);
	gtk_widget_set_visible(val, TRUE);
}

/*****************************************************************/

GtkWidget * AP_UnixBackstage::_makeInfoPage()
{
	GtkWidget * page = _page("Info");
	GtkWidget * body = GTK_WIDGET(
		g_object_get_data(G_OBJECT(page), "abi-bs-body"));

	GtkWidget * cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
	gtk_box_append(GTK_BOX(body), cols);

	/* ---- left column: properties + protect + inspect ---- */
	GtkWidget * left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(left, 360, -1);
	gtk_box_append(GTK_BOX(cols), left);

	GtkWidget * propsTitle = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(propsTitle), "<b>Properties</b>");
	gtk_label_set_xalign(GTK_LABEL(propsTitle), 0.0f);
	gtk_box_append(GTK_BOX(left), propsTitle);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_box_append(GTK_BOX(left), grid);
	m_wPropTitle    = _propRow(grid, 0, "Title:");
	m_wPropSubject  = _propRow(grid, 1, "Subject:");
	m_wPropAuthor   = _propRow(grid, 2, "Author:");
	m_wPropKeywords = _propRow(grid, 3, "Keywords:");
	m_wPropCategory = _propRow(grid, 4, "Category:");

	gtk_box_append(
		GTK_BOX(left),
		_bigRow("Document Properties…",
				"View and edit the document's properties",
				"dlgMetaData", nullptr, true));

	GtkWidget * protTitle = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(protTitle), "<b>Protect Document</b>");
	gtk_label_set_xalign(GTK_LABEL(protTitle), 0.0f);
	gtk_widget_set_margin_top(protTitle, 18);
	gtk_box_append(GTK_BOX(left), protTitle);
	m_wProtectStatus = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(m_wProtectStatus), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(m_wProtectStatus), TRUE);
	gtk_widget_add_css_class(m_wProtectStatus, "dim-label");
	gtk_box_append(GTK_BOX(left), m_wProtectStatus);
	gtk_box_append(
		GTK_BOX(left),
		_bigRow("Restrict Editing…",
				"Control how others can change this document",
				"restrictEditing", nullptr, true));

	GtkWidget * inspTitle = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(inspTitle), "<b>Inspect Document</b>");
	gtk_label_set_xalign(GTK_LABEL(inspTitle), 0.0f);
	gtk_widget_set_margin_top(inspTitle, 18);
	gtk_box_append(GTK_BOX(left), inspTitle);
	gtk_box_append(
		GTK_BOX(left),
		_bigRow("Inspect Document…",
				"Check for personal information and hidden content",
				"fileInspectDoc", nullptr, true));

	/* ---- right column: statistics + location ---- */
	GtkWidget * right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_hexpand(right, TRUE);
	gtk_box_append(GTK_BOX(cols), right);
	GtkWidget * statTitle = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(statTitle), "<b>Statistics</b>");
	gtk_label_set_xalign(GTK_LABEL(statTitle), 0.0f);
	gtk_box_append(GTK_BOX(right), statTitle);
	m_wStatGrid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(m_wStatGrid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(m_wStatGrid), 18);
	gtk_box_append(GTK_BOX(right), m_wStatGrid);

	GtkWidget * locTitle = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(locTitle), "<b>Location</b>");
	gtk_label_set_xalign(GTK_LABEL(locTitle), 0.0f);
	gtk_widget_set_margin_top(locTitle, 18);
	gtk_box_append(GTK_BOX(right), locTitle);
	m_wLocation = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(m_wLocation), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(m_wLocation),
						  PANGO_ELLIPSIZE_START);
	gtk_label_set_max_width_chars(GTK_LABEL(m_wLocation), 60);
	gtk_widget_add_css_class(m_wLocation, "dim-label");
	gtk_box_append(GTK_BOX(right), m_wLocation);

	gtk_widget_set_visible(page, TRUE);
	gtk_widget_set_visible(body, TRUE);
	return page;
}

GtkWidget * AP_UnixBackstage::_makeNewPage()
{
	GtkWidget * page = _page("New");
	GtkWidget * body = GTK_WIDGET(
		g_object_get_data(G_OBJECT(page), "abi-bs-body"));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Blank document", "Create a new empty document",
				"fileNew", nullptr, false));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("New from template…",
				"Create a document from a template file",
				"fileNewUsingTemplate", nullptr, false));
	gtk_widget_set_visible(page, TRUE);
	return page;
}

GtkWidget * AP_UnixBackstage::_makeOpenPage()
{
	GtkWidget * page = _page("Open");
	GtkWidget * body = GTK_WIDGET(
		g_object_get_data(G_OBJECT(page), "abi-bs-body"));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Browse…", "Find a document on this computer",
				"fileOpen", nullptr, false));
	GtkWidget * recTitle = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(recTitle), "<b>Recent</b>");
	gtk_label_set_xalign(GTK_LABEL(recTitle), 0.0f);
	gtk_widget_set_margin_top(recTitle, 18);
	gtk_box_append(GTK_BOX(body), recTitle);
	m_wRecentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_append(GTK_BOX(body), m_wRecentBox);
	gtk_widget_set_visible(page, TRUE);
	return page;
}

GtkWidget * AP_UnixBackstage::_makeSaveAsPage()
{
	GtkWidget * page = _page("Save As");
	GtkWidget * body = GTK_WIDGET(
		g_object_get_data(G_OBJECT(page), "abi-bs-body"));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Browse…", "Choose where to save this document",
				"fileSaveAs", nullptr, false));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Save as Template…",
				"Save the document as a reusable template",
				"fileSaveTemplate", nullptr, false));
	gtk_widget_set_visible(page, TRUE);
	return page;
}

GtkWidget * AP_UnixBackstage::_makePrintPage()
{
	GtkWidget * page = _page("Print");
	GtkWidget * body = GTK_WIDGET(
		g_object_get_data(G_OBJECT(page), "abi-bs-body"));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Print…", "Print this document",
				"cairoPrint", nullptr, false));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Print Preview", "Preview pages before printing",
				"printPreview", nullptr, false));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Page Setup…", "Paper size, orientation and margins",
				"pageSetup", nullptr, false));
	gtk_widget_set_visible(page, TRUE);
	return page;
}

GtkWidget * AP_UnixBackstage::_makeSharePage()
{
	GtkWidget * page = _page("Share");
	GtkWidget * body = GTK_WIDGET(
		g_object_get_data(G_OBJECT(page), "abi-bs-body"));
	GtkWidget * note = gtk_label_new(
		"Save a copy in another format to share.");
	gtk_label_set_xalign(GTK_LABEL(note), 0.0f);
	gtk_widget_add_css_class(note, "dim-label");
	gtk_box_append(GTK_BOX(body), note);
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Export a copy…", "Save in another document format",
				"fileExport", nullptr, false));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Save for the Web…",
				"Export as a web page",
				"fileSaveAsWeb", nullptr, false));
	gtk_box_append(
		GTK_BOX(body),
		_bigRow("Save as Image…",
				"Export the document as an image",
				"fileSaveImage", nullptr, false));
	gtk_widget_set_visible(page, TRUE);
	return page;
}

/*****************************************************************/

/* repopulate the Info page's live values from the document */
void AP_UnixBackstage::_refreshInfo()
{
	FV_View * pView = _view();
	PD_Document * pDoc = pView ? pView->getDocument() : nullptr;
	if (!pDoc)
		return;

	std::string prop;
	gtk_label_set_text(
		GTK_LABEL(m_wPropTitle),
		(pDoc->getMetaDataProp(PD_META_KEY_TITLE, prop) && !prop.empty())
			? prop.c_str() : "—");
	gtk_label_set_text(
		GTK_LABEL(m_wPropSubject),
		(pDoc->getMetaDataProp(PD_META_KEY_SUBJECT, prop) && !prop.empty())
			? prop.c_str() : "—");
	gtk_label_set_text(
		GTK_LABEL(m_wPropAuthor),
		(pDoc->getMetaDataProp(PD_META_KEY_CREATOR, prop) && !prop.empty())
			? prop.c_str() : "—");
	gtk_label_set_text(
		GTK_LABEL(m_wPropKeywords),
		(pDoc->getMetaDataProp(PD_META_KEY_KEYWORDS, prop) && !prop.empty())
			? prop.c_str() : "—");
	gtk_label_set_text(
		GTK_LABEL(m_wPropCategory),
		(pDoc->getMetaDataProp(PD_META_KEY_CATEGORY, prop) && !prop.empty())
			? prop.c_str() : "—");

	/* protection status mirrors the W44 restrict-editing state */
	if (pDoc->isProtectionEnforced())
	{
		const std::string mode = pDoc->getProtectionMode();
		gtk_label_set_text(
			GTK_LABEL(m_wProtectStatus),
			(std::string("This document is protected") +
			 (mode.empty() ? std::string(".")
						   : std::string(" (") + mode + ").")).c_str());
	}
	else
		gtk_label_set_text(
			GTK_LABEL(m_wProtectStatus),
			"Anyone can open and edit this document.");

	/* live statistics: pages/words/paragraphs/characters/lines from
	 * the same counter the Word Count dialog uses, plus the
	 * comments count from the privacy inspector's read pass */
	GtkWidget * child;
	while ((child = gtk_widget_get_first_child(m_wStatGrid)) != nullptr)
		gtk_grid_remove(GTK_GRID(m_wStatGrid), child);
	const FV_DocCount cnt = pView->countWords(true);
	UT_uint32 nComments = 0;
	{
		AP_PrivacyReport rep;
		AP_DocInspect::inspect(pDoc, rep);
		for (const AP_PrivacyCategory & c : rep.cats)
			if (c.mask & AP_PRIVACY_ANNOTATIONS)
				nComments = static_cast<UT_uint32>(c.details.size());
	}
	char nb[32];
	int row = 0;
	g_snprintf(nb, sizeof(nb), "%u", cnt.page);
	_statRow("Pages:", nb, row++);
	g_snprintf(nb, sizeof(nb), "%u", cnt.word);
	_statRow("Words:", nb, row++);
	g_snprintf(nb, sizeof(nb), "%u", cnt.para);
	_statRow("Paragraphs:", nb, row++);
	g_snprintf(nb, sizeof(nb), "%u", cnt.ch_sp);
	_statRow("Characters:", nb, row++);
	g_snprintf(nb, sizeof(nb), "%u", cnt.ch_no);
	_statRow("Characters (no spaces):", nb, row++);
	g_snprintf(nb, sizeof(nb), "%u", cnt.line);
	_statRow("Lines:", nb, row++);
	g_snprintf(nb, sizeof(nb), "%u", nComments);
	_statRow("Comments:", nb, row++);

	const char * fn = m_pFrame ? m_pFrame->getFilename() : nullptr;
	gtk_label_set_text(GTK_LABEL(m_wLocation),
					   (fn && *fn) ? fn : "Not saved yet");
}

/* recent list, same data source as the ribbon's Recent popover:
 * stored file:// URIs / plain paths, opened through openRecent */
void AP_UnixBackstage::_refreshRecent()
{
	if (!m_wRecentBox)
		return;
	GtkWidget * child;
	while ((child = gtk_widget_get_first_child(m_wRecentBox)) != nullptr)
		gtk_box_remove(GTK_BOX(m_wRecentBox), child);

	XAP_App * pApp = XAP_App::getApp();
	XAP_Prefs * pPrefs = pApp ? pApp->getPrefs() : nullptr;
	if (!pPrefs)
		return;
	const UT_uint32 n = pPrefs->getRecentCount();
	if (!n)
	{
		GtkWidget * lab = gtk_label_new(
			"Documents you open will show up here.");
		gtk_label_set_xalign(GTK_LABEL(lab), 0.0f);
		gtk_widget_add_css_class(lab, "dim-label");
		gtk_widget_set_margin_start(lab, 8);
		gtk_box_append(GTK_BOX(m_wRecentBox), lab);
		return;
	}
	for (UT_uint32 k = 1; k <= n; ++k)
	{
		const char * szStored = pPrefs->getRecent(k);
		if (!szStored || !*szStored)
			continue;
		const bool bIsUri = UT_go_path_is_uri(szStored);
		gchar * szBase = bIsUri ? UT_go_basename_from_uri(szStored)
								: g_path_get_basename(szStored);
		gchar * szFull = bIsUri ? UT_go_filename_from_uri(szStored)
								: g_strdup(szStored);
		GtkWidget * w = _bigRow(szBase ? szBase : szStored,
								szFull ? szFull : szStored,
								"openRecent", szStored, false);
		gtk_box_append(GTK_BOX(m_wRecentBox), w);
		g_free(szBase);
		g_free(szFull);
	}
}

void AP_UnixBackstage::refresh()
{
	const char * cur = page();
	if (cur && !strcmp(cur, "open"))
		_refreshRecent();
	else if (cur && !strcmp(cur, "info"))
		_refreshInfo();
	else
	{
		/* keep Info's live numbers honest even when the backstage
		 * opens on another page */
		_refreshInfo();
	}
}

const char * AP_UnixBackstage::page() const
{
	if (!m_wStack)
		return "";
	const char * name = gtk_stack_get_visible_child_name(
		GTK_STACK(m_wStack));
	return name ? name : "";
}

void AP_UnixBackstage::setPage(const char * szName)
{
	if (!m_wStack)
		return;
	static const char * const known[] =
		{ "info", "new", "open", "saveas", "print", "share" };
	bool bKnown = false;
	for (const char * k : known)
		if (szName && !strcmp(szName, k))
			bKnown = true;
	gtk_stack_set_visible_child_name(GTK_STACK(m_wStack),
									 bKnown ? szName : "info");
	for (GtkWidget * c = gtk_widget_get_first_child(m_wNav); c;
		 c = gtk_widget_get_next_sibling(c))
	{
		const char * rowPage = static_cast<const char *>(
			g_object_get_data(G_OBJECT(c), "abi-bs-page"));
		const bool active = rowPage &&
			!strcmp(rowPage, bKnown ? szName : "info");
		if (active)
			gtk_widget_add_css_class(c, "abinova-backstage-active");
		else
			gtk_widget_remove_css_class(c, "abinova-backstage-active");
	}
	refresh();
}

void AP_UnixBackstage::grabFocus()
{
	if (m_wBackBtn)
		gtk_widget_grab_focus(m_wBackBtn);
}

/*****************************************************************/

GtkWidget * AP_UnixBackstage::createWidget()
{
	/* one display-wide provider for the backstage chrome; theme
	 * colour lookups degrade harmlessly when a variable is missing */
	GtkCssProvider * css = gtk_css_provider_new();
	gtk_css_provider_load_from_string(css,
		".abinova-backstage { background: @theme_base_color; }"
		".abinova-backstage-nav { background: shade(@theme_bg_color, 0.93); }"
		".abinova-backstage-row { padding-top: 8px; padding-bottom: 8px; }"
		".abinova-backstage-active { "
		"  background: alpha(@theme_selected_bg_color, 0.35); }"
		".abinova-backstage-back { margin: 6px; }");
	gtk_style_context_add_provider_for_display(
		gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);

	m_wRoot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(m_wRoot, "abinova-backstage");
	gtk_widget_set_hexpand(m_wRoot, TRUE);
	gtk_widget_set_vexpand(m_wRoot, TRUE);

	/* ---- left nav column ---- */
	m_wNav = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(m_wNav, "abinova-backstage-nav");
	gtk_widget_set_size_request(m_wNav, 200, -1);
	gtk_box_append(GTK_BOX(m_wRoot), m_wNav);

	m_wBackBtn = gtk_button_new_from_icon_name("go-previous-symbolic");
	gtk_widget_set_halign(m_wBackBtn, GTK_ALIGN_START);
	gtk_widget_add_css_class(m_wBackBtn, "flat");
	gtk_widget_add_css_class(m_wBackBtn, "abinova-backstage-back");
	gtk_widget_set_tooltip_text(m_wBackBtn, "Back");
	g_signal_connect(m_wBackBtn, "clicked",
					 G_CALLBACK(_s_back_clicked), this);
	gtk_box_append(GTK_BOX(m_wNav), m_wBackBtn);

	for (const _NavSpec & spec : s_nav)
		gtk_box_append(GTK_BOX(m_wNav),
					   _navRow(spec.label, spec.page, spec.action));

	GtkWidget * sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_append(GTK_BOX(m_wRoot), sep);

	/* ---- content stack ---- */
	m_wStack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(m_wStack),
								  GTK_STACK_TRANSITION_TYPE_NONE);
	gtk_widget_set_hexpand(m_wStack, TRUE);
	gtk_widget_set_vexpand(m_wStack, TRUE);
	gtk_stack_add_named(GTK_STACK(m_wStack), _makeInfoPage(), "info");
	gtk_stack_add_named(GTK_STACK(m_wStack), _makeNewPage(), "new");
	gtk_stack_add_named(GTK_STACK(m_wStack), _makeOpenPage(), "open");
	gtk_stack_add_named(GTK_STACK(m_wStack), _makeSaveAsPage(),
						"saveas");
	gtk_stack_add_named(GTK_STACK(m_wStack), _makePrintPage(),
						"print");
	gtk_stack_add_named(GTK_STACK(m_wStack), _makeSharePage(),
						"share");
	gtk_box_append(GTK_BOX(m_wRoot), m_wStack);
	gtk_stack_set_visible_child_name(GTK_STACK(m_wStack), "info");

	GtkEventController * keys = gtk_event_controller_key_new();
	g_signal_connect(keys, "key-pressed", G_CALLBACK(_s_key), this);
	gtk_widget_add_controller(m_wRoot, keys);

	return m_wRoot;
}
