/* AbiWord
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

#include <gtk/gtk.h>

#include "ut_std_string.h"
#include "ap_UnixFrameImpl.h"
#include "ap_UnixApp.h"
#include "ev_UnixToolbar.h"
#include "ap_FrameData.h"
#include "ap_UnixTopRuler.h"
#include "ap_UnixLeftRuler.h"
#include "xap_UnixApp.h"
#include "xap_UnixDialogHelper.h"
#include "ap_UnixStatusBar.h"
#include "ut_debugmsg.h"
#include "ut_assert.h"
#include "ev_UnixMenuBar.h"
#include "ap_UnixRibbon.h"
#include "ap_Prefs.h"
#include "xap_App.h"
#include "xap_Prefs.h"


AP_UnixFrameImpl::AP_UnixFrameImpl(AP_UnixFrame *pUnixFrame) :
	XAP_UnixFrameImpl(static_cast<XAP_Frame *>(pUnixFrame)),
	m_dArea(nullptr),
	m_viewOverlay(nullptr),
	m_pVadj(nullptr),
	m_pHadj(nullptr),
	m_hScroll(nullptr),
	m_vScroll(nullptr),
	m_topRuler(nullptr),
	m_leftRuler(nullptr),
	m_grid(nullptr),
	m_innergrid(nullptr),
	m_wSunkenBox(nullptr),
	m_iHScrollSignal(0),
	m_iVScrollSignal(0),
	m_pRibbon(nullptr),
	m_wRibbon(nullptr),
	m_bRibbonMode(false)
{
	UT_DEBUGMSG(("Created AP_UnixFrameImpl %p \n",this));
}

AP_UnixFrameImpl::~AP_UnixFrameImpl()
{
	DELETEP(m_pRibbon);
}

XAP_FrameImpl * AP_UnixFrameImpl::createInstance(XAP_Frame *pFrame)
{
	XAP_FrameImpl *pFrameImpl = new AP_UnixFrameImpl(static_cast<AP_UnixFrame *>(pFrame));

	return pFrameImpl;
}

void AP_UnixFrameImpl::_bindToolbars(AV_View * pView)
{
	int nrToolbars = m_vecToolbarLayoutNames.getItemCount();
	for (int k = 0; k < nrToolbars; k++)
	{
		// TODO Toolbars are a frame-level item, but a view-listener is
		// TODO a view-level item.  I've bound the toolbar-view-listeners
		// TODO to the current view within this frame and have code in the
		// TODO toolbar to allow the view-listener to be rebound to a different
		// TODO view.  in the future, when we have support for multiple views
		// TODO in the frame (think splitter windows), we will need to have
		// TODO a loop like this to help change the focus when the current
		// TODO view changes.		
		EV_UnixToolbar * pUnixToolbar = reinterpret_cast<EV_UnixToolbar *>(m_vecToolbars.getNthItem(k));
		pUnixToolbar->bindListenerToView(pView);
	}	
}

// Does the initial show/hide of toolbars (based on the user prefs).
// This is needed because toggleBar is called only when the user
// (un)checks the show {Stantandard,Format,Extra} toolbar checkbox,
// and thus we have to manually call this function at startup.
void AP_UnixFrameImpl::_showOrHideToolbars()
{
	XAP_Frame* pFrame = getFrame();
	bool *bShowBar = static_cast<AP_FrameData*>(pFrame->getFrameData())->m_bShowBar;
	UT_uint32 cnt = m_vecToolbarLayoutNames.getItemCount();

	for (UT_uint32 i = 0; i < cnt; i++)
	{
		// TODO: The two next lines are here to bind the EV_Toolbar to the
		// AP_FrameData, but their correct place are next to the toolbar creation (JCA)
		EV_UnixToolbar * pUnixToolbar = static_cast<EV_UnixToolbar *> (m_vecToolbars.getNthItem(i));
		static_cast<AP_FrameData*> (pFrame->getFrameData())->m_pToolbar[i] = pUnixToolbar;
		static_cast<AP_UnixFrame *>(pFrame)->toggleBar(i, bShowBar[i]);
	}

	// the just-created bars default to their prefs; ribbon mode
	// hides them regardless
	if (m_bRibbonMode)
		_applyUIMode();
}

/*!
 * Refills the framedata class with pointers to the current toolbars. We 
 * need to do this after a toolbar icon and been dragged and dropped.
 */
void AP_UnixFrameImpl::_refillToolbarsInFrameData()
{
	UT_uint32 cnt = m_vecToolbarLayoutNames.getItemCount();

	for (UT_uint32 i = 0; i < cnt; i++)
	{
		EV_UnixToolbar * pUnixToolbar = static_cast<EV_UnixToolbar *> (m_vecToolbars.getNthItem(i));
		static_cast<AP_FrameData*>(getFrame()->getFrameData())->m_pToolbar[i] = pUnixToolbar;
	}
}

// Does the initial show/hide of statusbar (based on the user prefs).
// Idem.
void AP_UnixFrameImpl::_showOrHideStatusbar()
{
#ifdef ENABLE_STATUSBAR
	XAP_Frame* pFrame = getFrame();
	bool bShowStatusBar = static_cast<AP_FrameData*> (pFrame->getFrameData())->m_bShowStatusBar;
	static_cast<AP_UnixFrame *>(pFrame)->toggleStatusBar(bShowStatusBar);
#endif
}


void AP_UnixFrameImpl::ap_focus_in_event (GtkEventControllerMotion * /*c*/, gdouble /*x*/,
										  gdouble /*y*/, GtkWidget * drawing_area)
{
  gtk_widget_grab_focus (drawing_area);
}

void AP_UnixFrameImpl::ap_focus_out_event (GtkEventControllerMotion * /*c*/, GtkWidget * /*drawing_area*/)
{
}

GtkWidget * AP_UnixFrameImpl::_createDocumentWindow()
{
	XAP_Frame* pFrame = getFrame();
	bool bShowRulers = static_cast<AP_FrameData*>(pFrame->getFrameData())->m_bShowRuler;

	// create the rulers
	AP_UnixTopRuler * pUnixTopRuler = nullptr;
	AP_UnixLeftRuler * pUnixLeftRuler = nullptr;

	if ( bShowRulers )
	{
		pUnixTopRuler = new AP_UnixTopRuler(pFrame);
		UT_ASSERT(pUnixTopRuler);
		m_topRuler = pUnixTopRuler->createWidget();
		
		if (static_cast<AP_FrameData*>(pFrame->getFrameData())->m_pViewMode == VIEW_PRINT)
		  {
		    pUnixLeftRuler = new AP_UnixLeftRuler(pFrame);
		    UT_ASSERT(pUnixLeftRuler);
		    m_leftRuler = pUnixLeftRuler->createWidget();

		    // get the width from the left ruler and stuff it into the top ruler.
		    //pUnixTopRuler->setOffsetLeftRuler(pUnixLeftRuler->getWidth());
		  }
		else
		  {
		    m_leftRuler = nullptr;
		    //pUnixTopRuler->setOffsetLeftRuler(0);
		  }
	}
	else
	{
		m_topRuler = nullptr;
		m_leftRuler = nullptr;
	}

	static_cast<AP_FrameData*>(pFrame->getFrameData())->m_pTopRuler = pUnixTopRuler;
	static_cast<AP_FrameData*>(pFrame->getFrameData())->m_pLeftRuler = pUnixLeftRuler;

	// set up for scroll bars.
	m_pHadj = reinterpret_cast<GtkAdjustment *>(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	m_hScroll = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, m_pHadj);
	g_object_set_data(G_OBJECT(m_pHadj), "user_data", this);
	g_object_set_data(G_OBJECT(m_hScroll), "user_data", this);
	gtk_widget_set_hexpand(m_hScroll, TRUE);

	m_iHScrollSignal = g_signal_connect(G_OBJECT(m_pHadj), "value_changed", G_CALLBACK(XAP_UnixFrameImpl::_fe::hScrollChanged), nullptr);

	m_pVadj = reinterpret_cast<GtkAdjustment *>(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	m_vScroll = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, m_pVadj);
	g_object_set_data(G_OBJECT(m_pVadj), "user_data", this);
	g_object_set_data(G_OBJECT(m_vScroll), "user_data", this);
	gtk_widget_set_vexpand(m_vScroll, TRUE);

	m_iVScrollSignal = g_signal_connect(G_OBJECT(m_pVadj), "value_changed", G_CALLBACK(XAP_UnixFrameImpl::_fe::vScrollChanged), nullptr);

	// we don't want either scrollbar grabbing events from us
	gtk_widget_set_can_focus(m_hScroll, false);
	gtk_widget_set_can_focus(m_vScroll, false);

	// create a drawing area in the for our document window.
	m_dArea = ap_DocView_new();
	g_object_set_data(G_OBJECT(m_dArea), "user_data", this);
	UT_DEBUGMSG(("!!! drawing area m_dArea created! %p for %p \n",m_dArea,this));
	gtk_widget_set_can_focus(m_dArea, true);	// allow it to be focussed
	/* GTK4: keyboard focus requires 'focusable' (separate from
	 * can-focus); without it grab_focus silently fails and the
	 * drawing area never receives key events. */
	gtk_widget_set_focusable(m_dArea, true);

	// GTK4: all input goes through event controllers attached to the
	// drawing area; the widget pointer is passed as user_data so the
	// handlers can recover the XAP_UnixFrameImpl via "user_data".
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_dArea),
								   XAP_UnixFrameImpl::_fe::draw,
								   m_dArea, nullptr);

	GtkEventController * keyController = gtk_event_controller_key_new();
	g_signal_connect(keyController, "key-pressed",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::key_press_event), m_dArea);
	g_signal_connect(keyController, "key-released",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::key_release_event), m_dArea);
	gtk_widget_add_controller(m_dArea, keyController);

	GtkGesture * clickGesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(clickGesture), 0);
	g_signal_connect(clickGesture, "pressed",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::button_press_event), m_dArea);
	g_signal_connect(clickGesture, "released",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::button_release_event), m_dArea);
	gtk_widget_add_controller(m_dArea, GTK_EVENT_CONTROLLER(clickGesture));

	GtkEventController * motionController = gtk_event_controller_motion_new();
	g_signal_connect(motionController, "motion",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::motion_notify_event), m_dArea);
	// focus and XIM related (was enter/leave_notify_event)
	g_signal_connect(motionController, "enter", G_CALLBACK(ap_focus_in_event), m_dArea);
	g_signal_connect(motionController, "leave", G_CALLBACK(ap_focus_out_event), m_dArea);
	gtk_widget_add_controller(m_dArea, motionController);

	GtkEventController * scrollController =
		gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect(scrollController, "scroll",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::scroll_notify_event), m_dArea);
	gtk_widget_add_controller(m_dArea, scrollController);

	// was configure_event
	g_signal_connect(m_dArea, "resize",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::resize_event), m_dArea);

	//
	// Need this to fix screen flicker for abiwidget on focus in/out
	//
	GtkEventController * focusController = gtk_event_controller_focus_new();
	g_signal_connect(focusController, "enter",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::focus_in_event), m_dArea);
	g_signal_connect(focusController, "leave",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::focus_out_event), m_dArea);
	gtk_widget_add_controller(m_dArea, focusController);


	// create a table for scroll bars, rulers, and drawing area

	m_grid = gtk_grid_new();
	gtk_widget_set_hexpand(m_grid, TRUE);
	gtk_widget_set_vexpand(m_grid, TRUE);
	g_object_set_data(G_OBJECT(m_grid),"user_data", this);

	// NOTE:  in order to display w/ and w/o rulers, gtk needs two tables to
	// work with.  The 2 2x2 tables, (i)nner and (o)uter divide up the 3x3
	// table as follows.  The inner table is at the 1,1 table.
	//	+-----+---+
	//	| i i | o |
	//	| i i |   |
	//	+-----+---+
	//	|  o  | o |
	//	+-----+---+
		
	// scroll bars
	
	gtk_grid_attach(GTK_GRID(m_grid), m_hScroll, 0, 1, 1, 1);

	gtk_grid_attach(GTK_GRID(m_grid), m_vScroll, 1, 0, 1, 1);


	// arrange the widgets within our inner table.
	m_innergrid = gtk_grid_new();
	gtk_widget_set_hexpand(m_innergrid, TRUE);
	gtk_widget_set_vexpand(m_innergrid, TRUE);
	gtk_grid_attach(GTK_GRID(m_grid), m_innergrid, 0, 0, 1, 1); 

	// GTK4: the drawing area sits in a GtkOverlay so that the text
	// selection handles (FV_UnixSelectionHandles) can be added as
	// overlay children on top of it.  GTK3 used child GdkWindows for
	// these, which no longer exist.
	m_viewOverlay = gtk_overlay_new();
	gtk_widget_set_hexpand(m_viewOverlay, TRUE);
	gtk_widget_set_vexpand(m_viewOverlay, TRUE);
	gtk_overlay_set_child(GTK_OVERLAY(m_viewOverlay), m_dArea);

	if ( bShowRulers )
	{
		gtk_grid_attach(GTK_GRID(m_innergrid), m_topRuler, 0, 0, 2, 1);

		if (m_leftRuler)
			gtk_grid_attach(GTK_GRID(m_innergrid), m_leftRuler, 0, 1, 1, 1);

		gtk_grid_attach(GTK_GRID(m_innergrid), m_viewOverlay, 1, 1, 1, 1);
	}
	else	// no rulers
	{
		gtk_grid_attach(GTK_GRID(m_innergrid), m_viewOverlay, 1, 1, 1, 1);
	}
	// create a 3d box and put the table in it, so that we
	// get a sunken in look.
	m_wSunkenBox = gtk_frame_new(nullptr);
	gtk_frame_set_child(GTK_FRAME(m_wSunkenBox), m_grid);

	// (scrollbars are shown, only if needed, by _setScrollRange)
	gtk_widget_show(m_dArea);
	gtk_widget_show(m_innergrid);
	gtk_widget_show(m_grid);

	return m_wSunkenBox;
}

void AP_UnixFrameImpl::_createRibbonUI()
{
	// (re)create the ribbon next to the menubar; which of the two is
	// visible is governed by the RibbonUI preference.
	if (m_pRibbon)
	{
		if (m_wRibbon)
			gtk_widget_unparent(m_wRibbon);
		DELETEP(m_pRibbon);
		m_wRibbon = nullptr;
	}

	bool bRibbon = false;
	XAP_App::getApp()->getPrefsValueBool(AP_PREF_KEY_RibbonUI, bRibbon);
	m_bRibbonMode = bRibbon;

	m_pRibbon = new AP_UnixRibbon(getFrame(), m_pUnixMenu);
	m_wRibbon = m_pRibbon->createWidget();
	gtk_widget_insert_after(m_wRibbon, m_wVBox, m_pUnixMenu->getMenuBar());

	_applyUIMode();
}

void AP_UnixFrameImpl::_rebuildMenus()
{
	XAP_UnixFrameImpl::_rebuildMenus();
	// the menu rebuild replaces the action group the ribbon buttons
	// are bound to, so the ribbon must be rebuilt as well.
	if (m_pRibbon)
		_createRibbonUI();
}

void AP_UnixFrameImpl::refreshRibbon()
{
	if (m_pRibbon && m_bRibbonMode)
		m_pRibbon->refresh();
}

void AP_UnixFrameImpl::setRibbonMode(bool bRibbon)
{
	m_bRibbonMode = bRibbon;
	_applyUIMode();
}

void AP_UnixFrameImpl::_applyUIMode()
{
	if (!m_pUnixMenu || !m_wRibbon)
		return;

	gtk_widget_set_visible(m_pUnixMenu->getMenuBar(), !m_bRibbonMode);
	gtk_widget_set_visible(m_wRibbon, m_bRibbonMode);

	// the ribbon replaces the icon bars as well as the menubar; in
	// classic mode restore each bar to its own visibility pref.
	// m_vecToolbars may still be empty during window construction.
	AP_FrameData * pFrameData =
		static_cast<AP_FrameData *>(getFrame()->getFrameData());
	UT_uint32 nrBars = m_vecToolbars.getItemCount();
	for (UT_uint32 i = 0; i < nrBars && i < 4; ++i)
	{
		EV_Toolbar * pToolbar =
			static_cast<EV_Toolbar *>(m_vecToolbars.getNthItem(i));
		if (!pToolbar)
			continue;
		if (m_bRibbonMode)
			pToolbar->hide();
		else if (pFrameData && pFrameData->m_bShowBar[i])
			pToolbar->show();
	}
	if (m_bRibbonMode)
		m_pRibbon->refresh();
}

void AP_UnixFrameImpl::_hideMenuScroll(bool bHideMenuScroll)
{
  if(bHideMenuScroll)
  {
    UT_DEBUGMSG(("Hiding Menu \n"));
    gtk_widget_hide(m_pUnixMenu->getMenuBar());
    if (m_wRibbon)
      gtk_widget_hide(m_wRibbon);
    UT_DEBUGMSG(("Hiding scrollbar \n"));
    gtk_widget_hide(m_vScroll);
  }
  else
  {
    // restore whichever UI chrome the current mode shows
    _applyUIMode();
    gtk_widget_set_visible(m_vScroll, TRUE);
  }
}
void AP_UnixFrameImpl::_setWindowIcon()
{
	// attach program icon to window
	GtkWidget * window = getTopLevelWindow();
	// GTK4 only supports themed icon names on windows
	gtk_window_set_icon_name(GTK_WINDOW(window), "abiword");
}

void AP_UnixFrameImpl::_createWindow()
{
	_createTopLevelWindow();
	
	gtk_widget_show(getTopLevelWindow());

	if(getFrame()->getFrameMode() == XAP_NormalFrame)
	{
		// needs to be shown so that the following functions work
		// TODO: get rid of cursed flicker caused by initially
		// TODO: showing these and then hiding them (esp.
		// TODO: noticable in the gnome build with a toolbar disabled)
		_showOrHideToolbars();
		_showOrHideStatusbar();
	}
	if(getFrame()->isMenuScrollHidden())
	{
	    _hideMenuScroll(true);
	}
}

GtkWidget * AP_UnixFrameImpl::_createStatusBarWindow()
{
#ifdef ENABLE_STATUSBAR
	XAP_Frame* pFrame = getFrame();
	AP_UnixStatusBar * pUnixStatusBar = new AP_UnixStatusBar(pFrame);
	UT_ASSERT(pUnixStatusBar);

	static_cast<AP_FrameData *>(pFrame->getFrameData())->m_pStatusBar = pUnixStatusBar;
	
	return pUnixStatusBar->createWidget();
#else
	return nullptr;
#endif
}

void AP_UnixFrameImpl::_setScrollRange(apufi_ScrollType scrollType, int iValue, gfloat fUpperLimit, gfloat fSize)
{
	GtkAdjustment *pScrollAdjustment = (scrollType == apufi_scrollX) ? m_pHadj : m_pVadj;
	GtkWidget *wScrollWidget = (scrollType == apufi_scrollX) ? m_hScroll : m_vScroll;
	xxx_UT_DEBUGMSG(("Scroll Adjustment set to %d upper %f size %f\n",iValue, fUpperLimit, fSize));
	GR_Graphics * pGr = getFrame()->getCurrentView()->getGraphics ();
	XAP_Frame::tZoomType tZoom = getFrame()->getZoomType();
	if(pScrollAdjustment) //this isn't guaranteed in AbiCommand
	{
		gtk_adjustment_configure(pScrollAdjustment, iValue, 0.0, fUpperLimit,
                                 pGr->tluD(20.0), fSize, fSize);
	}

	// hide the horizontal scrollbar if the scroll range is such that the window can contain it all
	// show it otherwise
// Hide the horizontal scrollbar if we've set to page width or fit to page.
// This stops a resizing race condition.
//
 	if ((m_hScroll == wScrollWidget) && ((fUpperLimit <= fSize) ||(  tZoom == XAP_Frame::z_PAGEWIDTH) || (tZoom == XAP_Frame::z_WHOLEPAGE)))
	{
 		gtk_widget_hide(wScrollWidget);
	}
 	else if((wScrollWidget != m_vScroll) || !getFrame()->isMenuScrollHidden())
	{
 		gtk_widget_show(wScrollWidget);
	}
}

#define COLOR_MIX 0.67   //COLOR_MIX should be between 0 and 1

UT_RGBColor AP_UnixFrameImpl::getColorSelBackground () const
{
    if( XAP_App::getApp()->getNoGUI() ) 
        return(UT_RGBColor(0,0,0));

    UT_return_val_if_fail(m_dArea, UT_RGBColor(0,0,0));

    // Bug 13762 guess colours for the selection.
    // Code copied from gr_UnixCairoGraphics.

    // guess colours
    // WHITE
    GdkRGBA rgba2;
    rgba2.red = 1.;
    rgba2.green = 1.;
    rgba2.blue = 1.;
    rgba2.alpha = 1;
    // guess colours.
    // BLACK
    GdkRGBA rgba1;
    rgba1.red = 0.;
    rgba1.green = 0.;
    rgba1.blue = 0.;
    rgba1.alpha = 1;

    GdkRGBA rgba_;
    rgba_.red = rgba1.red*(1.-COLOR_MIX) + rgba2.red*COLOR_MIX;
    rgba_.green = rgba1.green*(1.-COLOR_MIX) + rgba2.green*COLOR_MIX;
    rgba_.blue = rgba1.blue*(1.-COLOR_MIX) + rgba2.blue*COLOR_MIX;

#if 0 // this totally broke in Gtk 3.20. Deprecated APIs return rubbish.
    // owen says that any widget should be ok, not just text widgets
    GtkStyleContext *pCtxt = gtk_widget_get_style_context(m_dArea);
    GdkRGBA rgba;
    gtk_style_context_get_background_color(pCtxt, GTK_STATE_FLAG_SELECTED, &rgba);
#endif
    return UT_RGBColor(rgba_.red * 255, rgba_.green * 255, rgba_.blue * 255);
}

UT_RGBColor AP_UnixFrameImpl::getColorSelForeground () const
{
  return UT_RGBColor(0,0,0);
#if 0 // don't risk it. return black.
  UT_return_val_if_fail(m_dArea, UT_RGBColor(0,0,0));
  
  // owen says that any widget should be ok, not just text widgets
  GtkStateFlags state;
  
  // our text widget has focus
  if (gtk_widget_has_focus(m_dArea))
    state = GTK_STATE_FLAG_SELECTED;
  else
    state = GTK_STATE_FLAG_ACTIVE;
  
  GtkStyleContext *pCtxt = gtk_widget_get_style_context(m_dArea);
  GdkRGBA rgba;
  gtk_style_context_get_color(pCtxt, state, &rgba);
  return UT_RGBColor (rgba.red * 255, rgba.green * 255, rgba.blue * 255);
#endif
}
