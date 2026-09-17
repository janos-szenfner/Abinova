/* GTK4 removed GtkWidgetPath and the ability to synthesize a style
 * context for an arbitrary selector. Style information is now obtained
 * from the style context of a real (hidden, unrealized) widget.
 *
 * The selector strings used by callers are legacy GTK3 widget paths
 * like "GtkButton" or "GtkTreeView.view"; we map them to donor widget
 * types kept alive for the lifetime of the application.
 */

#include <string.h>

#include "ut_assert.h"

#include "xap_GtkStyle.h"

static GtkWidget *
donor_widget (const char *selector)
{
	static GtkWidget *button = nullptr;
	static GtkWidget *textview = nullptr;
	static GtkWidget *label = nullptr;
	GtkWidget **slot = &label;

	if (strstr (selector, "Button") || strstr (selector, "button"))
	  {
		slot = &button;
	  }
	else if (strstr (selector, "TreeView") || strstr (selector, "textview"))
	  {
		slot = &textview;
	  }

	if (!*slot)
	  {
		if (slot == &button)
		  *slot = gtk_button_new ();
		else if (slot == &textview)
		  *slot = gtk_text_view_new ();
		else
		  *slot = gtk_label_new (nullptr);
		/* intentional leak: style donors live as long as the app */
		g_object_ref_sink (*slot);
	  }

	return *slot;
}

GtkStyleContext *
XAP_GtkStyle_get_style (GtkStyleContext * /*parent*/,
                        const char      *selector)
{
	GtkWidget *w = donor_widget (selector);
	return g_object_ref (gtk_widget_get_style_context (w));
}
