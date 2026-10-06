/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* The Abinova Document view Widget 
 *
 * Copyright (C) 2007 Michael Gorse
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

#include <string.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ap_DocView.h"
//#include "at_DocView.h"
#include "ut_debugmsg.h"

#include <gsf/gsf.h>

#include "fv_View.h"
#include "pd_Document.h"
#include "pd_Iterator.h"
#include "pf_Frag.h"

// our parent class
// static GtkDrawingAreaClass * parent_class = 0;

/**************************************************************************/
/**************************************************************************/

/* GtkAccessibleText implementation: without it the canvas is a
 * named-but-silent DOCUMENT node to screen readers.  Document
 * positions map 1:1 to exposed characters - text fragments emit
 * their UCS-4 char, strux (block/section) boundaries emit '\n',
 * embedded objects emit U+FFFC and format marks emit a space, so
 * FV_View insertion/selection positions can be returned to AT-SPI
 * as character offsets unchanged.  The FV_View to read from is
 * stashed on the widget by the frame impl (which owns the view
 * lifecycle) under AP_DOCVIEW_A11Y_VIEW_KEY. */

static FV_View *
s_docview_view(GtkAccessibleText * accessible)
{
	return static_cast<FV_View *>(
		g_object_get_data(G_OBJECT(accessible),
						  AP_DOCVIEW_A11Y_VIEW_KEY));
}

static void
s_docview_emit_char(GByteArray * buf, UT_UCS4Char ch)
{
	if (!g_unichar_validate(static_cast<gunichar>(ch)))
		ch = 0xFFFD;
	char utf8[8];
	int len = g_unichar_to_utf8(static_cast<gunichar>(ch), utf8);
	g_byte_array_append(buf, reinterpret_cast<const guint8 *>(utf8), len);
}

/* the exposed char at an absolute document position; only meaningful
 * while the iterator lands on a live fragment */
static UT_UCS4Char
s_docview_char_at(PD_DocIterator & it)
{
	const pf_Frag * frag = it.getFrag();
	if (!frag)
		return 0;
	if (frag->getType() == pf_Frag::PFT_Text)
	{
		UT_UCS4Char ch = it.getChar();
		return ch == UT_IT_ERROR ? 0 : ch;
	}
	if (frag->getType() == pf_Frag::PFT_Object)
		return 0xFFFC;
	if (frag->getType() == pf_Frag::PFT_FmtMark)
		return ' ';
	return '\n';
}

static bool
s_docview_is_boundary(PD_DocIterator & it)
{
	return s_docview_char_at(it) == '\n';
}

static bool
s_docview_is_word_char(UT_UCS4Char ch)
{
	return ch && (g_unichar_isalnum(static_cast<gunichar>(ch))
				  || ch == '_' || ch == '\'');
}

static GBytes *
ap_DocView_at_get_contents(GtkAccessibleText * accessible,
						   unsigned int start,
						   unsigned int end)
{
	FV_View * pView = s_docview_view(accessible);
	GByteArray * buf = g_byte_array_new();
	PD_Document * pDoc = pView ? pView->getDocument() : nullptr;
	if (!pDoc)
		return g_byte_array_free_to_bytes(buf);

	PD_DocIterator it(*pDoc, start);
	if (end == G_MAXUINT)
		end = 0xffffffff;
	it.setUpperLimit(end);
	for (unsigned int pos = start;
		 pos < end && it.getStatus() == UTIter_OK;
		 ++pos, ++it)
		s_docview_emit_char(buf, s_docview_char_at(it));
	return g_byte_array_free_to_bytes(buf);
}

static GBytes *
ap_DocView_at_get_contents_at(GtkAccessibleText * accessible,
							  unsigned int offset,
							  GtkAccessibleTextGranularity granularity,
							  unsigned int * start,
							  unsigned int * end)
{
	FV_View * pView = s_docview_view(accessible);
	PD_Document * pDoc = pView ? pView->getDocument() : nullptr;
	unsigned int a = offset, b = offset;
	if (pDoc)
	{
		PD_DocIterator it(*pDoc, offset);
		if (it.getStatus() == UTIter_OK)
		{
			switch (granularity)
			{
			case GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER:
				b = offset + 1;
				break;
			case GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD:
			{
				PD_DocIterator back(*pDoc, offset);
				while (back.getStatus() == UTIter_OK && a > 0)
				{
					back.setPosition(a - 1);
					if (!s_docview_is_word_char(s_docview_char_at(back)))
						break;
					--a;
				}
				while (it.getStatus() == UTIter_OK
					   && s_docview_is_word_char(s_docview_char_at(it)))
				{
					++b;
					it.setPosition(b);
				}
				break;
			}
			case GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE:
			{
				PD_DocIterator back(*pDoc, offset);
				while (back.getStatus() == UTIter_OK && a > 0)
				{
					back.setPosition(a - 1);
					UT_UCS4Char ch = s_docview_char_at(back);
					if (ch == '\n')
						break;
					--a;
					if (ch == '.' || ch == '!' || ch == '?')
						break;
				}
				while (it.getStatus() == UTIter_OK)
				{
					UT_UCS4Char ch = s_docview_char_at(it);
					++b;
					if (!ch || ch == '\n' || ch == '.' || ch == '!'
						|| ch == '?')
						break;
					it.setPosition(b);
				}
				break;
			}
			case GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE:
			case GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH:
			default:
			{
				/* rendered lines are a layout concept; report the
				 * block boundary instead - the exposed '\n' marks a
				 * strux position, which precedes its block's text */
				PD_DocIterator back(*pDoc, offset);
				while (back.getStatus() == UTIter_OK && a > 0)
				{
					back.setPosition(a - 1);
					if (s_docview_is_boundary(back))
						break;
					--a;
				}
				while (it.getStatus() == UTIter_OK)
				{
					it.setPosition(b);
					if (it.getStatus() != UTIter_OK
						|| s_docview_is_boundary(it))
						break;
					++b;
				}
				break;
			}
			}
		}
	}
	if (start)
		*start = a;
	if (end)
		*end = b;
	return ap_DocView_at_get_contents(accessible, a, b);
}

static unsigned int
ap_DocView_at_get_caret_position(GtkAccessibleText * accessible)
{
	FV_View * pView = s_docview_view(accessible);
	return pView ? static_cast<unsigned int>(pView->getInsPoint()) : 0;
}

static gboolean
ap_DocView_at_get_selection(GtkAccessibleText * accessible,
							gsize * n_ranges,
							GtkAccessibleTextRange ** ranges)
{
	FV_View * pView = s_docview_view(accessible);
	if (n_ranges)
		*n_ranges = 0;
	if (ranges)
		*ranges = nullptr;
	if (!pView || pView->isSelectionEmpty() || !n_ranges || !ranges)
		return FALSE;
	unsigned int ins = pView->getInsPoint();
	unsigned int anc = pView->getSelectionAnchor();
	*ranges = g_new(GtkAccessibleTextRange, 1);
	(*ranges)[0].start = MIN(ins, anc);
	(*ranges)[0].length = ins > anc ? ins - anc : anc - ins;
	*n_ranges = 1;
	return TRUE;
}

static gboolean
ap_DocView_at_get_attributes(GtkAccessibleText * /*accessible*/,
							 unsigned int /*offset*/,
							 gsize * n_ranges,
							 GtkAccessibleTextRange ** ranges,
							 char *** attribute_names,
							 char *** attribute_values)
{
	if (n_ranges)
		*n_ranges = 0;
	if (ranges)
		*ranges = nullptr;
	if (attribute_names)
		*attribute_names = nullptr;
	if (attribute_values)
		*attribute_values = nullptr;
	return FALSE;
}

static void
ap_DocView_at_get_default_attributes(GtkAccessibleText * /*accessible*/,
									 char *** attribute_names,
									 char *** attribute_values)
{
	if (attribute_names)
		*attribute_names = nullptr;
	if (attribute_values)
		*attribute_values = nullptr;
}

static void
ap_DocView_accessible_text_init(GtkAccessibleTextInterface * iface)
{
	iface->get_contents = ap_DocView_at_get_contents;
	iface->get_contents_at = ap_DocView_at_get_contents_at;
	iface->get_caret_position = ap_DocView_at_get_caret_position;
	iface->get_selection = ap_DocView_at_get_selection;
	iface->get_attributes = ap_DocView_at_get_attributes;
	iface->get_default_attributes = ap_DocView_at_get_default_attributes;
}

#define GET_CLASS(instance) G_TYPE_INSTANCE_GET_CLASS (instance, AP_DOCVIEW_TYPE, ApDocViewClass)

static void
ap_DocView_class_init(GtkWidgetClass *widget_class, gpointer)
{

#ifdef LOGFILE
	fprintf(getlogfile(),"ap_DocView class init \n");
#endif

	// set our parent class
//	parent_class = (GtkLayoutClass *) g_type_class_peek_parent (widget_class);
	static_cast<void>(widget_class);
}
GSF_CLASS_FULL(ApDocView, ap_DocView, nullptr, nullptr,
			   ap_DocView_class_init, nullptr, nullptr,
			   GTK_TYPE_DRAWING_AREA, 0,
			   GSF_INTERFACE(ap_DocView_accessible_text_init,
							 GTK_TYPE_ACCESSIBLE_TEXT))

/**
 * ap_DocView_new
 *
 * Creates a new ApDocView widget
 */
extern "C" GtkWidget *
ap_DocView_new (void)
{
	ApDocView * abi;
	UT_DEBUGMSG(("Constructing ApDocView \n"));
	abi = static_cast<ApDocView *>(g_object_new (ap_DocView_get_type (), nullptr));

	return GTK_WIDGET (abi);
}

