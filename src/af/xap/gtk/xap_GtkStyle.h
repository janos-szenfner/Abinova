

#ifndef __XAP_GTK_STYLE_H
#define __XAP_GTK_STYLE_H

#include <gtk/gtk.h>

GtkStyleContext *
XAP_GtkStyle_get_style (GtkStyleContext *parent,
                        const char      *selector);

/* The donor widget backing a selector. Needed for non-deprecated
 * queries like gtk_widget_get_color(). state_flags are applied to the
 * donor once at creation (only GTK_STATE_FLAG_SELECTED is used); pass
 * GTK_STATE_FLAG_NORMAL for the default donor. */
GtkWidget *
XAP_GtkStyle_get_widget (const char   *selector,
                         GtkStateFlags state_flags);


#endif
