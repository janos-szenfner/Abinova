/* Abinova
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

#include <cmath>
#include <string>

#include <gtk/gtk.h>

#include "ap_UnixDialog_Document.h"

#include "ut_string.h"
#include "ut_units.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "xap_UnixDialogHelper.h"
#include "xap_Strings.h"
#include "ap_Strings.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "pp_Property.h"
#include "pp_AttrProp.h"
#include "ev_EditMethod.h"
#include "ap_Prefs_SchemeIds.h"

/* custom response ids - the actions keep the dialog open, except
 * Print which closes it and hands off to the print dialog */
enum
{
	RESP_PRINT = 1
};

static FV_View * _doc_dialog_view(XAP_Frame * pFrame)
{
	return static_cast<FV_View *>(pFrame ? pFrame->getCurrentView()
										: nullptr);
}

/* invoke an edit method by name - same path the ribbon popovers use */
static void _call_edit_method(XAP_Frame * pFrame, const char * szMethod,
							  const char * szData)
{
	const EV_EditMethodContainer * pEMC =
		XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);
	EV_EditMethod * pEM = pEMC->findEditMethodByName(szMethod);
	UT_return_if_fail(pEM);
	EV_EditMethodCallData emcd(szData ? szData : "",
							   szData ? strlen(szData) : 0);
	pEM->Fn(_doc_dialog_view(pFrame), &emcd);
}

/* apply "k:v;k:v" props to the section containing the caret */
static void _apply_section_props(FV_View * pView, const char * szProps)
{
	UT_return_if_fail(pView && szProps);
	PP_PropertyVector props;
	std::string rest = szProps;
	size_t pos = 0;
	while (pos <= rest.size())
	{
		size_t semi = rest.find(';', pos);
		std::string kv = rest.substr(pos, semi == std::string::npos
									 ? std::string::npos : semi - pos);
		size_t colon = kv.find(':');
		if (colon > 0)
		{
			props.push_back(kv.substr(0, colon));
			props.push_back(kv.substr(colon + 1));
		}
		if (semi == std::string::npos)
			break;
		pos = semi + 1;
	}
	if (!props.empty())
		pView->setSectionFormat(props);
}

/* ============ Line Numbers… ============ */

struct _LineNumCtx
{
	FV_View *	pView;
	GtkWidget * chk;
	GtkWidget * start;
	GtkWidget * countby;
	GtkWidget * dist;
	GtkWidget * cont;
	GtkWidget * page;
	GtkWidget * sect;
};

static void _s_linenum_ok(GtkDialog * dlg, gint resp, gpointer data)
{
	_LineNumCtx * c = static_cast<_LineNumCtx *>(data);
	if (resp == GTK_RESPONSE_OK && c->pView)
	{
		const char * mode = "none";
		if (gtk_check_button_get_active(GTK_CHECK_BUTTON(c->chk)))
		{
			if (gtk_check_button_get_active(GTK_CHECK_BUTTON(c->page)))
				mode = "page";
			else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(c->sect)))
				mode = "section";
			else
				mode = "continuous";
		}
		std::string props = std::string("line-numbering:") + mode;
		char buf[128];
		g_snprintf(buf, sizeof(buf), ";line-number-start:%d;"
				   "line-number-count-by:%d;line-number-distance:%.2fin",
				   gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(c->start)),
				   gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(c->countby)),
				   gtk_spin_button_get_value(GTK_SPIN_BUTTON(c->dist)));
		props += buf;
		_apply_section_props(c->pView, props.c_str());
	}
	gtk_window_destroy(GTK_WINDOW(dlg));
}

/* Note: the layout engine does not render line numbers yet - the
 * settings are stored on the section and round-trip in .abw files */
void ap_showLineNumbersDialog(GtkWindow * parent, FV_View * pView)
{
	GtkWidget * dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "Line Numbers");
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
	gtk_dialog_add_buttons(GTK_DIALOG(dlg),
						   "_OK", GTK_RESPONSE_OK,
						   "_Cancel", GTK_RESPONSE_CANCEL, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	_LineNumCtx * c = new _LineNumCtx;
	c->pView = pView;
	g_object_set_data_full(G_OBJECT(dlg), "linectx", c,
						   [](gpointer p) { delete static_cast<_LineNumCtx *>(p); });

	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);
	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), box);

	c->chk = gtk_check_button_new_with_label("Add line numbering");
	gtk_box_append(GTK_BOX(box), c->chk);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_widget_set_margin_start(grid, 12);
	gtk_box_append(GTK_BOX(box), grid);

	auto spinrow = [&](const char * label, int row, double val,
					   double lo, double hi, double step, int digits) {
		GtkWidget * l = gtk_label_new(label);
		gtk_widget_set_halign(l, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);
		GtkWidget * s = gtk_spin_button_new_with_range(lo, hi, step);
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(s), digits);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), val);
		gtk_grid_attach(GTK_GRID(grid), s, 1, row, 1, 1);
		return s;
	};

	c->start = spinrow("Start at:", 0, 1, 0, 10000, 1, 0);
	c->dist = spinrow("From text:", 1, 0.0, 0.0, 10.0, 0.05, 2);
	c->countby = spinrow("Count by:", 2, 1, 1, 100, 1, 0);

	GtkWidget * f = gtk_frame_new("Numbering");
	GtkWidget * vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_top(vb, 6);
	gtk_widget_set_margin_bottom(vb, 6);
	gtk_widget_set_margin_start(vb, 6);
	gtk_widget_set_margin_end(vb, 6);
	gtk_frame_set_child(GTK_FRAME(f), vb);
	c->cont = gtk_check_button_new_with_label("Continuous");
	c->page = gtk_check_button_new_with_label("Restart each page");
	c->sect = gtk_check_button_new_with_label("Restart each section");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(c->page),
							   GTK_CHECK_BUTTON(c->cont));
	gtk_check_button_set_group(GTK_CHECK_BUTTON(c->sect),
							   GTK_CHECK_BUTTON(c->cont));
	gtk_check_button_set_active(GTK_CHECK_BUTTON(c->cont), TRUE);
	gtk_box_append(GTK_BOX(vb), c->cont);
	gtk_box_append(GTK_BOX(vb), c->page);
	gtk_box_append(GTK_BOX(vb), c->sect);
	gtk_box_append(GTK_BOX(box), f);

	GtkWidget * note = gtk_label_new(
		"Line numbers are stored in the document but are not "
		"rendered yet.");
	gtk_widget_add_css_class(note, "dim-label");
	gtk_widget_set_halign(note, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), note);

	/* reflect stored state */
	PP_PropertyVector props;
	if (pView && pView->getSectionFormat(props))
	{
		const std::string & m = PP_getAttribute("line-numbering", props);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(c->chk),
									!m.empty() && m != "none");
		if (m == "page")
			gtk_check_button_set_active(GTK_CHECK_BUTTON(c->page), TRUE);
		else if (m == "section")
			gtk_check_button_set_active(GTK_CHECK_BUTTON(c->sect), TRUE);
		const std::string & s = PP_getAttribute("line-number-start", props);
		if (!s.empty())
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->start),
									  atoi(s.c_str()));
		const std::string & n = PP_getAttribute("line-number-count-by", props);
		if (!n.empty())
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->countby),
									  atoi(n.c_str()));
		const std::string & d = PP_getAttribute("line-number-distance", props);
		if (!d.empty())
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->dist),
									  UT_convertToInches(d.c_str()));
	}

	g_signal_connect(dlg, "response", G_CALLBACK(_s_linenum_ok), c);
	gtk_window_present(GTK_WINDOW(dlg));
}

/* ============ Hyphenation Options… ============ */

struct _HyphCtx
{
	FV_View *	pView;
	GtkWidget * chk;
	GtkWidget * zone;
	GtkWidget * limit;
};

static void _s_hyph_ok(GtkDialog * dlg, gint resp, gpointer data)
{
	_HyphCtx * c = static_cast<_HyphCtx *>(data);
	if (resp == GTK_RESPONSE_OK && c->pView &&
		c->pView->getLayout() && c->pView->getLayout()->getDocument())
	{
		char buf[128];
		g_snprintf(buf, sizeof(buf),
				   "hyphenation:%s;hyphenation-zone:%.2fin;"
				   "hyphenation-limit:%d",
				   gtk_check_button_get_active(GTK_CHECK_BUTTON(c->chk))
				   ? "auto" : "none",
				   gtk_spin_button_get_value(GTK_SPIN_BUTTON(c->zone)),
				   gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(c->limit)));
		PD_Document * pDoc = c->pView->getLayout()->getDocument();
		std::string rest = buf;
		PP_PropertyVector props;
		size_t pos = 0;
		while (pos <= rest.size())
		{
			size_t semi = rest.find(';', pos);
			std::string kv = rest.substr(pos, semi == std::string::npos
										 ? std::string::npos : semi - pos);
			size_t colon = kv.find(':');
			if (colon > 0)
			{
				props.push_back(kv.substr(0, colon));
				props.push_back(kv.substr(colon + 1));
			}
			if (semi == std::string::npos)
				break;
			pos = semi + 1;
		}
		if (!props.empty())
			pDoc->setAttrProp(props);
	}
	gtk_window_destroy(GTK_WINDOW(dlg));
}

void ap_showHyphenationDialog(GtkWindow * parent, FV_View * pView)
{
	GtkWidget * dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "Hyphenation");
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
	gtk_dialog_add_buttons(GTK_DIALOG(dlg),
						   "_OK", GTK_RESPONSE_OK,
						   "_Cancel", GTK_RESPONSE_CANCEL, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	_HyphCtx * c = new _HyphCtx;
	c->pView = pView;
	g_object_set_data_full(G_OBJECT(dlg), "hyphctx", c,
						   [](gpointer p) { delete static_cast<_HyphCtx *>(p); });

	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);
	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), box);

	c->chk = gtk_check_button_new_with_label(
		"Automatically hyphenate document");
	gtk_box_append(GTK_BOX(box), c->chk);

	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_widget_set_margin_start(grid, 12);
	gtk_box_append(GTK_BOX(box), grid);

	GtkWidget * lz = gtk_label_new("Hyphenation zone:");
	gtk_widget_set_halign(lz, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), lz, 0, 0, 1, 1);
	c->zone = gtk_spin_button_new_with_range(0.0, 10.0, 0.05);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(c->zone), 2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->zone), 0.25);
	gtk_grid_attach(GTK_GRID(grid), c->zone, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("in"), 2, 0, 1, 1);

	GtkWidget * ll = gtk_label_new("Limit consecutive hyphens to:");
	gtk_widget_set_halign(ll, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), ll, 0, 1, 1, 1);
	c->limit = gtk_spin_button_new_with_range(0, 20, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->limit), 0);
	gtk_grid_attach(GTK_GRID(grid), c->limit, 1, 1, 1, 1);

	GtkWidget * note = gtk_label_new(
		"Hyphenation settings are stored in the document but are "
		"not rendered yet.");
	gtk_widget_add_css_class(note, "dim-label");
	gtk_widget_set_halign(note, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), note);

	/* reflect stored state */
	if (pView && pView->getLayout() && pView->getLayout()->getDocument())
	{
		const PP_AttrProp * pAttr =
			pView->getLayout()->getDocument()->getAttrProp();
		if (pAttr)
		{
			const PP_PropertyVector attrs = pAttr->getProperties();
			const std::string & m = PP_getAttribute("hyphenation", attrs);
			gtk_check_button_set_active(GTK_CHECK_BUTTON(c->chk),
										m == "auto" || m == "manual");
			const std::string & z = PP_getAttribute("hyphenation-zone",
													attrs);
			if (!z.empty())
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->zone),
										  UT_convertToInches(z.c_str()));
			const std::string & l = PP_getAttribute("hyphenation-limit",
													attrs);
			if (!l.empty())
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(c->limit),
										  atoi(l.c_str()));
		}
	}

	g_signal_connect(dlg, "response", G_CALLBACK(_s_hyph_ok), c);
	gtk_window_present(GTK_WINDOW(dlg));
}

/* ============ the Document dialog ============ */

XAP_Dialog * AP_UnixDialog_Document::static_constructor(
	XAP_DialogFactory * pFactory, XAP_Dialog_Id id)
{
	return new AP_UnixDialog_Document(pFactory, id);
}

AP_UnixDialog_Document::AP_UnixDialog_Document(XAP_DialogFactory * pDlgFactory,
											   XAP_Dialog_Id id)
	: AP_Dialog_Document(pDlgFactory, id),
	  m_wMainWindow(nullptr), m_wNotebook(nullptr),
	  m_wSpinTop(nullptr), m_wSpinBottom(nullptr),
	  m_wSpinLeft(nullptr), m_wSpinRight(nullptr),
	  m_wSpinGutter(nullptr), m_wGutterPos(nullptr),
	  m_wMultiPage(nullptr), m_wApplyTo(nullptr), m_wPreview(nullptr),
	  m_wSectionStart(nullptr), m_wOddEven(nullptr), m_wFirstPage(nullptr),
	  m_wSpinHeader(nullptr), m_wSpinFooter(nullptr), m_wVAlign(nullptr),
	  m_bWantPrint(false)
{
}

AP_UnixDialog_Document::~AP_UnixDialog_Document()
{
}

void AP_UnixDialog_Document::runModal(XAP_Frame * pFrame)
{
	UT_return_if_fail(pFrame);
	m_pFrame = pFrame;

	GtkWidget * mainWindow = _constructWindow();
	UT_return_if_fail(mainWindow);

	XAP_UnixFrameImpl * pImpl =
		static_cast<XAP_UnixFrameImpl *>(pFrame->getFrameImpl());
	GtkWidget * parentWindow = pImpl->getTopLevelWindow();
	gtk_window_set_transient_for(GTK_WINDOW(mainWindow),
								 GTK_WINDOW(parentWindow));
	gtk_window_set_modal(GTK_WINDOW(mainWindow), TRUE);
	gtk_widget_show(mainWindow);

	gint resp = abiRunModalDialog(GTK_DIALOG(mainWindow), pFrame, this,
								GTK_RESPONSE_CANCEL, true);
	switch (resp)
	{
	case GTK_RESPONSE_OK:
		setAnswer(a_OK);
		break;
	case RESP_PRINT:
		/* close and hand over to the regular print dialog */
		m_bWantPrint = true;
		setAnswer(a_CANCEL);
		break;
	default:
		setAnswer(a_CANCEL);
		break;
	}

	if (m_bWantPrint)
		_call_edit_method(pFrame, "print", nullptr);
}

static GtkWidget * _dropdown(const char * const * items)
{
	return gtk_drop_down_new_from_strings(items);
}

static guint _dropdown_index(GtkDropDown * dd)
{
	return gtk_drop_down_get_selected(dd);
}

static void _dropdown_select(GtkDropDown * dd, guint idx)
{
	gtk_drop_down_set_selected(dd, idx);
}

/* labelled spin in display units (in/cm/mm); value in those units */
GtkWidget * AP_UnixDialog_Document::_unitSpin(const char * szLabel,
											  GtkWidget * grid, int row,
											  float value)
{
	GtkWidget * l = gtk_label_new(szLabel);
	gtk_widget_set_halign(l, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);

	double max = (getMarginUnits() == DIM_MM) ? 500.0
		: (getMarginUnits() == DIM_CM) ? 50.0 : 20.0;
	GtkWidget * s = gtk_spin_button_new_with_range(-100.0, max, 0.01);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(s), 2);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(s), FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), value);
	gtk_editable_set_width_chars(GTK_EDITABLE(s), 6);
	g_signal_connect(s, "value-changed",
					 G_CALLBACK(_s_spin_changed), this);
	gtk_grid_attach(GTK_GRID(grid), s, 1, row, 1, 1);

	GtkWidget * u = gtk_label_new(UT_dimensionName(getMarginUnits()));
	gtk_widget_set_halign(u, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), u, 2, row, 1, 1);
	return s;
}

GtkWidget * AP_UnixDialog_Document::_constructMarginsPage()
{
	GtkWidget * outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_top(outer, 8);
	gtk_widget_set_margin_bottom(outer, 8);
	gtk_widget_set_margin_start(outer, 8);
	gtk_widget_set_margin_end(outer, 8);

	/* Margins frame */
	GtkWidget * fM = gtk_frame_new("Margins");
	GtkWidget * grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_widget_set_margin_top(grid, 6);
	gtk_widget_set_margin_bottom(grid, 6);
	gtk_widget_set_margin_start(grid, 6);
	gtk_widget_set_margin_end(grid, 6);
	gtk_frame_set_child(GTK_FRAME(fM), grid);

	m_wSpinTop = _unitSpin("Top:", grid, 0, getMarginTop());
	m_wSpinBottom = _unitSpin("Bottom:", grid, 1, getMarginBottom());
	m_wSpinLeft = _unitSpin("Left:", grid, 2, getMarginLeft());
	m_wSpinRight = _unitSpin("Right:", grid, 3, getMarginRight());
	m_wSpinGutter = _unitSpin("Gutter:", grid, 4, getMarginGutter());

	GtkWidget * lg = gtk_label_new("Gutter position:");
	gtk_widget_set_halign(lg, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(grid), lg, 3, 4, 1, 1);
	static const char * gpos[] = { "Left", "Top", nullptr };
	m_wGutterPos = _dropdown(gpos);
	_dropdown_select(GTK_DROP_DOWN(m_wGutterPos),
					 getGutterPosition() == GUTTER_TOP ? 1 : 0);
	gtk_grid_attach(GTK_GRID(grid), m_wGutterPos, 4, 4, 1, 1);
	gtk_box_append(GTK_BOX(outer), fM);

	/* Multiple pages */
	GtkWidget * fP = gtk_frame_new("Pages");
	GtkWidget * g2 = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(g2), 8);
	gtk_widget_set_margin_top(g2, 6);
	gtk_widget_set_margin_bottom(g2, 6);
	gtk_widget_set_margin_start(g2, 6);
	gtk_widget_set_margin_end(g2, 6);
	gtk_frame_set_child(GTK_FRAME(fP), g2);
	GtkWidget * lm = gtk_label_new("Multiple pages:");
	gtk_widget_set_halign(lm, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(g2), lm, 0, 0, 1, 1);
	static const char * multi[] = { "Normal", "Mirror margins",
									"2 pages per sheet", "Book fold",
									nullptr };
	m_wMultiPage = _dropdown(multi);
	_dropdown_select(GTK_DROP_DOWN(m_wMultiPage), getMultiplePages());
	g_signal_connect(m_wMultiPage, "notify::selected",
					 G_CALLBACK(_s_combo_changed), this);
	gtk_grid_attach(GTK_GRID(g2), m_wMultiPage, 1, 0, 1, 1);
	gtk_box_append(GTK_BOX(outer), fP);

	/* Preview + Apply to */
	GtkWidget * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_append(GTK_BOX(outer), row);

	GtkWidget * fV = gtk_frame_new("Preview");
	m_wPreview = gtk_drawing_area_new();
	gtk_widget_set_size_request(m_wPreview, 150, 190);
	gtk_widget_set_margin_top(m_wPreview, 6);
	gtk_widget_set_margin_bottom(m_wPreview, 6);
	gtk_widget_set_margin_start(m_wPreview, 6);
	gtk_widget_set_margin_end(m_wPreview, 6);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_wPreview),
								   _s_preview_draw, this, nullptr);
	gtk_frame_set_child(GTK_FRAME(fV), m_wPreview);
	gtk_box_append(GTK_BOX(row), fV);

	GtkWidget * fA = gtk_frame_new("Apply to");
	GtkWidget * g3 = gtk_grid_new();
	gtk_widget_set_margin_top(g3, 6);
	gtk_widget_set_margin_bottom(g3, 6);
	gtk_widget_set_margin_start(g3, 6);
	gtk_widget_set_margin_end(g3, 6);
	gtk_frame_set_child(GTK_FRAME(fA), g3);
	static const char * apply[] = { "Whole document", "This section",
									"This point forward", nullptr };
	m_wApplyTo = _dropdown(apply);
	_dropdown_select(GTK_DROP_DOWN(m_wApplyTo), getApplyTo());
	gtk_widget_set_valign(m_wApplyTo, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(g3), m_wApplyTo, 0, 0, 1, 1);
	gtk_box_append(GTK_BOX(row), fA);
	gtk_widget_set_hexpand(fA, TRUE);

	return outer;
}

GtkWidget * AP_UnixDialog_Document::_constructLayoutPage()
{
	GtkWidget * outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_top(outer, 8);
	gtk_widget_set_margin_bottom(outer, 8);
	gtk_widget_set_margin_start(outer, 8);
	gtk_widget_set_margin_end(outer, 8);

	/* Section */
	GtkWidget * fS = gtk_frame_new("Section");
	GtkWidget * g = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(g), 8);
	gtk_widget_set_margin_top(g, 6);
	gtk_widget_set_margin_bottom(g, 6);
	gtk_widget_set_margin_start(g, 6);
	gtk_widget_set_margin_end(g, 6);
	gtk_frame_set_child(GTK_FRAME(fS), g);
	GtkWidget * ls = gtk_label_new("Section start:");
	gtk_widget_set_halign(ls, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(g), ls, 0, 0, 1, 1);
	static const char * starts[] = { "Continuous", "New page",
									 "Even page", "Odd page", nullptr };
	m_wSectionStart = _dropdown(starts);
	_dropdown_select(GTK_DROP_DOWN(m_wSectionStart), getSectionStart());
	gtk_grid_attach(GTK_GRID(g), m_wSectionStart, 1, 0, 1, 1);
	gtk_box_append(GTK_BOX(outer), fS);

	/* Headers and footers */
	GtkWidget * fH = gtk_frame_new("Headers and footers");
	GtkWidget * g2 = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(g2), 6);
	gtk_grid_set_column_spacing(GTK_GRID(g2), 8);
	gtk_widget_set_margin_top(g2, 6);
	gtk_widget_set_margin_bottom(g2, 6);
	gtk_widget_set_margin_start(g2, 6);
	gtk_widget_set_margin_end(g2, 6);
	gtk_frame_set_child(GTK_FRAME(fH), g2);

	m_wOddEven = gtk_check_button_new_with_label("Different odd and even");
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wOddEven),
								getDifferentOddEven());
	gtk_grid_attach(GTK_GRID(g2), m_wOddEven, 0, 0, 2, 1);
	m_wFirstPage = gtk_check_button_new_with_label("Different first page");
	gtk_check_button_set_active(GTK_CHECK_BUTTON(m_wFirstPage),
							  getDifferentFirstPage());
	gtk_grid_attach(GTK_GRID(g2), m_wFirstPage, 0, 1, 2, 1);

	GtkWidget * lh = gtk_label_new("From edge:");
	gtk_widget_set_halign(lh, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(g2), lh, 0, 2, 1, 1);
	GtkWidget * hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(hb), gtk_label_new("Header:"));
	m_wSpinHeader = gtk_spin_button_new_with_range(0.0, 20.0, 0.01);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(m_wSpinHeader), 2);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(m_wSpinHeader), FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wSpinHeader),
							  getMarginHeader());
	gtk_editable_set_width_chars(GTK_EDITABLE(m_wSpinHeader), 6);
	gtk_box_append(GTK_BOX(hb), m_wSpinHeader);
	gtk_box_append(GTK_BOX(hb),
				   gtk_label_new(UT_dimensionName(getMarginUnits())));
	gtk_box_append(GTK_BOX(hb), gtk_label_new("  Footer:"));
	m_wSpinFooter = gtk_spin_button_new_with_range(0.0, 20.0, 0.01);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(m_wSpinFooter), 2);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(m_wSpinFooter), FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_wSpinFooter),
							  getMarginFooter());
	gtk_editable_set_width_chars(GTK_EDITABLE(m_wSpinFooter), 6);
	gtk_box_append(GTK_BOX(hb), m_wSpinFooter);
	gtk_box_append(GTK_BOX(hb),
				   gtk_label_new(UT_dimensionName(getMarginUnits())));
	gtk_grid_attach(GTK_GRID(g2), hb, 0, 3, 2, 1);
	gtk_box_append(GTK_BOX(outer), fH);

	/* Page: vertical alignment */
	GtkWidget * fV = gtk_frame_new("Page");
	GtkWidget * g3 = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(g3), 8);
	gtk_widget_set_margin_top(g3, 6);
	gtk_widget_set_margin_bottom(g3, 6);
	gtk_widget_set_margin_start(g3, 6);
	gtk_widget_set_margin_end(g3, 6);
	gtk_frame_set_child(GTK_FRAME(fV), g3);
	GtkWidget * lv = gtk_label_new("Vertical alignment:");
	gtk_widget_set_halign(lv, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(g3), lv, 0, 0, 1, 1);
	static const char * va[] = { "Top", "Center", "Justified", "Bottom",
								 nullptr };
	m_wVAlign = _dropdown(va);
	_dropdown_select(GTK_DROP_DOWN(m_wVAlign), getVerticalAlign());
	gtk_grid_attach(GTK_GRID(g3), m_wVAlign, 1, 0, 1, 1);
	gtk_box_append(GTK_BOX(outer), fV);

	/* Line Numbers… / Borders… */
	GtkWidget * brow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	GtkWidget * bl = gtk_button_new_with_label("Line Numbers…");
	g_signal_connect_swapped(bl, "clicked",
							 G_CALLBACK(+[] (AP_UnixDialog_Document * self) {
								 self->_doLineNumbers();
							 }), this);
	gtk_box_append(GTK_BOX(brow), bl);
	GtkWidget * bb = gtk_button_new_with_label("Borders…");
	g_signal_connect_swapped(bb, "clicked",
							 G_CALLBACK(+[] (AP_UnixDialog_Document * self) {
								 self->_doBorders();
							 }), this);
	gtk_box_append(GTK_BOX(brow), bb);
	gtk_box_append(GTK_BOX(outer), brow);

	return outer;
}

GtkWidget * AP_UnixDialog_Document::_constructWindow()
{
	GtkWidget * dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "Document");
	gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
	m_wMainWindow = dlg;

	/* GTK4 may finalize the dialog as soon as ::response is emitted;
	 * read the widget state while it is still alive. */
	g_signal_connect(dlg, "response",
					 G_CALLBACK(+[] (GtkDialog *, gint resp,
									 AP_UnixDialog_Document * self) {
						 if (resp == GTK_RESPONSE_OK)
							 self->_readWidgets();
					 }), this);

	gtk_dialog_add_buttons(GTK_DIALOG(dlg),
						   "_OK", GTK_RESPONSE_OK,
						   "_Cancel", GTK_RESPONSE_CANCEL, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	GtkWidget * area = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	GtkWidget * outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_append(GTK_BOX(area), outer);

	m_wNotebook = gtk_notebook_new();
	gtk_box_append(GTK_BOX(outer), m_wNotebook);
	gtk_notebook_append_page(GTK_NOTEBOOK(m_wNotebook),
							 _constructMarginsPage(),
							 gtk_label_new("Margins"));
	gtk_notebook_append_page(GTK_NOTEBOOK(m_wNotebook),
							 _constructLayoutPage(),
							 gtk_label_new("Layout"));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(m_wNotebook),
								  getActivePage() == PAGE_LAYOUT ? 1 : 0);

	/* left-side action row: Page Setup…  Default…  Print… */
	GtkWidget * actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_margin_start(actions, 8);
	gtk_widget_set_margin_end(actions, 8);
	gtk_widget_set_margin_bottom(actions, 6);
	gtk_box_append(GTK_BOX(outer), actions);

	GtkWidget * bps = gtk_button_new_with_label("Page Setup…");
	g_signal_connect_swapped(bps, "clicked",
							 G_CALLBACK(+[] (AP_UnixDialog_Document * self) {
								 self->_doPageSetup();
							 }), this);
	gtk_box_append(GTK_BOX(actions), bps);

	GtkWidget * bdef = gtk_button_new_with_label("Default…");
	g_signal_connect_swapped(bdef, "clicked",
							 G_CALLBACK(+[] (AP_UnixDialog_Document * self) {
								 self->_doDefault();
							 }), this);
	gtk_box_append(GTK_BOX(actions), bdef);

	GtkWidget * bpr = gtk_button_new_with_label("Print…");
	g_signal_connect_swapped(bpr, "clicked",
							 G_CALLBACK(+[] (AP_UnixDialog_Document * self) {
								 self->_doPrint();
							 }), this);
	gtk_box_append(GTK_BOX(actions), bpr);

	return dlg;
}

void AP_UnixDialog_Document::_readWidgets()
{
	setMarginTop(gtk_spin_button_get_value(GTK_SPIN_BUTTON(m_wSpinTop)));
	setMarginBottom(gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(m_wSpinBottom)));
	setMarginLeft(gtk_spin_button_get_value(GTK_SPIN_BUTTON(m_wSpinLeft)));
	setMarginRight(gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(m_wSpinRight)));
	setMarginGutter(gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(m_wSpinGutter)));
	setGutterPosition(_dropdown_index(GTK_DROP_DOWN(m_wGutterPos)) == 1
					  ? GUTTER_TOP : GUTTER_LEFT);
	setMultiplePages((tMultiPage)_dropdown_index(
		GTK_DROP_DOWN(m_wMultiPage)));
	setApplyTo((tApplyTo)_dropdown_index(GTK_DROP_DOWN(m_wApplyTo)));
	setSectionStart((tSectionStart)_dropdown_index(
		GTK_DROP_DOWN(m_wSectionStart)));
	setVerticalAlign((tVAlign)_dropdown_index(GTK_DROP_DOWN(m_wVAlign)));
	setDifferentOddEven(gtk_check_button_get_active(
		GTK_CHECK_BUTTON(m_wOddEven)));
	setDifferentFirstPage(gtk_check_button_get_active(
		GTK_CHECK_BUTTON(m_wFirstPage)));
	setMarginHeader(gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(m_wSpinHeader)));
	setMarginFooter(gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(m_wSpinFooter)));
}

/* mini page preview: shaded margins, mirrored pair for Mirror
 * margins / Book fold */
void AP_UnixDialog_Document::_s_preview_draw(GtkDrawingArea * /*area*/,
											 cairo_t * cr, int w, int h,
											 gpointer data)
{
	AP_UnixDialog_Document * self =
		static_cast<AP_UnixDialog_Document *>(data);
	UT_return_if_fail(self);

	self->_readWidgets();
	double t = self->getMarginTop(), b = self->getMarginBottom();
	double l = self->getMarginLeft(), r = self->getMarginRight();
	double g = self->getMarginGutter();
	bool mirrored = self->getMultiplePages() == MULTI_MIRROR ||
		self->getMultiplePages() == MULTI_BOOK;

	auto drawPage = [&](double px, double py, double pw, double ph,
						bool swapGutter) {
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_rectangle(cr, px, py, pw, ph);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);
		/* margins as fractions of the page */
		double mt = t * ph / 11.0, mb = b * ph / 11.0;
		double ml = l * pw / 8.5, mr = r * pw / 8.5;
		double mg = g * pw / 8.5;
		cairo_set_source_rgba(cr, 0.35, 0.55, 0.9, 0.35);
		cairo_rectangle(cr, px + ml + (swapGutter ? 0 : mg), py + mt,
						pw - ml - mr - mg, ph - mt - mb);
		cairo_fill(cr);
	};

	if (mirrored)
	{
		double pw = (w - 12.0) / 2.0;
		drawPage(2, 4, pw, h - 8.0, false);
		drawPage(10 + pw, 4, pw, h - 8.0, true);
	}
	else
	{
		double pw = w * 0.72;
		drawPage((w - pw) / 2.0, 4, pw, h - 8.0, false);
	}
}

void AP_UnixDialog_Document::_s_spin_changed(GtkSpinButton * /*spin*/,
											 gpointer data)
{
	AP_UnixDialog_Document * self =
		static_cast<AP_UnixDialog_Document *>(data);
	gtk_widget_queue_draw(self->m_wPreview);
}

void AP_UnixDialog_Document::_s_combo_changed(GtkDropDown * /*dd*/,
											  GParamSpec * /*pspec*/,
											  gpointer data)
{
	AP_UnixDialog_Document * self =
		static_cast<AP_UnixDialog_Document *>(data);
	gtk_widget_queue_draw(self->m_wPreview);
}

void AP_UnixDialog_Document::_redrawPreview()
{
	gtk_widget_queue_draw(m_wPreview);
}

/* Page Setup… - the regular Abinova page-setup dialog applies
 * size/orientation/scale to the document immediately */
void AP_UnixDialog_Document::_doPageSetup()
{
	_call_edit_method(m_pFrame, "pageSetup", nullptr);
}

/* Default… - persist the page setup to the user's normal.awt template
 * so new documents inherit it (the confirmation matches Word's) */
struct _DefCtx
{
	AP_UnixDialog_Document * self;
};

void AP_UnixDialog_Document::_s_default_response(GtkDialog * dlg,
												 gint resp, gpointer data)
{
	_DefCtx * c = static_cast<_DefCtx *>(data);
	if (resp == GTK_RESPONSE_YES && c->self)
		c->self->_writeDefaultTemplate();
	gtk_window_destroy(GTK_WINDOW(dlg));
}

void AP_UnixDialog_Document::_doDefault()
{
	_readWidgets();

	GtkWidget * dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "Abinova");
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dlg),
								 GTK_WINDOW(m_wMainWindow));
	gtk_dialog_add_buttons(GTK_DIALOG(dlg),
						   "_Yes", GTK_RESPONSE_YES,
						   "_No", GTK_RESPONSE_NO, nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);

	GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_top(box, 16);
	gtk_widget_set_margin_bottom(box, 16);
	gtk_widget_set_margin_start(box, 16);
	gtk_widget_set_margin_end(box, 16);
	GtkWidget * img = gtk_image_new_from_icon_name("dialog-question-symbolic");
	gtk_image_set_icon_size(GTK_IMAGE(img), GTK_ICON_SIZE_LARGE);
	gtk_widget_set_valign(img, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), img);
	GtkWidget * msg = gtk_label_new(
		"Do you want to change the default settings for page setup?\n"
		"This change will affect all new documents based on the\n"
		"NORMAL template.");
	gtk_label_set_wrap(GTK_LABEL(msg), TRUE);
	gtk_widget_set_halign(msg, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(box), msg);
	gtk_box_append(GTK_BOX(
		gtk_dialog_get_content_area(GTK_DIALOG(dlg))), box);

	_DefCtx * c = new _DefCtx{ this };
	g_object_set_data_full(G_OBJECT(dlg), "defctx", c,
						   [](gpointer p) { delete static_cast<_DefCtx *>(p); });
	g_signal_connect(dlg, "response", G_CALLBACK(_s_default_response), c);
	gtk_window_present(GTK_WINDOW(dlg));
}

/* write normal.awt into the user templates dir */
void AP_UnixDialog_Document::_writeDefaultTemplate()
{
	std::string dir = XAP_App::getApp()->getUserPrivateDirectory();
	dir += "/templates";
	g_mkdir_with_parents(dir.c_str(), 0700);
	std::string path = dir + "/normal.awt";

	UT_Dimension u = getMarginUnits();
	auto dim = [&](float v) {
		return std::string(UT_formatDimensionString(u, v));
	};

	const fp_PageSize & sz = getPageSize();
	UT_Dimension pu = sz.getDims();
	double w = sz.Width(pu), h = sz.Height(pu);
	if (!sz.isPortrait())
		std::swap(w, h);

	std::string xml;
	xml += "<?xml version=\"1.0\"?>\n";
	xml += "<!DOCTYPE abiword PUBLIC \"-//ABISOURCE//DTD AWML 1.0 Strict//EN\" \"http://www.abisource.com/awml.dtd\">\n";
	xml += "<abiword xmlns=\"http://www.abisource.com/awml.dtd\" "
		   "xmlns:awml=\"http://www.abisource.com/awml.dtd\" "
		   "version=\"0.99.2\" fileformat=\"1.0\" template=\"true\" "
		   "styles=\"unlocked\" props=\"lang:en-US; dom-dir:ltr\">\n";
	xml += "<styles>\n";
	xml += "<s type=\"P\" name=\"Normal\" basedon=\"\" "
		   "followedby=\"Current Settings\" "
		   "props=\"font-family:Carlito; font-size:12pt; "
		   "text-align:left; line-height:1.15\"/>\n";
	xml += "</styles>\n";
	xml += "<pagesize pagetype=\"";
	xml += sz.getPredefinedName();
	xml += "\" orientation=\"";
	xml += sz.isPortrait() ? "portrait" : "landscape";
	xml += "\" width=\"";
	xml += UT_formatDimensionString(pu, w);
	xml += "\" height=\"";
	xml += UT_formatDimensionString(pu, h);
	xml += "\" units=\"";
	xml += UT_dimensionName(pu);
	xml += "\" page-scale=\"1.000000\"/>\n";
	xml += "<section props=\"";
	xml += "page-margin-top:" + dim(getMarginTop()) + "; ";
	xml += "page-margin-bottom:" + dim(getMarginBottom()) + "; ";
	xml += "page-margin-left:" + dim(getMarginLeft()) + "; ";
	xml += "page-margin-right:" + dim(getMarginRight()) + "; ";
	xml += "page-margin-header:" + dim(getMarginHeader()) + "; ";
	xml += "page-margin-footer:" + dim(getMarginFooter());
	if (getMarginGutter() > 0.0f)
		xml += "; page-margin-gutter:" + dim(getMarginGutter());
	xml += "\">\n<p style=\"Normal\"/>\n</section>\n</abiword>\n";

	if (!g_file_set_contents(path.c_str(), xml.c_str(), xml.size(),
							 nullptr))
	{
		GtkWidget * err = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW(err), "Abinova");
		gtk_window_set_modal(GTK_WINDOW(err), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(err),
								   GTK_WINDOW(m_wMainWindow));
		gtk_dialog_add_button(GTK_DIALOG(err), "_OK", GTK_RESPONSE_OK);
		GtkWidget * msg = gtk_label_new(
			"The default template could not be saved.");
		gtk_widget_set_margin_top(msg, 16);
		gtk_widget_set_margin_bottom(msg, 16);
		gtk_widget_set_margin_start(msg, 16);
		gtk_widget_set_margin_end(msg, 16);
		gtk_box_append(GTK_BOX(
			gtk_dialog_get_content_area(GTK_DIALOG(err))), msg);
		g_signal_connect_swapped(err, "response",
								 G_CALLBACK(gtk_window_destroy), err);
		gtk_window_present(GTK_WINDOW(err));
	}
}

void AP_UnixDialog_Document::_doPrint()
{
	gtk_dialog_response(GTK_DIALOG(m_wMainWindow), RESP_PRINT);
}

void AP_UnixDialog_Document::_doLineNumbers()
{
	ap_showLineNumbersDialog(GTK_WINDOW(m_wMainWindow),
							 _doc_dialog_view(m_pFrame));
}

void AP_UnixDialog_Document::_doBorders()
{
	/* the border/shading dialog covers the border settings the
	 * engine supports */
	_call_edit_method(m_pFrame, "dlgBorders", nullptr);
}
