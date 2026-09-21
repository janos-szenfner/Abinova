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

	/* docked Styles pane (LibreOffice-style) */
	void			setStylesPaneVisible(bool bVisible);
	bool			isStylesPaneVisible() const;
	void			refreshStylesPane(const char * szCurrentStyle);

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
	 * docked Styles pane */
	GtkWidget * m_wDocPaned;
	GtkWidget * m_wStylesPaneW;
	class AP_UnixStylesPane * m_pStylesPane;
};
#endif
