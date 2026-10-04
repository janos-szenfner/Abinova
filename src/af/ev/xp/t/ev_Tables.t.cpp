/* AbiSource Application Framework
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

/* unit tests for the pure table-driven parts of af/ev: edit-method
 * lookup, binding maps, the keystroke/mouse event mapper, keyboard
 * and mouse dispatch, and the menu/toolbar action/label/layout
 * containers.  No UI is needed for any of this. */

#include <string.h>

#include <glib.h>

#include "tf_test.h"

#include "xap_App.h"

#include "ev_EditBits.h"
#include "ev_EditBinding.h"
#include "ev_EditEventMapper.h"
#include "ev_EditMethod.h"
#include "ev_Keyboard.h"
#include "ev_Menu_Actions.h"
#include "ev_Menu_Labels.h"
#include "ev_Menu_Layouts.h"
#include "ev_Mouse.h"
#include "ev_MouseListener.h"
#include "ev_NamedVirtualKey.h"
#include "ev_Toolbar_Actions.h"
#include "ev_Toolbar_Control.h"
#include "ev_Toolbar_Labels.h"
#include "ev_Toolbar_Layouts.h"

#define TFSUITE "core.af.ev.tables"

namespace {

/* a fake view pointer -- the edit method callbacks under test only
 * record the pointer, they never dereference it */
static int s_iFakeViewObject;
static AV_View * const FAKE_VIEW = reinterpret_cast<AV_View *>(&s_iFakeViewObject);

static int s_iCallCount = 0;
static AV_View * s_pLastView = nullptr;
static UT_UCS4Char s_lastData[64];
static UT_uint32 s_lastDataLen = 0;
static void * s_pLastContext = nullptr;

static bool tf_record_call(AV_View * pView, EV_EditMethodCallData * pCallData)
{
	s_iCallCount++;
	s_pLastView = pView;
	s_lastDataLen = 0;
	if (pCallData && pCallData->m_pData && pCallData->m_dataLength) {
		UT_uint32 n = pCallData->m_dataLength;
		if (n > 63)
			n = 63;
		for (UT_uint32 i = 0; i < n; i++)
			s_lastData[i] = pCallData->m_pData[i];
		s_lastDataLen = n;
	}
	return true;
}

static bool tf_record_call_ctxt(AV_View * pView, EV_EditMethodCallData * pCallData,
								void * context)
{
	s_pLastContext = context;
	return tf_record_call(pView, pCallData);
}

static void tf_reset(void)
{
	s_iCallCount = 0;
	s_pLastView = nullptr;
	s_pLastContext = nullptr;
	s_lastDataLen = 0;
}

/* the static table MUST be name-sorted: findEditMethodByName() does a
 * bsearch over it */
static EV_EditMethod s_methods[] =
{
	EV_EditMethod("alphaCmd",   tf_record_call, 0, "first test command"),
	EV_EditMethod("betaCmd",    tf_record_call, EV_EMT_REQUIREDATA, "data-hungry test command"),
	EV_EditMethod("gammaCmd",   tf_record_call, 0, "third test command"),
};

static EV_EditBinding * tf_bind_char(EV_EditBindingMap * pMap, UT_uint32 key,
									 EV_EditModifierState ems, const char * szMethod)
{
	TFPASS(pMap->setBinding(EV_EKP_PRESS | key | ems, szMethod));
	return pMap->findEditBinding(EV_EKP_PRESS | key | ems);
}

static EV_EditBits tf_mouse_bits(EV_EditMouseButton btn, EV_EditMouseOp op,
								 EV_EditMouseContext ctx)
{
	return static_cast<EV_EditBits>(btn) | static_cast<EV_EditBits>(op)
		| static_cast<EV_EditBits>(ctx);
}

} /* anonymous namespace */

/* ------------------------------------------------------------------ */
/* EV_EditMethodCallData / EV_EditMethod / EV_EditMethodContainer      */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("EV_EditMethodCallData")
{
	{
		EV_EditMethodCallData cd;
		TFPASS(cd.m_pData == nullptr);
		TFPASS(cd.m_dataLength == 0);
		TFPASS(!cd.m_bAllocatedData);
		TFPASS(cd.getX() == 0 && cd.getY() == 0);
	}

	{
		const UT_UCS4Char data[] = {'h', 'e', 'l', 'l', 'o'};
		EV_EditMethodCallData cd(data, 5);
		TFPASS(cd.m_bAllocatedData);
		TFPASS(cd.m_dataLength == 5);
		TFPASS(cd.m_pData != nullptr);
		TFPASS(cd.m_pData[0] == 'h' && cd.m_pData[4] == 'o');
		/* the container copies: mutating the source must not leak in */
		cd.m_xPos = 11;
		cd.m_yPos = 22;
		TFPASS(cd.getX() == 11 && cd.getY() == 22);
	}

	{
		/* zero-length data is a legal, NUL-initialised buffer */
		EV_EditMethodCallData cd(static_cast<const UT_UCS4Char*>(nullptr), 0);
		TFPASS(cd.m_pData != nullptr);
		TFPASS(cd.m_pData[0] == 0);
		TFPASS(cd.m_dataLength == 0);
	}

	{
		EV_EditMethodCallData cd("abc", 3);
		TFPASS(cd.m_dataLength == 3);
		TFPASS(cd.m_pData[2] == 'c');
	}

	{
		EV_EditMethodCallData cd(UT_String("testScript"));
		TFPASS(cd.getScriptName() == "testScript");
	}
}

TFTEST_MAIN("EV_EditMethod")
{
	tf_reset();

	EV_EditMethod plain("myCmd", tf_record_call, 0, "a description");
	TFPASS(!strcmp(plain.getName(), "myCmd"));
	TFPASS(!strcmp(plain.getDescription(), "a description"));
	TFPASS(plain.getType() == 0);

	EV_EditMethodCallData cd;
	TFPASS(plain.Fn(FAKE_VIEW, &cd));
	TFPASS(s_iCallCount == 1);
	TFPASS(s_pLastView == FAKE_VIEW);

	EV_EditMethod ctxt("ctxCmd", tf_record_call_ctxt, EV_EMT_REQUIREDATA,
					   "context command", reinterpret_cast<void*>(0xdead));
	TFPASS(ctxt.getType() == EV_EMT_REQUIREDATA);
	TFPASS(ctxt.Fn(FAKE_VIEW, &cd));
	TFPASS(s_pLastContext == reinterpret_cast<void*>(0xdead));
}

TFTEST_MAIN("EV_EditMethodContainer")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);

	TFPASS(emc.countEditMethods() == 3);
	TFPASS(emc.findEditMethodByName("alphaCmd") == &s_methods[0]);
	TFPASS(emc.findEditMethodByName("betaCmd") == &s_methods[1]);
	TFPASS(emc.findEditMethodByName("gammaCmd") == &s_methods[2]);
	/* second lookup comes back through the static hash cache */
	TFPASS(emc.findEditMethodByName("betaCmd") == &s_methods[1]);
	TFPASS(emc.findEditMethodByName("noSuchCommand") == nullptr);
	TFPASS(emc.findEditMethodByName(nullptr) == nullptr);

	/* getNthEditMethod spans the static table then the dynamic one */
	TFPASS(emc.getNthEditMethod(0) == &s_methods[0]);
	TFPASS(emc.getNthEditMethod(2) == &s_methods[2]);

	EV_EditMethod * dyn = new EV_EditMethod("zdynCmd", tf_record_call, 0, "dyn");
	TFPASS(emc.addEditMethod(dyn));
	TFPASS(emc.countEditMethods() == 4);
	TFPASS(emc.getNthEditMethod(3) == dyn);
	/* dynamic methods are found by linear search */
	TFPASS(emc.findEditMethodByName("zdynCmd") == dyn);

	TFPASS(emc.removeEditMethod(dyn));
	delete dyn;
	TFPASS(emc.countEditMethods() == 3);
	TFPASS(emc.findEditMethodByName("zdynCmd") == nullptr);
	TFPASS(!emc.removeEditMethod(dyn));
}

TFTEST_MAIN("ev_EditMethod lookup/exists/invoke")
{
	/* the app-level container holds all of the real edit methods */
	EV_EditMethodContainer * emc = XAP_App::getApp()->getEditMethodContainer();
	TFPASS(emc != nullptr);
	TFPASS(emc->countEditMethods() > 100);

	TFPASS(ev_EditMethod_exists("fileNew"));
	TFPASS(ev_EditMethod_exists(UT_String("fileNew")));
	TFPASS(!ev_EditMethod_exists("definitely.not.a.method"));
	TFPASS(!ev_EditMethod_exists(UT_String("definitely.not.a.method")));
	TFPASS(ev_EditMethod_lookup("fileNew") != nullptr);
	TFPASS(ev_EditMethod_lookup(UT_String("fileNew")) != nullptr);
	TFPASS(ev_EditMethod_lookup("definitely.not.a.method") == nullptr);

	/* register a dynamic method on the real container so the
	 * name-based invoke() path resolves to our own callback */
	EV_EditMethod * dyn =
		new EV_EditMethod("zzCovInvoke", tf_record_call, 0, "invoke test");
	emc->addEditMethod(dyn);

	tf_reset();
	/* no focused frame in the test harness -> Fn sees a null view */
	TFPASS(ev_EditMethod_invoke(dyn, UT_String("hello")));
	TFPASS(s_iCallCount == 1);
	TFPASS(s_pLastView == nullptr);
	TFPASS(s_lastDataLen == 5 && s_lastData[0] == 'h');

	UT_UCS4Char udata[] = {'X', 'Y', 'Z', 0};
	TFPASS(ev_EditMethod_invoke(dyn, UT_UCS4String(udata, 3)));
	TFPASS(s_lastDataLen == 3 && s_lastData[0] == 'X');

	EV_EditMethodCallData cd;
	TFPASS(ev_EditMethod_invoke(dyn, &cd));

	TFPASS(ev_EditMethod_invoke("zzCovInvoke", UT_String("d1")));
	TFPASS(ev_EditMethod_invoke("zzCovInvoke", UT_UCS4String(udata, 3)));
	TFPASS(ev_EditMethod_invoke("zzCovInvoke", "d2"));
	TFPASS(ev_EditMethod_invoke("zzCovInvoke", static_cast<const UT_UCS4Char*>(udata)));
	TFPASS(ev_EditMethod_invoke(UT_String("zzCovInvoke"), UT_String("d3")));
	TFPASS(ev_EditMethod_invoke(UT_String("zzCovInvoke"), UT_UCS4String(udata, 3)));

	/* failure paths */
	TFPASS(!ev_EditMethod_invoke(static_cast<const EV_EditMethod*>(nullptr), &cd));
	TFPASS(!ev_EditMethod_invoke(dyn, static_cast<EV_EditMethodCallData*>(nullptr)));
	TFPASS(!ev_EditMethod_invoke("no.such.method", UT_String("x")));
	TFPASS(!ev_EditMethod_invoke("zzCovInvoke", static_cast<const char*>(nullptr)));
	TFPASS(!ev_EditMethod_invoke("zzCovInvoke",
							   static_cast<const UT_UCS4Char*>(nullptr)));

	emc->removeEditMethod(dyn);
	delete dyn;
}

/* ------------------------------------------------------------------ */
/* EV_EditBindingMap                                                   */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("EV_EditBinding")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);
	EV_EditBindingMap * sub = new EV_EditBindingMap(&emc);

	EV_EditBinding bmethod(emc.findEditMethodByName("alphaCmd"));
	TFPASS(bmethod.getType() == EV_EBT_METHOD);
	TFPASS(bmethod.getMethod() == &s_methods[0]);

	EV_EditBinding bprefix(sub);
	TFPASS(bprefix.getType() == EV_EBT_PREFIX);
	TFPASS(bprefix.getMap() == sub);

	delete sub;
}

TFTEST_MAIN("EV_EditBindingMap key/nvk bindings")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);

	/* nothing allocated yet: lookups miss cleanly */
	TFPASS(map.findEditBinding(EV_EKP_PRESS | 'x') == nullptr);
	TFPASS(map.findEditBinding(EV_EKP_PRESS | EV_NVK_DELETE) == nullptr);
	TFPASS(map.findEditBinding(tf_mouse_bits(EV_EMB_BUTTON1,
											EV_EMO_SINGLECLICK,
											EV_EMC_TEXT)) == nullptr);

	/* plain char binding */
	EV_EditBinding * b =
		tf_bind_char(&map, 'x', EV_EMS_CONTROL, "alphaCmd");
	TFPASS(b && b->getType() == EV_EBT_METHOD);
	TFPASS(b->getMethod() == &s_methods[0]);

	/* shift is folded away for regular chars (NoShift modifier index) */
	EV_EditBinding * bs =
		map.findEditBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL | EV_EMS_SHIFT);
	TFPASS(bs == b);

	/* a different modifier slot is unbound */
	TFPASS(map.findEditBinding(EV_EKP_PRESS | 'x' | EV_EMS_ALT) == nullptr);

	/* setBinding replaces an existing char binding (no failure) */
	TFPASS(map.setBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL, "gammaCmd"));
	b = map.findEditBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL);
	TFPASS(b && b->getMethod() == &s_methods[2]);

	/* NVK binding; NVK tables keep the shift bit */
	EV_EditBits del = EV_EKP_PRESS | EV_NVK_DELETE | EV_EMS_CONTROL;
	TFPASS(map.setBinding(del, "betaCmd"));
	b = map.findEditBinding(del);
	TFPASS(b && b->getMethod() == &s_methods[1]);
	TFPASS(map.findEditBinding(EV_EKP_PRESS | EV_NVK_DELETE) == nullptr);

	/* re-binding an occupied NVK slot fails */
	TFPASS(!map.setBinding(del, "alphaCmd"));

	/* "NULL" on an occupied slot fails the same way -- removeBinding()
	 * is the real way to clear a slot */
	TFPASS(!map.setBinding(del, "NULL"));
	b = map.findEditBinding(del);
	TFPASS(b && b->getMethod() == &s_methods[1]);
	TFPASS(map.removeBinding(del));
	TFPASS(map.findEditBinding(del) == nullptr);

	/* "NULL" on an empty slot stores nullptr -- a no-op success */
	TFPASS(map.setBinding(del, "NULL"));
	TFPASS(map.findEditBinding(del) == nullptr);

	/* unknown method name -> binding fails */
	TFPASS(!map.setBinding(EV_EKP_PRESS | 'q', "no.such.method"));

	/* fullwidth char range remaps onto the latin1 slots */
	EV_EditBits wide = EV_EKP_PRESS | (65280 + 'z');
	TFPASS(map.setBinding(wide, "alphaCmd"));
	b = map.findEditBinding(wide);
	TFPASS(b && b->getMethod() == &s_methods[0]);

	/* removeBinding */
	TFPASS(map.removeBinding(wide));
	TFPASS(map.findEditBinding(wide) == nullptr);
	TFPASS(map.removeBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL));
	TFPASS(map.findEditBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL) == nullptr);

	/* removing from an unallocated table type is a clean miss */
	{
		EV_EditBindingMap fresh(&emc);
		TFPASS(!fresh.removeBinding(EV_EKP_PRESS | 'a'));
		TFPASS(!fresh.removeBinding(EV_EKP_PRESS | EV_NVK_F1));
		TFPASS(!fresh.removeBinding(tf_mouse_bits(EV_EMB_BUTTON1,
												EV_EMO_SINGLECLICK,
												EV_EMC_TEXT)));
	}
}

TFTEST_MAIN("EV_EditBindingMap mouse bindings")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);

	EV_EditBits click = tf_mouse_bits(EV_EMB_BUTTON1, EV_EMO_SINGLECLICK,
									  EV_EMC_TEXT);
	TFPASS(map.setBinding(click, "alphaCmd"));
	EV_EditBinding * b = map.findEditBinding(click);
	TFPASS(b && b->getType() == EV_EBT_METHOD);
	TFPASS(b->getMethod() == &s_methods[0]);

	/* occupied mouse slot -> fail */
	TFPASS(!map.setBinding(click, "gammaCmd"));

	/* different context -> miss */
	TFPASS(map.findEditBinding(tf_mouse_bits(EV_EMB_BUTTON1,
											EV_EMO_SINGLECLICK,
											EV_EMC_IMAGE)) == nullptr);
	/* different op -> miss */
	TFPASS(map.findEditBinding(tf_mouse_bits(EV_EMB_BUTTON1,
											EV_EMO_DOUBLECLICK,
											EV_EMC_TEXT)) == nullptr);
	/* button with no table -> miss */
	TFPASS(map.findEditBinding(tf_mouse_bits(EV_EMB_BUTTON3,
											EV_EMO_SINGLECLICK,
											EV_EMC_TEXT)) == nullptr);

	/* wheel-scroll quirk: a button-2 lookup immediately after a
	 * button-4 lookup is remapped to the button-4 table */
	EV_EditBits wheel = tf_mouse_bits(EV_EMB_BUTTON4, EV_EMO_SINGLECLICK,
									  EV_EMC_TEXT);
	TFPASS(map.setBinding(wheel, "gammaCmd"));
	map.findEditBinding(wheel); /* sets m_iLastMouseNo = 4 */
	EV_EditBits mid = tf_mouse_bits(EV_EMB_BUTTON2, EV_EMO_SINGLECLICK,
									EV_EMC_TEXT);
	b = map.findEditBinding(mid);
	TFPASS(b && b->getMethod() == &s_methods[2]);

	/* removeBinding on a populated mouse table */
	TFPASS(map.removeBinding(click));
	TFPASS(map.findEditBinding(click) == nullptr);
}

TFTEST_MAIN("EV_EditBindingMap prefix bindings")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap * sub = new EV_EditBindingMap(&emc);
	EV_EditBindingMap map(&emc);

	TFPASS(sub->setBinding(EV_EKP_PRESS | 'a', "alphaCmd"));

	EV_EditBits cx = EV_EKP_PRESS | 'x' | EV_EMS_CONTROL;
	TFPASS(map.setBinding(cx, new EV_EditBinding(sub)));
	EV_EditBinding * b = map.findEditBinding(cx);
	TFPASS(b && b->getType() == EV_EBT_PREFIX);
	TFPASS(b->getMap() == sub);

	/* an occupied NVK slot rejects a second prefix binding (char
	 * slots replace instead) */
	TFPASS(map.setBinding(EV_EKP_PRESS | EV_NVK_HOME, new EV_EditBinding(sub)));
	EV_EditBindingMap * rejected = new EV_EditBindingMap(&emc);
	TFPASS(!map.setBinding(EV_EKP_PRESS | EV_NVK_HOME,
						   new EV_EditBinding(rejected)));
	delete rejected;

	delete sub;
}

TFTEST_MAIN("EV_EditBindingMap findEditBits/getAll/getShortcutFor")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);

	TFPASS(map.setBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL, "alphaCmd"));
	TFPASS(map.setBinding(EV_EKP_PRESS | 'A', "alphaCmd"));
	TFPASS(map.setBinding(EV_EKP_PRESS | EV_NVK_F1, "betaCmd"));
	TFPASS(map.setBinding(tf_mouse_bits(EV_EMB_BUTTON1, EV_EMO_SINGLECLICK,
										EV_EMC_TEXT), "betaCmd"));

	/* getShortcutFor: char binding wins; 'x' uppercases in the label */
	const char * sc = map.getShortcutFor(&s_methods[0]);
	TFPASS(sc && strstr(sc, "Ctrl+") == sc);

	/* uppercase bound with no shift shows as Shift+ */
	{
		EV_EditBindingMap m2(&emc);
		TFPASS(m2.setBinding(EV_EKP_PRESS | 'A', "alphaCmd"));
		const char * s2 = m2.getShortcutFor(&s_methods[0]);
		TFPASS(s2 && strstr(s2, "Shift+") == s2);
	}

	/* NVK fallback: F1 is translated; F5 is not in the switch.  Note
	 * getShortcutFor() bails out early when the char table does not
	 * exist at all, so a dummy char binding is needed first. */
	{
		EV_EditBindingMap m2(&emc);
		TFPASS(m2.setBinding(EV_EKP_PRESS | 'a', "alphaCmd"));
		TFPASS(m2.setBinding(EV_EKP_PRESS | EV_NVK_F1, "betaCmd"));
		const char * s2 = m2.getShortcutFor(&s_methods[1]);
		TFPASS(s2 && strstr(s2, "F1"));

		EV_EditBindingMap m3(&emc);
		TFPASS(m3.setBinding(EV_EKP_PRESS | 'a', "alphaCmd"));
		TFPASS(m3.setBinding(EV_EKP_PRESS | EV_NVK_F5, "betaCmd"));
		const char * s3 = m3.getShortcutFor(&s_methods[1]);
		TFPASS(s3 && strstr(s3, "unmapped"));

		/* and with no char table at all the call returns nullptr */
		EV_EditBindingMap m4(&emc);
		TFPASS(m4.setBinding(EV_EKP_PRESS | EV_NVK_F1, "betaCmd"));
		TFPASS(m4.getShortcutFor(&s_methods[1]) == nullptr);
	}

	/* no binding at all -> nullptr */
	{
		EV_EditBindingMap m4(&emc);
		TFPASS(m4.getShortcutFor(&s_methods[0]) == nullptr);
	}

	/* findEditBits collects every slot bound to a method */
	std::vector<EV_EditBits> bits;
	map.findEditBits("betaCmd", bits);
	TFPASS(bits.size() == 2); /* F1 + the mouse click */
	bits.clear();
	map.findEditBits("no.such.method", bits);
	TFPASS(bits.empty());
	map.findEditBits(nullptr, bits);

	/* getAll reports every method binding keyed by edit-bits */
	std::map<EV_EditBits, const char*> all;
	map.getAll(all);
	TFPASS(all.size() == 4);
	TFPASS(all.count(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL) == 1);
	/* NVK keys in getAll() carry the raw index + NAMEDKEY flag, not
	 * the EKP_PRESS bit used for lookup */
	TFPASS(all.count(EV_NVK_F1) == 1);
	TFPASS(all.count(tf_mouse_bits(EV_EMB_BUTTON1, EV_EMO_SINGLECLICK,
								   EV_EMC_TEXT)) == 1);

	/* parseEditBinding is a stub that always fails */
	TFPASS(!map.parseEditBinding());
}

TFTEST_MAIN("EV_EditBindingMap resetAll")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);

	/* resetAll() touches every table unconditionally -- allocate all
	 * six mouse tables + the nvk and char tables first */
	EV_EditMouseButton btns[6] = {
		EV_EMB_BUTTON0, EV_EMB_BUTTON1, EV_EMB_BUTTON2,
		EV_EMB_BUTTON3, EV_EMB_BUTTON4, EV_EMB_BUTTON5
	};
	for (int i = 0; i < 6; i++)
		TFPASS(map.setBinding(tf_mouse_bits(btns[i], EV_EMO_RELEASE,
											EV_EMC_TEXT), "alphaCmd"));
	TFPASS(map.setBinding(EV_EKP_PRESS | 'a', "alphaCmd"));
	TFPASS(map.setBinding(EV_EKP_PRESS | EV_NVK_HOME, "gammaCmd"));

	map.resetAll();
	TFPASS(map.findEditBinding(EV_EKP_PRESS | 'a') == nullptr);
	TFPASS(map.findEditBinding(EV_EKP_PRESS | EV_NVK_HOME) == nullptr);
	TFPASS(map.findEditBinding(tf_mouse_bits(EV_EMB_BUTTON1,
											EV_EMO_RELEASE,
											EV_EMC_TEXT)) == nullptr);
}

/* ------------------------------------------------------------------ */
/* EV_EditEventMapper                                                  */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("EV_EditEventMapper")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);
	EV_EditBindingMap * sub = new EV_EditBindingMap(&emc);
	EV_EditBindingMap * sub2 = new EV_EditBindingMap(&emc);

	TFPASS(map.setBinding(EV_EKP_PRESS | 'a', "alphaCmd"));
	TFPASS(map.setBinding(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL,
						  new EV_EditBinding(sub)));
	TFPASS(sub->setBinding(EV_EKP_PRESS | 's', "betaCmd"));
	TFPASS(sub->setBinding(EV_EKP_PRESS | 't' | EV_EMS_CONTROL,
						   new EV_EditBinding(sub2)));
	TFPASS(sub2->setBinding(EV_EKP_PRESS | 'u', "gammaCmd"));
	TFPASS(map.setBinding(tf_mouse_bits(EV_EMB_BUTTON1, EV_EMO_SINGLECLICK,
										EV_EMC_TEXT), "gammaCmd"));

	EV_EditEventMapper eem(&map);
	EV_EditMethod * m = nullptr;

	/* bogus start: no binding */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'Q', &m) == EV_EEMR_BOGUS_START);

	/* complete: direct method binding */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'a', &m) == EV_EEMR_COMPLETE);
	TFPASS(m == &s_methods[0]);

	/* mouse event resolves the same way */
	TFPASS(eem.Mouse(tf_mouse_bits(EV_EMB_BUTTON1, EV_EMO_SINGLECLICK,
								   EV_EMC_TEXT), &m) == EV_EEMR_COMPLETE);
	TFPASS(m == &s_methods[2]);

	/* prefix -> INCOMPLETE, then a bound continuation -> COMPLETE */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL, &m)
		   == EV_EEMR_INCOMPLETE);
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 's', &m) == EV_EEMR_COMPLETE);
	TFPASS(m == &s_methods[1]);

	/* bogus continuation: prefix then unbound key -> BOGUS_CONT and
	 * the in-progress state is reset */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL, &m)
		   == EV_EEMR_INCOMPLETE);
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'Z', &m) == EV_EEMR_BOGUS_CONT);
	/* state was reset: 'Z' is now a bogus start */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'Z', &m) == EV_EEMR_BOGUS_START);

	/* two-level prefix chain */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL, &m)
		   == EV_EEMR_INCOMPLETE);
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 't' | EV_EMS_CONTROL, &m)
		   == EV_EEMR_INCOMPLETE);
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'u', &m) == EV_EEMR_COMPLETE);
	TFPASS(m == &s_methods[2]);

	/* bogus continuation through Mouse() too */
	TFPASS(eem.Keystroke(EV_EKP_PRESS | 'x' | EV_EMS_CONTROL, &m)
		   == EV_EEMR_INCOMPLETE);
	TFPASS(eem.Mouse(tf_mouse_bits(EV_EMB_BUTTON3, EV_EMO_SINGLECLICK,
								   EV_EMC_TEXT), &m) == EV_EEMR_BOGUS_CONT);
	TFPASS(eem.Mouse(tf_mouse_bits(EV_EMB_BUTTON3, EV_EMO_SINGLECLICK,
								   EV_EMC_TEXT), &m) == EV_EEMR_BOGUS_START);

	/* getShortcutFor forwards to the top-level map */
	TFPASS(eem.getShortcutFor(&s_methods[0]) != nullptr);
	TFPASS(eem.getShortcutFor(&s_methods[2]) == nullptr); /* mouse-only */

	delete sub2;
	delete sub;
}

/* ------------------------------------------------------------------ */
/* EV_Keyboard / EV_Mouse                                              */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("EV_Keyboard")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);
	EV_EditEventMapper eem(&map);
	EV_Keyboard kbd(&eem);

	tf_reset();
	/* null view is rejected outright */
	TFPASS(!kbd.invokeKeyboardMethod(nullptr, &s_methods[0], nullptr, 0));
	TFPASS(s_iCallCount == 0);
	/* null method is rejected */
	TFPASS(!kbd.invokeKeyboardMethod(FAKE_VIEW, nullptr, nullptr, 0));
	/* REQUIREDATA method without data is rejected */
	TFPASS(!kbd.invokeKeyboardMethod(FAKE_VIEW, &s_methods[1], nullptr, 0));
	TFPASS(s_iCallCount == 0);
	/* plain method dispatches */
	TFPASS(kbd.invokeKeyboardMethod(FAKE_VIEW, &s_methods[0], nullptr, 0));
	TFPASS(s_iCallCount == 1);
	TFPASS(s_pLastView == FAKE_VIEW);
	/* REQUIREDATA method with data dispatches and copies the data */
	const UT_UCS4Char data[] = {'K'};
	TFPASS(kbd.invokeKeyboardMethod(FAKE_VIEW, &s_methods[1], data, 1));
	TFPASS(s_lastDataLen == 1 && s_lastData[0] == 'K');
}

namespace {
class TestMouseListener : public EV_MouseListener
{
public:
	void signalMouse(EV_EditBits eb, UT_sint32 xPos, UT_sint32 yPos) override
	{
		count++;
		lastEB = eb;
		lastX = xPos;
		lastY = yPos;
	}
	void removeMouse(EV_Mouse*) override { removed++; }
	int count = 0;
	int removed = 0;
	EV_EditBits lastEB = 0;
	UT_sint32 lastX = 0, lastY = 0;
};
}

TFTEST_MAIN("EV_Mouse")
{
	EV_EditMethodContainer emc(G_N_ELEMENTS(s_methods), s_methods);
	EV_EditBindingMap map(&emc);
	EV_EditEventMapper eem(&map);
	EV_Mouse mouse(&eem);

	mouse.clearMouseContext();
	mouse.setEditEventMap(&eem);

	tf_reset();
	/* REQUIREDATA methods cannot fire from a mouse event */
	TFPASS(!mouse.invokeMouseMethod(FAKE_VIEW, &s_methods[1], 10, 20));
	TFPASS(s_iCallCount == 0);
	/* a normal method fires with the click coords in the call data */
	TFPASS(mouse.invokeMouseMethod(FAKE_VIEW, &s_methods[0], 10, 20));
	TFPASS(s_iCallCount == 1);

	/* signal with no listeners is a no-op */
	mouse.signal(EV_EMO_SINGLECLICK, 1, 2);

	TestMouseListener l1, l2;
	TFPASS(mouse.registerListener(&l1) == 0);
	TFPASS(mouse.registerListener(&l2) == 1);
	TFPASS(mouse.registerListener(nullptr) == -1);

	mouse.signal(EV_EMO_DOUBLECLICK, 3, 4);
	TFPASS(l1.count == 1 && l2.count == 1);
	TFPASS(l1.lastX == 3 && l1.lastY == 4);

	mouse.unregisterListener(0);
	mouse.signal(EV_EMO_DRAG, 5, 6);
	TFPASS(l1.count == 1);
	TFPASS(l2.count == 2);

	mouse.unregisterListener(0); /* idempotent */
	mouse.unregisterListener(-1);
	mouse.unregisterListener(99);

	mouse.removeListeners();
	TFPASS(l1.removed == 0); /* unregistered slot was already null */
	TFPASS(l2.removed == 1);
}

/* ------------------------------------------------------------------ */
/* EV_Menu_Action / EV_Menu_ActionSet                                  */
/* ------------------------------------------------------------------ */

namespace {

static const char * s_dynLabel = "computed-label";
static EV_Menu_ItemState s_menuState = EV_MIS_ZERO;

static Defun_EV_GetMenuItemState_Fn(tf_menu_state)
{
	UT_UNUSED(pAV_View);
	UT_UNUSED(id);
	return s_menuState;
}

static Defun_EV_GetMenuItemComputedLabel_Fn(tf_menu_label)
{
	UT_UNUSED(pLabel);
	UT_UNUSED(id);
	return s_dynLabel;
}

} /* anonymous namespace */

TFTEST_MAIN("EV_Menu_Action")
{
	EV_Menu_Action plain(static_cast<XAP_Menu_Id>(5), false, false, false,
						 false, "doThing", nullptr, nullptr);
	TFPASS(plain.getMenuId() == 5);
	TFPASS(!strcmp(plain.getMethodName(), "doThing"));
	TFPASS(!plain.hasDynamicLabel());
	TFPASS(plain.getDynamicLabel(nullptr) == nullptr);
	TFPASS(!plain.hasGetStateFunction());
	TFPASS(plain.getMenuItemState(nullptr) == EV_MIS_ZERO);
	TFPASS(!plain.raisesDialog());
	TFPASS(!plain.isCheckable());
	TFPASS(!plain.isRadio());
	TFPASS(plain.getScriptName().empty());

	EV_Menu_Action fancy(static_cast<XAP_Menu_Id>(6), false, false, true,
						 false, "toggleThing", tf_menu_state, tf_menu_label,
						 "script1");
	TFPASS(fancy.hasDynamicLabel());
	TFPASS(fancy.getDynamicLabel(nullptr) == s_dynLabel);
	TFPASS(fancy.hasGetStateFunction());
	s_menuState = static_cast<EV_Menu_ItemState>(EV_MIS_Toggled | EV_MIS_Gray);
	TFPASS(fancy.getMenuItemState(FAKE_VIEW) == s_menuState);
	TFPASS(fancy.isCheckable());
	TFPASS(fancy.getScriptName() == "script1");
}

TFTEST_MAIN("EV_Menu_ActionSet")
{
	EV_Menu_ActionSet aset(static_cast<XAP_Menu_Id>(10), static_cast<XAP_Menu_Id>(15));

	TFPASS(aset.getAction(static_cast<XAP_Menu_Id>(10)) == nullptr);
	TFPASS(aset.getAction(static_cast<XAP_Menu_Id>(9)) == nullptr);
	TFPASS(aset.getAction(static_cast<XAP_Menu_Id>(16)) == nullptr);

	TFPASS(aset.setAction(static_cast<XAP_Menu_Id>(10), false, false,
						  false, false, "m10", nullptr, nullptr));
	TFPASS(aset.setAction(static_cast<XAP_Menu_Id>(15), false, true,
						  false, false, "m15", nullptr, nullptr));
	const EV_Menu_Action * a = aset.getAction(static_cast<XAP_Menu_Id>(10));
	TFPASS(a && !strcmp(a->getMethodName(), "m10"));
	TFPASS(aset.getAction(static_cast<XAP_Menu_Id>(15))->raisesDialog());

	/* setAction replaces in place */
	TFPASS(aset.setAction(static_cast<XAP_Menu_Id>(10), false, false,
						  false, false, "m10b", nullptr, nullptr));
	TFPASS(!strcmp(aset.getAction(static_cast<XAP_Menu_Id>(10))->getMethodName(),
				   "m10b"));

	/* out-of-range writes fail */
	TFPASS(!aset.setAction(static_cast<XAP_Menu_Id>(9), false, false,
						   false, false, "x", nullptr, nullptr));
	TFPASS(!aset.setAction(static_cast<XAP_Menu_Id>(16), false, false,
						   false, false, "x", nullptr, nullptr));

	/* addAction inserts by id offset */
	TFPASS(aset.addAction(new EV_Menu_Action(static_cast<XAP_Menu_Id>(16),
											 false, false, false, false,
											 "m16", nullptr, nullptr)));
	TFPASS(!strcmp(aset.getAction(static_cast<XAP_Menu_Id>(16))->getMethodName(),
				   "m16"));

	/* a bogus id below the range is rejected (caller keeps
	 * ownership of the rejected action) */
	EV_Menu_Action * rejected = new EV_Menu_Action(static_cast<XAP_Menu_Id>(5),
												   false, false, false, false,
												   "bad", nullptr, nullptr);
	TFPASS(!aset.addAction(rejected));
	delete rejected;
}

/* ------------------------------------------------------------------ */
/* EV_Menu_Label / EV_Menu_LabelSet                                    */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("EV_Menu_LabelSet")
{
	EV_Menu_LabelSet lset("en-TEST", static_cast<XAP_Menu_Id>(10),
						  static_cast<XAP_Menu_Id>(12));
	TFPASS(lset.getLanguage() == "en-TEST");
	TFPASS(lset.getFirst() == 10);

	TFPASS(lset.getLabel(static_cast<XAP_Menu_Id>(9)) == nullptr);
	TFPASS(lset.getLabel(static_cast<XAP_Menu_Id>(13)) == nullptr);

	TFPASS(lset.setLabel(static_cast<XAP_Menu_Id>(10), "&Open", "Open a file"));
	EV_Menu_Label * l = lset.getLabel(static_cast<XAP_Menu_Id>(10));
	TFPASS(l && l->getMenuId() == 10);
	TFPASS(!strcmp(l->getMenuLabel(), "&Open"));
	TFPASS(!strcmp(l->getMenuStatusMessage(), "Open a file"));

	/* setLabel replaces */
	TFPASS(lset.setLabel(static_cast<XAP_Menu_Id>(10), "&Open2", "msg2"));
	TFPASS(!strcmp(lset.getLabel(static_cast<XAP_Menu_Id>(10))->getMenuLabel(),
				   "&Open2"));

	/* out-of-range writes fail */
	TFPASS(!lset.setLabel(static_cast<XAP_Menu_Id>(9), "x", "x"));
	TFPASS(!lset.setLabel(static_cast<XAP_Menu_Id>(13), "x", "x"));

	/* a missing entry lazily gets the TODO placeholder; the
	 * placeholder is appended to the table (the indexed slot
	 * itself stays empty) */
	l = lset.getLabel(static_cast<XAP_Menu_Id>(11));
	TFPASS(l && !strcmp(l->getMenuLabel(), "TODO"));
	TFPASS(!strcmp(l->getMenuStatusMessage(), "untranslated menu item"));

	lset.setLanguage("fr");
	TFPASS(lset.getLanguage() == "fr");

	/* the deep-copy ctor clones the table, holes and all */
	EV_Menu_LabelSet copy(&lset);
	TFPASS(copy.getLanguage() == "fr");
	TFPASS(copy.getFirst() == 10);
	TFPASS(!strcmp(copy.getLabel(static_cast<XAP_Menu_Id>(10))->getMenuLabel(),
				   "&Open2"));
	TFPASS(!strcmp(copy.getLabel(static_cast<XAP_Menu_Id>(11))->getMenuLabel(),
				   "TODO"));
}

TFTEST_MAIN("EV_Menu_LabelSet addLabel")
{
	/* addLabel() appends sequentially-addressable entries */
	EV_Menu_LabelSet lset("en", static_cast<XAP_Menu_Id>(20),
						  static_cast<XAP_Menu_Id>(22));
	TFPASS(lset.setLabel(static_cast<XAP_Menu_Id>(20), "a", "a"));
	TFPASS(lset.setLabel(static_cast<XAP_Menu_Id>(21), "b", "b"));
	TFPASS(lset.setLabel(static_cast<XAP_Menu_Id>(22), "c", "c"));

	TFPASS(lset.addLabel(new EV_Menu_Label(static_cast<XAP_Menu_Id>(23),
										 "d", "d")));
	TFPASS(!strcmp(lset.getLabel(static_cast<XAP_Menu_Id>(23))->getMenuLabel(),
				   "d"));

	/* the BOGUS2 compat case: an id one below the next sequential
	 * slot pops the last entry before appending */
	{
		EV_Menu_LabelSet l2("en", static_cast<XAP_Menu_Id>(30),
							static_cast<XAP_Menu_Id>(32));
		TFPASS(l2.setLabel(static_cast<XAP_Menu_Id>(30), "a", "a"));
		TFPASS(l2.setLabel(static_cast<XAP_Menu_Id>(31), "b", "b"));
		TFPASS(l2.setLabel(static_cast<XAP_Menu_Id>(32), "c", "c"));
		TFPASS(l2.addLabel(new EV_Menu_Label(static_cast<XAP_Menu_Id>(32),
										  "bogus2", "x")));
		TFPASS(!strcmp(l2.getLabel(static_cast<XAP_Menu_Id>(32))->getMenuLabel(),
					   "bogus2"));
	}
}

/* ------------------------------------------------------------------ */
/* EV_Menu_Layout                                                      */
/* ------------------------------------------------------------------ */

TFTEST_MAIN("EV_Menu_Layout")
{
	EV_Menu_Layout lay("TestMenu", 4);
	TFPASS(lay.getName() == "TestMenu");
	TFPASS(lay.getLayoutItemCount() == 4);
	TFPASS(lay.size() == 4);

	TFPASS(lay.getLayoutItem(0) == nullptr);
	TFPASS(lay.getLayoutItem(4) == nullptr);

	TFPASS(lay.setLayoutItem(0, static_cast<XAP_Menu_Id>(10), EV_MLF_Normal));
	TFPASS(lay.setLayoutItem(1, static_cast<XAP_Menu_Id>(11), EV_MLF_Separator));
	TFPASS(lay.setLayoutItem(2, static_cast<XAP_Menu_Id>(12),
							 EV_MLF_BeginSubMenu));
	TFPASS(lay.setLayoutItem(3, static_cast<XAP_Menu_Id>(0), EV_MLF_EndSubMenu));
	TFPASS(!lay.setLayoutItem(4, static_cast<XAP_Menu_Id>(13), EV_MLF_Normal));

	EV_Menu_LayoutItem * it = lay.getLayoutItem(1);
	TFPASS(it && it->getMenuId() == 11);
	TFPASS(it->getMenuLayoutFlags() == EV_MLF_Separator);

	TFPASS(lay.getLayoutIndex(static_cast<XAP_Menu_Id>(12)) == 2);
	/* not found returns 0 */
	TFPASS(lay.getLayoutIndex(static_cast<XAP_Menu_Id>(99)) == 0);

	/* setLayoutItem frees the previous item */
	TFPASS(lay.setLayoutItem(1, static_cast<XAP_Menu_Id>(14), EV_MLF_Normal));
	TFPASS(lay.getLayoutItem(1)->getMenuId() == 14);

	/* direct item ctor */
	EV_Menu_LayoutItem item(static_cast<XAP_Menu_Id>(77), EV_MLF_BeginPopupMenu);
	TFPASS(item.getMenuId() == 77);
	TFPASS(item.getMenuLayoutFlags() == EV_MLF_BeginPopupMenu);
}

/* ------------------------------------------------------------------ */
/* EV_Toolbar_* tables                                                 */
/* ------------------------------------------------------------------ */

namespace {

static const char * s_tbState = nullptr;
static Defun_EV_GetToolbarItemState_Fn(tf_tb_state)
{
	UT_UNUSED(pAV_View);
	UT_UNUSED(id);
	if (pszState)
		*pszState = s_tbState;
	return static_cast<EV_Toolbar_ItemState>(EV_TIS_Toggled | EV_TIS_Gray);
}

} /* anonymous namespace */

TFTEST_MAIN("EV_Toolbar_Action")
{
	EV_Toolbar_Action a(static_cast<XAP_Toolbar_Id>(3), EV_TBIT_ToggleButton,
						"tbCmd", 0x1, tf_tb_state);
	TFPASS(a.getToolbarId() == 3);
	TFPASS(a.getItemType() == EV_TBIT_ToggleButton);
	TFPASS(!strcmp(a.getMethodName(), "tbCmd"));
	TFPASS(a.getChangeMaskOfInterest() == 0x1);

	const char * st = nullptr;
	EV_Toolbar_ItemState tis = a.getToolbarItemState(FAKE_VIEW, &st);
	TFPASS(EV_TIS_ShouldBeToggled(tis));
	TFPASS(EV_TIS_ShouldBeGray(tis));
	TFPASS(!EV_TIS_ShouldBeHidden(tis));
	TFPASS(!EV_TIS_ShouldUseString(tis));

	/* no state function -> EV_TIS_ZERO */
	EV_Toolbar_Action b(static_cast<XAP_Toolbar_Id>(4), EV_TBIT_PushButton,
						"tb2", 0, nullptr);
	TFPASS(b.getToolbarItemState(FAKE_VIEW, &st) == EV_TIS_ZERO);
}

TFTEST_MAIN("EV_Toolbar_ActionSet")
{
	EV_Toolbar_ActionSet tset(static_cast<XAP_Toolbar_Id>(5), static_cast<XAP_Toolbar_Id>(8));

	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(5)) == nullptr);
	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(4)) == nullptr);
	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(9)) == nullptr);

	TFPASS(tset.setAction(static_cast<XAP_Toolbar_Id>(5), EV_TBIT_PushButton,
						  "b5", 0, nullptr));
	TFPASS(tset.setAction(static_cast<XAP_Toolbar_Id>(8), EV_TBIT_ComboBox,
						  "b8", 0, tf_tb_state));
	EV_Toolbar_Action * a = tset.getAction(static_cast<XAP_Toolbar_Id>(5));
	TFPASS(a && a->getItemType() == EV_TBIT_PushButton);
	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(8))->getItemType()
		   == EV_TBIT_ComboBox);

	/* replace in place */
	TFPASS(tset.setAction(static_cast<XAP_Toolbar_Id>(5), EV_TBIT_MenuButton,
						  "b5x", 0, nullptr));
	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(5))->getItemType()
		   == EV_TBIT_MenuButton);

	/* bounds */
	TFPASS(!tset.setAction(static_cast<XAP_Toolbar_Id>(4), EV_TBIT_PushButton,
						   "x", 0, nullptr));
	TFPASS(!tset.setAction(static_cast<XAP_Toolbar_Id>(9), EV_TBIT_PushButton,
						   "x", 0, nullptr));
	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(4)) == nullptr);
	TFPASS(tset.getAction(static_cast<XAP_Toolbar_Id>(9)) == nullptr);
}

TFTEST_MAIN("EV_Toolbar_LabelSet")
{
	EV_Toolbar_LabelSet tbs("en-TEST", static_cast<XAP_Toolbar_Id>(20),
							static_cast<XAP_Toolbar_Id>(22));
	TFPASS(!strcmp(tbs.getLanguage(), "en-TEST"));

	TFPASS(tbs.getLabel(static_cast<XAP_Toolbar_Id>(19)) == nullptr);
	TFPASS(tbs.getLabel(static_cast<XAP_Toolbar_Id>(23)) == nullptr);

	TFPASS(tbs.setLabel(static_cast<XAP_Toolbar_Id>(20), "Bold", "bold-icon",
						"Make bold", "Toggles bold"));
	EV_Toolbar_Label * l = tbs.getLabel(static_cast<XAP_Toolbar_Id>(20));
	TFPASS(l && l->getToolbarId() == 20);
	TFPASS(!strcmp(l->getToolbarLabel(), "Bold"));
	TFPASS(!strcmp(l->getIconName(), "bold-icon"));
	TFPASS(!strcmp(l->getToolTip(), "Make bold"));
	TFPASS(!strcmp(l->getStatusMsg(), "Toggles bold"));

	/* replace in place */
	TFPASS(tbs.setLabel(static_cast<XAP_Toolbar_Id>(20), "B", "b", "t", "s"));
	TFPASS(!strcmp(tbs.getLabel(static_cast<XAP_Toolbar_Id>(20))->getToolbarLabel(),
				   "B"));

	TFPASS(!tbs.setLabel(static_cast<XAP_Toolbar_Id>(19), "x", "x", "x", "x"));
	TFPASS(!tbs.setLabel(static_cast<XAP_Toolbar_Id>(23), "x", "x", "x", "x"));
	TFPASS(tbs.getLabel(static_cast<XAP_Toolbar_Id>(21)) == nullptr);

	tbs.setLanguage("de");
	TFPASS(!strcmp(tbs.getLanguage(), "de"));
}

TFTEST_MAIN("EV_Toolbar_Layout")
{
	EV_Toolbar_Layout tl("TestBar", 3);
	TFPASS(!strcmp(tl.getName(), "TestBar"));
	TFPASS(tl.getLayoutItemCount() == 3);

	TFPASS(tl.setLayoutItem(0, static_cast<XAP_Toolbar_Id>(1), EV_TLF_Normal));
	TFPASS(tl.setLayoutItem(1, static_cast<XAP_Toolbar_Id>(2), EV_TLF_Spacer));
	TFPASS(tl.setLayoutItem(2, static_cast<XAP_Toolbar_Id>(3), EV_TLF_Normal));

	EV_Toolbar_LayoutItem * it = tl.getLayoutItem(1);
	TFPASS(it && it->getToolbarId() == 2);
	TFPASS(it->getToolbarLayoutFlags() == EV_TLF_Spacer);

	/* replace in place */
	TFPASS(tl.setLayoutItem(1, static_cast<XAP_Toolbar_Id>(9), EV_TLF_Normal));
	TFPASS(tl.getLayoutItem(1)->getToolbarId() == 9);

	/* copy ctor clones every item */
	EV_Toolbar_Layout copy(&tl);
	TFPASS(!strcmp(copy.getName(), "TestBar"));
	TFPASS(copy.getLayoutItemCount() == 3);
	TFPASS(copy.getLayoutItem(1)->getToolbarId() == 9);
	TFPASS(copy.getLayoutItem(1) != tl.getLayoutItem(1));
}

namespace {
class TestToolbarControl : public EV_Toolbar_Control
{
public:
	TestToolbarControl() : EV_Toolbar_Control(nullptr) {}
	bool populate() override { return true; }
	void addItem(const char * s) { m_vecContents.push_back(s); }
};
}

TFTEST_MAIN("EV_Toolbar_Control")
{
	TestToolbarControl tc;
	TFPASS(tc.getContents() != nullptr);
	TFPASS(tc.getContents()->empty());
	TFPASS(tc.getPixelWidth() == 40);
	TFPASS(tc.getDroppedWidth() == 40);
	TFPASS(tc.getMaxLength() == 0);
	TFPASS(!tc.shouldSort());

	tc.addItem("one");
	tc.addItem("two");
	TFPASS(tc.getContents()->size() == 2);
	TFPASS(!strcmp(tc.getNthItem(0), "one"));
	TFPASS(!strcmp(tc.getNthItem(1), "two"));
}
