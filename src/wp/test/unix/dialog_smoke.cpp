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

/* dialog_smoke — TST08: open every registered GTK dialog on a display
 * (xvfb or live) and fail if it crashes or emits a GTK critical.
 *
 *   dialog-smoke --list      print the registered dialog ids, one/line
 *   dialog-smoke --id N      construct, present and dismiss dialog N
 *
 * Exit codes: 0 ok, 1 failure, 77 no display / interactive prerequisite.
 * The dlgswrap.sh wrapper runs one process per dialog under a timeout
 * so a hang or segfault is attributed to a single dialog id.
 */

#include <gtk/gtk.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "config.h"
#include "ut_types.h"
#include "ie_types.h"
#include "xap_App.h"
#include "ap_UnixApp.h"
#include "xap_Frame.h"
#include "xap_Dialog.h"
#include "xap_DialogFactory.h"
#include "xap_Dialog_Id.h"
#include "ap_Dialog_Id.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "ap_Dialog_Modeless.h"
#include "ap_Dialog_Replace.h"
#include "ap_Dialog_InsertBookmark.h"
#include "ap_Dialog_InsertHyperlink.h"
#include "ap_Dialog_GetStringCommon.h"
#include "ap_Dialog_ListRevisions.h"
#include "ap_Dialog_MarkRevisions.h"
#include "xap_Dlg_HTMLOptions.h"
#include "src/wp/impexp/epub/dialogs/xp/ap_Dialog_EpubExportOptions.h"

static int g_criticals = 0;
static int g_warnings = 0;

static GLogWriterOutput smoke_log_writer(GLogLevelFlags level,
										 const GLogField *fields,
										 gsize n_fields, gpointer)
{
	if (level & G_LOG_LEVEL_CRITICAL)
		g_criticals++;
	else if (level & G_LOG_LEVEL_WARNING)
		g_warnings++;
	return g_log_writer_standard_streams(level, fields, n_fields, nullptr);
}

static void smoke_segv(int)
{
	_exit(139);
}

/* pump the default main context until no work is pending */
static void pump(void)
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
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

struct DismissCtx {
	std::vector<GtkWidget *> preexisting;
	int tries;
	bool dismissed;
	int help_seen;	/* a new toplevel carried a Help button */
	int help_bad;	/* ...but it sat alone in its own row */
};

/* UI03: the dynamically added Help button must share an existing
 * action row with the dialog's other buttons — never land alone in a
 * second row below the declared ones (that was the reported bug).
 * Returns 0 when the toplevel has no Help button, 1 when it sits in a
 * row with at least one other button, -1 when stranded alone. */
static int help_button_row_ok(GtkWidget *top)
{
	if (!GTK_IS_DIALOG(top))
		return 0;
	std::vector<GtkWidget *> stack{top};
	GtkWidget *help = nullptr;
	while (!stack.empty() && !help) {
		GtkWidget *w = stack.back();
		stack.pop_back();
		if (gtk_dialog_get_response_for_widget(GTK_DIALOG(top), w)
			== GTK_RESPONSE_HELP)
			help = w;
		for (GtkWidget *c = gtk_widget_get_first_child(w); c;
			 c = gtk_widget_get_next_sibling(c))
			stack.push_back(c);
	}
	if (!help)
		return 0;
	GtkWidget *row = gtk_widget_get_parent(help);
	if (!GTK_IS_BOX(row))
		return -1;
	int buttons = 0;
	for (GtkWidget *c = gtk_widget_get_first_child(row); c;
		 c = gtk_widget_get_next_sibling(c))
		if (GTK_IS_BUTTON(c))
			buttons++;
	return buttons > 1 ? 1 : -1;
}

/* finds a toplevel that did not exist before the dialog ran and
 * dismisses it: "response" DELETE_EVENT for GtkDialogs (that is how
 * abiRunModalDialog reports cancel), window close for anything else.
 * The emit repeats until the toplevel is gone — a quit that lands
 * between the dialog's signal connect and g_main_loop_run is lost,
 * so a single emit can miss. */
static gboolean dismiss_new_toplevel(gpointer data)
{
	DismissCtx *ctx = static_cast<DismissCtx *>(data);
	GListModel *tl = gtk_window_get_toplevels();
	guint n = g_list_model_get_n_items(tl);
	bool anyNew = false;
	/* emit on EVERY new toplevel, last (newest) first: dialogs can
	 * stack nested modal loops (e.g. Clipart pops an error message
	 * inside an idle handler) and dismissing the parent while the
	 * child still blocks its nested loop deadlocks the parent loop. */
	for (guint i = n; i > 0; i--) {
		GtkWidget *w = GTK_WIDGET(g_list_model_get_item(tl, i - 1));
		bool isNew = w && !contains(ctx->preexisting, w);
		if (!isNew) {
			if (w)
				g_object_unref(w);
			continue;
		}
		anyNew = true;
		if (GTK_IS_DIALOG(w)) {
			/* UI03: check Help-button adjacency while the dialog is
			 * still presented */
			int row = help_button_row_ok(w);
			if (row > 0)
				ctx->help_seen = 1;
			else if (row < 0)
				ctx->help_bad = 1;
			g_signal_emit_by_name(w, "response",
								  GTK_RESPONSE_DELETE_EVENT);
		} else if (GTK_IS_WINDOW(w)) {
			gtk_window_close(GTK_WINDOW(w));
		}
		ctx->dismissed = true;
		g_object_unref(w);
	}
	if (ctx->dismissed && !anyNew)
		return G_SOURCE_REMOVE; /* all new toplevels gone */
	if (++ctx->tries >= 60) /* ~12s, then give up (wrapper timeout anyway) */
		return G_SOURCE_REMOVE;
	return G_SOURCE_CONTINUE;
}

static int run_one(XAP_DialogFactory *factory, XAP_Frame *frame,
				   XAP_Dialog_Id id, XAP_Dialog_Type type)
{
	/* criticals emitted before this dialog runs are harness noise
	 * (e.g. the GtkApplication::startup critical from newFrame, since
	 * the smoke binary never calls g_application_run) — only count
	 * what the dialog itself produces */
	g_criticals = 0;
	g_warnings = 0;

	if (getenv("SMOKE_DEBUG"))
		g_printerr("smoke-dbg: id %d requestDialog\n", static_cast<int>(id));
	XAP_Dialog *dlg = factory->requestDialog(id);
	if (!dlg) {
		g_print("smoke: id %d — factory returned no dialog\n",
				static_cast<int>(id));
		return 1;
	}
	if (getenv("SMOKE_DEBUG"))
		g_printerr("smoke-dbg: id %d constructed %p\n", static_cast<int>(id), (void *)dlg);

	/* same pre-run context the edit methods establish: view/doc
	 * wiring for dialogs that read document state */
	FV_View *view = static_cast<FV_View *>(frame->getCurrentView());
	PD_Document *doc = dynamic_cast<PD_Document *>(frame->getCurrentDoc());
	if (AP_Dialog_Modeless *ml = dynamic_cast<AP_Dialog_Modeless *>(dlg))
		ml->setView(view);
	if (auto *d = dynamic_cast<AP_Dialog_InsertBookmark *>(dlg))
		d->setDoc(view);
	else if (auto *d = dynamic_cast<AP_Dialog_InsertHyperlink *>(dlg))
		d->setDoc(view);
	else if (auto *d = dynamic_cast<AP_Dialog_GetStringCommon *>(dlg))
		d->setDoc(view);
	else if (auto *d = dynamic_cast<AP_Dialog_ListRevisions *>(dlg))
		d->setDocument(doc);
	else if (auto *d = dynamic_cast<AP_Dialog_MarkRevisions *>(dlg))
		d->setDocument(doc);
	else if (auto *d = dynamic_cast<AP_Dialog_Replace *>(dlg))
		d->setView(view);
	else if (auto *d = dynamic_cast<XAP_Dialog_HTMLOptions *>(dlg)) {
		/* HTML export installs its options struct before runModal */
		static XAP_Exp_HTMLOptions html_opt {};
		XAP_Dialog_HTMLOptions::getHTMLDefaults(&html_opt,
											  XAP_App::getApp());
		d->setHTMLOptions(&html_opt, XAP_App::getApp());
	}
	else if (auto *ep = dynamic_cast<AP_Dialog_EpubExportOptions *>(dlg)) {
		/* EPUB export installs its options struct before runModal */
		static XAP_Exp_EpubExportOptions epub_opt {};
		AP_Dialog_EpubExportOptions::getEpubExportDefaults(&epub_opt,
														 XAP_App::getApp());
		ep->setEpubExportOptions(&epub_opt, XAP_App::getApp());
	}

	DismissCtx ctx{toplevels(), 0, false, 0, 0};

	if (XAP_Dialog_Modeless *ml = dynamic_cast<XAP_Dialog_Modeless *>(dlg)) {
		ml->runModeless(frame);
		pump();
		/* close whatever window came up, like a user hitting [X] */
		dismiss_new_toplevel(&ctx);
		pump();
		/* if close-request didn't take, tear down through the dialog */
		for (GtkWidget *w : toplevels())
			if (!contains(ctx.preexisting, w)) {
				gtk_window_close(GTK_WINDOW(w));
				pump();
				break;
			}
	} else {
		g_timeout_add(300, dismiss_new_toplevel, &ctx);
		g_timeout_add(900, dismiss_new_toplevel, &ctx);
		dlg->runModal(frame);
		if (getenv("SMOKE_DEBUG"))
			g_printerr("smoke-dbg: id %d runModal returned\n", static_cast<int>(id));
		pump();
	}

	/* persistent dialogs live in the factory; releaseDialog ends the
	 * use (useEnd) for them and deletes non-persistent ones */
	const bool had_help_url = dlg->getHelpUrl().size() > 0;
	factory->releaseDialog(dlg);
	pump();

	/* UI03: a dialog advertising a help url must have presented its
	 * Help button inside an existing action row */
	if (had_help_url && (!ctx.help_seen || ctx.help_bad))
		g_critical("smoke: id %d — Help button missing or stranded in "
				   "its own row\n", static_cast<int>(id));

	g_print("smoke: id %d (type %d) — %d criticals, %d warnings\n",
			static_cast<int>(id), static_cast<int>(type),
			g_criticals, g_warnings);
	return g_criticals ? 1 : 0;
}

int main(int argc, char **argv)
{
	g_log_set_writer_func(smoke_log_writer, nullptr, nullptr);
	if (!getenv("SMOKE_CORE")) {
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = smoke_segv;
		sa.sa_flags = SA_RESETHAND;
		sigaction(SIGSEGV, &sa, nullptr);
		sigaction(SIGABRT, &sa, nullptr);
	}
	/* last-resort watchdog; dlgswrap wraps us in `timeout` as well */
	alarm(45);

	bool wantList = false;
	long wantId = -1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--list") == 0)
			wantList = true;
		else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc)
			wantId = strtol(argv[++i], nullptr, 10);
	}
	if (!wantList && wantId < 0) {
		g_printerr("usage: %s --list | --id N\n", argv[0]);
		return 2;
	}

	if (!gtk_init_check()) {
		g_printerr("smoke: no display\n");
		return 77;
	}

	XAP_App::s_szBuild_ID = "TEST";
	XAP_App::s_szAbiSuite_Home = g_get_tmp_dir();
	XAP_App::s_szBuild_Version = "TEST";
	XAP_App::s_szBuild_Options = "TEST";
	XAP_App::s_szBuild_Target = "TEST";

	AP_UnixApp *app = new AP_UnixApp(PACKAGE);
	if (!app || !app->initialize(TRUE)) {
		g_printerr("smoke: AP_UnixApp initialize failed\n");
		return 1;
	}
	XAP_Frame *frame = app->newFrame();
	if (!frame) {
		g_printerr("smoke: no frame\n");
		return 1;
	}

	/* a real document widens coverage — many dialogs read doc state */
	const char *src = getenv("ABINOVA_TEST_SRC_DIR");
	if (src) {
		char *path = g_strdup_printf("file://%s/test/wp/BillOfRights.abw",
									src);
		UT_Error err = frame->loadDocument(path, IEFT_Unknown, true);
		g_free(path);
		if (getenv("SMOKE_DEBUG"))
			g_printerr("smoke-dbg: loadDocument -> %d\n", err);
		if (err == UT_OK)
			frame->show();
		pump();
	}
	if (getenv("SMOKE_DEBUG"))
		g_printerr("smoke-dbg: view=%p doc=%p\n",
				   (void *)frame->getCurrentView(),
				   (void *)frame->getCurrentDoc());

	XAP_DialogFactory *factory =
		static_cast<XAP_DialogFactory *>(frame->getDialogFactory());
	if (!factory) {
		g_printerr("smoke: frame has no dialog factory\n");
		return 1;
	}

	/* the EPUB export-options dialog registers dynamically when the
	 * exporter is constructed — it is absent from the static table,
	 * and the runtime registers it on the app factory while the walk
	 * below enumerates the frame factory. */
	factory->registerDialog(ap_Dialog_EpubExportOptions_Constructor,
							XAP_DLGT_NON_PERSISTENT);

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
			return run_one(factory, frame, e->m_id, e->m_type);
	}
	g_printerr("smoke: id %ld not registered\n", wantId);
	return 1;
}
