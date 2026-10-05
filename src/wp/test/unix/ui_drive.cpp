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

/* ui_drive — COV12: scripted-interaction coverage driver.  Where
 * dialog-smoke only constructs + dismisses each dialog, this driver
 * walks the widget tree and exercises it: flips notebook/stack pages,
 * opens every popover and menu button, selects list/dropdown rows,
 * toggles switches, fills entries and activates buttons so the
 * on_* / apply handlers run for real.  It also drives the main
 * window (ribbon tabs, popover menus, side panes, rulers, statusbar)
 * and the AbiWidget public API.
 *
 *   ui-drive --list          print the registered dialog ids
 *   ui-drive --id N          construct dialog N, drive it, dismiss it
 *   ui-drive --frame         drive a fresh main window end to end
 *   ui-drive --abi           exercise the AbiWidget embeddable API
 *
 * Exit codes: 0 ok, 1 failure, 77 no display / interactive prerequisite.
 * drvwrap.sh runs one process per dialog under `timeout` so a hang or
 * segfault is attributed to a single dialog id; gcov counters are
 * dumped at checkpoints so even a killed process keeps its coverage.
 *
 * Buttons are activated for real, so the wrapper stages a scratch
 * document copy under $TMPDIR before running.
 */

#include <gtk/gtk.h>
#include <gsf/gsf.h>

#include <csignal>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "config.h"
#include "ut_types.h"
#include "ie_types.h"
#include "xap_App.h"
#include "ap_UnixApp.h"
#include "xap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"
#include "xap_DialogFactory.h"
#include "xap_Dialog_Id.h"
#include "ap_Dialog_Id.h"
#include "ap_Menu_Id.h"
#include "xap_UnixFrameImpl.h"
#include "ev_UnixMenu.h"
#include "ev_EditMethod.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "abiwidget.h"
#include "ap_Dialog_Modeless.h"
#include "ap_Dialog_Replace.h"
#include "ap_Dialog_InsertBookmark.h"
#include "ap_Dialog_InsertHyperlink.h"
#include "ap_Dialog_GetStringCommon.h"
#include "ap_Dialog_ListRevisions.h"
#include "ap_Dialog_MarkRevisions.h"
#include "xap_Dlg_HTMLOptions.h"

extern "C" void __gcov_dump(void); /* checkpoint coverage counters */

static int g_criticals = 0;
static int g_warnings = 0;

static GLogWriterOutput drive_log_writer(GLogLevelFlags level,
										 const GLogField *fields,
										 gsize n_fields, gpointer)
{
	if (level & G_LOG_LEVEL_CRITICAL)
		g_criticals++;
	else if (level & G_LOG_LEVEL_WARNING)
		g_warnings++;
	return g_log_writer_standard_streams(level, fields, n_fields, nullptr);
}

/* per-interaction guard:
 * - inside a guarded interact, ANY of our signals longjmps back so the
 *   faulting widget is skipped and the drive continues — the rescue
 *   budget is small because a jump out of mid-emission GTK code can
 *   abandon locks, and a jump out of a frame-building app call leaves
 *   half-built objects in global lists.
 * - outside the guard (or budget spent): keep counters and die. */
static sigjmp_buf g_jmp;
static sigjmp_buf g_sweep_jmp;
static volatile sig_atomic_t g_in_interact = 0;
static volatile sig_atomic_t g_sweep_depth = 0;
static volatile sig_atomic_t g_rescued = 0;
static int g_wedged_widgets = 0;
static int g_crashed_widgets = 0;

static void drive_fatal(int sig)
{
	/* a fault inside a toplevel scan is not the driven widget's
	 * fault: GTK's toplevel model can still hand back a window that
	 * was finalized moments ago (observed: ClipArt's nested message
	 * dialog destroyed inside abiRunModalDialog's own modal loop —
	 * g_list_model_get_item refs the dead object and any deref is
	 * UAF).  Rescue and skip the scan. */
	if (g_sweep_depth > 0 && g_rescued < 8) {
		g_rescued++;
		alarm(0);
		siglongjmp(g_sweep_jmp, 1);
	}
	if (g_in_interact && g_rescued < 8) {
		g_rescued++;
		g_in_interact = 0;
		alarm(0);
		/* best-effort fault stack — async-unsafe but only diagnostic;
		 * if it wedges the watchdog covers us */
		void *bt[32];
		int n = backtrace(bt, 32);
		backtrace_symbols_fd(bt, n, STDERR_FILENO);
		siglongjmp(g_jmp, sig == SIGALRM ? 2 : 1);
	}
	static const char msg[] = "drive: fatal signal, counters kept\n";
	(void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
	__gcov_dump();
	_exit(sig == SIGSEGV ? 139 : sig == SIGABRT ? 134 : 124);
}

/* pump the default main context until no work is pending.
 * NOTE: do NOT call __gcov_dump() here or mid-drive — in this libgcov
 * each object is written on the first dump and then unlinked, so an
 * early dump permanently discards the run's real coverage.  The only
 * safe dump points are the fatal-signal path and the end-of-leg
 * returns. */
static void pump(void)
{
	/* bound the drain two ways: a self-reposting source (idle that
	 * reschedules itself, a chatty dconf watch, an animation's frame
	 * clock) keeps pending() true forever, and on a live display each
	 * iteration can cost real layout/draw work — cap both iterations
	 * and wall time so the per-widget settle cost stays bounded */
	gint64 deadline = g_get_monotonic_time() + 40000;
	for (int i = 0; i < 300 && g_main_context_pending(nullptr); i++) {
		g_main_context_iteration(nullptr, FALSE);
		if (g_get_monotonic_time() >= deadline)
			break;
	}
}

/* pump for at most `ms` so async map/show handlers get a slice */
static void pump_for(guint ms)
{
	GMainContext *ctx = g_main_context_default();
	gint64 deadline = g_get_monotonic_time() + ms * 1000;
	while (g_get_monotonic_time() < deadline) {
		if (!g_main_context_iteration(ctx, FALSE))
			g_usleep(2000);
	}
}

static std::vector<GtkWidget *> toplevels(void)
{
	std::vector<GtkWidget *> out;
	GListModel *tl = gtk_window_get_toplevels();
	guint n = g_list_model_get_n_items(tl);
	for (guint i = 0; i < n; i++) {
		GtkWidget *w = GTK_WIDGET(g_list_model_get_item(tl, i));
		out.push_back(w);
		g_object_unref(w);
	}
	return out;
}

static bool contains(const std::vector<GtkWidget *> &v, GtkWidget *w)
{
	for (GtkWidget *x : v)
		if (x == w)
			return true;
	return false;
}

/* attach *out (weak ref) to the newest stray toplevel that is still
 * alive, or nullptr.  The toplevel model can keep listing a window
 * that is already mid-dispose — gtk_widget_in_destruction() is the
 * liveness probe there.  Such zombies cannot be driven, but their
 * abiRunModalDialog loop is still up (observed: ClipArt's nested
 * error message destroyed by GTK's own ::response handling); answer
 * them blind so the stuck loop unwinds, then keep scanning older
 * strays for a drivable one.
 *
 * GLib ≥ 2.80 no longer writes the location at add time — the caller
 * owns the pointer and the weak ref only clears it on finalize —
 * so *out must be assigned BEFORE g_object_add_weak_pointer. */
static bool g_trace = false;

static GtkWidget *attach_live_stray(
	const std::vector<GtkWidget *> &preexisting, GtkWidget **out)
{
	*out = nullptr;
	GListModel *tl = gtk_window_get_toplevels();
	for (guint i = g_list_model_get_n_items(tl); i > 0; i--) {
		GtkWidget *w = GTK_WIDGET(g_list_model_get_item(tl, i - 1));
		if (!w)
			continue;
		bool stray = !contains(preexisting, w);
		if (stray && !gtk_widget_in_destruction(w)) {
			*out = w;
			g_object_add_weak_pointer(G_OBJECT(w),
								  reinterpret_cast<gpointer *>(out));
			if (*out) {
				g_object_unref(w);
				break;
			}
		} else if (stray) {
			if (GTK_IS_DIALOG(w))
				g_signal_emit_by_name(w, "response",
									  GTK_RESPONSE_DELETE_EVENT);
			else if (GTK_IS_WINDOW(w))
				gtk_window_close(GTK_WINDOW(w));
		}
		g_object_unref(w);
	}
	return *out;
}

/* weak-ref slot: widget destruction during interaction (button
 * clicks rebuilding rows, popovers lazily attaching, dialogs closing)
 * must not leave a dangling pointer — weak refs null out on dispose. */
struct WeakChild {
	GtkWidget *w;
};

struct DriveCtx {
	GtkWidget *root;                    /* driven toplevel */
	std::vector<GtkWidget *> preexisting; /* toplevels present before */
	int interactions;
	bool rootDead;
	bool suppressSweep;                 /* inside a system modal loop */
	gint64 deadline;                    /* stop walking after this us
										 * (0 = none); the main
										 * window's tree is huge —
										 * under gcov the frame leg
										 * would outgrow its wrapper
										 * timeout without a bound */
};

/* GTK's own system toplevels (print/file dialogs) are not our
 * coverage target; driving them clicks real Print/Open buttons —
 * a real print job or GTK-internal D-Bus reentrancy crash. */
static bool is_system_dialog(GtkWidget *w)
{
	const char *tn = G_OBJECT_TYPE_NAME(w);
	return tn && (!strcmp(tn, "GtkPrintUnixDialog") ||
				  !strcmp(tn, "GtkAppChooserDialog"));
}

/* close every toplevel that is not ctx->root and did not exist before
 * the drive — nested dialogs opened by buttons we activated (file
 * choosers, message boxes, wizards).  Newest first so stacked modals
 * unwind in order.  No-op until root is known so the drive's own
 * dialog isn't mistaken for a stray before the first drive tick. */
static void sweep_strays(DriveCtx &ctx)
{
	if (!ctx.root || ctx.suppressSweep)
		return;
	bool closed = false;
	GListModel *tl = gtk_window_get_toplevels();
	for (guint i = g_list_model_get_n_items(tl); i > 0; i--) {
		GtkWidget *w = GTK_WIDGET(g_list_model_get_item(tl, i - 1));
		if (!w)
			continue;
		if (w != ctx.root && !contains(ctx.preexisting, w)) {
			if (GTK_IS_DIALOG(w))
				g_signal_emit_by_name(w, "response",
									  GTK_RESPONSE_DELETE_EVENT);
			else if (GTK_IS_WINDOW(w))
				gtk_window_close(GTK_WINDOW(w));
			closed = true;
		}
		g_object_unref(w);
	}
	/* only iterate when we actually closed something — every pump
	 * inside a nested modal loop re-dispatches other pending sources
	 * (crashed GTK's sync print D-Bus query when idle) */
	if (closed)
		pump();
}

/* run f under the same fault guard interact() uses for widgets —
 * app API calls in the abiwidget drive can fault inside product
 * code (a rescued jump keeps counters and skips the rest of the
 * section) */
template <typename F>
static bool call_guard(F &&f, const char *what)
{
	switch (sigsetjmp(g_jmp, 1)) {
	case 0:
		break;
	case 2:
		g_in_interact = 0;
		g_wedged_widgets++;
		g_printerr("drive: wedged in %s — section skipped\n", what);
		return false;
	default:
		g_in_interact = 0;
		g_crashed_widgets++;
		g_printerr("drive: crashed in %s — section skipped\n", what);
		return false;
	}
	g_in_interact = 1;
	/* generous: legit calls are slow under gcov (a doc save or a
	 * header/footer relayout can take tens of seconds) — the alarm
	 * is only for a truly wedged call */
	alarm(40);
	f();
	g_in_interact = 0;
	alarm(0);
	return true;
}

/* run f under the toplevel-scan fault guard — see drive_fatal's
 * g_sweep_depth comment for why model items can be stale.  Returns
 * false when a fault aborted the scan (the call may be retried on a
 * later tick). */
template <typename F>
static bool sweep_guard(F &&f)
{
	if (sigsetjmp(g_sweep_jmp, 1) != 0) {
		g_sweep_depth--;
		g_printerr("drive: fault scanning toplevels — scan skipped\n");
		return false;
	}
	g_sweep_depth++;
	f();
	g_sweep_depth--;
	return true;
}

/* repeating stray sweep — also runs inside nested modal loops our
 * activations may have entered, which a one-shot sweep cannot reach */
static gboolean stray_sweep_cb(gpointer data)
{
	DriveCtx &ctx = *static_cast<DriveCtx *>(data);
	sweep_guard([&ctx] {
		/* a system modal loop (print) owns the context — our sweep's
		 * iterations re-dispatch its queued sync sources and crash */
		if (ctx.root && !is_system_dialog(ctx.root))
			sweep_strays(ctx);
	});
	return G_SOURCE_CONTINUE;
}

static std::vector<WeakChild> snapshot_children(GtkWidget *w)
{
	/* two passes: weak pointers are registered on the final vector
	 * storage, so no push_back reallocation can leave them dangling */
	std::vector<GtkWidget *> raw;
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		raw.push_back(c);
	std::vector<WeakChild> out(raw.size());
	for (size_t i = 0; i < raw.size(); i++) {
		out[i].w = raw[i];
		g_object_add_weak_pointer(G_OBJECT(raw[i]),
								  reinterpret_cast<gpointer *>(&out[i].w));
	}
	return out;
}
static void release_snapshot(std::vector<WeakChild> &v)
{
	for (WeakChild &c : v)
		if (c.w)
			g_object_remove_weak_pointer(
				G_OBJECT(c.w), reinterpret_cast<gpointer *>(&c.w));
	v.clear();
}

static void drive_widget(GtkWidget *w, DriveCtx &ctx, int depth);

/* controls whose activation must never happen in the drive:
 * - native portal/GTK dialogs (file choosers, page setup) — they run
 *   outside our widget tree, cannot be swept closed, and block the
 *   drive or the user's real session
 * - the print machinery — a real activation would queue a job on the
 *   user's printer, and it crashed under our nested iterations
 * - File>Quit / Close — they destroy the window being walked
 * Detected via the abi-em-method / abi-menu-action bindings the
 * ribbon and menus stamp on their buttons. */
static bool never_activate(GtkWidget *w)
{
	static const char *const bad_methods[] = {
		/* window/app killers */
		"querySaveAndExit", "fileClose", "fileCloseWindow",
		"closeWindow", "closeWindowX",
		/* frame spawners — new toplevels the drive cannot own; fileNew
		 * also trips a latent statusbar fault once prior interactions
		 * have run, so skip the whole family */
		"fileNew",
		/* native file dialogs */
		"fileOpen", "openRecent", "openTemplate",
		"fileNewUsingTemplate", "fileSaveAs", "fileSaveAsWeb",
		"fileSaveImage", "fileSaveTemplate", "fileSaveEmbed",
		"fileExport", "fileImport", "importStyles",
		"insFile", "insMediaFile", "insScreenshot",
		"insertOnlineImage", "insertClipart", "fileInsertGraphic",
		"fileInsertPageBackgroundGraphic",
		"fileInsertPositionedGraphic", "fileRevert",
		/* printers */
		"print", "printDirectly", "printPreview", "printTB",
		"cairoPrint", "cairoPrintDirectly", "cairoPrintPreview"
	};
	const char *m = static_cast<const char *>(
		g_object_get_data(G_OBJECT(w), "abi-em-method"));
	if (m)
		for (const char *bad : bad_methods)
			if (!strcmp(m, bad))
				return true;
	static const _Ap_Menu_Id bad_ids[] = {
		AP_MENU_ID_FILE_NEW, AP_MENU_ID_FILE_OPEN, AP_MENU_ID_FILE_RECENT,
		AP_MENU_ID_FILE_NEW_USING_TEMPLATE,
		AP_MENU_ID_FILE_SAVEAS, AP_MENU_ID_FILE_SAVE_TEMPLATE,
		AP_MENU_ID_FILE_SAVEIMAGE, AP_MENU_ID_FILE_SAVEEMBED,
		AP_MENU_ID_FILE_EXPORT, AP_MENU_ID_FILE_IMPORT,
		AP_MENU_ID_FILE_IMPORTSTYLES, AP_MENU_ID_FILE_PAGESETUP,
		AP_MENU_ID_FILE_PRINT, AP_MENU_ID_FILE_PRINT_PREVIEW,
		AP_MENU_ID_FILE_PRINT_DIRECTLY, AP_MENU_ID_FILE_REVERT,
		AP_MENU_ID_FILE_EXIT, AP_MENU_ID_FILE_CLOSE,
		AP_MENU_ID_INSERT_FILE, AP_MENU_ID_INSERT_CLIPART,
		AP_MENU_ID_INSERT_PICTURES, AP_MENU_ID_INSERT_SCREENSHOT
	};
	const std::string *act = static_cast<const std::string *>(
		g_object_get_data(G_OBJECT(w), "abi-menu-action"));
	if (act) {
		for (_Ap_Menu_Id id : bad_ids) {
			char name[32];
			snprintf(name, sizeof(name), "item_%u",
					 static_cast<unsigned>(id));
			if (*act == name)
				return true;
		}
	}
	/* GMenuModel popover items carry no abi-menu-action data — their
	 * identity is the action name ("menu.item_<id>") */
	if (GTK_IS_ACTIONABLE(w)) {
		const char *an = gtk_actionable_get_action_name(
			GTK_ACTIONABLE(w));
		const char *p = an ? strstr(an, "item_") : nullptr;
		if (p) {
			unsigned idnum = static_cast<unsigned>(atoi(p + 5));
			for (_Ap_Menu_Id id : bad_ids)
				if (idnum == static_cast<unsigned>(id))
					return true;
		}
	}
	return false;
}

/* interact with a single widget; recursion into children happens in
 * drive_widget */
static void interact(GtkWidget *w, DriveCtx &ctx)
{
	if (++ctx.interactions > 4000 ||
		(ctx.deadline && g_get_monotonic_time() > ctx.deadline))
		return;

	if (g_trace) {
		const char *id = gtk_widget_get_name(w);
		if (GTK_IS_BUTTON(w)) {
			GtkWidget *c = gtk_widget_get_first_child(w);
			const char *lbl = (c && GTK_IS_LABEL(c))
				? gtk_label_get_text(GTK_LABEL(c)) : "?";
			fprintf(stderr, "interact %s name=%s label=%s\n",
					G_OBJECT_TYPE_NAME(w), id ? id : "-", lbl);
		} else
			fprintf(stderr, "interact %s name=%s\n",
					G_OBJECT_TYPE_NAME(w), id ? id : "-");
	}

	/* GTK's own file-chooser innards (dir lists, places sidebar) are
	 * not our coverage target, and row activation opens real files */
	if (GTK_IS_FILE_CHOOSER_WIDGET(w) || GTK_IS_FILE_CHOOSER_DIALOG(w))
		return;
	if (is_system_dialog(w))
		return;

	switch (sigsetjmp(g_jmp, 1)) {
	case 0:
		break;
	case 2: /* alarm rescue — handler wedged */
		g_in_interact = 0;
		g_wedged_widgets++;
		g_printerr("drive: wedged driving %s — widget skipped\n",
				   G_OBJECT_TYPE_NAME(w));
		/* every rescued jump can leave a GLib lock held (observed:
		 * longjmp out of g_signal_handlers_destroy's futex deadlocked
		 * the next signal call) — count faults across kinds and stop
		 * the walk instead of inviting a silent deadlock */
		if (g_crashed_widgets + g_wedged_widgets >= 4)
			ctx.rootDead = true;
		return;
	default: /* 1: real fault — the app crashed mid-handler */
		g_in_interact = 0;
		g_crashed_widgets++;
		g_printerr("drive: crashed driving %s — widget skipped\n",
				   G_OBJECT_TYPE_NAME(w));
		/* several faults in one drive means the dialog's C++ object
		 * is gone and every remaining widget dispatches to freed
		 * handlers — stop walking rather than burn alarm seconds */
		if (g_crashed_widgets + g_wedged_widgets >= 4)
			ctx.rootDead = true;
		return;
	}
	g_in_interact = 1;
	/* generous: legit handlers are slow under gcov (a doc load takes
	 * seconds) — the alarm is only for a truly wedged handler */
	alarm(20);

	if (GTK_IS_MENU_BUTTON(w)) {
		gtk_menu_button_popup(GTK_MENU_BUTTON(w));
		pump();
	} else if (GTK_IS_POPOVER(w)) {
		if (gtk_widget_get_parent(w))
			gtk_popover_popup(GTK_POPOVER(w));
		pump();
	} else if (GTK_IS_NOTEBOOK(w)) {
		GtkNotebook *nb = GTK_NOTEBOOK(w);
		int n = gtk_notebook_get_n_pages(nb);
		for (int i = 0; i < n && i < 24; i++) {
			gtk_notebook_set_current_page(nb, i);
			pump();
		}
		if (n > 0)
			gtk_notebook_set_current_page(nb, 0);
	} else if (GTK_IS_STACK(w)) {
		GtkStack *st = GTK_STACK(w);
		GtkWidget *first = gtk_widget_get_first_child(w);
		for (GtkWidget *c = first; c; c = gtk_widget_get_next_sibling(c)) {
			gtk_stack_set_visible_child(st, c);
			pump();
		}
		if (first)
			gtk_stack_set_visible_child(st, first);
	} else if (GTK_IS_EXPANDER(w)) {
		gtk_expander_set_expanded(GTK_EXPANDER(w), TRUE);
		pump();
	} else if (GTK_IS_DROP_DOWN(w)) {
		GtkDropDown *dd = GTK_DROP_DOWN(w);
		GListModel *m = gtk_drop_down_get_model(dd);
		if (m) {
			guint n = g_list_model_get_n_items(m);
			guint step = n > 4 ? n / 4 : 1;
			/* a ribbon dropdown's selection can trigger a full doc
			 * reflow (style/zoom lists) — a few selections cover the
			 * notify path without burning the whole leg's budget */
			for (guint i = 0, k = 0; i < n && k < 4; i += step, k++) {
				gtk_drop_down_set_selected(dd, i);
				pump();
			}
			gtk_drop_down_set_selected(dd, 0);
			pump();
		}
	} else if (GTK_IS_SPIN_BUTTON(w)) {
		GtkSpinButton *sb = GTK_SPIN_BUTTON(w);
		GtkAdjustment *adj = gtk_spin_button_get_adjustment(sb);
		double lo = gtk_adjustment_get_lower(adj);
		double hi = gtk_adjustment_get_upper(adj);
		gtk_spin_button_set_value(sb, lo + (hi - lo) / 2);
		pump();
		gtk_spin_button_spin(sb, GTK_SPIN_STEP_FORWARD, 1);
		pump();
	} else if (GTK_IS_SWITCH(w)) {
		gtk_switch_set_active(GTK_SWITCH(w), TRUE);
		pump();
		gtk_switch_set_active(GTK_SWITCH(w), FALSE);
		pump();
	} else if (GTK_IS_CHECK_BUTTON(w)) {
		gtk_check_button_set_active(GTK_CHECK_BUTTON(w), TRUE);
		pump();
		gtk_check_button_set_active(GTK_CHECK_BUTTON(w), FALSE);
		pump();
	} else if (GTK_IS_TOGGLE_BUTTON(w)) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
		pump();
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), FALSE);
		pump();
	} else if (GTK_IS_SCALE(w)) {
		GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(w));
		double lo = gtk_adjustment_get_lower(adj);
		double hi = gtk_adjustment_get_upper(adj);
		gtk_range_set_value(GTK_RANGE(w), lo + (hi - lo) / 2);
		pump();
	} else if (GTK_IS_SCROLLBAR(w)) {
		GtkAdjustment *adj = gtk_scrollbar_get_adjustment(GTK_SCROLLBAR(w));
		if (adj) {
			double lo = gtk_adjustment_get_lower(adj);
			double hi = gtk_adjustment_get_upper(adj)
				- gtk_adjustment_get_page_size(adj);
			if (hi > lo)
				gtk_adjustment_set_value(adj, lo + (hi - lo) / 2);
			pump();
		}
	} else if (GTK_IS_COLOR_DIALOG_BUTTON(w)) {
		GdkRGBA rgba {0.2f, 0.4f, 0.8f, 1.0f};
		gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(w),
										 &rgba);
		pump();
	} else if (GTK_IS_LINK_BUTTON(w)) {
		/* activating it would open a browser on the user's live
		 * display — skip */
	} else if (GTK_IS_ENTRY(w) || GTK_IS_TEXT(w)) {
		gtk_editable_set_text(GTK_EDITABLE(w), "Sample");
		pump();
	} else if (GTK_IS_TEXT_VIEW(w)) {
		GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
		gtk_text_buffer_insert_at_cursor(b, "x", 1);
		pump();
	} else if (GTK_IS_LIST_BOX(w)) {
		GtkListBox *lb = GTK_LIST_BOX(w);
		gtk_list_box_unselect_all(lb);
		GtkWidget *row = gtk_widget_get_first_child(w);
		for (int i = 0; row && i < 6; i++) {
			if (GTK_IS_LIST_BOX_ROW(row)) {
				gtk_list_box_select_row(lb, GTK_LIST_BOX_ROW(row));
				pump();
				gtk_widget_activate(row); /* emits row-activated */
				pump();
			}
			row = gtk_widget_get_next_sibling(row);
		}
	} else if (GTK_IS_FLOW_BOX(w)) {
		gtk_flow_box_select_all(GTK_FLOW_BOX(w));
		pump();
		gtk_flow_box_unselect_all(GTK_FLOW_BOX(w));
		pump();
	} else if (GTK_IS_LIST_VIEW(w)) {
		GtkSelectionModel *sel =
			gtk_list_view_get_model(GTK_LIST_VIEW(w));
		if (sel && GTK_IS_SELECTION_MODEL(sel)) {
			guint n = g_list_model_get_n_items(G_LIST_MODEL(sel));
			for (guint i = 0; i < n && i < 8; i++) {
				gtk_selection_model_select_item(sel, i, TRUE);
				pump();
			}
		}
		g_signal_emit_by_name(w, "activate", 0);
		pump();
	} else if (GTK_IS_COLUMN_VIEW(w)) {
		GtkSelectionModel *sel =
			gtk_column_view_get_model(GTK_COLUMN_VIEW(w));
		if (sel && GTK_IS_SELECTION_MODEL(sel)) {
			guint n = g_list_model_get_n_items(G_LIST_MODEL(sel));
			for (guint i = 0; i < n && i < 8; i++) {
				gtk_selection_model_select_item(sel, i, TRUE);
				pump();
			}
		}
		g_signal_emit_by_name(w, "activate", 0);
		pump();
	} else if (GTK_IS_GRID_VIEW(w)) {
		GtkSelectionModel *sel =
			gtk_grid_view_get_model(GTK_GRID_VIEW(w));
		if (sel && GTK_IS_SELECTION_MODEL(sel)) {
			guint n = g_list_model_get_n_items(G_LIST_MODEL(sel));
			for (guint i = 0; i < n && i < 8; i++) {
				gtk_selection_model_select_item(sel, i, TRUE);
				pump();
			}
		}
		g_signal_emit_by_name(w, "activate", 0);
		pump();
	} else if (GTK_IS_BUTTON(w)) {
		/* a real Print/Preview activation sends a job to the user's
		 * printer or launches the system previewer — real side
		 * effects, and the print machinery crashed under our nested
		 * iterations; never click them */
		GtkWidget *c = gtk_widget_get_first_child(w);
		const char *lbl = (c && GTK_IS_LABEL(c))
			? gtk_label_get_text(GTK_LABEL(c)) : nullptr;
		if (lbl && (!strcmp(lbl, "Print") || !strcmp(lbl, "Preview") ||
					!strcmp(lbl, "_Print") || !strcmp(lbl, "_Preview"))) {
			g_in_interact = 0;
			alarm(0);
			return;
		}
		/* controls that kill the walked window, open unreachable
		 * native dialogs, or touch the real printer/session are
		 * never activated — see never_activate */
		if (never_activate(w)) {
			g_in_interact = 0;
			alarm(0);
			return;
		}
		g_signal_emit_by_name(w, "clicked");
		pump();
		sweep_strays(ctx);
	}
	g_in_interact = 0;
	alarm(0);
}

static bool ctx_over_budget(const DriveCtx &ctx)
{
	return ctx.interactions > 4000 ||
		(ctx.deadline && g_get_monotonic_time() > ctx.deadline);
}

static void drive_widget(GtkWidget *w, DriveCtx &ctx, int depth)
{
	if (!w || ctx.rootDead || depth > 14 || ctx_over_budget(ctx))
		return;
	/* never descend into the GTK file chooser's widget internals */
	if (GTK_IS_FILE_CHOOSER_WIDGET(w))
		return;
	if (is_system_dialog(w))
		return;

	/* the widget can die inside its own interaction (e.g. activating
	 * a dialog's OK button) — a weak pointer keeps that detectable */
	GtkWidget *guard = w;
	g_object_add_weak_pointer(G_OBJECT(w),
							  reinterpret_cast<gpointer *>(&guard));

	interact(w, ctx);

	if (guard && !ctx.rootDead) {
		std::vector<WeakChild> kids = snapshot_children(w);
		for (WeakChild &c : kids) {
			if (c.w)
				drive_widget(c.w, ctx, depth + 1);
			if (ctx.rootDead || ctx_over_budget(ctx))
				break;
		}
		release_snapshot(kids);

		if (guard) {
			if (GTK_IS_MENU_BUTTON(w))
				gtk_menu_button_popdown(GTK_MENU_BUTTON(w));
			else if (GTK_IS_POPOVER(w))
				gtk_popover_popdown(GTK_POPOVER(w));
		}
	}

	if (guard)
		g_object_remove_weak_pointer(G_OBJECT(w),
									 reinterpret_cast<gpointer *>(&guard));
}

/* ---------------- dialog drive ---------------- */

static void on_root_destroy(GtkWidget *, gpointer data)
{
	*static_cast<bool *>(data) = true;
}

struct ModalDriveCtx {
	DriveCtx *ctx;
	GtkWidget *dlg;      /* the dialog's toplevel, resolved lazily */
	bool driven;
	bool answered;
	int tries;
};

/* inside runModal's nested loop: find the dialog toplevel, drive it
 * once, then answer OK; later passes fall back to DELETE_EVENT if
 * validation rejected the OK. */
static gboolean modal_drive_cb(gpointer data)
{
	ModalDriveCtx *m = static_cast<ModalDriveCtx *>(data);
	if (!m->driven) {
		/* the dialog toplevel may take a tick to appear — retry */
		m->dlg = nullptr;
		sweep_guard([&m] {
			attach_live_stray(m->ctx->preexisting, &m->dlg);
		});
		if (!m->dlg)
			return ++m->tries >= 40 ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
		m->ctx->root = m->dlg;
		g_signal_connect(m->dlg, "destroy",
						 G_CALLBACK(on_root_destroy), &m->ctx->rootDead);
		m->driven = true;
		if (is_system_dialog(m->dlg)) {
			/* GTK's own modal (print): don't drive its innards and
			 * never emit OK — that submits a real job.  CANCEL
			 * unwinds gtk_print_operation_run on its own; do NOT
			 * pump here — iterating inside its nested loop is what
			 * re-dispatched GTK's sync D-Bus source and crashed */
			m->answered = true;
			m->ctx->suppressSweep = true;
			if (g_trace)
				fprintf(stderr, "system dialog %s — cancel\n",
						G_OBJECT_TYPE_NAME(m->dlg));
			if (GTK_IS_DIALOG(m->dlg))
				g_signal_emit_by_name(m->dlg, "response",
									  GTK_RESPONSE_CANCEL);
			else
				gtk_window_close(GTK_WINDOW(m->dlg));
			return G_SOURCE_REMOVE;
		}
		if (g_trace)
			fprintf(stderr, "driving toplevel %s\n",
					G_OBJECT_TYPE_NAME(m->dlg));
		drive_widget(m->dlg, *m->ctx, 0);
		return G_SOURCE_CONTINUE;
	}
	if (!m->answered) {
		m->answered = true;
		if (m->dlg) {
			if (GTK_IS_DIALOG(m->dlg))
				g_signal_emit_by_name(m->dlg, "response",
									  GTK_RESPONSE_OK);
			else if (GTK_IS_WINDOW(m->dlg))
				gtk_window_close(GTK_WINDOW(m->dlg));
			pump();
		}
		return G_SOURCE_CONTINUE;
	}
	/* still up (validation rejected OK, or a nested stray is on top):
	 * emit DELETE_EVENT on the dialog and any stray toplevels */
	if (m->dlg) {
		if (GTK_IS_DIALOG(m->dlg))
			g_signal_emit_by_name(m->dlg, "response",
								  GTK_RESPONSE_DELETE_EVENT);
		else if (GTK_IS_WINDOW(m->dlg))
			gtk_window_close(GTK_WINDOW(m->dlg));
		pump();
	}
	if (!m->dlg || ++m->tries >= 80)
		return G_SOURCE_REMOVE;
	return G_SOURCE_CONTINUE;
}

static int run_dialog(XAP_DialogFactory *factory, XAP_Frame *frame,
					  XAP_Dialog_Id id, XAP_Dialog_Type type)
{
	g_criticals = 0;
	g_warnings = 0;

	XAP_Dialog *dlg = factory->requestDialog(id);
	if (!dlg) {
		g_print("drive: id %d — factory returned no dialog\n",
				static_cast<int>(id));
		return 1;
	}
	if (g_trace)
		fprintf(stderr, "id %d -> %s\n", static_cast<int>(id),
				typeid(*dlg).name());

	/* same pre-run context the edit methods establish: view/doc
	 * wiring for dialogs that read document state */
	FV_View *view = static_cast<FV_View *>(frame->getCurrentView());
	PD_Document *doc = dynamic_cast<PD_Document *>(frame->getCurrentDoc());
	if (AP_Dialog_Modeless *ml = dynamic_cast<AP_Dialog_Modeless *>(dlg))
		ml->setView(view);
	if (auto *bm = dynamic_cast<AP_Dialog_InsertBookmark *>(dlg))
		bm->setDoc(view);
	else if (auto *hl = dynamic_cast<AP_Dialog_InsertHyperlink *>(dlg))
		hl->setDoc(view);
	else if (auto *gs = dynamic_cast<AP_Dialog_GetStringCommon *>(dlg))
		gs->setDoc(view);
	else if (auto *lr = dynamic_cast<AP_Dialog_ListRevisions *>(dlg))
		lr->setDocument(doc);
	else if (auto *mr = dynamic_cast<AP_Dialog_MarkRevisions *>(dlg))
		mr->setDocument(doc);
	else if (auto *rp = dynamic_cast<AP_Dialog_Replace *>(dlg))
		rp->setView(view);
	else if (auto *ho = dynamic_cast<XAP_Dialog_HTMLOptions *>(dlg)) {
		/* HTML export installs its options struct before runModal */
		static XAP_Exp_HTMLOptions html_opt {};
		XAP_Dialog_HTMLOptions::getHTMLDefaults(&html_opt,
											  XAP_App::getApp());
		ho->setHTMLOptions(&html_opt, XAP_App::getApp());
	}

	/* 40s of widget budget inside the wrapper's 60s leg timeout */
	DriveCtx ctx {nullptr, {}, 0, false, false,
		g_get_monotonic_time() + 40 * G_USEC_PER_SEC};
	sweep_guard([&ctx] { ctx.preexisting = toplevels(); });

	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);

	if (XAP_Dialog_Modeless *ml = dynamic_cast<XAP_Dialog_Modeless *>(dlg)) {
		ml->runModeless(frame);
		pump_for(200);
		GtkWidget *w = nullptr;
		sweep_guard([&] {
			attach_live_stray(ctx.preexisting, &w);
		});
		if (w) {
			ctx.root = w;
			/* modeless dialogs can self-destroy on a driven button
			 * (Goto's Jump) — stop the walk when the window goes */
			g_signal_connect(w, "destroy",
							 G_CALLBACK(on_root_destroy), &ctx.rootDead);
			if (g_trace)
				fprintf(stderr, "modeless toplevel %s\n",
						G_OBJECT_TYPE_NAME(w));
			drive_widget(w, ctx, 0);
		}
		sweep_guard([&ctx] { sweep_strays(ctx); });
		if (g_trace)
			fprintf(stderr, "post-drive: close modeless\n");
		if (w && !ctx.rootDead) {
			if (GTK_IS_WINDOW(w))
				gtk_window_close(GTK_WINDOW(w));
			pump();
		}
		if (w)
			g_object_remove_weak_pointer(
				G_OBJECT(w), reinterpret_cast<gpointer *>(&w));
	} else {
		ModalDriveCtx md {&ctx, nullptr, false, false, 0};
		g_timeout_add(300, modal_drive_cb, &md);
		dlg->runModal(frame);
		if (md.dlg)
			g_object_remove_weak_pointer(
				G_OBJECT(md.dlg), reinterpret_cast<gpointer *>(&md.dlg));
		pump();
	}

	g_source_remove(sweeper);
	sweep_guard([&ctx] { sweep_strays(ctx); });
	if (g_trace)
		fprintf(stderr, "post-drive: release\n");

	call_guard([&] {
		factory->releaseDialog(dlg);
		pump();
	}, "dialog release");
	__gcov_dump();

	g_print("drive: id %d (type %d) — %d criticals, %d warnings, %d crashed, %d wedged\n",
			static_cast<int>(id), static_cast<int>(type),
			g_criticals, g_warnings, g_crashed_widgets, g_wedged_widgets);
	return 0; /* driven dialogs legitimately log warnings; only
			   * crashes/timeouts are failures for this driver */
}

/* ---------------- main-window drive ---------------- */

static int drive_frame(AP_UnixApp *app, const char *scratch)
{
	/* the global 120s watchdog predates the frame leg — a full
	 * ribbon/window walk legitimately takes minutes under gcov; the
	 * wrapper's `timeout` is the bound now (SIGTERM still dumps
	 * counters) and each widget keeps its own interact alarm */
	alarm(0);
	std::vector<GtkWidget *> before;
	sweep_guard([&before] { before = toplevels(); });

	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}
	char *uri = g_strdup_printf("file://%s", scratch);
	UT_Error err = frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	frame->show();
	pump_for(600);

	GtkWidget *win = nullptr;
	std::vector<GtkWidget *> after;
	sweep_guard([&after] { after = toplevels(); });
	for (GtkWidget *w : after) {
		if (!contains(before, w)) {
			win = w;
			break;
		}
	}
	if (!win) {
		g_printerr("drive: frame produced no toplevel (load err %d)\n", err);
		return 1;
	}

	/* the window itself stays out of the sweep set.  230s of walk
	 * budget inside the wrapper's 300s leg timeout — the ribbon's
	 * widget tree is too large to finish under gcov, so the deadline
	 * keeps the leg deterministic; coverage already earned still
	 * lands via the end-of-leg dump */
	DriveCtx ctx {win, std::move(before), 0, false, false,
		g_get_monotonic_time() + 230 * G_USEC_PER_SEC};
	ctx.preexisting.push_back(win);
	if (!sweep_guard([&] {
			g_signal_connect(win, "destroy",
							 G_CALLBACK(on_root_destroy), &ctx.rootDead);
		}))
		return 1;
	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);

	drive_widget(win, ctx, 0);
	if (g_trace)
		fprintf(stderr, "frame drive tail\n");

	/* the tail's pumps/sweeps re-dispatch whatever the walk left
	 * pending — including the same multi-second layout sources that
	 * can wedge an interact.  They run outside interact()'s alarm, so
	 * guard them or a hang here burns the whole wrapper timeout */
	g_source_remove(sweeper);
	call_guard([&] {
		pump_for(300);
		if (!ctx.rootDead)
			sweep_guard([&ctx] { sweep_strays(ctx); });
	}, "tail sweep");
	__gcov_dump();
	g_print("drive: frame — %d interactions, %d criticals\n",
			ctx.interactions, g_criticals);
	return 0;
}

/* ---------------- AbiWidget drive ---------------- */

static int drive_abiwidget(const char *scratch_uri)
{
	alarm(0); /* the sections' call_guard alarms are the real bound;
			   * the wrapper's timeout covers a total wedge */
	DriveCtx ctx {nullptr, {}, 0, false, false, 0};
	sweep_guard([&ctx] { ctx.preexisting = toplevels(); });
	GtkWidget *win = gtk_window_new();
	GtkWidget *w = abi_widget_new();
	gtk_window_set_child(GTK_WINDOW(win), w);
	gtk_window_set_default_size(GTK_WINDOW(win), 640, 480);
	gtk_window_present(GTK_WINDOW(win));
	ctx.root = win;
	/* edit methods reachable through the abi API (editHeader,
	 * saveImmediate, ...) answer user questions with modal
	 * MessageBoxes in their own nested loop — the periodic stray
	 * sweep dismisses them the same way it does for dialog legs */
	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);
	pump_for(400);

	AbiWidget *abi = ABI_WIDGET(w);
	/* each section runs under the interact fault guard: a crash deep
	 * in product code (e.g. the fl_DocListener stale-layout fault the
	 * drive surfaces on large deletions) skips the rest of that
	 * section instead of killing the leg; repeated faults mean the
	 * document is gone — bail to teardown rather than burn rescues */
	int base_faults = g_crashed_widgets + g_wedged_widgets;
	auto bail = [&] {
		return g_crashed_widgets + g_wedged_widgets - base_faults >= 3;
	};
#define ABI_SECTION(name, ...) \
	if (!bail()) \
		call_guard([&] { __VA_ARGS__; }, name);

	ABI_SECTION("load", abi_widget_load_file(abi, scratch_uri, nullptr);
		pump_for(250);
		abi_widget_turn_on_cursor(abi);
		abi_widget_draw(abi);
		pump_for(250));

	/* content insertion + selection + clipboard */
	ABI_SECTION("clipboard",
		abi_widget_insert_data(abi, "Drive text");
		abi_widget_insert_space(abi);
		abi_widget_select_all(abi);
		abi_widget_copy(abi);
		abi_widget_select_bod(abi);
		abi_widget_paste(abi);
		abi_widget_undo(abi);
		abi_widget_redo(abi);
		pump_for(250));

	/* align + character toggles */
	ABI_SECTION("charfmt",
		abi_widget_align_center(abi);
		abi_widget_align_justify(abi);
		abi_widget_align_left(abi);
		abi_widget_align_right(abi);
		abi_widget_toggle_bold(abi);
		abi_widget_toggle_italic(abi);
		abi_widget_toggle_underline(abi);
		abi_widget_toggle_strike(abi);
		abi_widget_toggle_overline(abi);
		abi_widget_toggle_topline(abi);
		abi_widget_toggle_bottomline(abi);
		abi_widget_toggle_sub(abi);
		abi_widget_toggle_super(abi);
		abi_widget_toggle_plain(abi);
		abi_widget_toggle_insert_mode(abi);
		abi_widget_toggle_insert_mode(abi);
		abi_widget_toggle_bullets(abi);
		abi_widget_toggle_numbering(abi);
		abi_widget_toggle_unindent(abi);
		abi_widget_set_text_color(abi, 200, 40, 40);
		pump_for(250));

	/* selection verbs */
	ABI_SECTION("select",
		abi_widget_select_block(abi);
		abi_widget_select_line(abi);
		abi_widget_select_word(abi);
		abi_widget_select_bob(abi);
		abi_widget_select_bod(abi);
		abi_widget_select_bol(abi);
		abi_widget_select_bow(abi);
		abi_widget_select_eob(abi);
		abi_widget_select_eod(abi);
		abi_widget_select_eol(abi);
		abi_widget_select_eow(abi);
		abi_widget_select_left(abi);
		abi_widget_select_next_line(abi);
		abi_widget_select_page_down(abi);
		abi_widget_select_page_up(abi);
		pump_for(250));

	/* caret movement verbs */
	ABI_SECTION("moveto",
		abi_widget_moveto_bob(abi);
		abi_widget_moveto_bod(abi);
		abi_widget_moveto_bol(abi);
		abi_widget_moveto_bop(abi);
		abi_widget_moveto_bow(abi);
		abi_widget_moveto_eob(abi);
		abi_widget_moveto_eod(abi);
		abi_widget_moveto_eol(abi);
		abi_widget_moveto_eop(abi);
		abi_widget_moveto_eow(abi);
		abi_widget_moveto_left(abi);
		abi_widget_moveto_right(abi);
		abi_widget_moveto_next_line(abi);
		abi_widget_moveto_prev_line(abi);
		abi_widget_moveto_next_page(abi);
		abi_widget_moveto_prev_page(abi);
		abi_widget_moveto_next_screen(abi);
		abi_widget_moveto_prev_screen(abi);
		abi_widget_moveto_to_xy(abi, 20, 20);
		pump_for(250));

	/* find */
	ABI_SECTION("find",
		abi_widget_set_find_string(abi, const_cast<gchar *>("the"));
		abi_widget_find_next(abi, FALSE);
		abi_widget_find_prev(abi);
		pump_for(250));

	/* misc state */
	ABI_SECTION("misc",
		abi_widget_get_page_count(abi);
		abi_widget_get_current_page_num(abi);
		abi_widget_set_current_page(abi, 1);
		abi_widget_set_word_selections(abi, TRUE);
		abi_widget_get_word_selections(abi);
		abi_widget_set_show_margin(abi, TRUE);
		abi_widget_get_show_margin(abi);
		abi_widget_set_show_authors(abi, TRUE);
		abi_widget_get_show_authors(abi);
		abi_widget_toggle_rulers(abi, FALSE);
		abi_widget_toggle_rulers(abi, TRUE);
		abi_widget_insert_table(abi, 2, 3);
		gint32 mx = 0, my = 0;
		abi_widget_get_mouse_pos(abi, &mx, &my);
		pump_for(250));

	/* headers/footers */
	ABI_SECTION("headers",
		abi_widget_edit_header(abi);
		pump_for(250);
		abi_widget_remove_header(abi);
		abi_widget_edit_footer(abi);
		pump_for(250);
		abi_widget_remove_footer(abi);
		pump_for(250));

	/* fonts + styles */
	ABI_SECTION("styles",
		abi_widget_set_font_name(abi, const_cast<gchar *>("Serif"));
		abi_widget_set_font_size(abi, const_cast<gchar *>("14"));
		abi_widget_get_font_names(abi);
		abi_widget_set_style(abi, const_cast<char *>("Heading 1"));
		pump_for(250));

	/* edit-method invocation + content extraction */
	ABI_SECTION("invoke",
		abi_widget_invoke(abi, "selectAll");
		abi_widget_invoke_ex(abi, "insertData", "xyz", 0, 0);
		gint len = 0;
		gchar *text = abi_widget_get_content(abi, "text/plain",
											nullptr, &len);
		g_free(text);
		gchar *sel = abi_widget_get_selection(abi, "text/plain", &len);
		g_free(sel);
		pump_for(250));

	/* rendering + save round-trip */
	ABI_SECTION("save",
		GdkPixbuf *pix = abi_widget_render_page_to_image(abi, 0);
		if (pix)
			g_object_unref(pix);
		abi_widget_save(abi, "/tmp/ui-drive-abi.abw", ".abw", nullptr);
		abi_widget_save_immediate(abi);
		GsfOutput *mem = gsf_output_memory_new();
		abi_widget_save_to_gsf(abi, mem, ".abw", nullptr);
		g_object_unref(mem);
		pump_for(300));

	/* layout-mode switches and the deletion verbs are where the drive
	 * most often trips layout-listener faults in product code; doing
	 * them last keeps a crash from costing the earlier sections */
	ABI_SECTION("views",
		abi_widget_view_formatting_marks(abi);
		abi_widget_view_online_layout(abi);
		pump_for(200);
		abi_widget_view_normal_layout(abi);
		pump_for(200);
		abi_widget_view_print_layout(abi);
		pump_for(200);
		abi_widget_zoom_whole(abi);
		abi_widget_zoom_width(abi);
		abi_widget_set_zoom_percentage(abi, 120);
		abi_widget_get_zoom_percentage(abi);
		pump_for(250));

	ABI_SECTION("delete",
		abi_widget_select_word(abi);
		abi_widget_delete_bol(abi);
		abi_widget_delete_bow(abi);
		abi_widget_delete_bob(abi);
		abi_widget_delete_bod(abi);
		abi_widget_delete_eob(abi);
		abi_widget_delete_eod(abi);
		abi_widget_delete_eol(abi);
		abi_widget_delete_eow(abi);
		abi_widget_delete_left(abi);
		abi_widget_delete_right(abi);
		pump_for(250));
#undef ABI_SECTION

	g_source_remove(sweeper);
	call_guard([&] {
		gtk_window_destroy(GTK_WINDOW(win));
		pump_for(250);
	}, "teardown");
	__gcov_dump();
	g_print("drive: abiwidget done — %d criticals, %d crashed, %d wedged\n",
			g_criticals, g_crashed_widgets, g_wedged_widgets);
	return 0;
}

int main(int argc, char **argv)
{
	g_log_set_writer_func(drive_log_writer, nullptr, nullptr);
	g_trace = getenv("UI_DRIVE_TRACE") != nullptr;
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = drive_fatal;
	sa.sa_flags = SA_NODEFER; /* stay installed: the interact guard
							   longjmps out of repeated crashes */
	sigaction(SIGSEGV, &sa, nullptr);
	sigaction(SIGABRT, &sa, nullptr);
	sigaction(SIGALRM, &sa, nullptr);
	/* the wrapper's `timeout` kills with SIGTERM — treat it like the
	 * watchdog so a hung leg still leaves its counters behind */
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGINT, &sa, nullptr);
	alarm(120); /* last-resort watchdog; drvwrap wraps us in `timeout` */

	bool wantList = false, wantFrame = false, wantAbi = false;
	long wantId = -1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--list") == 0)
			wantList = true;
		else if (strcmp(argv[i], "--frame") == 0)
			wantFrame = true;
		else if (strcmp(argv[i], "--abi") == 0)
			wantAbi = true;
		else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc)
			wantId = strtol(argv[++i], nullptr, 10);
	}
	if (!wantList && !wantFrame && !wantAbi && wantId < 0) {
		g_printerr("usage: %s --list | --id N | --frame | --abi\n", argv[0]);
		return 2;
	}

	if (!gtk_init_check()) {
		g_printerr("drive: no display\n");
		return 77;
	}

	XAP_App::s_szBuild_ID = "TEST";
	XAP_App::s_szAbiSuite_Home = g_get_tmp_dir();
	XAP_App::s_szBuild_Version = "TEST";
	XAP_App::s_szBuild_Options = "TEST";
	XAP_App::s_szBuild_Target = "TEST";

	AP_UnixApp *app = new AP_UnixApp(PACKAGE);
	if (!app || !app->initialize(TRUE)) {
		g_printerr("drive: AP_UnixApp initialize failed\n");
		return 1;
	}

	/* the real binary goes through g_application_run, which registers
	 * the app and emits "startup" before any window exists; frames
	 * build GtkApplicationWindows that warn (and app-scoped widget
	 * paths misbehave/crash) when startup never fired */
	{
		GtkApplication *gtkApp =
			static_cast<XAP_UnixApp *>(app)->getGtkApp();
		g_application_register(G_APPLICATION(gtkApp), nullptr, nullptr);
		g_signal_emit_by_name(gtkApp, "startup");
		pump();
	}

	/* richer fixture than the smoke's BillOfRights: styles, TOC,
	 * fields, lists, table, notes — more dialog state to exercise;
	 * the wrapper stages a scratch copy so drive-time Save is safe */
	const char *src = getenv("UI_DRIVE_DOC");
	char scratchbuf[1024];
	if (!src) {
		const char *top = getenv("ABINOVA_TEST_SRC_DIR");
		snprintf(scratchbuf, sizeof(scratchbuf),
				 "%s/test/wp/cov07/rich.abw", top ? top : ".");
		src = scratchbuf;
	}

	if (wantAbi) {
		char *uri = g_strdup_printf("file://%s", src);
		int rc = drive_abiwidget(uri);
		g_free(uri);
		return rc;
	}
	if (wantFrame)
		return drive_frame(app, src);

	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}
	{
		char *uri = g_strdup_printf("file://%s", src);
		UT_Error err = frame->loadDocument(uri, IEFT_Unknown, true);
		g_free(uri);
		if (err == UT_OK)
			frame->show();
		pump();
	}

	XAP_DialogFactory *factory =
		static_cast<XAP_DialogFactory *>(frame->getDialogFactory());
	if (!factory) {
		g_printerr("drive: frame has no dialog factory\n");
		return 1;
	}

	if (wantList) {
		for (UT_uint32 i = 0; i < factory->getDialogTableSize(); i++) {
			const XAP_DialogFactory::_dlg_table *e =
				factory->getDialogTableEntry(i);
			if (e)
				g_print("%d\n", static_cast<int>(e->m_id));
		}
		return 0;
	}

	for (UT_uint32 i = 0; i < factory->getDialogTableSize(); i++) {
		const XAP_DialogFactory::_dlg_table *e =
			factory->getDialogTableEntry(i);
		if (e && static_cast<int>(e->m_id) == wantId)
			return run_dialog(factory, frame, e->m_id, e->m_type);
	}
	g_printerr("drive: id %ld not registered\n", wantId);
	return 1;
}
