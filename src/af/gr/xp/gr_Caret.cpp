/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* An autonomous caret class.
 *
 * Authors: Patrick Lam
 *          Dom Lachowicz
 *          Tomas Frydrych
 *          Marc Maurer <uwog@uwog.net>
 * Inspired by Mike Nordell (tamlin@algonet.se)
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
#include <string>
#include "config.h"
#endif

#include "gr_Caret.h"
#include "gr_Graphics.h"
#include "gr_Painter.h"
#include "ut_debugmsg.h"
#include <inttypes.h>
#include <cmath>
#include <math.h>
#include <stdio.h>
#include <time.h>

static const UT_uint32 CURSOR_DELAY_TIME = 10; // milliseconds

// Caret glide/fade animation, ported from wordcraft's measured Word
// behaviour (crates/ui-egui/src/caret_glide.rs):
//  - a key/typing move glides the caret to its new place in
//    GLIDE_SECS on an ease curve; a mid-flight move snaps to the
//    in-flight target and glides from there
//  - a click fades the caret in at its new place without gliding;
//    a script/control-channel move jumps with no fade
//  - the blink is a fade: each on/off transition takes GLIDE_SECS;
//    a gliding caret stays lit
//  - no glide on a changed view (scroll/zoom/resize/other doc), a
//    caret that wasn't drawn, or moves that start or end off screen
//    (OFFSCREEN_SLACK_PX of slack is allowed)
// ABINOVA_CARET_GLIDE_MS overrides the duration (0 freezes all
// animation for scripted screenshots); the CaretAnimation pref turns
// animations off entirely.
static const double  GLIDE_SECS = 0.10;
static const double  MAX_GLIDE_SECS = 10.0;
// how far outside the window a glide may start or end (device px)
static const UT_sint32 OFFSCREEN_SLACK_PX = 48;
// a key/button event within this window (us) caused the caret move
static const gint64  INPUT_CAUSE_US = 300000;
// a caret this long without an enabled draw cycle is "not shown" and
// never glides (must exceed the blink period)
static const gint64  SHOWN_STALE_US = 1500000;

gint64 GR_Caret::s_iLastKeyUs = 0;
gint64 GR_Caret::s_iLastPointerUs = 0;
bool   GR_Caret::s_bAnimationsOn = true;

#include <gtk/gtk.h>

// Description of m_enabler:
// The problem is that a complicated draw operation will be somewhat
// inefficient & ugly, because the caret gets drawn/undrawn every time
// one of the primitive draw operations takes place.  The solution
// there is for the caret to only be reinstated after the whole draw
// operation is done.  A good heuristic for that is to only draw the
// caret after 10ms of no drawing activity.  This can be done by
// scheduling the reinstating _blink for 10ms in the future; if
// there's another draw that happens to take place before 10ms, then
// your scheduled _blink gets cancelled.

GR_Caret::GR_Caret(GR_Graphics * pG)
	:  	m_xPoint(0), // init the x and y point to some value, since we don't have a sane value here
		m_yPoint(0),
		m_xPoint2(0),
		m_yPoint2(0),
		m_pClr(nullptr),
		m_pG(pG),
		m_iWindowWidth(0),
		m_iWindowHeight(0),
		m_worker(nullptr),
		m_enabler(nullptr),
		m_blinkTimeout(nullptr),
		m_nDisableCount(1),
		m_bCursorBlink(true),
		m_bCursorIsOn(false),
		m_bPositionSet(false),
		m_bRecursiveDraw(false),
		m_bSplitCaret(false),
		m_bCaret1OnScreen(false),
		m_bCaret2OnScreen(false),
		m_clrInsert(0,0,0),
		m_clrOverwrite(255,0,0),
		m_insertMode (true),
		m_bRemote(false),
		m_clrRemote(0,0,0),
		m_sID(""),
		m_iCaretNumber(0),
		m_iLastDrawTime(0),
		m_iRetry(0),
		m_bPendingBlink(false),
		m_animWorker(nullptr),
		m_uViewKey(0),
		m_uPendingViewKey(0),
		m_bViewKeySet(false),
		m_bGliding(false),
		m_iGlideStartUs(0),
		m_dFromX(0),
		m_dFromY(0),
		m_dFromH(0),
		m_dFromX2(0),
		m_dFromY2(0),
		m_dFromH2(0),
		m_bFading(false),
		m_iFadeStartUs(0),
		m_dFadeFrom(0),
		m_dFadeTo(0),
		m_dOpacity(0),
		m_bDrawn(false),
		m_bInPaint(false),
		m_bAnimTickPending(false),
		m_bAnimRunning(false),
		m_iLastShownUs(0)
{
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	m_worker = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_work, this, UT_WorkerFactory::TIMER, outMode));
	m_worker->set(_getCursorBlinkTime());

	m_enabler = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_enable, this, UT_WorkerFactory::TIMER, outMode));
	m_enabler->set(CURSOR_DELAY_TIME);

	m_blinkTimeout = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_blink_timeout, this, UT_WorkerFactory::TIMER, outMode));
	m_blinkTimeout->set(_getCursorBlinkTimeout());

	m_animWorker = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_anim, this, UT_WorkerFactory::TIMER, outMode));
	m_animWorker->set(16);

	setBlink (false);
}

// TODO: fix this code duplication just for the extra sDocUUID (what 
// is it doing here anyway? - MARCM
GR_Caret::GR_Caret(GR_Graphics * pG, const std::string& sId)
	:  	m_xPoint(0), // init the x and y point to some value, since we don't have a sane value here
		m_yPoint(0),
		m_xPoint2(0),
		m_yPoint2(0),
		m_pClr(nullptr),
		m_pG(pG),
		m_iWindowWidth(0),
		m_iWindowHeight(0),
		m_worker(nullptr),
		m_enabler(nullptr),
		m_blinkTimeout(nullptr),
		m_nDisableCount(1),
		m_bCursorBlink(true),
		m_bCursorIsOn(false),
		m_bPositionSet(false),
		m_bRecursiveDraw(false),
		m_bSplitCaret(false),
		m_bCaret1OnScreen(false),
		m_bCaret2OnScreen(false),
		m_clrInsert(0,0,0),
		m_clrOverwrite(255,0,0),
		m_insertMode (true),
		m_bRemote(true),
		m_clrRemote(0,0,0),
		m_sID(sId),
		m_iCaretNumber(0),
		m_iLastDrawTime(0),
		m_iRetry(0),
		m_bPendingBlink(false),
		m_animWorker(nullptr),
		m_uViewKey(0),
		m_uPendingViewKey(0),
		m_bViewKeySet(false),
		m_bGliding(false),
		m_iGlideStartUs(0),
		m_dFromX(0),
		m_dFromY(0),
		m_dFromH(0),
		m_dFromX2(0),
		m_dFromY2(0),
		m_dFromH2(0),
		m_bFading(false),
		m_iFadeStartUs(0),
		m_dFadeFrom(0),
		m_dFadeTo(0),
		m_dOpacity(0),
		m_bDrawn(false),
		m_bInPaint(false),
		m_bAnimTickPending(false),
		m_bAnimRunning(false),
		m_iLastShownUs(0)
{
	UT_WorkerFactory::ConstructMode outMode = UT_WorkerFactory::NONE;
	m_worker = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_work, this, UT_WorkerFactory::TIMER, outMode));
	m_worker->set(_getCursorBlinkTime());

	m_enabler = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_enable, this, UT_WorkerFactory::TIMER, outMode));
	m_enabler->set(CURSOR_DELAY_TIME);

	m_blinkTimeout = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_blink_timeout, this, UT_WorkerFactory::TIMER, outMode));
	m_blinkTimeout->set(_getCursorBlinkTimeout());

	m_animWorker = static_cast<UT_Timer *>(UT_WorkerFactory::static_constructor
		(s_anim, this, UT_WorkerFactory::TIMER, outMode));
	m_animWorker->set(16);

	m_iCaretNumber = pG->m_vecCarets.size() + 1;

	setBlink (false);
}

GR_Caret::~GR_Caret()
{
	m_worker->stop();
	m_enabler->stop();
	m_blinkTimeout->stop();
	m_animWorker->stop();

	DELETEP(m_worker);
	DELETEP(m_enabler);
	DELETEP(m_blinkTimeout);
	DELETEP(m_animWorker);
}

void GR_Caret::s_work(UT_Worker * _w)
{
	GR_Caret * c = static_cast<GR_Caret *>(_w->getInstanceData());
	xxx_UT_DEBUGMSG((" Caret timer called Disable Count = %d \n",c->m_nDisableCount));
	if (c->m_nDisableCount == 0)
	{
		c->setPendingBlink();
		c->m_pG->flush(); // set redraw for wayland
	}
}

/** One-time enabler. */
void GR_Caret::s_enable(UT_Worker * _w)
{
	GR_Caret * c = static_cast<GR_Caret *>(_w->getInstanceData());

	c->m_worker->stop();
	c->_blink(true);
	if (!c->m_bCursorIsOn)
		c->_blink(true); // blink again
	else
	{
		c->_blink(true); // ?? - MARCM
		c->_blink(true);
	}
	c->m_worker->start();
	c->m_enabler->stop();
}

void GR_Caret::s_blink_timeout(UT_Worker *)
{
}

/* ~60 Hz ticker while a glide or fade is in flight: asks for a
 * repaint; the frame is drawn in _blink()/_paintCaret() on the next
 * _draw. Stops itself when the caret is disabled or nothing is left
 * animating. */
void GR_Caret::s_anim(UT_Worker * _w)
{
	GR_Caret * c = static_cast<GR_Caret *>(_w->getInstanceData());
	if (c->m_nDisableCount == 0 && (c->m_bGliding || c->m_bFading))
	{
		c->m_bAnimTickPending = true;
		c->setPendingBlink();
		c->m_pG->flush();
	}
	else
	{
		c->_animStop();
	}
}

void GR_Caret::noteKeyInput(void)
{
	s_iLastKeyUs = g_get_monotonic_time();
}

void GR_Caret::notePointerInput(void)
{
	s_iLastPointerUs = g_get_monotonic_time();
}

void GR_Caret::setAnimationsEnabled(bool bOn)
{
	s_bAnimationsOn = bOn;
}

bool GR_Caret::animationsEnabled(void)
{
	return s_bAnimationsOn;
}

void GR_Caret::setViewKey(UT_uint64 iKey)
{
	m_uPendingViewKey = iKey;
}

/* The glide/fade duration in seconds: GLIDE_SECS, or
 * ABINOVA_CARET_GLIDE_MS/1000 when the env var is set and parses to a
 * finite non-negative value (clamped to MAX_GLIDE_SECS). */
double GR_Caret::_animSecs(void)
{
	static double s_secs = -1.0;
	if (s_secs < 0.0)
	{
		const char * v = g_getenv("ABINOVA_CARET_GLIDE_MS");
		double ms = -1.0;
		if (v && *v)
		{
			char * end = nullptr;
			double d = g_strtod(v, &end);
			if (end != v && std::isfinite(d))
				ms = d;
		}
		s_secs = (ms >= 0.0)
			? ((ms / 1000.0 < MAX_GLIDE_SECS) ? ms / 1000.0 : MAX_GLIDE_SECS)
			: GLIDE_SECS;
	}
	return s_secs;
}

bool GR_Caret::_animationsOn(void) const
{
	return s_bAnimationsOn && _animSecs() > 0.0;
}

/* Word's glide curve, cubic-bezier(0,0,0,1): more than a third of the
 * way after 7% of the time, nine tenths after half. Closed form for
 * these control points: time is u^3, distance 3u^2 - 2u^3. */
double GR_Caret::_ease(double s)
{
	if (!std::isfinite(s))
		return 1.0;
	if (s < 0.0)
		s = 0.0;
	if (s > 1.0)
		s = 1.0;
	return 3.0 * pow(s, 2.0 / 3.0) - 2.0 * s;
}

/* Progress 0..1 of an animation started at iStartUs, or -1 once it is
 * over (or the clock went backwards). */
double GR_Caret::_animProgress(gint64 iStartUs, gint64 iNowUs) const
{
	double secs = _animSecs();
	if (secs <= 0.0)
		return -1.0;
	double s = (iNowUs - iStartUs) / (secs * 1000000.0);
	return (s >= 0.0 && s < 1.0) ? s : -1.0;
}

/* Is a caret bar (top at x,y, h tall) at least partly within the
 * window plus OFFSCREEN_SLACK_PX of slack? */
bool GR_Caret::_barNearScreen(double x, double y, double h) const
{
	double slack = m_pG->tluD(OFFSCREEN_SLACK_PX);
	return x >= -slack && x <= static_cast<double>(m_iWindowWidth) + slack &&
		y + h >= -slack && y <= static_cast<double>(m_iWindowHeight) + slack;
}

void GR_Caret::_animStart(void)
{
	if (!m_bAnimRunning && m_animWorker)
	{
		m_animWorker->start();
		m_bAnimRunning = true;
	}
}

void GR_Caret::_animStop(void)
{
	if (m_bAnimRunning && m_animWorker)
	{
		m_animWorker->stop();
		m_bAnimRunning = false;
	}
}

UT_uint32 GR_Caret::_getCursorBlinkTime() const
{
	// gtk-cursor-blink-time's default is 1200 ms; settings can be null
	// headless, so give blink a defined value before g_object_get.
	UT_uint32 blink = 1200;
	GtkSettings * settings = gtk_settings_get_default ();

	if (settings)
		g_object_get (G_OBJECT(settings), "gtk-cursor-blink-time", &blink, nullptr);

	// wordcraft's measured blink is 0.53 s on, 0.53 s off; use the GTK
	// half-period when it is larger, never shorter than 530 ms
	UT_uint32 half = blink/2;
	return (half < 530 ? 530 : half);
}

UT_uint32 GR_Caret::_getCursorBlinkTimeout() const
{
	UT_uint32 timeout = 0;
	GtkSettings * settings = gtk_settings_get_default ();

	// retrieves the blink timeout in seconds
	g_object_get (G_OBJECT(settings), "gtk-cursor-blink-timeout", &timeout, nullptr);
	return (timeout == 0 ? 2147483647 : timeout * 1000);
}

bool GR_Caret::_getCanCursorBlink() const
{
	return m_bCursorBlink;
}

void GR_Caret::setRemoteColor(UT_RGBColor clrRemote)
{
	m_clrRemote = clrRemote;
}


void GR_Caret::setWindowSize(UT_uint32 width, UT_uint32 height)
{
	m_iWindowWidth = width; m_iWindowHeight = height;

	if(m_xPoint < m_pG->tlu(3)+1 || m_yPoint < 0 || m_xPoint > static_cast<UT_sint32>(m_iWindowWidth) || m_yPoint > static_cast<UT_sint32>(m_iWindowHeight))
		m_bCaret1OnScreen = false;
	else
		m_bCaret1OnScreen = true;
	
	if(m_xPoint2 < m_pG->tlu(3)+1 || m_yPoint2 < 0 || m_xPoint2 > static_cast<UT_sint32>(m_iWindowWidth) || m_yPoint2 > static_cast<UT_sint32>(m_iWindowHeight))
		m_bCaret2OnScreen = false;
	else
		m_bCaret2OnScreen = true;
}

void GR_Caret::setCoords(UT_sint32 x, UT_sint32 y, UT_uint32 h,
						 UT_sint32 x2, UT_sint32 y2, UT_uint32 h2,
						 bool bPointDirection, const UT_RGBColor * pClr)
{
	/* Glide/fade classification, wordcraft caret_glide semantics: a
	 * move caused by keys glides from where the caret was heading and
	 * fades in; a pointer move fades in at the new place; anything
	 * else just jumps. No glide on a changed view, a caret that
	 * wasn't shown, or endpoints off screen. */
	if (_animationsOn() && m_bPositionSet)
	{
		gint64 now = g_get_monotonic_time();
		bool sameView = m_bViewKeySet && (m_uPendingViewKey == m_uViewKey);

		// the text shifted under a glide (e.g. pixel snapping): carry
		// the glide along with its moving target
		if (m_bGliding &&
			(x != m_xPoint || y != m_yPoint ||
			 x2 != m_xPoint2 || y2 != m_yPoint2))
		{
			m_dFromX += x - m_xPoint;
			m_dFromY += y - m_yPoint;
			m_dFromX2 += x2 - m_xPoint2;
			m_dFromY2 += y2 - m_yPoint2;
		}
		if (!sameView)
			m_bGliding = false;

		bool moved = m_pG->tdu(x) != m_pG->tdu(m_xPoint) ||
			m_pG->tdu(y) != m_pG->tdu(m_yPoint) ||
			m_pG->tdu(x2) != m_pG->tdu(m_xPoint2) ||
			m_pG->tdu(y2) != m_pG->tdu(m_yPoint2) ||
			h != m_iPointHeight || h2 != m_iPointHeight2;
		if (moved)
		{
			bool byKeys = s_iLastKeyUs != 0 &&
				(now - s_iLastKeyUs) < INPUT_CAUSE_US;
			bool byPointer = s_iLastPointerUs != 0 &&
				(now - s_iLastPointerUs) < INPUT_CAUSE_US;
			if (byKeys || byPointer)
			{
				// a key or a click turns the caret on, from however
				// visible it was
				_setFadeTarget(now, 1.0);
			}
			else
			{
				// a script moved it: it is simply there
				m_bFading = false;
			}
			m_bGliding = byKeys && !byPointer && sameView &&
				(now - m_iLastShownUs) < SHOWN_STALE_US &&
				_barNearScreen(m_xPoint, m_yPoint, m_iPointHeight) &&
				_barNearScreen(x, y, h);
			if (m_bGliding)
			{
				// glide from where the caret was heading (the
				// in-flight target), not where it has got to
				m_iGlideStartUs = now;
				m_dFromX = m_xPoint;
				m_dFromY = m_yPoint;
				m_dFromH = m_iPointHeight;
				m_dFromX2 = m_xPoint2;
				m_dFromY2 = m_yPoint2;
				m_dFromH2 = m_iPointHeight2;
				_animStart();
			}
		}
	}
	m_uViewKey = m_uPendingViewKey;
	m_bViewKeySet = true;

	m_xPoint = x; m_yPoint = y; m_iPointHeight = h;
	m_xPoint2 = x2; m_yPoint2 = y2; m_iPointHeight2 = h2;
	m_bPointDirection = bPointDirection; m_pClr = pClr;
	m_bPositionSet = true;
	if(m_bRemote)
	  {
	    UT_DEBUGMSG(("Remote caret %p set to x %d \n", static_cast<void*>(this), x));
	  }
	if(x < m_pG->tlu(3)+1 || y <= 0 || x > static_cast<UT_sint32>(m_iWindowWidth) || y > static_cast<UT_sint32>(m_iWindowHeight))
		m_bCaret1OnScreen = false;
	else
		m_bCaret1OnScreen = true;
	
	if(x2 < m_pG->tlu(3)+1 || y2 <= 0 || x2 > static_cast<UT_sint32>(m_iWindowWidth) || y2 > static_cast<UT_sint32>(m_iWindowHeight))
		m_bCaret2OnScreen = false;
	else
		m_bCaret2OnScreen = true;
}

void GR_Caret::enable()
{
	if (m_bRecursiveDraw)
		return;

	// If the caret is already enabled, just return
	if (m_nDisableCount == 0)
	{
		xxx_UT_DEBUGMSG(("Don't emable disable Count is already zero \n"));
		return;
	}
	// Check to see if we still have pending disables.
	--m_nDisableCount;
	if (m_nDisableCount != 0)
	{
		xxx_UT_DEBUGMSG(("Don't emable, disable Count has not reached zero \n"));
		return;
	}
	// stop pending enables; in 10 ms, really enable blinking.
	m_enabler->stop();
	m_enabler->start();
}

void GR_Caret::disable(bool bNoMulti)
{
	if (m_bRecursiveDraw)
		return;

	if (bNoMulti && (m_nDisableCount > 0))
		return;

	m_nDisableCount++;
	if (m_nDisableCount == 1)
		_hardHide();

	if (m_worker)
		m_worker->stop();
	if (m_enabler)
		m_enabler->stop();
	if (m_animWorker)
		m_animWorker->stop();
	m_bAnimRunning = false;
}

/** Determines whether Abi is going to blink the caret or not.
 * If not, then _blink() won't actually clear the caret; it'll only draw. */
void GR_Caret::setBlink(bool bBlink)
{
	gboolean can;
	GtkSettings * settings = gtk_settings_get_default ();

	g_object_get (G_OBJECT(settings), "gtk-cursor-blink", &can, nullptr);
	m_bCursorBlink = (can != FALSE);
	UT_UNUSED(bBlink);
}
/*!
 * Erase the current caret if it's Position overlaps calling value.
 * This prevents remote carets saving a drawn caret in the saveRectange() call
 */
void GR_Caret::JustErase(UT_sint32 xPoint,UT_sint32 yPoint)
{
       if(m_bRecursiveDraw || m_bInPaint)
       {
	    xxx_UT_DEBUGMSG(("Doing recursive Just Erase! - abort \n"));
	    return;
       }
       if ((m_bCursorIsOn || m_bDrawn) && (((xPoint -m_pG->tlu(2)-1) <= m_xPoint) && (xPoint >= (m_xPoint-m_pG->tlu(2))-1)) && ((yPoint - m_pG->tlu(1)) <= m_yPoint) && (yPoint >= (m_yPoint - m_pG->tlu(1))))
       {
	    xxx_UT_DEBUGMSG(("Doing Just Erase! now \n"));
	    _hardHide();
	    m_nDisableCount = 1;
       }
}

void GR_Caret::_erase()
{
	if (m_bCursorIsOn || m_bDrawn)
		_hardHide();
}

/* Remove the caret pixels and drop the logical on-state, but leave a
 * glide in flight alone — the disable/enable pairs that bracket
 * insertion-point updates would otherwise kill every glide. An
 * interrupted fade is forgotten; the next draw starts from the
 * blink's own on/off state again. */
void GR_Caret::_hardHide(void)
{
	_eraseCaretPixels();
	m_bCursorIsOn = false;
	m_dOpacity = 0.0;
	m_dFadeTo = 0.0;
	m_bFading = false;
	_animStop();
}

void GR_Caret::_eraseCaretPixels(void)
{
	if (!m_bDrawn)
		return;
	if (m_pG->getPaintCount() == 0)
	{
		/* no draw cycle owns a cairo context right now: beginPaint
		 * aims the graphics at its backing surface so the restore
		 * lands immediately (the next redraw picks it up) — same
		 * painter bracket the old _erase()->_blink() path had */
		GR_Painter painter(m_pG, false);
		_eraseCaretPixels();
		return;
	}
	m_pG->restoreRectangle(m_iCaretNumber*3+0);
	if(m_bSplitCaret)
	{
		m_pG->restoreRectangle(m_iCaretNumber*3+1);
		m_pG->restoreRectangle(m_iCaretNumber*3+2);
		m_bSplitCaret = false;
	}
	m_bDrawn = false;
}

/* The logical blink toggle (the old _blink's on<->off flip): decides
 * which way the caret should be heading and aims the fade at it. */
void GR_Caret::_advanceBlink(gint64 iNowUs)
{
	if (m_bCursorIsOn)
	{
		m_bCursorIsOn = false;
		_setFadeTarget(iNowUs, 0.0);
		return;
	}
	// if neither caret is on screen, stay off
	if(!m_bCaret1OnScreen && !m_bCaret2OnScreen)
		return;
	m_bCursorIsOn = true;
	_setFadeTarget(iNowUs, 1.0);
}

/* Aim the caret's opacity at dTo over _animSecs() (instant when
 * animations are off). */
void GR_Caret::_setFadeTarget(gint64 iNowUs, double dTo)
{
	m_dFadeTo = dTo;
	if (!_animationsOn())
	{
		m_bFading = false;
		m_dOpacity = dTo;
		return;
	}
	m_dFadeFrom = m_dOpacity;
	m_iFadeStartUs = iNowUs;
	m_bFading = fabs(dTo - m_dOpacity) > 0.01;
	if (m_bFading)
		_animStart();
}

void GR_Caret::_blink(bool bExplicit)
{
	if(m_bRecursiveDraw)
	{
	    xxx_UT_DEBUGMSG(("Doing recursive blink! - abort \n"));
	    return;
	}
	if (!m_bPositionSet)
		return;
	if(!m_bPendingBlink)
		return;
	struct timespec spec;

	UT_clock_gettime_realtime(&spec);

    gint64 s  = spec.tv_sec;
    gint64 ms = static_cast<gint64>(round(spec.tv_nsec / 1.0e6)); // Convert nanoseconds to milliseconds
	gint64 this_time = 1000*s + ms;
	gint64 time_between = this_time - m_iLastDrawTime;
	m_iLastDrawTime = this_time;
    //
	// If this number is high enough the caret will not blink at all
	// This places the caret on the screen during rapid moves.
	//
	if(time_between < _getCursorBlinkTime()/2)
	{
		m_iRetry++;
		xxx_UT_DEBUGMSG(("Caret redraw after %d ms \n",time_between));
	}
	else
	{
		m_iRetry = 0;
	}
	m_bRecursiveDraw = true;
	GR_Painter painter (m_pG, false);
	m_bRecursiveDraw = false;

	// Blink if: (a) _blink explicitly called (not autoblink); or
	//           (b) autoblink and caret blink enabled; or
	//           (c) autoblink, caret blink disabled, caret is off
	if (bExplicit || _getCanCursorBlink () || !m_bCursorIsOn)
	{
		m_bRecursiveDraw = true;
		UT_RGBColor oldColor;
		m_pG->getColor(oldColor);

		if (m_bAnimTickPending)
		{
			// a repaint the glide/fade ticker asked for: draw the
			// next animation frame, don't toggle the blink
			m_bAnimTickPending = false;
			m_bPendingBlink = false;
		}
		else
		{
			_advanceBlink(g_get_monotonic_time());
		}
		_paintCaret(painter);

 		m_pG->setColor(oldColor);
		m_bRecursiveDraw = false;
	}
	//
	// Places the caret on the screen during rapid moves like pressing the arrow keys
	//
	if(m_iRetry == 0)
		m_bPendingBlink = false;
}

/* Paint the caret's current animated state: advances the glide and
 * fade clocks to now, erases or draws accordingly, and keeps the
 * animation ticker running while anything is still in flight. */
void GR_Caret::_paintCaret(GR_Painter & painter)
{
	gint64 now = g_get_monotonic_time();
	if (!_animationsOn())
	{
		// animations got turned off mid-flight: land instantly
		m_bGliding = false;
		if (m_bFading)
		{
			m_bFading = false;
			m_dOpacity = m_dFadeTo;
		}
	}

	if (m_bFading)
	{
		double s = _animProgress(m_iFadeStartUs, now);
		if (s < 0.0)
		{
			m_bFading = false;
			m_dOpacity = m_dFadeTo;
		}
		else
			m_dOpacity = m_dFadeFrom + (m_dFadeTo - m_dFadeFrom) * s;
	}

	double x = m_xPoint, y = m_yPoint, h = m_iPointHeight;
	double x2 = m_xPoint2, y2 = m_yPoint2, h2 = m_iPointHeight2;
	if (m_bGliding)
	{
		double s = _animProgress(m_iGlideStartUs, now);
		if (s < 0.0)
			m_bGliding = false;
		else
		{
			double e = _ease(s);
			x = m_dFromX + (x - m_dFromX) * e;
			y = m_dFromY + (y - m_dFromY) * e;
			h = m_dFromH + (h - m_dFromH) * e;
			x2 = m_dFromX2 + (x2 - m_dFromX2) * e;
			y2 = m_dFromY2 + (y2 - m_dFromY2) * e;
			h2 = m_dFromH2 + (h2 - m_dFromH2) * e;
		}
	}

	if (m_nDisableCount == 0)
		m_iLastShownUs = now;

	// a gliding caret stays lit; a move that found it dark still
	// fades in while it glides
	double op = m_dOpacity;
	if (m_bGliding)
		op = (m_bFading && m_dFadeTo > m_dFadeFrom) ? m_dOpacity : 1.0;

	if (op <= 0.01 || (!m_bCaret1OnScreen && !m_bCaret2OnScreen))
	{
		_eraseCaretPixels();
		if (!m_bGliding && !m_bFading)
			_animStop();
		return;
	}

	_drawCaretBars(painter, x, y, h, x2, y2, h2, op);

	if (m_bGliding || m_bFading)
		_animStart();
	else
		_animStop();
}

/* Draw the caret bar(s) at the given position and opacity. The
 * geometry matches the old _blink draw block: a 2px bar slightly
 * covering the char edge, direction flags and a joining line for the
 * split (bidi) caret. */
void GR_Caret::_drawCaretBars(GR_Painter & painter, double dx, double dy,
							  double dh, double dx2, double dy2, double dh2,
							  double dOpacity)
{
	UT_sint32 x  = static_cast<UT_sint32>(floor(dx + 0.5));
	UT_sint32 y  = static_cast<UT_sint32>(floor(dy + 0.5));
	UT_sint32 h  = static_cast<UT_sint32>(floor(dh + 0.5));
	UT_sint32 x2 = static_cast<UT_sint32>(floor(dx2 + 0.5));
	UT_sint32 y2 = static_cast<UT_sint32>(floor(dy2 + 0.5));
	UT_sint32 h2 = static_cast<UT_sint32>(floor(dh2 + 0.5));
	UT_UNUSED(h2);

	// move the saved/drawn pixels along: restore what the previous
	// frame painted before saving the new spot
	_eraseCaretPixels();

	// we position the regular LTR caret so that it slightly
	// covers the right edge of the character; in order to be
	// consitent we will do notionally the same for the RTL
	// carets, where this requires addition in the calculation
	// of coordinances instead of substraction; we will use
	// the following value as a sign to avoid having to branch
	// all the draw calls (Tomas, Oct 26, 2003).

	UT_sint32 iDelta = m_bPointDirection ? 1 : -1;

	m_bInPaint = true;
	m_bRecursiveDraw = false;
	m_pG->allCarets()->JustErase(x,y);
	m_bRecursiveDraw = true;

	UT_Rect r0(x-m_pG->tlu(2),
			   y+m_pG->tlu(1),
			   m_pG->tlu(5),
			   h+m_pG->tlu(2));
	m_pG->saveRectangle(r0,m_iCaretNumber*3+0);

	m_bSplitCaret = (x != x2) || (y != y2);
	if(m_bSplitCaret)
	{
		// have to save the rectangle for the joining line and the
		// secondary caret before we draw anything
		UT_sint32 xmin = UT_MIN(x, x2);
		UT_sint32 xmax = UT_MAX(x, x2);
		UT_sint32 ymin = UT_MIN(y, y2);
		UT_sint32 ymax = UT_MAX(y, y2);

		UT_Rect r2(xmin-m_pG->tlu(1),
				   ymin + h,
				   xmax - xmin + m_pG->tlu(2),
				   ymax - ymin + m_pG->tlu(1));
		m_pG->saveRectangle(r2,m_iCaretNumber*3+2);

		if(m_bCaret2OnScreen)
		{
			UT_Rect r1(x2 - m_pG->tlu(2),
					   y2 + m_pG->tlu(1),
					   m_pG->tlu(5),
					   h);
			m_pG->saveRectangle(r1,m_iCaretNumber*3+1);
		}
	}

	if(m_bRemote)
		m_pG->setColor(m_clrRemote);
	else if(m_insertMode)
		m_pG->setColor(m_clrInsert);
	else
		m_pG->setColor(m_clrOverwrite);
	if (dOpacity < 1.0)
		m_pG->setAlpha(dOpacity);

	if(m_bCaret1OnScreen)
	{
		// draw the primary caret
		xxx_UT_DEBUGMSG(("Draw Caret reTry %d \n",m_iRetry));

		UT_sint32 x1 = x + iDelta * m_pG->tlu(1);
		UT_sint32 xe = x;
		while(m_pG->_tduX(x1) == m_pG->_tduX(xe))
		{
			x1 += iDelta;
		}
		painter.drawLine(x1,
						 y + m_pG->tlu(1),
						 x1,
						 y + h+m_pG->tlu(1));

		painter.drawLine(xe,
						 y + m_pG->tlu(1),
						 xe,
						 y + h + m_pG->tlu(1));
	}

	if(m_bSplitCaret)
	{
		// each of the two carets will have a small flag at the top
		// indicating the direction of writing to which it
		// applies
		if(m_bCaret1OnScreen)
		{
			// draw the flag of the primary caret
			if(m_bPointDirection)
			{
				//primary RTL caret flag
				painter.drawLine(x - m_pG->tlu(2),
								 y + m_pG->tlu(1),
								 x /*- m_pG->tlu(1)*/,
								 y + m_pG->tlu(1));

				painter.drawLine(x - m_pG->tlu(1),
								 y + m_pG->tlu(2),
								 x /*- m_pG->tlu(1)*/,
								 y+m_pG->tlu(2));
			}
			else
			{
				// primary LTR caret flag
				painter.drawLine(x + m_pG->tlu(1),
								 y + m_pG->tlu(1),
								 x + m_pG->tlu(3),
								 y + m_pG->tlu(1));

				painter.drawLine(x + m_pG->tlu(1),
								 y + m_pG->tlu(2),
								 x + m_pG->tlu(2),
								 y + m_pG->tlu(2));
			}
		}

		// Now we deal with the secondary caret needed on ltr-rtl boundary
		if(m_bCaret2OnScreen)
		{
			// draw the caret

			painter.drawLine(x2 - iDelta * m_pG->tlu(1),
							 y2 + m_pG->tlu(1),
							 x2 - iDelta * m_pG->tlu(1),
							 y2 + h + m_pG->tlu(1));

			painter.drawLine(x2,
							 y2 + m_pG->tlu(1),
							 x2,
							 y2 + h + m_pG->tlu(1));

			// Now draw the line that links the two carets
			painter.drawLine(x,
							 y + h,
							 x2,
							 y2 + h);

			// now draw the direction flag for the secondary caret
			if(m_bPointDirection)
			{
				// secondary LTR caret flag
				painter.drawLine(x2 + m_pG->tlu(1),
								 y2 + m_pG->tlu(1),
								 x2 + m_pG->tlu(3),
								 y2 + m_pG->tlu(1));

				painter.drawLine(x2 + m_pG->tlu(1),
								 y2 + m_pG->tlu(2),
								 x2 + m_pG->tlu(2),
								 y2 + m_pG->tlu(2));
			}
			else
			{
				// secondary RTL caret flag
				painter.drawLine(x2 - m_pG->tlu(2),
								 y2 + m_pG->tlu(1),
								 x2 /*- m_pG->tlu(1)*/,
								 y2 + m_pG->tlu(1));

				painter.drawLine(x2 - m_pG->tlu(1),
								 y2 + m_pG->tlu(2),
								 x2 /*- m_pG->tlu(1)*/,
								 y2 + m_pG->tlu(2));
			}
		}
	}

	if (dOpacity < 1.0)
		m_pG->setAlpha(1.0);
	m_bDrawn = true;
	m_bInPaint = false;
}

void GR_Caret::setPendingBlink()
{
	xxx_UT_DEBUGMSG(("Pending blink set in GR_CARET \n"));
	m_bPendingBlink = true;
}

//
// Returns true if we did a blink
//
bool GR_Caret::doBlinkIfNeeded()
{
	if(!m_bPendingBlink)
	{
		return false;
	}
	_blink(true);
	m_bPendingBlink = false;
	return true;
}
/*!
 * Only call this is you are absolutely certain you need it!
 */
void GR_Caret::forceDraw(void)
{
	if(m_nDisableCount < 2)
	{
		_blink(true);
	}
}

void GR_Caret::resetBlinkTimeout(void)
{
	// reset the blink timeout timer
	m_blinkTimeout->stop();
	m_blinkTimeout->start();
	
	// resume normal blinking if we previously hit the
	// blink timeout
	if (!isEnabled())
		enable();
}

//////////////////////////////////////////////////////////////////////////////////////
