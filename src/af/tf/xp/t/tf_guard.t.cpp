/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
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

/*
 * TST01 demo suite for the shared test-harness primitives.  The
 * watchdog legs deliberately hang and crash inside the guard so the
 * classification is proven to fire — a wrapper that only passes on
 * healthy code would be no guard at all.  The enumerator legs pull
 * the app's real action sets, and the widget legs exercise the
 * ui_drive helpers on a synthetic tree.
 */

#include "tf_test.h"
#include "tf_guard.h"
#include "tf_actions.h"
#include "tf_widgets.h"

#include "xap_App.h"
#include "xap_EditMethods.h"
#include "ap_Menu_Id.h"
#include "ap_Toolbar_Id.h"

#include <algorithm>
#include <csignal>
#include <memory>
#include <string>
#include <vector>

#define TFSUITE "core.af.tf.guard"

namespace
{

volatile int s_ran = 0;

void quick_action(void *)
{
	s_ran++;
}

/* the artificial 2s-blocking handler the watchdog must reap */
void blocking_action(void *)
{
	g_usleep(2 * G_USEC_PER_SEC);
}

void faulting_action(void *)
{
	raise(SIGFPE);
}

struct FlagCtx
{
	volatile bool *flag;
};

gboolean set_flag_cb(gpointer d)
{
	*static_cast<FlagCtx *>(d)->flag = true;
	return G_SOURCE_REMOVE;
}

/* an always-ready source at G_PRIORITY_HIGH — while it lives it
 * starves the G_PRIORITY_DEFAULT_IDLE sentinel completely, which is
 * exactly what a wedged main loop looks like to idle_sentinel() */
struct HogCtx
{
	gint64 until;
};

gboolean hog_idle_cb(gpointer d)
{
	HogCtx *h = static_cast<HogCtx *>(d);
	if (g_get_monotonic_time() >= h->until)
		return G_SOURCE_REMOVE;
	return G_SOURCE_CONTINUE;
}

} /* anonymous namespace */

TFTEST_MAIN("watchdog returns OK for an action inside its budget")
{
	tf_guard::install();
	s_ran = 0;
	TFPASS(tf_guard::call(quick_action, nullptr, 1000) ==
		   tf_guard::OK);
	TFPASS(s_ran == 1);
}

TFTEST_MAIN("watchdog reaps an artificial 2s-blocking handler")
{
	tf_guard::install();
	gint64 t0 = g_get_monotonic_time();
	/* 300ms budget against a 2s block — proves the guard actually
	 * fires rather than just passing healthy code */
	TFPASS(tf_guard::call(blocking_action, nullptr, 300) ==
		   tf_guard::HUNG);
	TFPASS(g_get_monotonic_time() - t0 < G_USEC_PER_SEC);
}

TFTEST_MAIN("watchdog classifies a fatal signal as FAULT")
{
	tf_guard::install();
	TFPASS(tf_guard::call(faulting_action, nullptr, 1000) ==
		   tf_guard::FAULT);
}

TFTEST_MAIN("watchdog guards a std::function action")
{
	tf_guard::install();
	int hits = 0;
	TFPASS(tf_guard::call([&hits] { hits += 3; }, 1000) ==
		   tf_guard::OK);
	TFPASS(hits == 3);
}

TFTEST_MAIN("pump dispatches deferred idle work")
{
	volatile bool fired = false;
	FlagCtx fc { &fired };
	g_idle_add(set_flag_cb, &fc);
	tf_guard::pump();
	TFPASS(fired);
}

TFTEST_MAIN("idle sentinel fires on a serviced main loop")
{
	long ms = tf_guard::idle_sentinel(2000);
	TFPASS(ms >= 0);
	TFPASS(ms < 1000);
}

TFTEST_MAIN("idle sentinel times out under a starving source")
{
	HogCtx hog { g_get_monotonic_time() + 800 * 1000 };
	g_idle_add_full(G_PRIORITY_HIGH, hog_idle_cb, &hog, nullptr);
	TFPASS(tf_guard::idle_sentinel(500) < 0);
	/* run the hog out so later sentinels are not starved too */
	tf_guard::pump_for(500);
}

TFTEST_MAIN("responsive_after proves the loop services an action's wake")
{
	tf_guard::install();
	s_ran = 0;
	TFPASS(tf_guard::responsive_after(quick_action, nullptr, 2000));
	TFPASS(s_ran == 1);
	/* the 2s block outruns its 300ms budget — the sentinel can
	 * never be serviced, so the whole check must fail */
	TFPASS(!tf_guard::responsive_after(blocking_action, nullptr, 300));
}

TFTEST_MAIN("menu action enumerator lists the registered set")
{
	XAP_App *app = XAP_App::getApp();
	TFPASS(app != nullptr);
	const EV_Menu_ActionSet *as = app->getMenuActionSet();
	TFPASS(as != nullptr);

	/* the bounds the app built the set with are the BOGUS sentinels */
	TFPASS(as->getFirstId() ==
		   static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS1__));
	TFPASS(as->getLastId() ==
		   static_cast<XAP_Menu_Id>(AP_MENU_ID__BOGUS2__));

	std::vector<const EV_Menu_Action *> acts =
		tf_actions::menu_actions(as);
	TFPASS(acts.size() > 200);

	bool resolves = true;
	UT_uint32 named = 0;
	for (const EV_Menu_Action *a : acts)
	{
		if (as->getAction(a->getMenuId()) != a)
			resolves = false;
		if (a->getMethodName() && *a->getMethodName())
			++named;
	}
	TFPASS(resolves);
	/* most actions name a method; sentinel and separator rows
	 * keep a null one */
	TFPASS(named > 200);

	std::vector<XAP_Menu_Id> ids = tf_actions::menu_action_ids(as);
	TFPASS(ids.size() == acts.size());
	TFPASS(std::find(ids.begin(), ids.end(),
					 AP_MENU_ID_EDIT_COPY) != ids.end());
}

TFTEST_MAIN("toolbar action enumerator lists the registered set")
{
	XAP_App *app = XAP_App::getApp();
	TFPASS(app != nullptr);
	const EV_Toolbar_ActionSet *as = app->getToolbarActionSet();
	TFPASS(as != nullptr);

	TFPASS(as->getFirstId() ==
		   static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID__BOGUS1__));
	TFPASS(as->getLastId() ==
		   static_cast<XAP_Toolbar_Id>(AP_TOOLBAR_ID__BOGUS2__));

	std::vector<const EV_Toolbar_Action *> acts =
		tf_actions::toolbar_actions(as);
	TFPASS(acts.size() > 30);

	bool resolves = true;
	for (const EV_Toolbar_Action *a : acts)
		if (as->getAction(a->getToolbarId()) != a)
			resolves = false;
	TFPASS(resolves);
}

TFTEST_MAIN("edit-method enumerator names the real table")
{
	std::unique_ptr<EV_EditMethodContainer> emc(AP_GetEditMethods());
	TFPASS(emc != nullptr);

	std::vector<std::string> names =
		tf_actions::edit_method_names(emc.get());
	TFPASS(names.size() > 500);
	TFPASS(std::find(names.begin(), names.end(), "fileSave") !=
		   names.end());
	TFPASS(std::find(names.begin(), names.end(), "insertData") !=
		   names.end());
	TFPASS(std::find(names.begin(), names.end(), "undo") !=
		   names.end());
	bool all_named = true;
	for (const std::string &n : names)
		if (n.empty())
			all_named = false;
	TFPASS(all_named);
}

TFTEST_MAIN("widget helpers walk a synthetic tree")
{
	/* widget creation needs the type system, not a display —
	 * gtk_init_check() initializes GTK either way */
	gtk_init_check();

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *b1 = gtk_button_new_with_label("one");
	GtkWidget *b2 = gtk_button_new_with_label("two");
	GtkWidget *b3 = gtk_button_new_with_label("three");
	gtk_box_append(GTK_BOX(inner), b1);
	gtk_box_append(GTK_BOX(inner), b2);
	gtk_box_append(GTK_BOX(box), inner);
	gtk_box_append(GTK_BOX(box), b3);
	gtk_widget_set_name(b3, "tf-target");
	g_object_set_data(G_OBJECT(b2), "abi-em-method",
					  const_cast<char *>("fileSave"));
	g_object_set_data(G_OBJECT(b2), "abi-em-data",
					  const_cast<char *>("payload"));

	int seen = 0;
	tf_widgets::for_each(box, [&seen](GtkWidget *) { ++seen; });
	/* 5 widgets we added, plus whatever internal children
	 * (button labels) GTK keeps below them */
	TFPASS(seen > 5);

	std::vector<GtkWidget *> buttons;
	tf_widgets::find_all(box, [](GtkWidget *c) {
		return GTK_IS_BUTTON(c);
	}, buttons);
	TFPASS(buttons.size() == 3);

	TFPASS(tf_widgets::find_type(box, GTK_TYPE_BUTTON) == b1);
	TFPASS(tf_widgets::find_named(box, "tf-target") == b3);
	TFPASS(tf_widgets::find_named(box, "missing") == nullptr);
	TFPASS(tf_widgets::find_em_row(box, "fileSave") == b2);
	TFPASS(tf_widgets::find_em_row(box, "fileSave", "payload") == b2);
	TFPASS(tf_widgets::find_em_row(box, "fileSave", "other") ==
		   nullptr);
	TFPASS(tf_widgets::find_em_row(box, "bogus") == nullptr);
	TFPASS(tf_widgets::has_em_row(box, "fileSave"));
	TFPASS(!tf_widgets::has_em_row(box, "bogus"));

	g_object_ref_sink(box);
	g_object_unref(box);
}
