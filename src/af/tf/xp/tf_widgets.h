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
 * TST01: widget-tree helpers for the UI driver and widget-level
 * tests — generic depth-first traversal plus the find-by-* queries
 * ui_drive's legs kept open-coding.  GTK4 has no
 * gtk_container_forall; children are reached via
 * gtk_widget_get_first_child / gtk_widget_get_next_sibling.
 *
 * The em-row helpers key on the "abi-em-method" / "abi-em-data"
 * qdata the menu and toolbar builders attach to each actionable
 * row — that is how a test activates a ribbon button, popover row
 * or menu item BY NAME instead of by tree position.
 */

#ifndef TF_WIDGETS_H
#define TF_WIDGETS_H

#include <functional>
#include <vector>
#include <cstring>

#include <gtk/gtk.h>

namespace tf_widgets {

typedef std::function<bool (GtkWidget *)> Pred;

/* depth-first pre-order visit of w and every descendant */
inline void for_each(GtkWidget *w,
					 const std::function<void (GtkWidget *)> &fn)
{
	fn(w);
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		for_each(c, fn);
}

/* first widget matching pred in pre-order, or nullptr */
inline GtkWidget *find(GtkWidget *w, const Pred &pred)
{
	if (!w)
		return nullptr;
	if (pred(w))
		return w;
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
	{
		GtkWidget *r = find(c, pred);
		if (r)
			return r;
	}
	return nullptr;
}

/* every descendant matching pred, pre-order */
inline void find_all(GtkWidget *w, const Pred &pred,
					 std::vector<GtkWidget *> &out)
{
	if (pred(w))
		out.push_back(w);
	for (GtkWidget *c = gtk_widget_get_first_child(w); c;
		 c = gtk_widget_get_next_sibling(c))
		find_all(c, pred, out);
}

inline GtkWidget *find_type(GtkWidget *w, GType t)
{
	return find(w, [t](GtkWidget *c) {
		return g_type_check_instance_is_a(
			reinterpret_cast<GTypeInstance *>(c), t) != 0;
	});
}

inline GtkWidget *find_named(GtkWidget *w, const char *name)
{
	return find(w, [name](GtkWidget *c) {
		const char *n = gtk_widget_get_name(c);
		return n && !strcmp(n, name);
	});
}

/* action row bound via the abi-em-method / abi-em-data qdata the
 * menu and toolbar builders set; data == nullptr matches any
 * payload */
inline GtkWidget *find_em_row(GtkWidget *w, const char *method,
							  const char *data = nullptr)
{
	return find(w, [method, data](GtkWidget *c) {
		const char *m = static_cast<const char *>(
			g_object_get_data(G_OBJECT(c), "abi-em-method"));
		if (!m || strcmp(m, method))
			return false;
		if (!data)
			return true;
		const char *d = static_cast<const char *>(
			g_object_get_data(G_OBJECT(c), "abi-em-data"));
		return d && !strcmp(d, data);
	});
}

inline bool has_em_row(GtkWidget *w, const char *method)
{
	return find_em_row(w, method) != nullptr;
}

/* every toplevel the windowing system knows about */
inline std::vector<GtkWidget *> toplevels(void)
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

/* activate a button-ish widget the way a user click would */
inline void click(GtkWidget *w)
{
	g_signal_emit_by_name(w, "clicked");
}

inline void activate(GtkWidget *w)
{
	g_signal_emit_by_name(w, "activate", 0);
}

} /* namespace tf_widgets */

#endif /* TF_WIDGETS_H */
