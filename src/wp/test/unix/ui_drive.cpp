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
 *   ui-drive --ev            COV13: menu action layer, real input
 *                            events (XTest), image/media graphics
 *   ui-drive --fmt           COV14: text/fmt/gtk — selection handles,
 *                            paste-options tag, text/image/frame
 *                            drags (GDK wayland paths when a wayland
 *                            compositor is live)
 *   ui-drive --freeze        TST12: heartbeat-monitored op script —
 *                            fails when a synchronous stall stretches
 *                            a main-loop poll gap past the bound
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
#include <functional>
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
#include "gtktexthandleprivate.h"
#include "fv_FrameEdit.h"
#include "ap_FrameData.h"
#include "ap_TopRuler.h"
#include "ap_LeftRuler.h"
#include "ap_UnixTopRuler.h"
#include "ap_UnixLeftRuler.h"

#include <cmath>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <dlfcn.h>
#endif

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
static sigjmp_buf g_probe_jmp;
static volatile sig_atomic_t g_in_interact = 0;
static volatile sig_atomic_t g_sweep_depth = 0;
static volatile sig_atomic_t g_in_probe = 0;
static volatile sig_atomic_t g_rescued = 0;
static int g_wedged_widgets = 0;
static int g_crashed_widgets = 0;

/* FRZ05 spin diagnostics: a fixed-size record of what the drive is
 * doing right now — safe to poke from signal context.  Set it before
 * every phase that runs OUTSIDE interact()'s alarm (modal loops,
 * pump_for, runModal, releaseDialog) so a leg that freezes without
 * tripping a guarded wedge still reports where it died — either via
 * the SIGQUIT probe or the fatal path below. */
static char g_phase[192] = "init";
static void set_phase(const char *phase)
{
	if (!phase)
		phase = "?";
	strncpy(g_phase, phase, sizeof(g_phase) - 1);
	g_phase[sizeof(g_phase) - 1] = 0;
}
static void set_phase_widget(const char *prefix, GtkWidget *w)
{
	const char *tn = w ? G_OBJECT_TYPE_NAME(w) : "-";
	const char *nm = w ? gtk_widget_get_name(w) : nullptr;
	snprintf(g_phase, sizeof(g_phase), "%s %s name=%s",
			 prefix ? prefix : "widget", tn, nm ? nm : "-");
}

/* TST12 heartbeat: a g_timeout only fires while the default main
 * context is being serviced, so the gap between consecutive firings
 * measures real UI-thread stalls — a usleep, a blocking whole-file
 * read, or a serializing loop all stretch the next firing.  The
 * --freeze leg scripts the operations FRZ01-03 touched and fails
 * when a gap crosses the bound in effect at fire time: interactive
 * ops keep the 750ms task bound while document load/import get a
 * generous bound (a legit open is ~1s under gcov instrumentation —
 * the tripwire exists to catch pathological stalls, not slow work). */
static const guint HB_POLL_MS = 40;
static const gint64 HB_BOUND_US = 750 * G_TIME_SPAN_MILLISECOND;
static const gint64 HB_LOAD_BOUND_US = 6 * G_TIME_SPAN_SECOND;
static gint64 g_hb_prev = 0;
static gint64 g_hb_bound = HB_BOUND_US;
static gint64 g_hb_worst = 0;		/* largest gap seen, for reporting */
static char g_hb_worst_phase[192] = "-";
static gint64 g_hb_worst_over = 0;	/* largest gap−bound — the gate */
static gint64 g_hb_trip_gap = 0;
static char g_hb_trip_phase[192] = "-";

/* set the bound the heartbeat applies to upcoming gaps — stays in
 * effect through the phase's own tail pump so a synchronous stall is
 * attributed to the phase that ran it */
static void hb_expect(gint64 bound_us)
{
	g_hb_bound = bound_us;
}

static gboolean heartbeat_cb(gpointer)
{
	gint64 now = g_get_monotonic_time();
	gint64 gap = g_hb_prev ? now - g_hb_prev : 0;
	if (gap > g_hb_worst) {
		g_hb_worst = gap;
		snprintf(g_hb_worst_phase, sizeof(g_hb_worst_phase),
				 "%s", g_phase);
	}
	if (gap - g_hb_bound > g_hb_worst_over) {
		g_hb_worst_over = gap - g_hb_bound;
		g_hb_trip_gap = gap;
		snprintf(g_hb_trip_phase, sizeof(g_hb_trip_phase),
				 "%s", g_phase);
	}
	g_hb_prev = now;
	return G_SOURCE_CONTINUE;
}

static void dump_diag(int sig)
{
	char buf[256];
	int n = snprintf(buf, sizeof(buf),
					 "drive: signal %d in phase: %s\n", sig, g_phase);
	(void)!write(STDERR_FILENO, buf, n);
	/* best-effort fault stack — async-unsafe but only diagnostic;
	 * if it wedges the watchdog covers us */
	void *bt[32];
	int nb = backtrace(bt, 32);
	backtrace_symbols_fd(bt, nb, STDERR_FILENO);
}

/* SIGQUIT probe: dump the current phase + stack but keep running —
 * `kill -QUIT <pid>` on a spun leg shows exactly where it is stuck. */
static void drive_probe(int sig)
{
	dump_diag(sig);
}

static void drive_fatal(int sig)
{
	/* a fault inside a toplevel scan is not the driven widget's
	 * fault: GTK's toplevel model can still hand back a window that
	 * was finalized moments ago (observed: ClipArt's nested message
	 * dialog destroyed inside abiRunModalDialog's own modal loop —
	 * g_list_model_get_item refs the dead object and any deref is
	 * UAF).  Rescue and skip the scan. */
	/* the emission probe is the innermost guard — it runs in the
	 * rescue return path, after the other guards were unwound */
	if (g_in_probe) {
		g_in_probe = 0;
		alarm(0);
		siglongjmp(g_probe_jmp, 1);
	}
	if (g_sweep_depth > 0 && g_rescued < 8) {
		g_rescued++;
		alarm(0);
		siglongjmp(g_sweep_jmp, 1);
	}
	if (g_in_interact && g_rescued < 8) {
		g_rescued++;
		g_in_interact = 0;
		alarm(0);
		dump_diag(sig);
		siglongjmp(g_jmp, sig == SIGALRM ? 2 : 1);
	}
	static const char msg[] = "drive: fatal signal, counters kept\n";
	(void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
	/* the wedge is somewhere outside every guard (a pump, a nested
	 * modal loop, runModal itself) — the phase + stack are the only
	 * diagnosis a wrapper timeout gets */
	dump_diag(sig);
	__gcov_dump();
	_exit(sig == SIGSEGV ? 139 : sig == SIGABRT ? 134 : 124);
}

/* A rescue that longjmped out of a faulting signal handler can leave
 * GLib's emission locks stranded — the very next g_signal_emit then
 * futex-blocks forever, and the deadlock lands in UNGUARDED code
 * (observed: post-walk gtk_menu_button_popdown, sweep emits inside
 * pump()).  Probe emission on a scratch widget under its own alarm:
 * if it wedges, the process can never emit again — keep counters and
 * exit rather than hang until the wrapper timeout. */
static bool glib_emission_alive(void)
{
	if (sigsetjmp(g_probe_jmp, 1) != 0) {
		g_in_probe = 0;
		alarm(0);
		return false;
	}
	g_in_probe = 1;
	alarm(5);
	GtkWidget *scratch = gtk_button_new();
	g_object_ref_sink(scratch);
	g_signal_emit_by_name(scratch, "clicked");
	g_object_unref(scratch);
	alarm(0);
	g_in_probe = 0;
	return true;
}

static void post_rescue_check(void)
{
	if (!glib_emission_alive()) {
		g_printerr("drive: signal emission dead after rescue — "
				   "exiting with counters\n");
		__gcov_dump();
		_exit(3);
	}
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
	int straysClosed;                   /* stray toplevels the sweep
										 * answered — the ruler leg
										 * asserts its double-click
										 * dialog by watching this */
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
			ctx.straysClosed++;
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
		post_rescue_check();
		return false;
	default:
		g_in_interact = 0;
		g_crashed_widgets++;
		g_printerr("drive: crashed in %s — section skipped\n", what);
		post_rescue_check();
		return false;
	}
	g_in_interact = 1;
	set_phase(what);
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
		post_rescue_check();
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
		 * have run, so skip the whole family.  newWindow builds a
		 * second frame on the same doc and crashed in layout collapse
		 * under the drive (fp_Container::countCons on a stale
		 * container inside fl_DocSectionLayout::collapse) */
		"fileNew", "newWindow",
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
		"cairoPrint", "cairoPrintDirectly", "cairoPrintPreview",
		/* URI launchers — a real activation opens a browser window
		 * on the user's live session via the shared session bus;
		 * helpSearch's internal browser still resolves external
		 * links through openURL, so keep it out too */
		"helpCheckVer", "helpReportBug", "helpSearch"
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
		AP_MENU_ID_INSERT_PICTURES, AP_MENU_ID_INSERT_SCREENSHOT,
		AP_MENU_ID_WINDOW_NEW,
		/* URI launchers — see helpCheckVer/helpReportBug above */
		AP_MENU_ID_HELP_CHECKVER, AP_MENU_ID_HELP_REPORT_BUG
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
	 * identity is the action name ("menu.item_<id>").  The GTK4
	 * popover's item widget (GtkPopoverMenuItemWidget) is NOT a
	 * GtkActionable but does expose the item's action in an
	 * "action-name" property, which GtkActionable also has — read it
	 * generically so both paths are covered */
	if (g_object_class_find_property(
			G_OBJECT_GET_CLASS(w), "action-name")) {
		gchar *an = nullptr;
		g_object_get(w, "action-name", &an, nullptr);
		const char *p = an ? strstr(an, "item_") : nullptr;
		if (p) {
			unsigned idnum = static_cast<unsigned>(atoi(p + 5));
			for (_Ap_Menu_Id id : bad_ids) {
				if (idnum == static_cast<unsigned>(id)) {
					g_free(an);
					return true;
				}
			}
		}
		g_free(an);
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

	set_phase_widget("interact", w);
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
		 * the next signal call) — bail now if emission is dead, else
		 * count faults across kinds and stop the walk instead of
		 * inviting a silent deadlock */
		post_rescue_check();
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
		post_rescue_check();
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
		set_phase("modal: attach stray");
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
		set_phase("modal: drive widgets");
		drive_widget(m->dlg, *m->ctx, 0);
		return G_SOURCE_CONTINUE;
	}
	if (!m->answered) {
		set_phase("modal: answer OK");
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
	set_phase("modal: drain (dialog still up)");
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

	set_phase("dialog: request");
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
		g_get_monotonic_time() + 40 * G_USEC_PER_SEC, 0};
	sweep_guard([&ctx] { ctx.preexisting = toplevels(); });

	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);

	if (XAP_Dialog_Modeless *ml = dynamic_cast<XAP_Dialog_Modeless *>(dlg)) {
		set_phase("dialog: runModeless");
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
		set_phase("dialog: runModal");
		dlg->runModal(frame);
		set_phase("dialog: runModal returned");
		if (md.dlg)
			g_object_remove_weak_pointer(
				G_OBJECT(md.dlg), reinterpret_cast<gpointer *>(&md.dlg));
		pump();
	}

	g_source_remove(sweeper);
	set_phase("dialog: final stray sweep");
	sweep_guard([&ctx] { sweep_strays(ctx); });
	if (g_trace)
		fprintf(stderr, "post-drive: release\n");

	set_phase("dialog: release");
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

	set_phase("frame: newFrame");
	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}
	set_phase("frame: loadDocument");
	char *uri = g_strdup_printf("file://%s", scratch);
	UT_Error err = frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	frame->show();
	set_phase("frame: settle pump");
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
		g_get_monotonic_time() + 230 * G_USEC_PER_SEC, 0};
	ctx.preexisting.push_back(win);
	if (!sweep_guard([&] {
			g_signal_connect(win, "destroy",
							 G_CALLBACK(on_root_destroy), &ctx.rootDead);
		}))
		return 1;
	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);

	set_phase("frame: drive widgets");
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

/* ---------------- RUL01: ruler drag drive ---------------- */

/* Scripted-pointer leg for RUL01.  GTK4 dropped the implicit press
 * grab the rulers relied on, so a drag that leaves the ruler band used
 * to stop receiving motion and silently cancel.  Drags now go through
 * GtkGestureDrag which reports drag-update/drag-end offsets regardless
 * of where the pointer is.  This leg injects REAL pointer events with
 * XTest (motion via XWarpPointer — window-relative, so no root
 * coordinate math) and asserts the Word-parity semantics:
 *
 *   - a click on the band adds a tab stop
 *   - dragging a tab stop vertically off the band deletes it
 *   - a paragraph-marker drag keeps tracking horizontally while the
 *     pointer strays off the band, and applies at the tracked x
 *   - a left-ruler margin drag keeps tracking vertically and applies
 *     at the tracked y
 *   - a double-click on the ruler opens the Paragraph dialog
 *
 * Needs the X11 backend plus libXtst; exits 77 (SKIP) without them.
 * libXtst is dlopen'ed so the binary still links everywhere. */

#ifdef GDK_WINDOWING_X11
typedef int (*XTestBtnFn)(Display *, unsigned int, Bool, unsigned long);

/* press at (px,py) device px on the ruler via the XP entry points and
 * release without document side effects: a same-spot release is the
 * "clicked, no move" no-op for markers and existing tabs; a pending
 * NEW tab (draggingTabForTest() < 0) is dropped by an off-band release,
 * which rewrites the unchanged tab list.  Returns the DraggingWhat the
 * press grabbed. */
static AP_TopRuler::DraggingWhat probe_top_ruler(AP_TopRuler *tr,
												 int px, int py,
												 UT_sint32 *tabIdx)
{
	GR_Graphics *rg = tr->getGraphics();
	tr->mousePress(0, EV_EMB_BUTTON1,
				   static_cast<UT_uint32>(rg->tlu(px)),
				   static_cast<UT_uint32>(rg->tlu(py)));
	AP_TopRuler::DraggingWhat w = tr->draggingWhatForTest();
	if (tabIdx)
		*tabIdx = tr->draggingTabForTest();
	if (w == AP_TopRuler::DW_TABSTOP && tr->draggingTabForTest() < 0)
		tr->mouseRelease(0, EV_EMB_BUTTON1, rg->tlu(px), -1000);
	else
		tr->mouseRelease(0, EV_EMB_BUTTON1, rg->tlu(px), rg->tlu(py));
	return w;
}

static AP_LeftRuler::DraggingWhat probe_left_ruler(AP_LeftRuler *lr,
												   int px, int py)
{
	GR_Graphics *rg = lr->getGraphics();
	lr->mousePress(0, EV_EMB_BUTTON1,
				   static_cast<UT_uint32>(rg->tlu(px)),
				   static_cast<UT_uint32>(rg->tlu(py)));
	AP_LeftRuler::DraggingWhat w = lr->draggingWhatForTest();
	lr->mouseRelease(0, EV_EMB_BUTTON1, rg->tlu(px), rg->tlu(py));
	return w;
}

/* scan device-px columns x0..x1 of the top ruler at row py and return
 * the CENTER of the first contiguous run where the probe matches.
 * Aiming at the run center (rather than the first matching px) keeps
 * the real XTest press inside the hit box even when the pointer lands
 * a few px off the warp target — observed jitter is several px as the
 * frame's layout settles under Xvfb. */
static int probe_top_center(AP_TopRuler *tr, int py, int x0, int x1,
							AP_TopRuler::DraggingWhat want,
							bool wantExistingTab)
{
	int runStart = -1;
	for (int px = x0; px <= x1; px++) {
		UT_sint32 ti = 0;
		AP_TopRuler::DraggingWhat w = probe_top_ruler(tr, px, py, &ti);
		bool match = (w == want) && (!wantExistingTab || ti >= 0);
		if (match && runStart < 0)
			runStart = px;
		else if (!match && runStart >= 0)
			return (runStart + px - 1) / 2;
	}
	return runStart >= 0 ? (runStart + x1) / 2 : -1;
}

struct RulerInject {
	Display *dpy;
	Window xid;
	XTestBtnFn fakeBtn;
};

/* warp the real pointer to widget-space (px,py) and give the main
 * loop a slice so GDK dispatches the resulting MotionNotify */
static void rul_move(RulerInject &inj, GtkWidget *w, double px, double py)
{
	GtkWidget *native = GTK_WIDGET(gtk_widget_get_native(w));
	graphene_point_t in = {(float)px, (float)py}, out;
	if (native && gtk_widget_compute_point(w, native, &in, &out)) {
		XWarpPointer(inj.dpy, None, inj.xid, 0, 0, 0, 0,
					 (int)lrint(out.x), (int)lrint(out.y));
		XFlush(inj.dpy);
		if (g_trace)
			fprintf(stderr, "rul_move w(%g,%g) -> native(%g,%g)\n",
					px, py, out.x, out.y);
	} else if (g_trace)
		fprintf(stderr, "rul_move w(%g,%g): compute_point failed\n",
				px, py);
	pump_for(35);
}

static void rul_btn(RulerInject &inj, bool down)
{
	inj.fakeBtn(inj.dpy, 1, down ? True : False, 0);
	XFlush(inj.dpy);
	pump_for(45);
}

/* GtkGestureClick folds presses that land within gtk-double-click-time
 * (~250ms) and distance of each other into n_press > 1 — and the
 * ruler's click handler turns n_press == 2 into dlgParagraph.  Every
 * synthetic press pair that is not meant to be a double-click must be
 * separated by more than that window or the leg opens modal dialogs it
 * never intended to. */
static void rul_gap(void)
{
	pump_for(450);
}

/* a ruler press while a document change is in flight early-outs and
 * grabs nothing — drain the main loop until the piece table is idle */
static void wait_pt_idle(FV_View *view)
{
	gint64 deadline = g_get_monotonic_time() + 2000000;
	while (view->getDocument()->isPieceTableChanging() &&
		   g_get_monotonic_time() < deadline)
		pump_for(40);
}

/* under Xvfb the freshly-mapped frame keeps resizing for a second or
 * two and every resize shifts the ruler's column origin, which makes a
 * drag's tracked coordinate recompute against a moving reference.
 * Probe a fixed marker's x until it stops moving (and the widget stops
 * changing height) so the leg exercises a stable layout. */
static void settle_ruler(AP_TopRuler *tr, GtkWidget *tw)
{
	int lastX = -1, lastH = -1, stable = 0;
	gint64 deadline = g_get_monotonic_time() + 4000000;
	int py = (3 * gtk_widget_get_height(tw) / 4) - 3;
	while (stable < 3 && g_get_monotonic_time() < deadline) {
		pump_for(150);
		int cx = probe_top_center(tr, py, 8,
								  gtk_widget_get_width(tw) - 8,
								  AP_TopRuler::DW_LEFTINDENT, false);
		int h = gtk_widget_get_height(tw);
		if (cx >= 0 && cx == lastX && h == lastH)
			stable++;
		else {
			stable = 0;
			lastX = cx;
			lastH = h;
		}
	}
}

static int drive_ruler(AP_UnixApp *app, const char *scratch)
{
	alarm(0);
	set_phase("ruler: init");
	GdkDisplay *gd = gdk_display_get_default();
	if (!GDK_IS_X11_DISPLAY(gd)) {
		g_printerr("ruler: not an X11 display — XTest injection needs "
				   "Xvfb\n");
		return 77;
	}
	Display *dpy = gdk_x11_display_get_xdisplay(gd);
	void *xt = dlopen("libXtst.so.6", RTLD_NOW | RTLD_LOCAL);
	if (!xt)
		xt = dlopen("libXtst.so", RTLD_NOW | RTLD_LOCAL);
	XTestBtnFn fakeBtn = xt ? reinterpret_cast<XTestBtnFn>(
		dlsym(xt, "XTestFakeButtonEvent")) : nullptr;
	if (!fakeBtn) {
		g_printerr("ruler: libXtst unavailable — cannot inject buttons\n");
		return 77;
	}

	/* the ruler's double-click handler runs dlgParagraph modally; an
	 * accidental multi-press (or the deliberate one at the end) wedges
	 * the leg without a stray sweeper inside the nested loop.  Same
	 * mechanism the dialog legs use. */
	DriveCtx sctx {nullptr, {}, 0, false, false, 0, 0};
	sweep_guard([&sctx] { sctx.preexisting = toplevels(); });

	set_phase("ruler: new frame");
	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("ruler: no frame\n");
		return 1;
	}
	(void)scratch;
	/* load a brand-new empty document rather than the staged rich.abw:
	 * the ruler hit-tests the block under the caret, and a blank first
	 * paragraph has no indents, no table cell and a full-width column,
	 * so every marker has room to move and tab anchors snap
	 * predictably */
	UT_Error err = frame->loadDocument(
		static_cast<const char *>(nullptr), IEFT_Unknown);
	frame->show();
	pump_for(700);
	if (err != UT_OK) {
		g_printerr("ruler: new-document err %d\n", err);
		return 1;
	}

	FV_View *view = static_cast<FV_View *>(frame->getCurrentView());
	AP_FrameData *fd = static_cast<AP_FrameData *>(frame->getFrameData());
	if (!view || !fd) {
		g_printerr("ruler: no view/frame data\n");
		return 1;
	}
	if (!fd->m_bShowRuler || !fd->m_pTopRuler || !fd->m_pLeftRuler) {
		frame->toggleRuler(true);
		pump_for(300);
	}
	AP_TopRuler *tr = fd->m_pTopRuler;
	AP_LeftRuler *lr = fd->m_pLeftRuler;
	if (!tr || !lr) {
		g_printerr("ruler: rulers not enabled\n");
		return 77;
	}
	AP_UnixTopRuler *utr = dynamic_cast<AP_UnixTopRuler *>(tr);
	AP_UnixLeftRuler *ulr = dynamic_cast<AP_UnixLeftRuler *>(lr);
	GtkWidget *tw = utr ? utr->getWidget() : nullptr;
	GtkWidget *lw = ulr ? ulr->getWidget() : nullptr;
	if (!tw || !lw ||
		!gtk_widget_get_realized(tw) || !gtk_widget_get_realized(lw)) {
		g_printerr("ruler: ruler widgets not realized\n");
		return 77;
	}
	int tw_w = gtk_widget_get_width(tw), tw_h = gtk_widget_get_height(tw);
	int lw_w = gtk_widget_get_width(lw), lw_h = gtk_widget_get_height(lw);
	if (tw_w < 200 || tw_h < 10 || lw_h < 200 || lw_w < 10) {
		g_printerr("ruler: rulers not laid out (%dx%d / %dx%d)\n",
				   tw_w, tw_h, lw_w, lw_h);
		return 1;
	}

	GtkWidget *native = GTK_WIDGET(gtk_widget_get_native(tw));
	GdkSurface *surf = gtk_native_get_surface(GTK_NATIVE(native));
	if (!surf || !GDK_IS_X11_SURFACE(surf)) {
		g_printerr("ruler: no X11 surface\n");
		return 77;
	}
	RulerInject inj { dpy, gdk_x11_surface_get_xid(surf), fakeBtn };

	/* let the freshly-mapped window finish resizing before measuring
	 * anything — each resize moves the column origin mid-run */
	set_phase("ruler: settle");
	settle_ruler(tr, tw);

	sctx.root = native;
	guint sweeper = g_timeout_add(150, stray_sweep_cb, &sctx);

	GR_Graphics *rg = tr->getGraphics();
	int fails = 0;

	/* ---- tab click-add, then drag-off-band deletes ---- */
	set_phase("ruler: tab click-add");

	AP_TopRulerInfo ti0;
	view->getTopRulerInfo(&ti0);

	int tabX = -1;
	for (int px = 40; px < tw_w - 8; px += 2) {
		UT_sint32 ti = 0;
		if (probe_top_ruler(tr, px, tw_h / 2, &ti) == AP_TopRuler::DW_TABSTOP &&
			ti < 0) {
			tabX = px;
			break;
		}
	}
	if (tabX < 0) {
		g_printerr("FAIL ruler: no free tab-zone point found\n");
		fails++;
	} else {
		rul_move(inj, tw, tabX, tw_h / 2);
		rul_btn(inj, true);
		rul_btn(inj, false);
		rul_gap();
		AP_TopRulerInfo ti1;
		view->getTopRulerInfo(&ti1);
		if (ti1.m_iTabStops == ti0.m_iTabStops + 1) {
			g_print("ruler: click on band added a tab stop (%d -> %d)\n",
					ti0.m_iTabStops, ti1.m_iTabStops);
		} else {
			g_printerr("FAIL ruler: click on band did not add a tab "
					   "(%d -> %d)\n", ti0.m_iTabStops, ti1.m_iTabStops);
			fails++;
		}

		/* the committed tab snaps to the ruler grid, so its hit box can
		 * sit a few px off the click point — locate the box with the
		 * probe (presses in the tab zone that create a pending tab are
		 * dropped off-band inside probe_top_ruler) and press at its
		 * center; retry with a fresh probe in case the box shifted
		 * while the frame's layout was still settling. */
		set_phase("ruler: tab grab + drag-off");
		bool grabbed = false;
		int grabX = -1, grabY = tw_h / 2;
		for (int attempt = 0; attempt < 4 && !grabbed; attempt++) {
			if (grabX < 0)
				grabX = probe_top_center(tr, grabY,
										 tabX - 30, tabX + 30,
										 AP_TopRuler::DW_TABSTOP, true);
			if (grabX < 0) {
				g_printerr("ruler: probe found no tab near %d "
						   "(attempt %d)\n", tabX, attempt);
				break;
			}
			wait_pt_idle(view);
			rul_move(inj, tw, grabX, grabY);
			rul_btn(inj, true);
			if (g_trace)
				fprintf(stderr, "tab-grab @(%d,%d): what=%d idx=%d "
						"landed=(%d,%d)\n",
						grabX, grabY, tr->draggingWhatForTest(),
						static_cast<int>(tr->draggingTabForTest()),
						rg->tdu(tr->lastPressXForTest()),
						rg->tdu(tr->lastPressYForTest()));
			if (tr->draggingWhatForTest() == AP_TopRuler::DW_TABSTOP &&
				tr->draggingTabForTest() >= 0) {
				grabbed = true;
				break;
			}
			/* a pending-new tab is dropped off-band; any other grab
			 * gets a zero-delta release — both side-effect free */
			if (tr->draggingWhatForTest() == AP_TopRuler::DW_TABSTOP)
				rul_move(inj, tw, grabX, tw_h + 30);
			rul_btn(inj, false);
			rul_gap();
			/* the delivered press coordinate can sit several px off
			 * the warp target while the frame's layout settles —
			 * feed the landing error back into the aim before
			 * re-probing */
			grabX = 2 * grabX - rg->tdu(tr->lastPressXForTest());
			grabY = 2 * grabY - rg->tdu(tr->lastPressYForTest());
			int cx = probe_top_center(tr, grabY,
									  grabX - 20, grabX + 20,
									  AP_TopRuler::DW_TABSTOP, true);
			if (cx >= 0)
				grabX = cx;
		}
		if (!grabbed) {
			g_printerr("FAIL ruler: press never grabbed the new tab\n");
			fails++;
		} else {
			rul_move(inj, tw, grabX, tw_h + 30); /* off the band */
			rul_btn(inj, false);
			rul_gap();
			AP_TopRulerInfo ti2;
			view->getTopRulerInfo(&ti2);
			if (ti2.m_iTabStops == ti0.m_iTabStops) {
				g_print("ruler: tab dragged off the band deleted "
						"(%d -> %d)\n", ti1.m_iTabStops, ti2.m_iTabStops);
			} else {
				g_printerr("FAIL ruler: off-band release did not delete "
						   "the tab (%d -> %d)\n",
						   ti1.m_iTabStops, ti2.m_iTabStops);
				fails++;
			}
		}
	}

	/* ---- marker drag that leaves the band applies at tracked x ---- */
	set_phase("ruler: marker drag off-band");

	/* the tab delete above reformats the block; the resulting relayout
	 * can shift the column's window position a frame or two later.  let
	 * it settle before probing marker positions or the drag's tracked x
	 * gets recomputed against a moving origin */
	settle_ruler(tr, tw);

	/* the indent markers occupy the lower half of the band (the upper
	 * part, just above the 3/4 line, grabs the single indent; the
	 * bottom box grabs the paired one) — try both rows */
	struct { int px, py; AP_TopRuler::DraggingWhat what; } hit {
		-1, -1, AP_TopRuler::DW_NOTHING };
	const int probeY[] = { (3 * tw_h / 4) - 3, (3 * tw_h / 4) + 3 };
	const AP_TopRuler::DraggingWhat markerKinds[] = {
		AP_TopRuler::DW_LEFTINDENT,
		AP_TopRuler::DW_LEFTINDENTWITHFIRST,
		AP_TopRuler::DW_RIGHTINDENT,
		AP_TopRuler::DW_FIRSTLINEINDENT,
		AP_TopRuler::DW_LEFTMARGIN,
		AP_TopRuler::DW_RIGHTMARGIN
	};
	for (int py : probeY) {
		for (AP_TopRuler::DraggingWhat kind : markerKinds) {
			int cx = probe_top_center(tr, py, 8, tw_w - 8, kind, false);
			if (cx >= 0) {
				hit = { cx, py, kind };
				break;
			}
		}
		if (hit.px >= 0)
			break;
	}
	/* drag direction chosen so the tracked property grows: left
	 * markers right, right markers left — either way ~80 px and
	 * comfortably inside the column */
	const int dx = (hit.what == AP_TopRuler::DW_RIGHTINDENT ||
					hit.what == AP_TopRuler::DW_RIGHTMARGIN) ? -80 : 80;
	if (hit.px < 0) {
		g_printerr("FAIL ruler: no paragraph marker found to drag\n");
		fails++;
	} else {
		bool markerGrabbed = false;
		for (int attempt = 0; attempt < 4 && !markerGrabbed; attempt++) {
			wait_pt_idle(view);
			rul_move(inj, tw, hit.px, hit.py);
			rul_btn(inj, true);
			if (g_trace)
				fprintf(stderr, "marker press @(%d,%d): want=%d got=%d "
						"landed=(%d,%d)\n",
						hit.px, hit.py, hit.what, tr->draggingWhatForTest(),
						rg->tdu(tr->lastPressXForTest()),
						rg->tdu(tr->lastPressYForTest()));
			if (tr->draggingWhatForTest() == hit.what) {
				markerGrabbed = true;
				break;
			}
			/* landed off the marker: release cleanly and re-probe —
			 * a pending tab goes off-band, anything else is a
			 * zero-delta no-op */
			if (tr->draggingWhatForTest() == AP_TopRuler::DW_TABSTOP)
				rul_move(inj, tw, hit.px, tw_h + 30);
			rul_btn(inj, false);
			rul_gap();
			hit.px = 2 * hit.px - rg->tdu(tr->lastPressXForTest());
			hit.py = 2 * hit.py - rg->tdu(tr->lastPressYForTest());
			int cx = probe_top_center(tr, hit.py,
									  hit.px - 16, hit.px + 16,
									  hit.what, false);
			if (cx >= 0)
				hit.px = cx;
		}
		if (!markerGrabbed) {
			g_printerr("FAIL ruler: real press did not grab marker "
					   "(want %d got %d)\n",
					   hit.what, tr->draggingWhatForTest());
			fails++;
		} else {
			rul_move(inj, tw, hit.px + dx / 2, hit.py);     /* in-band */
			rul_move(inj, tw, hit.px + dx, tw_h + 30);      /* off-band */
			rul_btn(inj, false);
			rul_gap();

			AP_TopRulerInfo ti3;
			view->getTopRulerInfo(&ti3);
			UT_sint32 d = 0;
			const char *what = "?";
			switch (hit.what) {
			case AP_TopRuler::DW_LEFTINDENT:
			case AP_TopRuler::DW_LEFTINDENTWITHFIRST:
				d = ti3.m_xrLeftIndent - ti0.m_xrLeftIndent;
				what = "left indent"; break;
			case AP_TopRuler::DW_FIRSTLINEINDENT:
				d = ti3.m_xrFirstLineIndent - ti0.m_xrFirstLineIndent;
				what = "first-line indent"; break;
			case AP_TopRuler::DW_RIGHTINDENT:
				d = ti3.m_xrRightIndent - ti0.m_xrRightIndent;
				what = "right indent"; break;
			case AP_TopRuler::DW_LEFTMARGIN:
				d = ti3.u.c.m_xaLeftMargin - ti0.u.c.m_xaLeftMargin;
				what = "left margin"; break;
			case AP_TopRuler::DW_RIGHTMARGIN:
				d = ti3.u.c.m_xaRightMargin - ti0.u.c.m_xaRightMargin;
				what = "right margin"; break;
			default: break;
			}
			/* applied at the tracked x means ~80 px of motion, not the
			 * silent cancel (delta 0) the broken wiring produced.
			 * The column origin can still drift by a frame while the
			 * window settles, so assert magnitude rather than sign:
			 * anything in the same ballpark as the pointer travel is
			 * a faithful apply, 0 is a cancel. */
			UT_sint32 mag = d < 0 ? -d : d;
			if (mag >= rg->tlu(30) && mag <= rg->tlu(400)) {
				g_print("ruler: %s drag off-band applied at tracked x "
						"(delta %d lu)\n", what, d);
			} else {
				g_printerr("FAIL ruler: %s off-band drag delta %d lu, "
						   "expected magnitude %d..%d\n",
						   what, d, rg->tlu(30), rg->tlu(400));
				fails++;
			}
		}
	}

	/* ---- left ruler: margin drag straying horizontally applies ---- */
	set_phase("ruler: left margin drag");
	pump_for(300);

	AP_LeftRulerInfo li0;
	view->getLeftRulerInfo(&li0);
	int margY = -1;
	for (int py = 8; py < lw_h - 8; py += 2) {
		if (probe_left_ruler(lr, lw_w / 2, py) ==
				AP_LeftRuler::DW_TOPMARGIN) {
			margY = py;
			break;
		}
	}
	if (margY < 0) {
		g_printerr("FAIL ruler: no top-margin marker on left ruler\n");
		fails++;
	} else {
		bool marginGrabbed = false;
		GR_Graphics *lg = lr->getGraphics();
		for (int attempt = 0; attempt < 4 && !marginGrabbed; attempt++) {
			wait_pt_idle(view);
			rul_move(inj, lw, lw_w / 2, margY);
			rul_btn(inj, true);
			if (g_trace)
				fprintf(stderr, "margin press @%d: got=%d landed=%d\n",
						margY, lr->draggingWhatForTest(),
						lg->tdu(lr->lastPressYForTest()));
			if (lr->draggingWhatForTest() == AP_LeftRuler::DW_TOPMARGIN) {
				marginGrabbed = true;
				break;
			}
			rul_btn(inj, false);
			rul_gap();
			margY = 2 * margY - lg->tdu(lr->lastPressYForTest());
			for (int py = margY - 10; py <= margY + 10; py += 2) {
				if (probe_left_ruler(lr, lw_w / 2, py) ==
						AP_LeftRuler::DW_TOPMARGIN) {
					margY = py;
					break;
				}
			}
		}
		if (!marginGrabbed) {
			g_printerr("FAIL ruler: left press did not grab top margin\n");
			fails++;
		} else {
			rul_move(inj, lw, lw_w / 2, margY + 40);        /* in-band */
			rul_move(inj, lw, lw_w + 40, margY + 80);       /* off-band */
			rul_btn(inj, false);
			rul_gap();
			AP_LeftRulerInfo li1;
			view->getLeftRulerInfo(&li1);
			UT_sint32 d = li1.m_yTopMargin - li0.m_yTopMargin;
			UT_sint32 lo = rg->tlu(55), hi = rg->tlu(115);
			if (d >= lo && d <= hi) {
				g_print("ruler: left-ruler margin drag off-band applied "
						"at tracked y (delta %d lu)\n", d);
			} else {
				g_printerr("FAIL ruler: left margin off-band delta %d "
						   "lu, expected %d..%d\n", d, lo, hi);
				fails++;
			}
		}
	}

	/* ---- double-click opens the Paragraph dialog ---- */
	set_phase("ruler: double-click");

	{
		/* the dialog runs modally inside the second press's dispatch;
		 * the stray sweep answers it within ~150ms and bumps the
		 * counter, which is also the proof the dialog appeared */
		int closedBefore = sctx.straysClosed;
		int cx = tabX >= 0 ? tabX : tw_w / 2;
		rul_move(inj, tw, cx, tw_h / 2);
		rul_btn(inj, true);
		rul_btn(inj, false);
		rul_btn(inj, true);
		rul_btn(inj, false);
		pump_for(800);
		if (sctx.straysClosed > closedBefore)
			g_print("ruler: double-click opened a dialog\n");
		else {
			g_printerr("FAIL ruler: double-click opened no dialog\n");
			fails++;
		}
		pump_for(150);
	}

	set_phase("ruler: done");
	g_source_remove(sweeper);
	g_print("ruler: %s (%d failures)\n", fails ? "FAILURES" : "all ok",
			fails);
	return fails ? 1 : 0;
#else
	(void)app; (void)scratch;
	g_printerr("ruler: built without X11 backend\n");
	return 77;
#endif
}

/* ---------------- AbiWidget drive ---------------- */

static int drive_abiwidget(const char *scratch_uri)
{
	alarm(0); /* the sections' call_guard alarms are the real bound;
			   * the wrapper's timeout covers a total wedge */
	DriveCtx ctx {nullptr, {}, 0, false, false, 0, 0};
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

/* ---------------- COV13: event plumbing + graphics drive ----------------
 *
 * The --ev leg targets af/ev/gtk and af/gr/gtk, which sit below the
 * reach of the dialog sweep: EV_UnixMouse / ev_UnixKeyboard only run
 * on real GdkEvent input, the EV_UnixMenu action layer only runs on
 * menu dispatch, and the cairo image/media classes only run on real
 * image data.  Two prongs:
 *
 *  - direct calls on exported product APIs (menu action creation /
 *    activation / state changes, charDataEvent, image
 *    codec + blip effects, print graphics, cursor name table) and
 *    media/blip-effect fixtures loaded into a live frame;
 *  - the input plumbing itself: the GTK event controllers hand their
 *    current GdkEvent to EV_UnixMouse / ev_UnixKeyboard, which is
 *    null whenever the handler runs outside a real GDK dispatch —
 *    and GTK4 exports no event constructors, so in-process
 *    fabrication is impossible anyway.  Calling the same methods
 *    with a null event runs every entry path, modifier map and
 *    edit-event-mapper dispatch they contain; the gdk_*_event_get_*
 *    getters are g_return_val_if_fail'd, so they log a critical and
 *    return defaults rather than crashing. */

#include "ev_UnixKeyboard.h"
#include "ev_UnixMouse.h"
#include "ev_UnixMenuPopup.h"
#include "ev_EditEventMapper.h"
#include "ap_Prefs_SchemeIds.h"
#include "gr_Graphics.h"
#include "gr_UnixImage.h"
#include "gr_CairoImage.h"
#include "gr_CairoPrintGraphics.h"
#include "gr_GtkMediaManager.h"
#include "fl_DocLayout.h"
#include "fl_TOCLayout.h"
#include "fp_TOCContainer.h"
#include "fp_Line.h"
#include "fp_ContainerObject.h"

#include <optional>
#include "ut_bytebuf.h"
#include "xap_Dlg_Print.h"

/* find a widget of a given GType anywhere below w (used for the doc
 * drawing area and for the context-menu popover) */
static GtkWidget *find_type(GtkWidget *w, GType t)
{
	if (g_type_check_instance_is_a(
			reinterpret_cast<GTypeInstance *>(w), t))
		return w;
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c)) {
		if (GtkWidget *f = find_type(c, t))
			return f;
	}
	return nullptr;
}

/* drive the input-event classes the way the frame's _fe handlers do:
 * they forward gtk_event_controller_get_current_event() — null
 * outside a real GDK dispatch — so the same methods run with a null
 * event here.  The gdk_*_event_get_* getters are g_return_val_if
 * fail'd, so they log a critical and return defaults while every
 * entry path, modifier map and mapper dispatch still executes. */
static void drive_input_events(AV_View *view, EV_EditEventMapper *eem)
{
	if (!view || !eem)
		return;
	EV_UnixMouse mouse(eem);
	mouse.mouseClick(view, nullptr, 100.0, 120.0, 1);
	mouse.mouseClick(view, nullptr, 100.0, 120.0, 2);
	mouse.mouseUp(view, nullptr, 100.0, 120.0);
	mouse.mouseMotion(view, nullptr, 110.0, 125.0);
	mouse.mouseMotion(view, nullptr, 130.0, 140.0);
	mouse.mouseScroll(view, nullptr, 100.0, 120.0);
	ev_UnixKeyboard kbd(eem);
	kbd.keyPressEvent(view, nullptr);
}

/* load a small xml fixture into its own frame; the window is added to
 * preexisting so the stray sweep does not answer it like a dialog.
 * Returns the new frame's view via view_out. */
static bool load_fixture_frame(AP_UnixApp *app, DriveCtx &ctx,
							   const std::string &path, AV_View **view_out)
{
	if (view_out)
		*view_out = nullptr;
	XAP_Frame *frame = app->newFrame();
	if (!frame)
		return false;
	char *uri = g_strdup_printf("file://%s", path.c_str());
	UT_Error err = frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	frame->show();
	/* register the new toplevel BEFORE pumping: the stray sweep ticks
	 * every 150 ms inside the layout pump below, and a window it
	 * doesn't know is a stray it closes — leaving the returned view
	 * dangling */
	{
		std::vector<GtkWidget *> tops;
		sweep_guard([&tops] { tops = toplevels(); });
		for (GtkWidget *w : tops)
			if (!contains(ctx.preexisting, w))
				ctx.preexisting.push_back(w);
	}
	pump_for(1200);
	/* any toplevel that appeared during layout gets the same
	 * protection */
	std::vector<GtkWidget *> tops;
	sweep_guard([&tops] { tops = toplevels(); });
	for (GtkWidget *w : tops)
		if (!contains(ctx.preexisting, w))
			ctx.preexisting.push_back(w);
	if (view_out)
		*view_out = frame->getCurrentView();
	return err == UT_OK;
}

static std::string write_fixture(const char *name, const std::string &xml)
{
	std::string path = std::string(g_get_tmp_dir()) + "/ui-drive-" + name;
	if (!g_file_set_contents(path.c_str(), xml.data(),
							 static_cast<gssize>(xml.size()), nullptr))
		return "";
	return path;
}

static gchar *file_b64(const char *path)
{
	gchar *buf = nullptr;
	gsize len = 0;
	if (!g_file_get_contents(path, &buf, &len, nullptr))
		return nullptr;
	gchar *b64 = g_base64_encode(reinterpret_cast<const guchar *>(buf), len);
	g_free(buf);
	return b64;
}

static int drive_ev(AP_UnixApp *app, const char *scratch)
{
	alarm(0);
	DriveCtx ctx {nullptr, {}, 0, false, false, 0, 0};
	sweep_guard([&ctx] { ctx.preexisting = toplevels(); });

	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}
	char *uri = g_strdup_printf("file://%s", scratch);
	frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	frame->show();
	pump_for(800);

	AV_View *view = frame->getCurrentView();
	GtkWidget *win = nullptr;
	{
		std::vector<GtkWidget *> after;
		sweep_guard([&after] { after = toplevels(); });
		for (GtkWidget *w : after)
			if (!contains(ctx.preexisting, w)) {
				win = w;
				break;
			}
	}
	ctx.root = win;
	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);

#define EV_SECTION(name, ...) call_guard([&] { __VA_ARGS__; }, name)

	/* the fixture-loaded views: captured so later sections can reach
	 * the fixture's layout/embed managers */
	AV_View *mediaView = nullptr;

	/* ---- input-event plumbing: EV_UnixMouse + ev_UnixKeyboard
	 * entry paths and mapper dispatch via a null current event —
	 * exactly what the frame's handlers see outside a GDK dispatch.
	 * Runs early: a later rescued fault can strand GLib locks and
	 * wedge everything downstream, so the reliable coverage goes
	 * first and the crash-prone widget churn goes last. ---- */
	EV_SECTION("input events",
		drive_input_events(view, app->getEditEventMapper()););

	/* ---- charDataEvent: the text half of keyPressEvent needs no
	 * GdkEvent — empty, ascii, UTF-8, astral and modified input ---- */
	EV_SECTION("charDataEvent",
		if (view && app->getEditEventMapper()) {
			ev_UnixKeyboard kbd(app->getEditEventMapper());
			kbd.charDataEvent(view, static_cast<EV_EditBits>(0), "ab", 2);
			kbd.charDataEvent(view, static_cast<EV_EditBits>(0), "", 0);
			kbd.charDataEvent(view, static_cast<EV_EditBits>(0),
							  "\xC3\xA9", 2);           /* é */
			kbd.charDataEvent(view, static_cast<EV_EditBits>(0),
							  "\xF0\x9D\x94\x80", 4);   /* astral */
			kbd.charDataEvent(view, static_cast<EV_EditBits>(
								  EV_EMS_CONTROL), "a", 1);
			pump_for(200);
		});

	/* ---- cursor name table: every cursor enum through setCursor
	 * walks the switch that maps them to GTK cursor names ---- */
	EV_SECTION("cursors",
		GR_Graphics *gr = view ? view->getGraphics() : nullptr;
		if (gr) {
			for (int c = GR_Graphics::GR_CURSOR_DEFAULT;
				 c <= GR_Graphics::GR_CURSOR_COPYTEXT; c++)
				gr->setCursor(static_cast<GR_Graphics::Cursor>(c));
			gr->setCursor(GR_Graphics::GR_CURSOR_IBEAM);
			(void)gr->getCursor();
		});

	/* ---- raster/vector image codecs + blip effects: deterministic
	 * coverage of the whole GR_UnixImage / GR_RSVGVectorImage API
	 * surface without depending on draw timing ---- */
	EV_SECTION("images",
		const char *top = getenv("ABINOVA_TEST_SRC_DIR");
		std::string png = std::string(top ? top : ".") +
			"/test/wp/cov07/red.png";
		GR_Graphics *gr = view ? view->getGraphics() : nullptr;
		GdkPixbuf *px = gdk_pixbuf_new_from_file(png.c_str(), nullptr);
		if (px) {
			GR_UnixImage img("red.png", px); /* steals the ref */
			UT_ConstByteBufPtr buf;
			if (img.convertToBuffer(buf) && buf && buf->getLength()) {
				GR_UnixImage img2("red2.png", GR_Image::GRT_Raster);
				img2.convertFromBuffer(buf, "image/png", 64, 64);
				img2.hasAlpha();
				img2.isTransparentAt(1, 1);
				img2.rowStride();
				img2.getData();
				img2.scale(32, 32);
				if (gr) {
					UT_Rect rec(0, 0, img2.getDisplayWidth() + 10,
								img2.getDisplayHeight() + 10);
					img2.scaleImageTo(gr, rec);
				}
				/* all four blip-effect branches */
				GR_BlipEffects fx;
				fx.duotone = true;
				fx.duoLo = UT_RGBColor(16, 32, 48);
				fx.duoHi = UT_RGBColor(255, 255, 255);
				img2.applyBlipEffects(fx);
				GR_BlipEffects fx2;
				fx2.grayscale = true;
				fx2.lum = true;
				fx2.lumBright = 0.2;
				fx2.lumContrast = 0.1;
				fx2.alphaMod = 0.5;
				img2.applyBlipEffects(fx2);
			}
			img.saveToPNG("/tmp/ui-drive-red.png");
		}
		/* vector image: rsvg decode + render + segment */
		UT_ByteBuf *svgb = new UT_ByteBuf();
		static const char svg[] =
			"<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24'>"
			"<rect x='1' y='1' width='22' height='22' fill='#3080c0'/></svg>";
		svgb->append(reinterpret_cast<const UT_Byte *>(svg), sizeof(svg) - 1);
		UT_ConstByteBufPtr svgp(svgb);
		GR_RSVGVectorImage vimg("t.svg");
		if (vimg.convertFromBuffer(svgp, "image/svg+xml", 48, 48)) {
			vimg.hasAlpha();
			vimg.isTransparentAt(2, 2);
			vimg.getDisplayWidth();
			vimg.getDisplayHeight();
			if (gr) {
				UT_Rect rec(0, 0, 48, 48);
				vimg.scaleImageTo(gr, rec);
				GR_Image *seg = vimg.createImageSegment(gr, rec);
				delete seg;
			}
		});

	/* ---- print graphics: the PRINTER_ONLY cairo subclass ---- */
	EV_SECTION("printgraphics",
		cairo_surface_t *surf = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, 612, 792);
		cairo_t *cr = cairo_create(surf);
		{
			GR_CairoPrintGraphics pg(cr, 144);
			pg.getCapability();
			pg.queryProperties(GR_Graphics::DGP_SCREEN);
			pg.queryProperties(GR_Graphics::DGP_PAPER);
			pg.queryProperties(GR_Graphics::DGP_OPAQUEOVERLAY);
			pg.getResolutionRatio();
			pg.setResolutionRatio(1.0);
			pg.getGUIFont();
			pg.startPrint();
			pg.startPage("p1", 1, true, 612, 792);
			pg.startPage("p2", 2, false, 792, 612);
			pg.canQuickPrint();
			pg.endPrint();
			UT_Rect q(0, 0, 10, 10);
			pg.queueDraw(&q);
		}
		/* pg's dtor cairo_destroys the context it borrowed */
		cairo_surface_destroy(surf));

	/* ---- printerless printing: the export path needs no printer at
	 * all (ACTION_EXPORT writes a PDF through the full
	 * BeginPrint/PrintPage/EndPrint pipeline) and the own-drawn
	 * preview window is stray-swept.  The real GtkPrintUnixDialog is
	 * NOT invoked here — its queued sync D-Bus sources crash under
	 * our nested sweeps; the --id 8 leg covers it.  Runs early: the
	 * media-fixture leg is the flakiest on this box and a fatal there
	 * must not starve this coverage ---- */
	EV_SECTION("print export",
		XAP_DialogFactory *df = static_cast<XAP_DialogFactory *>(
			frame->getDialogFactory());
		if (df && view) {
			XAP_Dialog_Print *pd = static_cast<XAP_Dialog_Print *>(
				df->requestDialog(XAP_DIALOG_ID_PRINT));
			if (pd) {
				pd->PrintDirectly(frame, "/tmp/ui-drive-print.pdf",
								  nullptr);
				pump_for(300);
				df->releaseDialog(pd);
			}
		});

	EV_SECTION("print preview",
		const EV_EditMethodContainer *emc =
			XAP_App::getApp()->getEditMethodContainer();
		AV_View *av = frame->getCurrentView();
		if (emc && av) {
			EV_EditMethodCallData emcd;
			/* own preview window — stray-swept */
			EV_EditMethod *em =
				emc->findEditMethodByName("printPreview");
			if (em)
				em->Fn(av, &emcd);
			pump_for(400);
		});

	/* ---- fixtures: an abwn with audio+video embeds drives the media
	 * manager's makeEmbedView/render/setRun/release path, and an abw
	 * whose image carries all the blip-effect props drives the
	 * importer → applyBlipEffects chain ---- */
	EV_SECTION("fixtures",
		const char *top = getenv("ABINOVA_TEST_SRC_DIR");
		std::string png = std::string(top ? top : ".") +
			"/test/wp/cov07/red.png";
		gchar *b64 = file_b64(png.c_str());
		if (b64) {
			std::string media =
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<abiword template=\"false\" fileformat=\"1.0\" "
				"xmlns=\"http://www.abisource.com/awml.dtd\" "
				"version=\"0.99.2\" xml:space=\"preserve\" "
				"props=\"lang:en-US\">\n<data>\n"
				"<d name=\"obj-vid\" mime-type=\"video/mp4\" "
				"base64=\"yes\">QUJDREVGRw==</d>\n"
				"<d name=\"obj-aud\" mime-type=\"audio/mpeg\" "
				"base64=\"yes\">QUJDREVGRw==</d>\n"
				"<d name=\"snapshot-png-obj-vid\" mime-type=\"image/png\" "
				"base64=\"yes\">" + std::string(b64) + "</d>\n"
				"<d name=\"snapshot-png-obj-aud\" mime-type=\"image/png\" "
				"base64=\"yes\">" + std::string(b64) + "</d>\n"
				/* hostile: traversal in both the dataid and the
				 * media-name — neither may escape the temp dir or
				 * the export member name */
				"<d name=\"obj-../evil\" mime-type=\"video/mp4\" "
				"base64=\"yes\">QUJDREVGRw==</d>\n"
				"<d name=\"obj-plain\" mime-type=\"application/octet-stream\" "
				"base64=\"yes\">QUJDREVGRw==</d>\n"
				"</data>\n<section>\n"
				"<p>Video: <embed dataid=\"obj-vid\" props=\""
				"embed-type:media; media-kind:video; "
				"media-name:t.mp4\"/></p>\n"
				"<p>Audio: <embed dataid=\"obj-aud\" props=\""
				"embed-type:media; media-kind:audio; "
				"media-name:t.mp3\"/></p>\n"
				"<p>Evil: <embed dataid=\"obj-../evil\" props=\""
				"embed-type:media; media-kind:video; "
				"media-name:../../evil.sh\"/></p>\n"
				/* no props at all → missing embed-type → the
				 * \"default\" manager, not std::string(nullptr) */
				"<p>Plain: <embed dataid=\"obj-plain\"/></p>\n"
				"</section>\n</abiword>\n";
			std::string mp = write_fixture("media.abwn", media);
			if (!mp.empty())
				load_fixture_frame(app, ctx, mp, &mediaView);

			std::string blip =
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<abiword template=\"false\" fileformat=\"1.0\" "
				"xmlns=\"http://www.abisource.com/awml.dtd\" "
				"version=\"0.99.2\" xml:space=\"preserve\" "
				"props=\"lang:en-US\">\n<data>\n"
				"<d name=\"red.png\" mime-type=\"image/png\" "
				"base64=\"yes\">" + std::string(b64) + "</d>\n"
				"</data>\n<section>\n"
				"<p>a<image dataid=\"red.png\" props=\""
				"image-duotone:102030 ffffff; image-grayscale:1; "
				"image-lum:0.2 0.1; image-alpha-mod:0.5; "
				"width:1.0in; height:1.0in\"/></p>\n"
				"</section>\n</abiword>\n";
			std::string bp = write_fixture("blip.abw", blip);
			if (!bp.empty())
				load_fixture_frame(app, ctx, bp, nullptr);
			g_free(b64);
		});

	/* ---- media manager: getDataItem → tmpfile → GtkVideo open ---- */
	EV_SECTION("media modify",
		FV_View *fv = mediaView ? static_cast<FV_View *>(mediaView)
							  : nullptr;
		if (fv && fv->getLayout()) {
			GR_EmbedManager *em = fv->getLayout()->getEmbedManager("media");
			GR_GtkMediaManager *mm =
				static_cast<GR_GtkMediaManager *>(em);
			if (mm) {
				mm->isDefault();
				mm->getObjectType();
				mm->getMimeType();
				mm->getMimeTypeDescription();
				mm->getMimeTypeSuffix();
				mm->isEdittable(0);
				mm->isResizeable(0);
				/* opens a transient GtkVideo window — the stray
				 * sweep answers it */
				mm->modify(0);
				pump_for(400);
				/* uid 2 is the "obj-../evil" item — its dataid is
				 * sanitized into a flat tmpfile prefix, so this
				 * must still launch (and get swept) rather than
				 * traverse out of the temp dir */
				mm->modify(2);
				pump_for(300);
				/* bounds guards on the embed-view vector */
				UT_Rect r(0, 0, 10, 10);
				mm->getWidth(-1);
				mm->getAscent(-1);
				mm->render(-1, r);
			}
			/* the prop-less embed landed on the "default" manager —
			 * its modify is a no-op but must not crash */
			GR_EmbedManager *dm =
				fv->getLayout()->getEmbedManager("default");
			if (dm)
				dm->modify(0);
		});

	/* ---- menu action layer: refresh walk, lazy action creation for
	 * all three shapes (plain / checkable / shared radio), direct
	 * menuEvent dispatch and the GAction signal paths.  Last: the
	 * action churn is the most crash-prone part of the leg. ---- */
	EV_SECTION("menu",
		EV_UnixMenu *menu = static_cast<EV_UnixMenu *>(
			frame->getMainMenu());
		if (menu && view) {
			menu->refreshMenu(view);
			static const _Ap_Menu_Id ids[] = {
				/* plain */
				AP_MENU_ID_EDIT_UNDO, AP_MENU_ID_EDIT_REDO,
				AP_MENU_ID_EDIT_SELECTALL, AP_MENU_ID_EDIT_COPY,
				AP_MENU_ID_EDIT_CUT, AP_MENU_ID_EDIT_PASTE,
				AP_MENU_ID_FMT, AP_MENU_ID_FMT_BOLD,
				AP_MENU_ID_FMT_ITALIC, AP_MENU_ID_FMT_UNDERLINE,
				AP_MENU_ID_FMT_STRIKE, AP_MENU_ID_FMT_SUPERSCRIPT,
				AP_MENU_ID_FMT_SUBSCRIPT,
				AP_MENU_ID_INSERT_BREAK,
				AP_MENU_ID_TOOLS_SPELLING,
				/* checkable */
				AP_MENU_ID_VIEW_RULER, AP_MENU_ID_VIEW_STATUSBAR,
				AP_MENU_ID_VIEW_SHOWPARA, AP_MENU_ID_VIEW_GRIDLINES,
				AP_MENU_ID_VIEW_LOCKSTYLES, AP_MENU_ID_VIEW_TB_1,
				AP_MENU_ID_VIEW_TB_2,
				AP_MENU_ID_TOOLS_AUTOSPELL,
				/* radio groups */
				AP_MENU_ID_VIEW_ZOOM_200, AP_MENU_ID_VIEW_ZOOM_100,
				AP_MENU_ID_VIEW_ZOOM_75, AP_MENU_ID_VIEW_ZOOM_50,
				AP_MENU_ID_VIEW_ZOOM_WHOLE, AP_MENU_ID_VIEW_ZOOM_WIDTH,
				AP_MENU_ID_VIEW_NORMAL, AP_MENU_ID_VIEW_WEB,
				AP_MENU_ID_VIEW_PRINT,
				AP_MENU_ID_ALIGN_LEFT, AP_MENU_ID_ALIGN_CENTER,
				AP_MENU_ID_ALIGN_RIGHT, AP_MENU_ID_ALIGN_JUSTIFY
			};
			for (_Ap_Menu_Id id : ids)
				menu->ensureAction(static_cast<XAP_Menu_Id>(id));
			/* the lookup must walk both rec vectors and miss too */
			menu->lookupAction(static_cast<XAP_Menu_Id>(
								   AP_MENU_ID_EDIT_SELECTALL));
			menu->lookupAction(static_cast<XAP_Menu_Id>(
								   AP_MENU_ID_FILE_OPEN));
			menu->refreshMenu(view);

			/* direct dispatch — safe methods only (no dialogs, no
			 * window killers; see never_activate for the class) */
			menu->menuEvent(static_cast<XAP_Menu_Id>(
								AP_MENU_ID_EDIT_SELECTALL));
			menu->menuEvent(static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_UNDO));
			menu->menuEvent(static_cast<XAP_Menu_Id>(AP_MENU_ID_EDIT_COPY));
			pump_for(150);

			/* GAction "activate" → _wd::s_onActivate → menuEvent +
			 * refresh + the frame-survival check */
			GAction *a = menu->lookupAction(static_cast<XAP_Menu_Id>(
											  AP_MENU_ID_EDIT_SELECTALL));
			if (a)
				g_action_activate(a, nullptr);
			pump_for(100);

			/* checkable "change-state" → _wd::s_onChangeState bool
			 * branch; toggle twice to restore */
			GAction *c = menu->lookupAction(static_cast<XAP_Menu_Id>(
											  AP_MENU_ID_VIEW_SHOWPARA));
			if (c) {
				g_action_change_state(c, g_variant_new_boolean(TRUE));
				pump_for(100);
				g_action_change_state(c, g_variant_new_boolean(FALSE));
			}
			/* radio "change-state" → string branch: the shared group
			 * action takes "<menu id>" as the target value */
			GAction *r = menu->lookupAction(static_cast<XAP_Menu_Id>(
											  AP_MENU_ID_VIEW_ZOOM_100));
			if (r) {
				char t[32];
				g_snprintf(t, sizeof(t), "%u",
						   static_cast<unsigned>(AP_MENU_ID_VIEW_ZOOM_100));
				g_action_change_state(r, g_variant_new_string(t));
				pump_for(100);
				g_snprintf(t, sizeof(t), "%u",
						   static_cast<unsigned>(AP_MENU_ID_VIEW_ZOOM_200));
				g_action_change_state(r, g_variant_new_string(t));
			}
			/* toolbar toggle → viewTB1 tears down and rebuilds the
			 * EV_UnixToolbar widgets — the biggest ev_UnixToolbar /
			 * ev_UnixFontCombo coverage lever */
			GAction *tb = menu->lookupAction(static_cast<XAP_Menu_Id>(
											   AP_MENU_ID_VIEW_TB_1));
			if (tb) {
				g_action_change_state(tb, g_variant_new_boolean(FALSE));
				pump_for(400);
				g_action_change_state(tb, g_variant_new_boolean(TRUE));
				pump_for(400);
			}
			menu->refreshMenu(view);
			pump_for(200);
		});

	/* ---- popup menu synthesis: covers the isPopup menu-item branch
	 * and the popup class without going through the nested modal
	 * loop the real context menu uses ---- */
	EV_SECTION("popup synth",
		EV_UnixMenuPopup *popup = new EV_UnixMenuPopup(
			static_cast<XAP_UnixApp *>(app), frame,
			"ContextText", AP_PREF_DEFAULT_StringSet);
		if (popup) {
			popup->synthesizeMenuPopup();
			popup->refreshMenu(view);
			delete popup;
		});

	g_source_remove(sweeper);
	call_guard([&] { pump_for(300); }, "tail");
	__gcov_dump();
	g_print("drive: ev done — %d criticals, %d crashed, %d wedged\n",
			g_criticals, g_crashed_widgets, g_wedged_widgets);
	return 0;
}

/* ---------------- COV14: text/fmt/gtk drive ----------------
 *
 * The --fmt leg exercises the gtk half of the view layer:
 *  - FvTextHandle / FV_UnixSelectionHandles: the cursor and
 *    selection handles that live in the frame's document GtkOverlay
 *    — driven through the real view (visual selection on, selection
 *    extension, scroll re-updates) plus a standalone handle where the
 *    whole mode/position/visibility/drag API runs deterministically;
 *  - FV_UnixPasteTag: a real copy+paste through the session clipboard
 *    arms the floating paste-options button, whose popover entries
 *    are clicked for real;
 *  - FV_UnixVisualDrag / FV_UnixVisualInlineImage / FV_UnixFrameEdit:
 *    the button/drag/release entry points plus the drag-out-of-window
 *    paths — local drag buffer, temp file, content providers and
 *    gdk_drag_begin, which with a live wayland compositor runs
 *    through GDK's wayland drag/surface code.
 */

/* collect widgets carrying a CSS class anywhere below w */
static void find_css_class(GtkWidget *w, const char *cls,
						   std::vector<GtkWidget *> &out)
{
	if (gtk_widget_has_css_class(w, cls))
		out.push_back(w);
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		find_css_class(c, cls, out);
}

/* every GtkButton below w */
static void find_buttons(GtkWidget *w, std::vector<GtkWidget *> &out)
{
	if (GTK_IS_BUTTON(w))
		out.push_back(w);
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		find_buttons(c, out);
}

static int g_handle_dragged = 0;
static int g_handle_finished = 0;
static void count_handle_dragged(FvTextHandle *, FvTextHandlePosition,
								 gint, gint, gpointer)
{
	g_handle_dragged++;
}
static void count_handle_finished(FvTextHandle *, FvTextHandlePosition,
								  gpointer)
{
	g_handle_finished++;
}

/* emit the GtkGestureDrag signal sequence on every drag gesture
 * attached to w — GTK4 hands us no synthetic event constructors, but
 * the gesture's own signals may be emitted directly, which runs the
 * handle's drag-begin/update/end handlers (and its handle-dragged /
 * drag-finished emissions) exactly as a real finger-drag would */
static void emit_drag_gestures(GtkWidget *w)
{
	GListModel *ctrls = gtk_widget_observe_controllers(w);
	guint n = g_list_model_get_n_items(ctrls);
	for (guint i = 0; i < n; i++) {
		GtkEventController *c = GTK_EVENT_CONTROLLER(
			g_list_model_get_item(ctrls, i));
		if (GTK_IS_GESTURE_DRAG(c)) {
			g_signal_emit_by_name(c, "drag-begin", 10.0, 10.0);
			g_signal_emit_by_name(c, "drag-update", 6.0, 4.0);
			g_signal_emit_by_name(c, "drag-update", 9.0, 7.0);
			g_signal_emit_by_name(c, "drag-end", 9.0, 7.0);
		}
		g_object_unref(c);
	}
	g_object_unref(ctrls);
}

/* run a widget's draw callback synchronously, whether or not the
 * compositor ever presents our window — snapshot_child walks the
 * widget's own snapshot vfunc, which for a GtkDrawingArea runs its
 * draw func */
static void snapshot_widget(GtkWidget *w)
{
	GtkWidget *p = gtk_widget_get_parent(w);
	if (!p)
		return;
	GtkSnapshot *snap = gtk_snapshot_new();
	gtk_widget_snapshot_child(p, w, snap);
	GskRenderNode *node = gtk_snapshot_free_to_node(snap);
	if (node)
		gsk_render_node_unref(node);
}

static int drive_fmt(AP_UnixApp *app, const char *scratch)
{
	alarm(0);
	DriveCtx ctx {nullptr, {}, 0, false, false, 0, 0};
	sweep_guard([&ctx] { ctx.preexisting = toplevels(); });

	GdkDisplay *disp = gdk_display_get_default();
	g_print("drive: fmt — gdk backend %s\n",
			disp ? G_OBJECT_TYPE_NAME(disp) : "none");

	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}
	char *uri = g_strdup_printf("file://%s", scratch);
	frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	frame->show();
	pump_for(800);

	FV_View *view = static_cast<FV_View *>(frame->getCurrentView());
	GtkWidget *win = nullptr;
	{
		std::vector<GtkWidget *> after;
		sweep_guard([&after] { after = toplevels(); });
		for (GtkWidget *w : after)
			if (!contains(ctx.preexisting, w)) {
				win = w;
				break;
			}
	}
	ctx.root = win;
	guint sweeper = g_timeout_add(150, stray_sweep_cb, &ctx);

#define FMT_SECTION(name, ...) call_guard([&] { __VA_ARGS__; }, name)

	/* ---- standalone FvTextHandle: the whole mode/position/
	 * visibility/drag API runs deterministically; a direct snapshot
	 * fires the handle's draw callbacks even if the compositor never
	 * presents the window.  Runs first — a rescued fault in a later
	 * section can strand state, so the reliable coverage goes early. */
	FMT_SECTION("texthandle",
		GtkWidget *hwin = gtk_window_new();
		GtkWidget *ovl = gtk_overlay_new();
		gtk_overlay_set_child(GTK_OVERLAY(ovl),
							  gtk_label_new("anchor"));
		gtk_window_set_child(GTK_WINDOW(hwin), ovl);
		gtk_window_set_default_size(GTK_WINDOW(hwin), 400, 300);
		gtk_window_present(GTK_WINDOW(hwin));
		ctx.preexisting.push_back(hwin); /* keep the stray sweep off */
		pump_for(200);

		FvTextHandle *h = _fv_text_handle_new(ovl);
		g_signal_connect(h, "handle-dragged",
						 G_CALLBACK(count_handle_dragged), nullptr);
		g_signal_connect(h, "drag-finished",
						 G_CALLBACK(count_handle_finished), nullptr);
		GdkRectangle r {60, 40, 1, 16};
		_fv_text_handle_set_mode(h, FV_TEXT_HANDLE_MODE_CURSOR);
		/* selection positions are rejected in cursor mode */
		_fv_text_handle_set_position(
			h, FV_TEXT_HANDLE_POSITION_SELECTION_START, &r);
		_fv_text_handle_set_position(h, FV_TEXT_HANDLE_POSITION_CURSOR,
									 &r);
		_fv_text_handle_set_visible(h, FV_TEXT_HANDLE_POSITION_CURSOR,
									TRUE);
		(void)_fv_text_handle_get_mode(h);
		(void)_fv_text_handle_get_is_dragged(
			h, FV_TEXT_HANDLE_POSITION_CURSOR);
		_fv_text_handle_set_mode(h, FV_TEXT_HANDLE_MODE_SELECTION);
		GdkRectangle r2 {140, 90, 1, 16};
		_fv_text_handle_set_position(
			h, FV_TEXT_HANDLE_POSITION_SELECTION_START, &r2);
		_fv_text_handle_set_position(
			h, FV_TEXT_HANDLE_POSITION_SELECTION_END, &r);
		_fv_text_handle_set_visible(
			h, FV_TEXT_HANDLE_POSITION_SELECTION_START, TRUE);
		pump_for(250);
		/* gestures + draw callbacks on the real handle widgets */
		{
			std::vector<GtkWidget *> hw;
			find_css_class(ovl, "cursor-handle", hw);
			for (GtkWidget *d : hw) {
				emit_drag_gestures(d);
				snapshot_widget(d);
			}
		}
		pump_for(200);
		/* a dragged handle ignores set_visible, an undragged one takes
		 * it — both arms of the guard */
		_fv_text_handle_set_visible(
			h, FV_TEXT_HANDLE_POSITION_SELECTION_END, FALSE);
		/* mode NONE hides both handles; positions are rejected in it */
		_fv_text_handle_set_mode(h, FV_TEXT_HANDLE_MODE_NONE);
		_fv_text_handle_set_position(h, FV_TEXT_HANDLE_POSITION_CURSOR,
									 &r);
		(void)_fv_text_handle_get_mode(h);
		g_object_unref(h);          /* finalize: widgets + gestures */
		gtk_window_destroy(GTK_WINDOW(hwin));
		pump_for(200));

	/* ---- selection handles through the real view: visual selection
	 * on, then empty → non-empty selections so _updateSelectionHandles
	 * walks the cursor and the two-selection-handle modes on the
	 * frame's own overlay ---- */
	FMT_SECTION("selhandles",
		if (view) {
			view->setVisualSelectionEnabled(true);
			view->warpInsPtToXY(80, 100, true);
			pump_for(250);
			/* extend → both handles in selection mode */
			view->extSelToXY(220, 180, false);
			pump_for(250);
			/* scrolls re-run the handle update */
			view->setXScrollOffset(40);
			view->setYScrollOffset(40);
			view->setXScrollOffset(0);
			view->setYScrollOffset(0);
			pump_for(200);
			/* drag gestures on the frame's real handles feed the
			 * view's updateSelection* callbacks */
			if (win) {
				std::vector<GtkWidget *> hw;
				find_css_class(win, "cursor-handle", hw);
				for (GtkWidget *d : hw)
					emit_drag_gestures(d);
			}
			pump_for(250);
			view->setVisualSelectionEnabled(false); /* hide() path */
			view->setVisualSelectionEnabled(true);
			pump_for(200);
		});

	/* ---- paste options tag: a real copy + paste through the session
	 * clipboard arms the floating tag; each popover entry then runs
	 * _optionPicked → applyPasteTagOption for real ---- */
	FMT_SECTION("paste tag",
		if (view && win) {
			view->cmdSelect(0, 0, FV_DOCPOS_BOD, FV_DOCPOS_EOD);
			view->cmdCopy();
			pump_for(250);
			/* paste at a known body position: the tag is suppressed
			 * inside tables/headers/frames, and much of rich.abw is
			 * one — just past BOD is guaranteed body text.  Copy and
			 * paste back to back with no pumping: on a live session a
			 * clipboard manager can steal ownership within a tick,
			 * flipping gdk_clipboard_is_local off and forcing the
			 * async server read path */
			PT_DocPosition body = view->mapDocPos(FV_DOCPOS_BOD) + 2;
			auto paste_mid = [&] {
				view->cmdSelect(0, 0, FV_DOCPOS_BOD, FV_DOCPOS_EOD);
				view->cmdCopy();
				if (g_trace) {
					GdkClipboard *cb = gdk_display_get_clipboard(disp);
					XAP_UnixClipboard *clip =
						static_cast<XAP_UnixApp *>(app)->getClipboard();
					fprintf(stderr,
							"fmt: is_local %d canPaste %d sel %d\n",
							cb ? gdk_clipboard_is_local(cb) : -1,
							clip ? clip->canPaste(
								XAP_UnixClipboard::TAG_ClipboardOnly) : -1,
							!view->isSelectionEmpty());
				}
				view->cmdSelect(body, body);
				view->cmdPaste();
				if (g_trace)
					fprintf(stderr, "fmt: point %d\n",
							static_cast<int>(view->getPoint()));
				pump_for(400);
			};
			paste_mid();
			if (g_trace)
				fprintf(stderr, "fmt: tag armed %d point %d intable %d\n",
						view->hasPasteTag(),
						static_cast<int>(view->getPoint()),
						view->isInTable(view->getPoint()));
			view->popupPasteTagMenu();
			pump_for(300);

			std::vector<GtkWidget *> tags;
			find_css_class(win, "paste-tag", tags);
			GtkPopover *pop = nullptr;
			if (!tags.empty())
				pop = gtk_menu_button_get_popover(
					GTK_MENU_BUTTON(tags.front()));
			auto tag_click = [&](const char *label) {
				if (!pop)
					return;
				std::vector<GtkWidget *> btns;
				find_buttons(GTK_WIDGET(pop), btns);
				for (GtkWidget *b : btns)
					if (g_strcmp0(gtk_button_get_label(GTK_BUTTON(b)),
								  label) == 0)
						g_signal_emit_by_name(b, "clicked");
			};
			tag_click("Merge Formatting"); /* delete + re-paste path */
			pump_for(400);
			/* applying disarms the tag — re-arm for each option */
			paste_mid();
			tag_click("Keep Text Only");
			pump_for(400);
			paste_mid();
			tag_click("Keep Source Formatting");
			pump_for(300);
			paste_mid();
			view->popupPasteTagMenu();
			pump_for(250);
			/* opens the Paste Special dialog; the stray sweep answers
			 * it on the next tick */
			tag_click("Paste Special…");
			pump_for(500);
			paste_mid();
			view->dismissPasteTag();      /* hide() path */
			pump_for(200);
		});

	/* ---- visual text drag: copyToLocal builds the local drag
	 * buffer, an in-window drag runs _mouseDrag, an out-of-window one
	 * builds the drag document + temp file + content providers and
	 * calls gdk_drag_begin — on a live wayland session that is GDK's
	 * wayland drag path ---- */
	FMT_SECTION("visualdrag",
		if (view) {
			/* out-of-window with an empty local buffer: early out */
			view->dragVisualText(-50, 100);
			view->cmdSelect(0, 0, FV_DOCPOS_BOD, FV_DOCPOS_EOD);
			PT_DocPosition a = view->getSelectionLeftAnchor();
			PT_DocPosition b = view->getSelectionRightAnchor();
			view->copyToLocal(a, b);
			view->dragVisualText(100, 100);  /* in-window: _mouseDrag */
			view->dragVisualText(-50, 100);  /* drag out of window   */
			pump_for(350);
		});

	/* ---- inline-image drag: a fixture carrying an embedded image;
	 * btn0/btn1/drag/release entry points plus out-of-window drag
	 * attempts ---- */
	FMT_SECTION("inline image",
		const char *top = getenv("ABINOVA_TEST_SRC_DIR");
		std::string png = std::string(top ? top : ".") +
			"/test/wp/cov07/red.png";
		gchar *b64 = file_b64(png.c_str());
		if (b64) {
			std::string blip =
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<abiword template=\"false\" fileformat=\"1.0\" "
				"xmlns=\"http://www.abisource.com/awml.dtd\" "
				"version=\"0.99.2\" xml:space=\"preserve\" "
				"props=\"lang:en-US\">\n<data>\n"
				"<d name=\"red.png\" mime-type=\"image/png\" "
				"base64=\"yes\">" + std::string(b64) + "</d>\n"
				"</data>\n<section>\n"
				"<p>a<image dataid=\"red.png\" props=\""
				"width:1.0in; height:1.0in\"/></p>\n"
				"</section>\n</abiword>\n";
			std::string bp = write_fixture("fmt-blip.abw", blip);
			g_free(b64);
			AV_View *v2 = nullptr;
			if (!bp.empty() && load_fixture_frame(app, ctx, bp, &v2)) {
				FV_View *fv2 = static_cast<FV_View *>(v2);
				if (fv2) {
					/* locate the image object by doc position, then
					 * probe the window for a pixel that maps to it —
					 * press there (edge hits arm RESIZE, center arms
					 * DRAGGING), drag in-window, then out for the
					 * getPNGImage/dragImageToFile path */
					PT_DocPosition imgpos = 0;
					for (PT_DocPosition p = 2; p < 15 && !imgpos; p++) {
						fv2->cmdSelect(p, p + 1);
						if (fv2->isImageSelected())
							imgpos = p;
					}
					int ix = -1, iy = -1;
					for (int px = 20; px < 700 && ix < 0; px += 12)
						for (int py = 40; py < 600; py += 12)
							if (imgpos &&
								fv2->getDocPositionFromXY(px, py)
									== imgpos) {
								ix = px;
								iy = py;
								break;
							}
					if (ix >= 0) {
						fv2->btn0InlineImage(ix, iy);
						fv2->btn1InlineImage(ix, iy);
						fv2->dragInlineImage(ix + 10, iy);
						fv2->dragInlineImage(ix + 30, iy);
						fv2->dragInlineImage(-60, iy);
						fv2->releaseInlineImage(ix + 10, iy);
						fv2->btn0InlineImage(ix, iy);
						fv2->btn1InlineImage(ix, iy);
						fv2->btn1CopyImage(ix, iy);
						fv2->dragInlineImage(-60, iy);
					}
					pump_for(350);
				}
			}
		});

	/* ---- frame drag: the posimage fixture carries a positioned
	 * image frame; btn0/btn1 near it arm frame-edit state and an
	 * out-of-window drag runs the image-wrapper → dragImageToFile
	 * path (getPNGImage + gdk_drag_begin) ---- */
	FMT_SECTION("frame edit",
		const char *top = getenv("ABINOVA_TEST_SRC_DIR");
		std::string fp = std::string(top ? top : ".") +
			"/test/wp/posimage.abw";
		AV_View *v3 = nullptr;
		if (load_fixture_frame(app, ctx, fp, &v3)) {
			FV_View *fv3 = static_cast<FV_View *>(v3);
			if (fv3) {
				/* frame is positioned ~3in from the column edge, ~2.5in
				 * from the paragraph → sweep a grid of grabs; the first
				 * press selects it (EXISTING_SELECTED), the second arms
				 * DRAG_EXISTING, then the out-of-window drag reaches
				 * the image-wrapper/dragImageToFile path */
				FV_FrameEdit *fe = fv3->getFrameEdit();
				for (int px = 200; px <= 500 && fe; px += 30)
					for (int py = 180; py <= 400; py += 30) {
						fv3->btn0Frame(px, py);
						fv3->btn1Frame(px, py);
						fv3->btn1Frame(px, py);
						fv3->dragFrame(px + 10, py + 10);
						fv3->dragFrame(-60, py + 10);
					}
				pump_for(350);
				if (fe && fe->isActive())
					fe->setMode(FV_FrameEdit_NOT_ACTIVE);
				pump_for(200);
			}
		});

	g_source_remove(sweeper);
	call_guard([&] { pump_for(300); }, "tail");
	__gcov_dump();
	g_print("drive: fmt done — %d criticals, %d crashed, %d wedged, "
			"%d handle drags, %d finishes\n",
			g_criticals, g_crashed_widgets, g_wedged_widgets,
			g_handle_dragged, g_handle_finished);
	return 0;
}

/* narrow repro for the frame-leg fileNew crash: boot one frame on the
 * scratch doc, then invoke the fileNew edit method directly — reaches
 * the populateStrux/statusbar heap overwrite in seconds instead of a
 * full widget walk, so it can run under valgrind */
static int drive_filenew(AP_UnixApp *app, const char *src)
{
	alarm(0);
	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}
	char *uri = g_strdup_printf("file://%s", src);
	UT_Error err = frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	if (err != UT_OK) {
		g_printerr("drive: load err %d\n", err);
		return 1;
	}
	frame->show();
	pump_for(300);
	AV_View *av = frame->getCurrentView();
	const EV_EditMethodContainer *emc =
		XAP_App::getApp()->getEditMethodContainer();
	if (!av || !emc) {
		g_printerr("drive: no view/edit-method container\n");
		return 1;
	}
	EV_EditMethod *em = emc->findEditMethodByName("fileNew");
	if (!em) {
		g_printerr("drive: no fileNew method\n");
		return 1;
	}
	set_phase("filenew: invoke");
	EV_EditMethodCallData emcd;
	em->Fn(av, &emcd);
	pump_for(300);
	set_phase("filenew: done");
	return 0;
}

/* ---------------- TOC01: context-menu TOC update leg ----------------
 *
 * The reported freeze: right-click a TOC, choose "Update Table of
 * Contents".  The ContextTOC popover is mapped inside
 * runModalContextMenu's nested g_main_loop when the action fires, so
 * fillTOC() runs while the menu is still up.  This leg drives that
 * path end-to-end: the real contextTOC edit method opens the popover
 * and spins the nested loop, a tick source inside the loop activates
 * the item_<AP_MENU_ID_REF_UPDATETOC> action like a real click, then
 * pops the menu down so the loop unwinds.  The modal path is followed
 * by direct cmdUpdateTOC + undo/redo cycles on both a one-page and a
 * multi-page fixture.  A wedge parks the watchdog (SIGTERM) or the
 * SIGQUIT probe on the exact stack.
 */

struct TocRunCtx {
	GtkWidget *toplevel;
	GtkWidget *popover;
	int        updates;
};

/* the ContextTOC popup is a GtkPopoverMenu; a real frame also has
 * toolbar/menu-button popovers parked realized-but-unmapped in the
 * same tree, so only a MAPPED popover-menu counts */
static GtkWidget *find_popover_widget(GtkWidget *w)
{
	if (!w)
		return nullptr;
	if (GTK_IS_POPOVER_MENU(w) && gtk_widget_get_mapped(w))
		return w;
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c)) {
		GtkWidget *r = find_popover_widget(c);
		if (r)
			return r;
	}
	return nullptr;
}

static fp_Line *toc_first_line(fp_ContainerObject *con)
{
	if (!con)
		return nullptr;
	if (con->getContainerType() == FP_CONTAINER_LINE)
		return static_cast<fp_Line *>(con);
	fp_Container *ctr = dynamic_cast<fp_Container *>(con);
	if (!ctr)
		return nullptr;
	for (UT_sint32 i = 0; i < ctr->countCons(); i++) {
		fp_Line *line = toc_first_line(ctr->getNthCon(i));
		if (line)
			return line;
	}
	return nullptr;
}

/* the popover's item widgets are GtkModelButtons whose "action-name"
 * property GTK4 leaves unset (the menu tracker owns the action
 * binding), so check the GMenuModel itself for the item carrying the
 * wanted action */
static bool model_has_action(GMenuModel *m, const char *action)
{
	if (!m)
		return false;
	int n = g_menu_model_get_n_items(m);
	for (int i = 0; i < n; i++) {
		gchar *an = nullptr;
		g_menu_model_get_item_attribute(m, i,
										G_MENU_ATTRIBUTE_ACTION, "s", &an);
		bool hit = an && action && !strcmp(an, action);
		g_free(an);
		if (hit)
			return true;
		GMenuModel *sub =
			g_menu_model_get_item_link(m, i, G_MENU_LINK_SUBMENU);
		if (!sub)
			sub = g_menu_model_get_item_link(m, i, G_MENU_LINK_SECTION);
		if (sub) {
			bool r = model_has_action(sub, action);
			g_object_unref(sub);
			if (r)
				return true;
		}
	}
	return false;
}

static gboolean toc_menu_tick(gpointer data)
{
	TocRunCtx *tc = static_cast<TocRunCtx *>(data);
	if (!tc->popover) {
		tc->popover = find_popover_widget(tc->toplevel);
		if (!tc->popover)
			return G_SOURCE_CONTINUE;
	}
	/* activate "Update Table" while the popover is still mapped —
	 * the order a real item click produces.  gtk_widget_activate_action
	 * resolves through the widget's action muxer (which carries the
	 * "menu" group the popup inserted), running the same handler a
	 * click reaches; popdown afterwards unwinds the nested loop like
	 * the click's own dismissal does */
	char name[64];
	snprintf(name, sizeof(name), "menu.item_%u",
			 static_cast<unsigned>(AP_MENU_ID_REF_UPDATETOC));
	GMenuModel *mm = GTK_IS_POPOVER_MENU(tc->popover)
		? gtk_popover_menu_get_menu_model(GTK_POPOVER_MENU(tc->popover))
		: nullptr;
	if (model_has_action(mm, name) &&
		gtk_widget_activate_action(tc->popover, name, nullptr))
		tc->updates++;
	else
		g_printerr("drive: toc — %s missing or disabled\n", name);
	gtk_popover_popdown(GTK_POPOVER(tc->popover));
	return G_SOURCE_REMOVE;
}

static bool toc_drive_doc(AP_UnixApp *app, DriveCtx &ctx,
						  const std::string &path)
{
	set_phase("toc: load");
	AV_View *av = nullptr;
	if (!load_fixture_frame(app, ctx, path, &av) || !av) {
		g_printerr("drive: toc — cannot load %s\n", path.c_str());
		return false;
	}
	FV_View *view = static_cast<FV_View *>(av);
	FL_DocLayout *lay = view->getLayout();
	if (!lay || lay->getNumTOCs() == 0) {
		g_printerr("drive: toc — no TOC in %s\n", path.c_str());
		return false;
	}
	XAP_Frame *frame = static_cast<XAP_Frame *>(view->getParentData());
	GtkWidget *win = nullptr;
	if (frame)
		win = static_cast<XAP_UnixFrameImpl *>(frame->getFrameImpl())
			->getTopLevelWindow();

	/* point the context click at the middle of a TOC entry line */
	fl_TOCLayout *pTOC = lay->getNthTOC(0);
	fp_Line *line = toc_first_line(
		static_cast<fp_TOCContainer *>(pTOC->getFirstContainer()));
	std::optional<UT_Rect> rect = line ? line->getScreenRect()
									 : std::optional<UT_Rect>();
	EV_EditMethodCallData emcd;
	if (rect) {
		emcd.m_xPos = rect->left + rect->width / 2;
		emcd.m_yPos = rect->top + rect->height / 2;
	}

	const EV_EditMethodContainer *emc =
		XAP_App::getApp()->getEditMethodContainer();
	EV_EditMethod *em = emc ? emc->findEditMethodByName("contextTOC")
							: nullptr;
	if (!em) {
		g_printerr("drive: toc — no contextTOC method\n");
		return false;
	}

	TocRunCtx tc {win, nullptr, 0};
	guint tick = g_timeout_add(50, toc_menu_tick, &tc);
	set_phase("toc: context menu Update Table");
	em->Fn(view, &emcd);
	if (!tc.updates) {
		/* popover never surfaced (compositor denied it) — the modal
		 * path could not run; report as unusable environment */
		g_source_remove(tick);
		g_printerr("drive: toc — context menu did not reach Update "
				   "Table in %s\n", path.c_str());
		return false;
	}
	pump_for(300);

	set_phase("toc: re-update + undo");
	view->cmdUpdateTOC();
	pump_for(200);
	/* a real edit gives undo something to chew on, then undo it —
	 * proves the update left history machinery sane */
	view->cmdCharInsert("x", false);
	view->cmdUndo(1);
	pump_for(200);
	if (!view->isSelectionEmpty())
		g_printerr("drive: toc — selection left after undo\n");

	g_print("drive: toc — %s updated via context menu + re-update ok\n",
			path.c_str());
	return true;
}

static int drive_toc(AP_UnixApp *app, const char *)
{
	alarm(0);
	DriveCtx ctx {nullptr, {}, 0, false, false, 0, 0};
	sweep_guard([&ctx] { ctx.preexisting = toplevels(); });

	const char *top = getenv("ABINOVA_TEST_SRC_DIR");
	std::string small = std::string(top ? top : ".") +
		"/test/wp/toc.abw";

	/* multi-page fixture: enough Heading-1 chapters + body text to
	 * span several pages, so the TOC update re-lays out a real
	 * document instead of a two-line stub */
	std::string xml =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE abiword PUBLIC \"-//ABISOURCE//DTD AWML 1.0 Strict//EN\" "
		"\"http://www.abisource.com/awml-1.0.dtd\">\n"
		"<abiword version=\"1.0\" file-format=\"1.1\">\n"
		"<section>\n"
		"<toc props=\"toc-has-heading:0; toc-source-style1:Heading 1; "
		"toc-dest-style1:Contents 1\">\n"
		"<p props=\"toc-dest-style:1\"></p>\n"
		"</toc>\n";
	for (int i = 1; i <= 25; i++) {
		xml += "<p style=\"Heading 1\">Chapter " + std::to_string(i) +
			"</p>\n";
		for (int j = 0; j < 30; j++)
			xml += "<p>Body text of chapter " + std::to_string(i) +
				" paragraph " + std::to_string(j) + ".</p>\n";
	}
	xml += "</section>\n</abiword>\n";
	std::string multi = write_fixture("toc-multi.abw", xml);

	int rc = 0;
	if (!toc_drive_doc(app, ctx, small))
		rc = 1;
	if (!multi.empty() && !toc_drive_doc(app, ctx, multi))
		rc = 1;

	__gcov_dump();
	g_print("drive: toc done — %d criticals, %d crashed, %d wedged\n",
			g_criticals, g_crashed_widgets, g_wedged_widgets);
	return rc;
}

/* ---------------- TST12: heartbeat freeze leg ---------------- */

/* async-load completion mirror of the edit-method embed path — the
 * leg drives the same GIO pattern s_embedFileInDoc uses so a
 * regression back to a synchronous whole-file read shows up as a
 * heartbeat gap */
struct EmbedWait {
	gboolean done;
	gboolean ok;
	gchar *bytes;
	gsize len;
};

static void embed_load_done(GObject *src, GAsyncResult *res, gpointer data)
{
	EmbedWait *w = static_cast<EmbedWait *>(data);
	GError *err = nullptr;
	w->ok = g_file_load_contents_finish(G_FILE(src), res, &w->bytes,
										&w->len, nullptr, &err);
	g_clear_error(&err);
	w->done = TRUE;
}

/* answer the newest stray toplevel without driving its widgets — the
 * freeze leg only measures dialog open/close cost; a synchronous
 * drive_widget tree walk would itself show up as a heartbeat gap and
 * measure the harness, not the app */
static gboolean freeze_answer_cb(gpointer data)
{
	DriveCtx *ctx = static_cast<DriveCtx *>(data);
	GtkWidget *w = nullptr;
	if (!sweep_guard([&] { attach_live_stray(ctx->preexisting, &w); }))
		return G_SOURCE_CONTINUE;
	if (!w)
		return G_SOURCE_CONTINUE;
	if (GTK_IS_DIALOG(w))
		g_signal_emit_by_name(w, "response", GTK_RESPONSE_DELETE_EVENT);
	else if (GTK_IS_WINDOW(w))
		gtk_window_close(GTK_WINDOW(w));
	/* answering can finalize the dialog — the weak ref then already
	 * nulled w, and removing a dead weak pointer is a critical */
	if (w)
		g_object_remove_weak_pointer(G_OBJECT(w),
									 reinterpret_cast<gpointer *>(&w));
	return G_SOURCE_REMOVE;
}

static int drive_freeze(AP_UnixApp *app, const char *src)
{
	alarm(0); /* the wrapper's `timeout` bounds the leg */
	g_timeout_add(HB_POLL_MS, heartbeat_cb, nullptr);
	g_hb_prev = g_get_monotonic_time();

	set_phase("freeze: newFrame");
	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("drive: no frame\n");
		return 1;
	}

	hb_expect(HB_LOAD_BOUND_US);
	set_phase("freeze: loadDocument");
	char *uri = g_strdup_printf("file://%s", src);
	UT_Error err = frame->loadDocument(uri, IEFT_Unknown, true);
	g_free(uri);
	frame->show();
	pump_for(400);
	hb_expect(HB_BOUND_US);
	FV_View *fv = static_cast<FV_View *>(frame->getCurrentView());
	if (err != UT_OK || !fv) {
		g_printerr("drive: load err %d\n", err);
		return 1;
	}

	/* the biggest bundled binary fixture through the real importer —
	 * the .doc path reads + parses the whole file.  Runs first while
	 * the frame's document is still clean so the in-place reload can
	 * never pop a "save changes?" modal */
	hb_expect(HB_LOAD_BOUND_US);
	set_phase("freeze: doc import");
	{
		const char *top = getenv("ABINOVA_TEST_SRC_DIR");
		char *dpath = g_strdup_printf("%s/test/wp/fields.doc",
									  top ? top : ".");
		if (g_file_test(dpath, G_FILE_TEST_EXISTS)) {
			char *u2 = g_strdup_printf("file://%s", dpath);
			frame->loadDocument(u2, IEFT_Unknown, false);
			g_free(u2);
			pump_for(300);
			fv = static_cast<FV_View *>(frame->getCurrentView());
		}
		g_free(dpath);
	}
	hb_expect(HB_BOUND_US);
	if (!fv) {
		g_printerr("drive: no view after import\n");
		return 1;
	}

	/* burst typing through the insertData edit method — the same
	 * path a keyboard burst takes: piece-table inserts, layout,
	 * change-history serial bumps, redraw scheduling */
	set_phase("freeze: burst type");
	{
		const EV_EditMethodContainer *emc =
			XAP_App::getApp()->getEditMethodContainer();
		EV_EditMethod *em = emc ? emc->findEditMethodByName("insertData")
								: nullptr;
		static const char burst[] =
			"The freeze leg types this sentence eight times over. ";
		for (int i = 0; i < 8 && em; i++) {
			EV_EditMethodCallData emcd(burst,
									 static_cast<UT_uint32>(sizeof(burst) - 1));
			em->Fn(fv, &emcd);
			pump_for(60);
		}
	}

	set_phase("freeze: scroll");
	for (int i = 0; i < 6; i++)
		fv->cmdScroll(AV_SCROLLCMD_PAGEDOWN);
	pump_for(200);
	fv->cmdScroll(AV_SCROLLCMD_TOBOTTOM);
	fv->cmdScroll(AV_SCROLLCMD_TOTOP);
	pump_for(150);

	/* FRZ02: the first tick on a dirty doc pays the real serialize;
	 * the second with unchanged content must take the serial-skip
	 * path — both stay under the heartbeat bound */
	set_phase("freeze: autosave serialize");
	frame->autosaveTick();
	pump_for(100);
	set_phase("freeze: autosave skip");
	frame->autosaveTick();
	pump_for(100);

	/* open + close a couple of small dialogs: request + answer only,
	 * no widget walk (a synchronous drive_widget tree walk would
	 * itself show up as a heartbeat gap and measure the harness, not
	 * the app) — construction, show and teardown are what get timed */
	set_phase("freeze: dialogs");
	{
		XAP_DialogFactory *factory =
			static_cast<XAP_DialogFactory *>(frame->getDialogFactory());
		const XAP_Dialog_Id want[] = {XAP_DIALOG_ID_ZOOM,
									  XAP_DIALOG_ID_ABOUT};
		int done = 0;
		for (size_t w = 0; factory && w < G_N_ELEMENTS(want); w++) {
			const XAP_DialogFactory::_dlg_table *e = nullptr;
			for (UT_uint32 i = 0; i < factory->getDialogTableSize(); i++) {
				const XAP_DialogFactory::_dlg_table *t =
					factory->getDialogTableEntry(i);
				if (t && t->m_id == want[w]) {
					e = t;
					break;
				}
			}
			if (!e)
				continue;
			XAP_Dialog *dlg = factory->requestDialog(e->m_id);
			if (!dlg)
				continue;
			DriveCtx dctx {nullptr, {}, 0, false, false, 0, 0};
			sweep_guard([&dctx] { dctx.preexisting = toplevels(); });
			if (XAP_Dialog_Modeless *ml =
					dynamic_cast<XAP_Dialog_Modeless *>(dlg)) {
				ml->runModeless(frame);
				pump_for(250);
				GtkWidget *top = nullptr;
				sweep_guard([&] {
					attach_live_stray(dctx.preexisting, &top);
				});
				if (top) {
					gtk_window_close(GTK_WINDOW(top));
					if (top)
						g_object_remove_weak_pointer(
							G_OBJECT(top),
							reinterpret_cast<gpointer *>(&top));
				}
				pump_for(150);
			} else {
				g_timeout_add(150, freeze_answer_cb, &dctx);
				dlg->runModal(frame);
				pump();
			}
			factory->releaseDialog(dlg);
			pump_for(100);
			done++;
		}
		if (!done)
			g_printerr("drive: no cheap dialogs registered\n");
	}

	/* FRZ03's fixed path: a multi-MB blob must reach the doc through
	 * async I/O plus a bounded insert.  Load 8MB with the same GIO
	 * async call insMediaFile uses, then embed it like
	 * s_embedBufferInDoc does — a sync re-read of this size or a
	 * stray UI-thread sleep shows up in the heartbeat */
	set_phase("freeze: async media embed");
	{
		gchar *mpath = g_strdup_printf("%s/ui-drive-freeze-embed.bin",
									   g_get_tmp_dir());
		FILE *f = fopen(mpath, "wb");
		if (f) {
			static char blk[4096];
			memset(blk, 0x5a, sizeof(blk));
			for (int i = 0; i < 2048; i++) /* 8MB */
				fwrite(blk, 1, sizeof(blk), f);
			fclose(f);
		}
		GFile *gf = g_file_new_for_path(mpath);
		EmbedWait w {FALSE, FALSE, nullptr, 0};
		g_file_load_contents_async(gf, nullptr, embed_load_done, &w);
		g_object_unref(gf);
		gint64 deadline = g_get_monotonic_time() + 30 * G_USEC_PER_SEC;
		while (!w.done && g_get_monotonic_time() < deadline)
			g_main_context_iteration(nullptr, TRUE);
		if (w.ok && w.bytes) {
			UT_ByteBufPtr buf(new UT_ByteBuf);
			buf->ins(0, reinterpret_cast<const UT_Byte *>(w.bytes),
					 static_cast<UT_uint32>(w.len));
			set_phase("freeze: embed insert");
			fv->cmdInsertEmbed(buf, fv->getPoint(),
							   "application/octet-stream",
							   "embed-type: file; media-kind: file; "
							   "media-name: freeze.bin");
			pump_for(150);
		}
		g_free(w.bytes);
		unlink(mpath);
		g_free(mpath);
	}

	set_phase("freeze: report");
	g_print("freeze: worst main-loop gap %lld ms at \"%s\" "
			"(interactive bound %lld ms)\n",
			static_cast<long long>(g_hb_worst / 1000),
			g_hb_worst_phase,
			static_cast<long long>(HB_BOUND_US / 1000));
	/* drop the recovery copy the first tick wrote — the leg proved
	 * the serialize happened; no need to leave it in the user dir */
	frame->discardAutosaveFile();
	__gcov_dump();
	if (g_hb_worst_over > 0) {
		g_printerr("drive: heartbeat stall %lld ms in \"%s\" "
				   "(%lld ms over its bound)\n",
				   static_cast<long long>(g_hb_trip_gap / 1000),
				   g_hb_trip_phase,
				   static_cast<long long>(g_hb_worst_over / 1000));
		return 1;
	}
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
	/* SIGQUIT is the live probe: dump phase+stack, keep running */
	struct sigaction sq;
	memset(&sq, 0, sizeof(sq));
	sq.sa_handler = drive_probe;
	sq.sa_flags = SA_RESTART;
	sigaction(SIGQUIT, &sq, nullptr);
	alarm(120); /* last-resort watchdog; drvwrap wraps us in `timeout` */

	bool wantList = false, wantFrame = false, wantAbi = false,
		 wantEv = false, wantFmt = false, wantFileNew = false,
		 wantRuler = false, wantFreeze = false, wantToc = false;
	long wantId = -1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--list") == 0)
			wantList = true;
		else if (strcmp(argv[i], "--frame") == 0)
			wantFrame = true;
		else if (strcmp(argv[i], "--abi") == 0)
			wantAbi = true;
		else if (strcmp(argv[i], "--ev") == 0)
			wantEv = true;
		else if (strcmp(argv[i], "--fmt") == 0)
			wantFmt = true;
		else if (strcmp(argv[i], "--filenew") == 0)
			wantFileNew = true;
		else if (strcmp(argv[i], "--ruler") == 0)
			wantRuler = true;
		else if (strcmp(argv[i], "--freeze") == 0)
			wantFreeze = true;
		else if (strcmp(argv[i], "--toc") == 0)
			wantToc = true;
		else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc)
			wantId = strtol(argv[++i], nullptr, 10);
	}
	if (!wantList && !wantFrame && !wantAbi && !wantEv && !wantFmt &&
		!wantFileNew && !wantRuler && !wantFreeze && !wantToc &&
		wantId < 0) {
		g_printerr("usage: %s --list | --id N | --frame | --abi | --ev"
				   " | --fmt | --filenew | --ruler | --freeze"
				   " | --toc\n", argv[0]);
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
		GError *gerr = nullptr;
		if (!g_application_register(G_APPLICATION(gtkApp), nullptr, &gerr))
			g_printerr("drive: g_application_register failed: %s\n",
					   gerr ? gerr->message : "?");
		g_clear_error(&gerr);
		/* emitting "startup" on an unregistered app crashes inside
		 * gtk_application_startup (it lists actions, which asserts
		 * is_registered and returns NULL) — gate on the real state */
		if (g_application_get_is_registered(G_APPLICATION(gtkApp)))
			g_signal_emit_by_name(gtkApp, "startup");
		else
			g_printerr("drive: app not registered — "
					   "startup emit skipped\n");
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
	if (wantEv)
		return drive_ev(app, src);
	if (wantFmt)
		return drive_fmt(app, src);
	if (wantFileNew)
		return drive_filenew(app, src);
	if (wantRuler)
		return drive_ruler(app, src);
	if (wantFreeze)
		return drive_freeze(app, src);
	if (wantToc)
		return drive_toc(app, src);

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
