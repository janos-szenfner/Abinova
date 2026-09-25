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
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <cstring>
#include <cmath>
#include <vector>

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
#include "ap_UnixStylesPane.h"
#include "ap_UnixSelPane.h"
#include "ap_UnixIconsPane.h"
#include "ap_UnixCommentsPane.h"
#include "ap_UnixNavPane.h"
#include "ap_Prefs.h"
#include "xap_App.h"
#include "xap_Prefs.h"
#include "xav_Listener.h"
#include "fv_View.h"
#include "fl_DocLayout.h"
#include "pd_Document.h"
#include "gr_UnixCairoGraphics.h"


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
	m_iScrollAnimID(0),
	m_dScrollAnimTarget(0.0),
	m_pRibbon(nullptr),
	m_wRibbon(nullptr),
	m_bRibbonMode(false),
	m_wDocPaned(nullptr),
	m_wSideDeck(nullptr),
	m_wStylesPaneW(nullptr),
	m_pStylesPane(nullptr),
	m_wSelPaneW(nullptr),
	m_pSelPane(nullptr),
	m_wIconsPaneW(nullptr),
	m_pIconsPane(nullptr),
	m_wCommentsPaneW(nullptr),
	m_pCommentsPane(nullptr),
	m_wNavPaneW(nullptr),
	m_pNavPane(nullptr),
	m_bGridlines(false),
	m_wSplitPaned(nullptr),
	m_wSplitGrid(nullptr),
	m_dArea2(nullptr),
	m_pVadj2(nullptr),
	m_vScroll2(nullptr),
	m_iVScrollSignal2(0),
	m_pG2(nullptr),
	m_pDocLayout2(nullptr),
	m_pView2(nullptr),
	m_pScrollObj2(nullptr),
	m_pScrollListener2(nullptr),
	m_lidScroll2(0),
	m_pPane1View(nullptr)
{
	UT_DEBUGMSG(("Created AP_UnixFrameImpl %p \n",this));
}

AP_UnixFrameImpl::~AP_UnixFrameImpl()
{
	if (m_iScrollAnimID && m_dArea)
		gtk_widget_remove_tick_callback(m_dArea, m_iScrollAnimID);
	m_iScrollAnimID = 0;
	/* tear down the split pane without touching the frame, which is
	 * already mid-destruction; the widgets die with the window */
	if (m_pView2)
	{
		m_pView2->removeListener(m_lidScroll2);
		m_pView2->removeScrollListener(m_pScrollObj2);
	}
	DELETEP(m_pScrollObj2);
	DELETEP(m_pScrollListener2);
	DELETEP(m_pView2);
	DELETEP(m_pDocLayout2);
	DELETEP(m_pG2);
	DELETEP(m_pRibbon);
	DELETEP(m_pStylesPane);
	DELETEP(m_pSelPane);
	DELETEP(m_pIconsPane);
	DELETEP(m_pCommentsPane);
	DELETEP(m_pNavPane);
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

	/* wrap the document area in a GtkPaned so the side deck can be
	 * docked on the right (LibreOffice-style); hidden until a pane
	 * is toggled on */
	m_wDocPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_start_child(GTK_PANED(m_wDocPaned), m_wSunkenBox);
	gtk_paned_set_resize_start_child(GTK_PANED(m_wDocPaned), TRUE);
	gtk_paned_set_shrink_start_child(GTK_PANED(m_wDocPaned), FALSE);

	m_wSideDeck = gtk_stack_new();
	m_pStylesPane = new AP_UnixStylesPane(pFrame);
	m_wStylesPaneW = m_pStylesPane->createWidget();
	gtk_stack_add_named(GTK_STACK(m_wSideDeck), m_wStylesPaneW,
						"styles");
	m_pSelPane = new AP_UnixSelPane(pFrame);
	m_wSelPaneW = m_pSelPane->createWidget();
	gtk_stack_add_named(GTK_STACK(m_wSideDeck), m_wSelPaneW,
						"objects");
	m_pIconsPane = new AP_UnixIconsPane(pFrame);
	m_wIconsPaneW = m_pIconsPane->createWidget();
	gtk_stack_add_named(GTK_STACK(m_wSideDeck), m_wIconsPaneW,
						"icons");
	m_pCommentsPane = new AP_UnixCommentsPane(pFrame);
	m_wCommentsPaneW = m_pCommentsPane->createWidget();
	gtk_stack_add_named(GTK_STACK(m_wSideDeck), m_wCommentsPaneW,
						"comments");
	m_pNavPane = new AP_UnixNavPane(pFrame);
	m_wNavPaneW = m_pNavPane->createWidget();
	gtk_stack_add_named(GTK_STACK(m_wSideDeck), m_wNavPaneW,
						"navigation");
	gtk_widget_set_visible(m_wSideDeck, FALSE);
	gtk_paned_set_end_child(GTK_PANED(m_wDocPaned), m_wSideDeck);
	gtk_paned_set_resize_end_child(GTK_PANED(m_wDocPaned), FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(m_wDocPaned), FALSE);

	return m_wDocPaned;
}

/* helpers for the deferred paned-position set: the widget is tracked
 * through a heap cell that a weak ref clears on destruction, so a
 * frame closed before the idle fires can't use a dead widget */
static void s_panedCellCleared(gpointer d, GObject * /*dead*/)
{
	*static_cast<GtkWidget **>(d) = nullptr;
}

static gboolean s_panedPositionIdle(gpointer data)
{
	GtkWidget ** pp = static_cast<GtkWidget **>(data);
	GtkWidget * w = *pp;
	if (w)
		g_object_weak_unref(G_OBJECT(w), s_panedCellCleared, pp);
	delete pp;
	if (!w)
		return G_SOURCE_REMOVE;
	GtkWidget * end = gtk_paned_get_end_child(GTK_PANED(w));
	GtkAllocation alloc;
	gtk_widget_get_allocation(w, &alloc);
	if (end && gtk_widget_get_visible(end) && alloc.width > 400)
		gtk_paned_set_position(GTK_PANED(w), alloc.width - 300);
	return G_SOURCE_REMOVE;
}

/* shows the deck on szPage ("styles" or "objects"), or hides it
 * when bVisible is false */
static void s_deckShow(GtkWidget * deck, const char * szPage,
					   bool bVisible, GtkWidget * paned)
{
	gtk_widget_set_visible(deck, bVisible);
	if (!bVisible)
		return;
	gtk_stack_set_visible_child_name(GTK_STACK(deck), szPage);
	/* the paned position must be set after the end child maps,
	 * otherwise it clamps to 0 and the pane stays collapsed.
	 * The widget is tracked through a heap cell + weak ref so a
	 * frame closed before the idle fires can't UAF. */
	GtkWidget ** pp = new GtkWidget *(paned);
	g_object_weak_ref(G_OBJECT(paned), s_panedCellCleared, pp);
	g_idle_add(s_panedPositionIdle, pp);
}

void AP_UnixFrameImpl::setStylesPaneVisible(bool bVisible)
{
	if (!m_wDocPaned || !m_wSideDeck)
		return;
	s_deckShow(m_wSideDeck, "styles", bVisible, m_wDocPaned);
	if (bVisible)
	{
		/* doc styles exist by the time the user can click the button;
		 * createWidget ran before the view was attached */
		m_pStylesPane->rebuildList();
	}
}

bool AP_UnixFrameImpl::isStylesPaneVisible() const
{
	const char * cur = m_wSideDeck
		? gtk_stack_get_visible_child_name(GTK_STACK(m_wSideDeck))
		: nullptr;
	return cur && gtk_widget_get_visible(m_wSideDeck) &&
		!strcmp(cur, "styles");
}

void AP_UnixFrameImpl::setSelPaneVisible(bool bVisible)
{
	if (!m_wDocPaned || !m_wSideDeck)
		return;
	s_deckShow(m_wSideDeck, "objects", bVisible, m_wDocPaned);
	if (bVisible)
		m_pSelPane->rebuildList();
}

bool AP_UnixFrameImpl::isSelPaneVisible() const
{
	const char * cur = m_wSideDeck
		? gtk_stack_get_visible_child_name(GTK_STACK(m_wSideDeck))
		: nullptr;
	return cur && gtk_widget_get_visible(m_wSideDeck) &&
		!strcmp(cur, "objects");
}

void AP_UnixFrameImpl::setIconsPaneVisible(bool bVisible)
{
	if (!m_wDocPaned || !m_wSideDeck)
		return;
	s_deckShow(m_wSideDeck, "icons", bVisible, m_wDocPaned);
}

bool AP_UnixFrameImpl::isIconsPaneVisible() const
{
	const char * cur = m_wSideDeck
		? gtk_stack_get_visible_child_name(GTK_STACK(m_wSideDeck))
		: nullptr;
	return cur && gtk_widget_get_visible(m_wSideDeck) &&
		!strcmp(cur, "icons");
}

void AP_UnixFrameImpl::toggleIconsPane()
{
	setIconsPaneVisible(!isIconsPaneVisible());
}

void AP_UnixFrameImpl::setCommentsPaneVisible(bool bVisible)
{
	if (!m_wDocPaned || !m_wSideDeck)
		return;
	s_deckShow(m_wSideDeck, "comments", bVisible, m_wDocPaned);
	if (bVisible && m_pCommentsPane)
		m_pCommentsPane->refresh();
	if (bVisible)
	{
		/* the pane's widgets must not keep keyboard focus: comment
		 * insertion auto-shows the pane and the caret sits inside the
		 * comment, so typing has to reach the document canvas */
		focusDocument();
		/* showing the reviewing pane must also make the anchored text
		 * visible in the document; the layout listener reformats when
		 * the preference changes */
		XAP_Prefs * pPrefs = XAP_App::getApp()->getPrefs();
		if (pPrefs)
		{
			XAP_PrefsScheme * pScheme = pPrefs->getCurrentScheme(true);
			bool bDisplay = false;
			if (pScheme
				&& (!pScheme->getValueBool(AP_PREF_KEY_DisplayAnnotations, bDisplay)
					|| !bDisplay))
			{
				pScheme->setValue(AP_PREF_KEY_DisplayAnnotations, "1");
			}
		}
	}
}

void AP_UnixFrameImpl::focusDocument()
{
	if (m_dArea && gtk_widget_get_realized(m_dArea))
		gtk_widget_grab_focus(m_dArea);
}

bool AP_UnixFrameImpl::isCommentsPaneVisible() const
{
	const char * cur = m_wSideDeck
		? gtk_stack_get_visible_child_name(GTK_STACK(m_wSideDeck))
		: nullptr;
	return cur && gtk_widget_get_visible(m_wSideDeck) &&
		!strcmp(cur, "comments");
}

void AP_UnixFrameImpl::toggleCommentsPane()
{
	setCommentsPaneVisible(!isCommentsPaneVisible());
}

void AP_UnixFrameImpl::refreshCommentsPane()
{
	if (isCommentsPaneVisible() && m_pCommentsPane)
		m_pCommentsPane->refresh();
}

void AP_UnixFrameImpl::refreshSelPane()
{
	if (isSelPaneVisible() && m_pSelPane)
		m_pSelPane->refresh();
}

void AP_UnixFrameImpl::toggleSelPane()
{
	setSelPaneVisible(!isSelPaneVisible());
}

void AP_UnixFrameImpl::refreshStylesPane(const char * szCurrentStyle)
{
	if (m_pStylesPane && isStylesPaneVisible())
		m_pStylesPane->refresh(szCurrentStyle);
}

/* ===== Navigation pane (headings list in the side deck) ===== */

void AP_UnixFrameImpl::setNavPaneVisible(bool bVisible)
{
	if (!m_wDocPaned || !m_wSideDeck)
		return;
	s_deckShow(m_wSideDeck, "navigation", bVisible, m_wDocPaned);
	if (bVisible && m_pNavPane)
		m_pNavPane->rebuildList();
}

bool AP_UnixFrameImpl::isNavPaneVisible() const
{
	const char * cur = m_wSideDeck
		? gtk_stack_get_visible_child_name(GTK_STACK(m_wSideDeck))
		: nullptr;
	return cur && gtk_widget_get_visible(m_wSideDeck) &&
		!strcmp(cur, "navigation");
}

void AP_UnixFrameImpl::toggleNavPane()
{
	fprintf(stderr, "NAVDBG toggleNavPane: vis=%d deck=%p paned=%p\n",
			(int)isNavPaneVisible(), (void*)m_wSideDeck, (void*)m_wDocPaned);
	setNavPaneVisible(!isNavPaneVisible());
}

void AP_UnixFrameImpl::refreshNavPane()
{
	if (isNavPaneVisible() && m_pNavPane)
		m_pNavPane->refresh();
}

/* ===== Gridlines ===== */

void AP_UnixFrameImpl::toggleGridlines()
{
	m_bGridlines = !m_bGridlines;
	if (m_dArea)
		gtk_widget_queue_draw(m_dArea);
	if (m_dArea2)
		gtk_widget_queue_draw(m_dArea2);
}

/* Draw Table rubber-band: dashed rect while a draw-table drag is
 * in progress (FV_View keeps the rect in window pixels) */
static void _postDocDrawTableRect(cairo_t * cr, AV_View * pView)
{
	FV_View * pFV = static_cast<FV_View *>(pView);
	if (!pFV)
		return;
	UT_Rect r;
	if (!pFV->getTableDrawRect(&r) || r.width <= 0 || r.height <= 0)
		return;
	cairo_save(cr);
	cairo_set_source_rgba(cr, 0.2, 0.45, 0.9, 0.85);
	cairo_set_line_width(cr, 1.2);
	static const double dashes[] = { 4.0, 3.0 };
	cairo_set_dash(cr, dashes, 2, 0);
	cairo_rectangle(cr, r.left + 0.5, r.top + 0.5, r.width, r.height);
	cairo_stroke(cr);
	cairo_restore(cr);
}

/* light grid anchored to the scroll offsets so it scrolls with the
 * document; spacing tracks the zoom so it stays ~1 cm on screen */
void AP_UnixFrameImpl::_postDocDraw(GtkWidget * w, cairo_t * cr,
									AV_View * pView)
{
	if (!pView || !w)
		return;
	GR_Graphics * pG = pView->getGraphics();
	if (!pG)
		return;
	if (m_bGridlines)
	{
	GtkAllocation alloc;
	gtk_widget_get_allocation(w, &alloc);
	double xoff = pG->tduD(pView->getXScrollOffset());
	double yoff = pG->tduD(pView->getYScrollOffset());
	double step = 37.8 * pG->getZoomPercentage() / 100.0;
	if (step < 8.0)
		step = 8.0;

	cairo_save(cr);
	cairo_set_source_rgba(cr, 0.45, 0.5, 0.6, 0.22);
	cairo_set_line_width(cr, 1.0);
	double sx = std::fmod(-xoff, step);
	if (sx > 0)
		sx -= step;
	for (double x = sx; x <= alloc.width; x += step)
	{
		cairo_move_to(cr, x + 0.5, 0);
		cairo_line_to(cr, x + 0.5, alloc.height);
	}
	double sy = std::fmod(-yoff, step);
	if (sy > 0)
		sy -= step;
	for (double y = sy; y <= alloc.height; y += step)
	{
		cairo_move_to(cr, 0, y + 0.5);
		cairo_line_to(cr, alloc.width, y + 0.5);
	}
	cairo_stroke(cr);
	cairo_restore(cr);
	}

	_postDocDrawTableRect(cr, pView);
}

/* ===== Split view ===== */

/* secondary-pane scrollbar calibration; mirrors AP_UnixFrame::
 * setYScrollRange but for the split pane's own layout and
 * adjustment */
class ap_Pane2ViewListener : public AV_Listener
{
public:
	ap_Pane2ViewListener(AP_UnixFrameImpl * pImpl, AV_View * pView)
		: m_pImpl(pImpl), m_pView(pView) {}
	bool notify(AV_View * /*pView*/, const AV_ChangeMask mask) override
	{
		if (mask & (AV_CHG_PAGECOUNT | AV_CHG_WINDOWSIZE))
			m_pImpl->setScrollRange2();
		return true;
	}
	AV_ListenerType getType() const override
		{ return AV_LISTENER_SCROLLBAR; }
private:
	AP_UnixFrameImpl * m_pImpl;
	AV_View * m_pView;
};

void AP_UnixFrameImpl::_drawPane2(GtkDrawingArea * /*area*/, cairo_t * cr,
								  int /*width*/, int /*height*/, gpointer w)
{
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		g_object_get_data(G_OBJECT(w), "user_data"));
	FV_View * pView = pImpl ? pImpl->m_pView2 : nullptr;
	double x, y, width, height;
	cairo_clip_extents(cr, &x, &y, &width, &height);
	width -= x;
	height -= y;
	if (pView)
	{
		GR_CairoGraphics * pGr =
			static_cast<GR_CairoGraphics *>(pView->getGraphics());
		if (pGr->getPaintCount() > 0)
			return;
		UT_Rect rClip;
		rClip.left = pGr->tlu(x);
		rClip.top = pGr->tlu(y);
		rClip.width = pGr->tlu(width);
		rClip.height = pGr->tlu(height);
		GR_UnixCairoGraphics * pUGr =
			static_cast<GR_UnixCairoGraphics *>(pGr);
		pUGr->beginFrame();
		pView->drawImmediate(&rClip);
		pUGr->endFrame(cr);
		pImpl->_postDocDraw(GTK_WIDGET(w), cr, pView);
	}
}

void AP_UnixFrameImpl::_resizePane2(GtkDrawingArea * /*area*/, gint width,
									gint height, GtkWidget * w)
{
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		g_object_get_data(G_OBJECT(w), "user_data"));
	if (pImpl && pImpl->m_pView2)
		pImpl->m_pView2->setWindowSize(width, height);
}

/* the split paned gets a deferred halving once it has an
 * allocation; retries while the widget is still unallocated */
struct _PanePosCell { GtkWidget * w; int tries; };

static void s_panePosCellCleared(gpointer d, GObject * /*dead*/)
{
	static_cast<_PanePosCell *>(d)->w = nullptr;
}

static gboolean s_splitPositionIdle(gpointer data)
{
	_PanePosCell * c = static_cast<_PanePosCell *>(data);
	GtkWidget * w = c->w;
	bool bDone = false;
	if (w)
	{
		GtkAllocation alloc;
		gtk_widget_get_allocation(w, &alloc);
		if (alloc.height > 200)
		{
			gtk_paned_set_position(GTK_PANED(w), alloc.height / 2);
			bDone = true;
		}
		else if (++c->tries > 120)
			bDone = true;
	}
	else
		bDone = true;
	if (bDone)
	{
		if (w)
			g_object_weak_unref(G_OBJECT(w), s_panePosCellCleared, c);
		delete c;
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

void AP_UnixFrameImpl::_vScrollChanged2(GtkAdjustment * adj,
										gpointer /*data*/)
{
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(
		g_object_get_data(G_OBJECT(adj), "user_data"));
	if (pImpl && pImpl->m_pView2)
		pImpl->m_pView2->sendVerticalScrollEvent(
			static_cast<UT_sint32>(gtk_adjustment_get_value(adj)));
}

/* view-driven scroll feedback for pane 2 - same arithmetic as
 * AP_UnixFrame::_scrollFuncY but writes the pane-2 adjustment */
void AP_UnixFrameImpl::_scrollFuncY2(void * pData, UT_sint32 yoff,
									 UT_sint32 /*yrange*/)
{
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(pData);
	FV_View * pView = pImpl->m_pView2;
	if (!pView || !pImpl->m_pVadj2)
		return;

	gfloat yoffNew = yoff;
	gfloat yoffMax = gtk_adjustment_get_upper(pImpl->m_pVadj2) -
		gtk_adjustment_get_page_size(pImpl->m_pVadj2);
	if (yoffMax <= 0)
		yoffNew = 0;
	else if (yoffNew > yoffMax)
		yoffNew = yoffMax;

	GR_Graphics * pG = pView->getGraphics();
	UT_sint32 dy = static_cast<UT_sint32>(
		pG->tluD(static_cast<UT_sint32>(pG->tduD(
			static_cast<UT_sint32>(pView->getYScrollOffset() - yoffNew)))));
	gfloat yoffDisc = static_cast<UT_sint32>(pView->getYScrollOffset()) - dy;

	if (pG->tdu(static_cast<UT_sint32>(yoffDisc) -
				pView->getYScrollOffset()) == 0)
		return;

	g_signal_handler_block(pImpl->m_pVadj2, pImpl->m_iVScrollSignal2);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(pImpl->m_pVadj2), yoffNew);
	g_signal_handler_unblock(pImpl->m_pVadj2, pImpl->m_iVScrollSignal2);

	pView->setYScrollOffset(static_cast<UT_sint32>(yoffDisc));
}

/* pane 2 has no horizontal scrollbar; scroll the view directly */
void AP_UnixFrameImpl::_scrollFuncX2(void * pData, UT_sint32 xoff,
									 UT_sint32 /*xrange*/)
{
	AP_UnixFrameImpl * pImpl = static_cast<AP_UnixFrameImpl *>(pData);
	if (pImpl->m_pView2)
		pImpl->m_pView2->setXScrollOffset(xoff);
}

/* recalibrate the split pane's scrollbar when the document or the
 * pane size changes */
void AP_UnixFrameImpl::setScrollRange2()
{
	if (!m_pView2 || !m_pDocLayout2 || !m_pVadj2 || !m_dArea2)
		return;
	GR_Graphics * pGr = m_pView2->getGraphics();
	int height = m_pDocLayout2->getHeight();
	GtkAllocation alloc;
	gtk_widget_get_allocation(m_dArea2, &alloc);
	int windowHeight = static_cast<int>(pGr->tluD(alloc.height));

	int newvalue = m_pView2->getYScrollOffset();
	int newmax = height - windowHeight;
	if (newmax <= 0)
		newvalue = 0;
	else if (newvalue > newmax)
		newvalue = newmax;

	bool bDifferentPosition =
		(newvalue != static_cast<int>(
			gtk_adjustment_get_value(m_pVadj2) + 0.5));
	bool bDifferentLimits =
		((height - windowHeight) != static_cast<int>(
			gtk_adjustment_get_upper(m_pVadj2) -
			gtk_adjustment_get_page_size(m_pVadj2) + 0.5));

	if (bDifferentPosition || bDifferentLimits)
	{
		gtk_adjustment_configure(m_pVadj2, newvalue, 0.0,
								 static_cast<gfloat>(height),
								 pGr->tluD(20.0),
								 static_cast<gfloat>(windowHeight),
								 static_cast<gfloat>(windowHeight));
		m_pView2->sendVerticalScrollEvent(newvalue,
			static_cast<UT_sint32>(
				gtk_adjustment_get_upper(m_pVadj2) -
				gtk_adjustment_get_page_size(m_pVadj2)));
	}
}

/* which view a pane's widgets drive: the primary pane always maps
 * to m_pPane1View while split (even when the secondary pane holds
 * the "current" view); not split means single-pane current view */
AV_View * AP_UnixFrameImpl::paneView(int iPane)
{
	if (iPane == 1)
		return m_pView2;
	if (m_pPane1View)
		return m_pPane1View;
	return getFrame() ? getFrame()->getCurrentView() : nullptr;
}

void AP_UnixFrameImpl::_preDocInput(GtkWidget * w)
{
	if (w == m_dArea2 && m_pView2)
		_setActivePane(m_pView2);
	else if (w == m_dArea && m_pPane1View)
		_setActivePane(m_pPane1View);
}

AV_View * AP_UnixFrameImpl::_viewForScrollAdj(GtkAdjustment * adj)
{
	if (adj && m_pVadj2 && adj == m_pVadj2 && m_pView2)
		return m_pView2;
	return paneView(0);
}

bool AP_UnixFrameImpl::_isPaneView(AV_View * pView)
{
	if (getFrame() && getFrame()->getCurrentView() == pView)
		return true;
	return pView && (pView == m_pView2 || pView == m_pPane1View);
}

/* the primary view is recreated on document reload; keep the
 * pane-1 binding pointing at it so the primary scrollbars keep
 * working while the split is open.  If the frame switched to a
 * different document the split pane's layout is stale - drop it
 * before the old document can be deleted out from under it. */
void AP_UnixFrameImpl::notifyViewChanged(AV_View * pView)
{
	if (m_pView2 && pView && pView != m_pView2 &&
		static_cast<FV_View *>(pView)->getDocument() !=
			m_pView2->getDocument())
	{
		/* rebind pane 1 first so the teardown reactivates the
		 * new view rather than the about-to-be-deleted old one */
		m_pPane1View = pView;
		setSplitView(false);
		return;
	}
	if (m_pPane1View && pView && pView != m_pView2)
		m_pPane1View = pView;
}

/* move the frame's current view (and the ruler/statusbar bindings)
 * to the pane the user is interacting with */
void AP_UnixFrameImpl::_setActivePane(AV_View * pView)
{
	XAP_Frame * pFrame = getFrame();
	if (!pFrame || !pView || pFrame->getCurrentView() == pView)
		return;
	AV_View * pOld = pFrame->getCurrentView();
	if (pOld)
		pOld->focusChange(AV_FOCUS_NONE);
	pFrame->setView(pView);

	AP_FrameData * pFrameData =
		static_cast<AP_FrameData *>(pFrame->getFrameData());
	if (pFrameData)
	{
		UT_sint32 iZoom = pFrame->getZoomPercentage();
		if (pFrameData->m_bShowRuler)
		{
			if (pFrameData->m_pTopRuler)
				pFrameData->m_pTopRuler->setView(pView, iZoom);
			if (pFrameData->m_pLeftRuler)
				pFrameData->m_pLeftRuler->setView(pView, iZoom);
		}
		if (pFrameData->m_pStatusBar)
			pFrameData->m_pStatusBar->setView(pView);
	}
	pView->focusChange(AV_FOCUS_HERE);
	refreshRibbon();
}

void AP_UnixFrameImpl::toggleSplitView()
{
	setSplitView(!isSplitView());
}

void AP_UnixFrameImpl::setSplitView(bool bSplit)
{
	if (bSplit == isSplitView())
		return;
	if (!m_wDocPaned || !m_wSunkenBox)
		return;

	XAP_Frame * pFrame = getFrame();

	if (!bSplit)
	{
		if (m_pPane1View)
			_setActivePane(m_pPane1View);
		if (m_pView2)
		{
			m_pView2->removeListener(m_lidScroll2);
			m_pView2->removeScrollListener(m_pScrollObj2);
		}
		DELETEP(m_pScrollObj2);
		DELETEP(m_pScrollListener2);
		DELETEP(m_pView2);
		DELETEP(m_pDocLayout2);
		DELETEP(m_pG2);
		m_pPane1View = nullptr;
		if (m_wSplitPaned)
		{
			/* unparenting drops the last reference on the sunken
			 * box - hold a ref while it is between parents.  The
			 * split paned itself is destroyed when it is unparented
			 * from m_wDocPaned (its floating ref was sunk), so it
			 * must not be unreffed again. */
			g_object_ref(m_wSunkenBox);
			gtk_paned_set_start_child(GTK_PANED(m_wSplitPaned), nullptr);
			gtk_paned_set_end_child(GTK_PANED(m_wSplitPaned), nullptr);
			gtk_paned_set_start_child(GTK_PANED(m_wDocPaned),
									m_wSunkenBox);
			g_object_unref(m_wSunkenBox);
			m_wSplitPaned = nullptr;
		}
		m_wSplitGrid = nullptr;
		m_dArea2 = nullptr;
		m_vScroll2 = nullptr;
		m_pVadj2 = nullptr;
		m_iVScrollSignal2 = 0;
		m_lidScroll2 = 0;
		return;
	}

	FV_View * pView1 = pFrame
		? static_cast<FV_View *>(pFrame->getCurrentView()) : nullptr;
	PD_Document * pDoc = pView1
		? static_cast<PD_Document *>(pView1->getDocument()) : nullptr;
	if (!pView1 || !pDoc)
		return;

	m_pPane1View = pView1;

	m_pVadj2 = GTK_ADJUSTMENT(
		gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	g_object_set_data(G_OBJECT(m_pVadj2), "user_data", this);
	m_vScroll2 = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, m_pVadj2);
	g_object_set_data(G_OBJECT(m_vScroll2), "user_data", this);
	gtk_widget_set_vexpand(m_vScroll2, TRUE);
	gtk_widget_set_can_focus(m_vScroll2, false);
	m_iVScrollSignal2 = g_signal_connect(
		G_OBJECT(m_pVadj2), "value_changed",
		G_CALLBACK(_vScrollChanged2), nullptr);

	m_dArea2 = ap_DocView_new();
	g_object_set_data(G_OBJECT(m_dArea2), "user_data", this);
	gtk_widget_set_can_focus(m_dArea2, true);
	gtk_widget_set_focusable(m_dArea2, true);
	gtk_widget_set_hexpand(m_dArea2, TRUE);
	gtk_widget_set_vexpand(m_dArea2, TRUE);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(m_dArea2),
								   _drawPane2, m_dArea2, nullptr);

	GtkEventController * key2 = gtk_event_controller_key_new();
	g_signal_connect(key2, "key-pressed",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::key_press_event),
					 m_dArea2);
	g_signal_connect(key2, "key-released",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::key_release_event),
					 m_dArea2);
	gtk_widget_add_controller(m_dArea2, key2);

	GtkGesture * click2 = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click2), 0);
	g_signal_connect(click2, "pressed",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::button_press_event),
					 m_dArea2);
	g_signal_connect(click2, "released",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::button_release_event),
					 m_dArea2);
	gtk_widget_add_controller(m_dArea2, GTK_EVENT_CONTROLLER(click2));

	GtkEventController * motion2 = gtk_event_controller_motion_new();
	g_signal_connect(motion2, "motion",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::motion_notify_event),
					 m_dArea2);
	// IM focus tracking, same as the primary drawing area
	g_signal_connect(motion2, "enter",
					 G_CALLBACK(ap_focus_in_event), m_dArea2);
	g_signal_connect(motion2, "leave",
					 G_CALLBACK(ap_focus_out_event), m_dArea2);
	gtk_widget_add_controller(m_dArea2, motion2);

	GtkEventController * scroll2 = gtk_event_controller_scroll_new(
		GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect(scroll2, "scroll",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::scroll_notify_event),
					 m_dArea2);
	gtk_widget_add_controller(m_dArea2, scroll2);

	GtkEventController * focus2 = gtk_event_controller_focus_new();
	g_signal_connect(focus2, "enter",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::focus_in_event),
					 m_dArea2);
	g_signal_connect(focus2, "leave",
					 G_CALLBACK(XAP_UnixFrameImpl::_fe::focus_out_event),
					 m_dArea2);
	gtk_widget_add_controller(m_dArea2, focus2);

	g_signal_connect(m_dArea2, "resize",
					 G_CALLBACK(_resizePane2), m_dArea2);

	m_wSplitGrid = gtk_grid_new();
	gtk_widget_set_hexpand(m_wSplitGrid, TRUE);
	gtk_widget_set_vexpand(m_wSplitGrid, TRUE);
	gtk_grid_attach(GTK_GRID(m_wSplitGrid), m_dArea2, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(m_wSplitGrid), m_vScroll2, 1, 0, 1, 1);

	/* same graphics + layout + view stack _showDocument builds for
	 * a cloned window, minus the title-bar/selection listeners */
	{
		GR_UnixCairoAllocInfo ai(m_dArea2);
		m_pG2 = XAP_App::getApp()->newGraphics(ai);
	}
	if (m_pG2)
	{
		GR_UnixCairoGraphics * pUGr =
			static_cast<GR_UnixCairoGraphics *>(m_pG2);
		GtkWidget * w = gtk_entry_new();
		g_object_ref_sink(w);
		pUGr->init3dColors(w);
		g_object_unref(w);
		m_pG2->setZoomPercentage(pFrame->getZoomPercentage());
		m_pDocLayout2 = new FL_DocLayout(pDoc, m_pG2);
	}
	if (m_pDocLayout2)
		m_pView2 = new FV_View(XAP_App::getApp(), pFrame,
							   m_pDocLayout2);

	if (!m_pView2)
	{
		/* allocation failed - drop the partial stack and bail */
		DELETEP(m_pDocLayout2);
		DELETEP(m_pG2);
		m_pPane1View = nullptr;
		if (m_wSplitGrid)
			gtk_widget_unparent(m_wSplitGrid);
		m_wSplitGrid = nullptr;
		m_dArea2 = nullptr;
		m_vScroll2 = nullptr;
		m_pVadj2 = nullptr;
		m_iVScrollSignal2 = 0;
		return;
	}

	AP_FrameData * pFrameData =
		static_cast<AP_FrameData *>(pFrame->getFrameData());
	if (pFrameData)
	{
		m_pView2->setShowPara(pFrameData->m_bShowPara);
		m_pView2->setInsertMode(pFrameData->m_bInsertMode);
	}

	m_pScrollObj2 = new AV_ScrollObj(this, _scrollFuncX2,
									 _scrollFuncY2);
	m_pScrollListener2 = new ap_Pane2ViewListener(this, m_pView2);
	m_pView2->addScrollListener(m_pScrollObj2);
	m_pView2->addListener(m_pScrollListener2, &m_lidScroll2);

	m_wSplitPaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	/* replacing the paned's start child unparents m_wSunkenBox and
	 * drops its last reference - keep it alive across the move */
	g_object_ref(m_wSunkenBox);
	gtk_paned_set_start_child(GTK_PANED(m_wDocPaned), m_wSplitPaned);
	gtk_paned_set_start_child(GTK_PANED(m_wSplitPaned), m_wSunkenBox);
	g_object_unref(m_wSunkenBox);
	gtk_paned_set_end_child(GTK_PANED(m_wSplitPaned), m_wSplitGrid);
	gtk_paned_set_resize_start_child(GTK_PANED(m_wSplitPaned), TRUE);
	gtk_paned_set_shrink_start_child(GTK_PANED(m_wSplitPaned), FALSE);
	gtk_paned_set_resize_end_child(GTK_PANED(m_wSplitPaned), FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(m_wSplitPaned), FALSE);

	_PanePosCell * cell = new _PanePosCell{ m_wSplitPaned, 0 };
	g_object_weak_ref(G_OBJECT(m_wSplitPaned), s_panePosCellCleared,
					  cell);
	g_idle_add(s_splitPositionIdle, cell);

	m_pDocLayout2->fillLayouts();
	m_pView2->setYScrollOffset(pView1->getYScrollOffset());
}

/* ===== Arrange All (X11 window tiling) ===== */

bool AP_UnixFrameImpl::arrangeAllWindows()
{
	GdkDisplay * display = gdk_display_get_default();
	if (!display || !GDK_IS_X11_DISPLAY(display))
		return false;

	XAP_App * pApp = XAP_App::getApp();
	if (!pApp)
		return false;

	std::vector<Window> wins;
	GdkSurface * firstSurface = nullptr;
	for (UT_sint32 i = 0; i < pApp->getFrameCount(); ++i)
	{
		XAP_Frame * f = pApp->getFrame(i);
		if (!f || !f->getFrameImpl())
			continue;
		GtkWidget * tl = static_cast<XAP_UnixFrameImpl *>(
			f->getFrameImpl())->getTopLevelWindow();
		if (!tl)
			continue;
		GdkSurface * s = gtk_native_get_surface(GTK_NATIVE(tl));
		if (s && GDK_IS_X11_SURFACE(s))
		{
			wins.push_back(gdk_x11_surface_get_xid(s));
			if (!firstSurface)
				firstSurface = s;
		}
	}
	if (wins.size() < 2)
		return true;   // nothing to arrange

	Display * xdpy = gdk_x11_display_get_xdisplay(
		GDK_X11_DISPLAY(display));

	/* workarea of the monitor under the first window */
	GdkRectangle geo = { 0, 0, 1280, 800 };
	GdkMonitor * mon = firstSurface
		? gdk_display_get_monitor_at_surface(display, firstSurface)
		: nullptr;
	if (mon)
		gdk_x11_monitor_get_workarea(mon, &geo);

	int n = static_cast<int>(wins.size());
	int cols = static_cast<int>(std::ceil(std::sqrt(n)));
	int rows = (n + cols - 1) / cols;
	int cw = geo.width / cols;
	int ch = geo.height / rows;

	Atom netState = gdk_x11_get_xatom_by_name_for_display(
		display, "_NET_WM_STATE");
	Atom maxH = gdk_x11_get_xatom_by_name_for_display(
		display, "_NET_WM_STATE_MAXIMIZED_HORZ");
	Atom maxV = gdk_x11_get_xatom_by_name_for_display(
		display, "_NET_WM_STATE_MAXIMIZED_VERT");
	Atom fullscreen = gdk_x11_get_xatom_by_name_for_display(
		display, "_NET_WM_STATE_FULLSCREEN");

	for (int i = 0; i < n; ++i)
	{
		int col = i % cols, row = i / cols;
		/* drop maximized/fullscreen state or the WM overrides our
		 * placement */
		if (netState != None)
		{
			XClientMessageEvent ev = {};
			ev.type = ClientMessage;
			ev.window = wins[i];
			ev.message_type = netState;
			ev.format = 32;
			ev.data.l[0] = 0; /* _NET_WM_STATE_REMOVE */
			ev.data.l[1] = maxH;
			ev.data.l[2] = maxV;
			XSendEvent(xdpy, DefaultRootWindow(xdpy), False,
					   SubstructureRedirectMask | SubstructureNotifyMask,
					   reinterpret_cast<XEvent *>(&ev));
			ev.data.l[1] = fullscreen;
			ev.data.l[2] = 0;
			XSendEvent(xdpy, DefaultRootWindow(xdpy), False,
					   SubstructureRedirectMask | SubstructureNotifyMask,
					   reinterpret_cast<XEvent *>(&ev));
		}
		XMoveResizeWindow(xdpy, wins[i],
						  geo.x + col * cw, geo.y + row * ch, cw, ch);
	}
	XFlush(xdpy);
	return true;
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
