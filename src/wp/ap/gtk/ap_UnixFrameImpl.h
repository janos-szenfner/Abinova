/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2002 William Lachance
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

#ifndef AP_UNIXFRAMEIMPL_H
#define AP_UNIXFRAMEIMPL_H
#include "xap_Frame.h"
#include "ap_Frame.h"
#include "ap_UnixFrame.h"
#include "ie_types.h"
#include "xap_UnixFrameImpl.h"
#include "ap_DocView.h"


class XAP_UnixApp;
class AP_UnixFrame;

enum apufi_ScrollType: uint8_t { apufi_scrollX, apufi_scrollY }; // can we use namespaces yet? this is quite ugly

class AP_UnixFrameImpl : public XAP_UnixFrameImpl
{
 public:
	AP_UnixFrameImpl(AP_UnixFrame *pUnixFrame);
	virtual ~AP_UnixFrameImpl();
	virtual XAP_FrameImpl * createInstance(XAP_Frame *pFrame) override;

	virtual UT_RGBColor getColorSelBackground() const override;
	virtual UT_RGBColor getColorSelForeground() const override;

	// GTK4 GtkFrame has no shadow type; kept for the abiwidget property API
	int getShadowType () const { return 0; }
	void setShadowType (int /*shadow*/) {}

	GtkWidget * getDrawingArea() const {return m_dArea;}
	static void ap_focus_in_event (GtkEventControllerMotion * c, gdouble x,
								   gdouble y, GtkWidget * drawing_area);
	static void ap_focus_out_event (GtkEventControllerMotion * c, GtkWidget * drawing_area);
	virtual GtkWidget * getViewWidget(void) const override
	{ return m_dArea; }

	/* sync ribbon button states and contextual tabs with the view;
	 * called from the view listener on every change notify */
	void refreshRibbon();

	/* docked side panes (LibreOffice-style deck): the paned end child
	 * is a GtkStack holding the Styles pane and the Selection pane;
	 * one is visible at a time */
	void			setStylesPaneVisible(bool bVisible);
	bool			isStylesPaneVisible() const;
	void			refreshStylesPane(const char * szCurrentStyle);
	void			setSelPaneVisible(bool bVisible);
	bool			isSelPaneVisible() const;
	void			refreshSelPane();
	virtual void	toggleSelPane() override;
	void			setIconsPaneVisible(bool bVisible);
	bool			isIconsPaneVisible() const;
	virtual void	toggleIconsPane() override;
	virtual void	setCommentsPaneVisible(bool bVisible) override;
	bool			isCommentsPaneVisible() const;
	virtual void	toggleCommentsPane() override;
	void			refreshCommentsPane();
	/* moves keyboard focus back to the document canvas, e.g. after
	 * a side-pane button was clicked so typing reaches the view */
	void			focusDocument();

 protected:
	friend class AP_UnixFrame;
	void _showOrHideStatusbar(void);
	void _showOrHideToolbars(void);

	virtual void _hideMenuScroll(bool bHideMenuScroll) override;

	virtual void _createRibbonUI() override;
	virtual void _rebuildMenus() override;
	virtual void setRibbonMode(bool bRibbon) override;
	void _applyUIMode();


	virtual void _refillToolbarsInFrameData() override;
	void _bindToolbars(AV_View * pView);
	virtual void _createWindow();

	virtual GtkWidget * _createDocumentWindow() override;
	virtual GtkWidget * _createStatusBarWindow() override;

	virtual void _setWindowIcon() override;
	void _setScrollRange(apufi_ScrollType scrollType, int iValue, gfloat fUpperLimit, gfloat fSize);

	GtkWidget * m_dArea;
	GtkWidget * m_viewOverlay;
	GtkAdjustment *	m_pVadj;
	GtkAdjustment *	m_pHadj;
	GtkWidget * m_hScroll;
	GtkWidget * m_vScroll;
	GtkWidget * m_topRuler;
	GtkWidget * m_leftRuler;
	GtkWidget * m_grid;
	GtkWidget * m_innergrid;
	GtkWidget * m_wSunkenBox;
	gulong      m_iHScrollSignal;
	gulong      m_iVScrollSignal;

	/* eased vertical scroll: tick callback glides the view toward
	 * m_dScrollAnimTarget instead of jumping per wheel notch */
	guint       m_iScrollAnimID;
	gdouble     m_dScrollAnimTarget;

	class AP_UnixRibbon * m_pRibbon;
	GtkWidget * m_wRibbon;
	bool        m_bRibbonMode;

	/* document area wrapped in a GtkPaned whose end child is the
	 * side deck (GtkStack of docked panes) */
	GtkWidget * m_wDocPaned;
	GtkWidget * m_wSideDeck;
	GtkWidget * m_wStylesPaneW;
	class AP_UnixStylesPane * m_pStylesPane;
	GtkWidget * m_wSelPaneW;
	class AP_UnixSelPane * m_pSelPane;
	GtkWidget * m_wIconsPaneW;
	class AP_UnixIconsPane * m_pIconsPane;
	GtkWidget * m_wCommentsPaneW;
	class AP_UnixCommentsPane * m_pCommentsPane;
};
#endif
