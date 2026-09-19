/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copryight (C) 2025 Hubert Figuière
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

#include <gtk/gtk.h>

#include "ut_types.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_Strings.h"
#include "xap_Dlg_Zoom.h"
#include "ap_Frame.h"
#include "ev_EditMethod.h"
#include "ap_UnixStatusBar.h"
#include "xap_UnixDialogHelper.h"

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

class ap_usb_TextListener : public AP_StatusBarFieldListener
{
public:
	ap_usb_TextListener(AP_StatusBarField *pStatusBarField, GtkWidget *pLabel) : AP_StatusBarFieldListener(pStatusBarField) { m_pLabel = pLabel; }
	virtual void notify() override;

protected:
	GtkWidget *m_pLabel;
};

void ap_usb_TextListener::notify()
{
	UT_ASSERT(m_pLabel);

	AP_StatusBarField_TextInfo * textInfo = ((AP_StatusBarField_TextInfo *)m_pStatusBarField);

	gtk_label_set_label(GTK_LABEL(m_pLabel), textInfo->getBuf().c_str());

	// we conditionally update the size request, if the representative string (or an earlier
	// size) wasn't large enough, if the element uses the representative string method
	// and is aligned with the center
	if (textInfo->getFillMethod() == REPRESENTATIVE_STRING && 
	    textInfo->getAlignmentMethod() == CENTER) {
		GtkRequisition requisition;
		gint iOldWidthRequest, iOldHeightRequest;
		gtk_widget_get_size_request(m_pLabel, &iOldWidthRequest, &iOldHeightRequest);
		gtk_widget_set_size_request(m_pLabel, -1, -1);
		gtk_widget_get_preferred_size(m_pLabel, &requisition, nullptr);
		if (requisition.width > iOldWidthRequest)
			gtk_widget_set_size_request(m_pLabel, requisition.width, -1);
		else
			gtk_widget_set_size_request(m_pLabel, iOldWidthRequest, -1);
	}
}


class ap_usb_ProgressListener : public AP_StatusBarFieldListener
{
public:
	ap_usb_ProgressListener(AP_StatusBarField *pStatusBarField, GtkWidget *wProgress) : AP_StatusBarFieldListener(pStatusBarField) 
        { 
	    m_wProgress = wProgress; 
	}
	virtual void notify() override;

protected:
	GtkWidget *m_wProgress;
};

void ap_usb_ProgressListener::notify()
{
	UT_ASSERT(m_wProgress);

	AP_StatusBarField_ProgressBar * pProgress = ((AP_StatusBarField_ProgressBar *)m_pStatusBarField);
	if(pProgress->isDefinate())
        {
	    double fraction = pProgress->getFraction();
	    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_wProgress),fraction);
	}
	else
	{
	    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(m_wProgress));
	}
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

static void s_zoom_value_changed(GtkRange * range, AP_UnixStatusBar * sb);

void AP_UnixStatusBar::applyZoom(UT_sint32 iZoom)
{
	UT_return_if_fail(getFrame());
	iZoom = UT_MAX(iZoom, (UT_sint32)XAP_DLG_ZOOM_MINIMUM_ZOOM);
	iZoom = UT_MIN(iZoom, (UT_sint32)XAP_DLG_ZOOM_MAXIMUM_ZOOM);
	AP_Frame * pFrame = static_cast<AP_Frame*>(getFrame());
	pFrame->setZoomType(XAP_Frame::z_PERCENT);
	pFrame->quickZoom(iZoom);
	updateZoomWidgets();
}

void AP_UnixStatusBar::updateZoomWidgets(void)
{
	if (!m_wZoomScale || !m_wZoomLabel || !getFrame())
		return;
	UT_uint32 iZoom = getFrame()->getZoomPercentage();
	m_bZoomSync = true;
	gtk_range_set_value(GTK_RANGE(m_wZoomScale), iZoom);
	m_bZoomSync = false;
	char buf[16];
	snprintf(buf, sizeof(buf), "%u%%", iZoom);
	gtk_button_set_label(GTK_BUTTON(m_wZoomLabel), buf);
}

bool AP_UnixStatusBar::notify(AV_View * pView, const AV_ChangeMask mask)
{
	bool bResult = AP_StatusBar::notify(pView, mask);
	// keep the zoom slider/label in sync when the zoom changes
	// from elsewhere (dialog, toolbar, keyboard)
	updateZoomWidgets();
	return bResult;
}

void AP_UnixStatusBar::onZoomSliderValue(double dValue)
{
	if (m_bZoomSync)
		return;
	applyZoom(static_cast<UT_sint32>(dValue + 0.5));
}

static void s_zoom_value_changed(GtkRange * range, AP_UnixStatusBar * sb)
{
	UT_return_if_fail(sb);
	sb->onZoomSliderValue(gtk_range_get_value(range));
}

static void s_zoom_out_clicked(GtkButton * /*btn*/, AP_UnixStatusBar * sb)
{
	UT_return_if_fail(sb && sb->getStatusBarFrame());
	sb->applyZoom(static_cast<UT_sint32>(sb->getStatusBarFrame()->getZoomPercentage()) - 10);
}

static void s_zoom_in_clicked(GtkButton * /*btn*/, AP_UnixStatusBar * sb)
{
	UT_return_if_fail(sb && sb->getStatusBarFrame());
	sb->applyZoom(static_cast<UT_sint32>(sb->getStatusBarFrame()->getZoomPercentage()) + 10);
}

static void s_zoom_label_clicked(GtkButton * /*btn*/, AP_UnixStatusBar * /*sb*/)
{
	// open the Zoom dialog, like clicking the indicator in MS Word
	EV_EditMethodContainer * pEMC = XAP_App::getApp()->getEditMethodContainer();
	UT_return_if_fail(pEMC);
	EV_EditMethod * pEM = pEMC->findEditMethodByName("dlgZoom");
	UT_return_if_fail(pEM);
	ev_EditMethod_invoke(pEM, UT_String(""));
}

static void s_zoom_reset_clicked(GtkButton * /*btn*/, AP_UnixStatusBar * sb)
{
	UT_return_if_fail(sb && sb->getStatusBarFrame());
	sb->applyZoom(100);
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

AP_UnixStatusBar::AP_UnixStatusBar(XAP_Frame * pFrame)
	: AP_StatusBar(pFrame)
{
	m_wStatusBar = nullptr;
	m_wProgressFrame = nullptr;
	m_wZoomScale = nullptr;
	m_wZoomLabel = nullptr;
	m_bZoomSync = false;
}

AP_UnixStatusBar::~AP_UnixStatusBar(void)
{
}

void AP_UnixStatusBar::setView(AV_View * pView)
{
	// let the base class do it's thing	
	AP_StatusBar::setView(pView);
}

void AP_UnixStatusBar::showProgressBar(void)
{
  gtk_widget_show(m_wProgressFrame);
}

void AP_UnixStatusBar::hideProgressBar(void)
{
  gtk_widget_hide(m_wProgressFrame);
}

// FIXME: we need more sanity checking here to make sure everything allocates correctly
GtkWidget * AP_UnixStatusBar::createWidget(void)
{
	UT_ASSERT(!m_wStatusBar);
	
	// probably should make this into an event box (if we want the user to be able to interact with the status bar)
	m_wStatusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	gtk_widget_show(m_wStatusBar);

	for (UT_sint32 k=0; k<getFields()->getItemCount(); k++) {
		AP_StatusBarField * pf = (AP_StatusBarField *)m_vecFields.getNthItem(k);
		UT_nonnull_or_continue(pf); // we should NOT have null elements

		// set up a frame for status bar elements so they look like status bar elements, 
		// and not just normal widgets
		GtkWidget *pStatusBarElement = nullptr;
		UT_DEBUGMSG(("Fill method %d \n",pf->getFillMethod()));
		if (pf->getFillMethod() == REPRESENTATIVE_STRING || (pf->getFillMethod() == MAX_POSSIBLE)){ //AP_StatusBarField_TextInfo *pf_TextInfo = dynamic_cast<AP_StatusBarField_TextInfo*>(pf))
		  AP_StatusBarField_TextInfo *pf_TextInfo = static_cast<AP_StatusBarField_TextInfo*>(pf);
			pStatusBarElement = gtk_frame_new(nullptr);
			GtkWidget *pStatusBarElementLabel = gtk_label_new(pf_TextInfo->getRepresentativeString());
			gtk_widget_set_margin_top(pStatusBarElementLabel, 3);
			gtk_widget_set_margin_bottom(pStatusBarElementLabel, 3);
			gtk_widget_set_margin_start(pStatusBarElementLabel, 3);
			gtk_widget_set_margin_end(pStatusBarElementLabel, 3);
			pf->setListener((AP_StatusBarFieldListener *)(new ap_usb_TextListener(pf_TextInfo, pStatusBarElementLabel)));
			xap_gtk_container_add (pStatusBarElement, pStatusBarElementLabel);

			// align
			if (pf_TextInfo->getAlignmentMethod() == LEFT) {
				gtk_label_set_xalign(GTK_LABEL(pStatusBarElementLabel), 0.0);
				gtk_label_set_yalign(GTK_LABEL(pStatusBarElementLabel), 0.0);
			}

			// size and place
			if (pf_TextInfo->getFillMethod() == REPRESENTATIVE_STRING) {
				GtkRequisition requisition;
				gtk_widget_get_preferred_size(pStatusBarElementLabel, &requisition, nullptr);
				gtk_widget_set_size_request(pStatusBarElementLabel, requisition.width, -1);

				gtk_box_append(GTK_BOX(m_wStatusBar), pStatusBarElement);
			}
			else { // fill
				gtk_box_append(GTK_BOX(m_wStatusBar), pStatusBarElement);
			gtk_widget_set_hexpand(pStatusBarElement, TRUE);
			}

			gtk_label_set_label(GTK_LABEL(pStatusBarElementLabel), ""); 
			gtk_widget_show(pStatusBarElementLabel);
		}
		else if(pf->getFillMethod() == 	PROGRESS_BAR)
		{
			GtkRequisition requisition;
			pStatusBarElement = gtk_frame_new(nullptr);
			gtk_widget_get_preferred_size(pStatusBarElement, &requisition, nullptr);
			gtk_widget_set_size_request(pStatusBarElement, -1, requisition.height);
			gtk_box_append(GTK_BOX(m_wStatusBar), pStatusBarElement);
			gtk_widget_set_hexpand(pStatusBarElement, TRUE);
			GtkWidget *  pProgress= gtk_progress_bar_new();
			xap_gtk_container_add (pStatusBarElement,pProgress);
			gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR(pProgress),0.01);

			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(pProgress),0.0);
			gtk_widget_show(pProgress);
			pf->setListener((AP_StatusBarFieldListener *)(new ap_usb_ProgressListener(pf, pProgress)));
			m_wProgressFrame = pStatusBarElement;

		}
		else
		{
		        UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		}

		gtk_widget_show(pStatusBarElement);
	}

	// LibreOffice/MS-Word-style zoom control pinned to the right end of
	// the status bar: [−] [─── slider ───] [+] [100%]
	{
		const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
		std::string sZoomIn, sZoomOut, sZoom, sZoomLevel;
		if (pSS)
		{
			pSS->getValueUTF8(XAP_STRING_ID_SB_Zoom_In, sZoomIn);
			pSS->getValueUTF8(XAP_STRING_ID_SB_Zoom_Out, sZoomOut);
			pSS->getValueUTF8(XAP_STRING_ID_SB_Zoom_Slider, sZoom);
			pSS->getValueUTF8(XAP_STRING_ID_SB_Zoom_Level, sZoomLevel);
		}

		GtkWidget * pZoomBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_margin_start(pZoomBox, 4);
		gtk_widget_set_margin_end(pZoomBox, 4);

		GtkWidget * pZoomOut = gtk_button_new_from_icon_name("zoom-out-symbolic");
		gtk_widget_set_tooltip_text(pZoomOut, sZoomOut.c_str());
		gtk_widget_add_css_class(pZoomOut, "flat");
		g_signal_connect(pZoomOut, "clicked",
						 G_CALLBACK(s_zoom_out_clicked), this);
		gtk_box_append(GTK_BOX(pZoomBox), pZoomOut);

		m_wZoomScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
												XAP_DLG_ZOOM_MINIMUM_ZOOM,
												XAP_DLG_ZOOM_MAXIMUM_ZOOM,
												5.0);
		gtk_scale_set_draw_value(GTK_SCALE(m_wZoomScale), FALSE);
		gtk_widget_set_size_request(m_wZoomScale, 110, -1);
		gtk_widget_set_tooltip_text(m_wZoomScale, sZoom.c_str());
		g_signal_connect(m_wZoomScale, "value-changed",
						 G_CALLBACK(s_zoom_value_changed), this);
		gtk_box_append(GTK_BOX(pZoomBox), m_wZoomScale);

		GtkWidget * pZoomIn = gtk_button_new_from_icon_name("zoom-in-symbolic");
		gtk_widget_set_tooltip_text(pZoomIn, sZoomIn.c_str());
		gtk_widget_add_css_class(pZoomIn, "flat");
		g_signal_connect(pZoomIn, "clicked",
						 G_CALLBACK(s_zoom_in_clicked), this);
		gtk_box_append(GTK_BOX(pZoomBox), pZoomIn);

		GtkWidget * pZoomReset = gtk_button_new_from_icon_name("zoom-original-symbolic");
		gtk_widget_set_tooltip_text(pZoomReset, "100%");
		gtk_widget_add_css_class(pZoomReset, "flat");
		g_signal_connect(pZoomReset, "clicked",
						 G_CALLBACK(s_zoom_reset_clicked), this);
		gtk_box_append(GTK_BOX(pZoomBox), pZoomReset);

		// the percentage is a button: clicking it opens the Zoom dialog,
		// like the indicator in Word/LibreOffice.
		m_wZoomLabel = gtk_button_new_with_label("100%");
		gtk_widget_set_tooltip_text(m_wZoomLabel, sZoomLevel.c_str());
		gtk_widget_add_css_class(m_wZoomLabel, "flat");
		gtk_widget_set_margin_start(m_wZoomLabel, 6);
		g_signal_connect(m_wZoomLabel, "clicked",
						 G_CALLBACK(s_zoom_label_clicked), this);
		gtk_box_append(GTK_BOX(pZoomBox), m_wZoomLabel);

		gtk_box_append(GTK_BOX(m_wStatusBar), pZoomBox);
		updateZoomWidgets();
	}

	gtk_widget_set_visible(m_wStatusBar, TRUE);
	hideProgressBar();
	return m_wStatusBar;
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
	
void AP_UnixStatusBar::show(void)
{
	gtk_widget_show (m_wStatusBar);
}

void AP_UnixStatusBar::hide(void)
{
	gtk_widget_hide (m_wStatusBar);
	m_pFrame->queue_resize();
}
