/* GTK4 removed GtkWidgetPath and the ability to synthesize a style
 * context for an arbitrary selector. Style information is now obtained
 * from the style context of a real (hidden, unrealized) widget.
 *
 * The selector strings used by callers are legacy GTK3 widget paths
 * like "GtkButton" or "textview.view"; we map them to donor widget
 * types kept alive for the lifetime of the application.
 */

#include <string.h>

#include "ut_assert.h"

#include "xap_GtkStyle.h"

static GtkWidget *
donor_widget (const char *selector, gboolean selected)
{
	static GtkWidget *donors[2][3] = { { nullptr, nullptr, nullptr },
					   { nullptr, nullptr, nullptr } };
	int index = 2; /* label */

	if (strstr (selector, "Button") || strstr (selector, "button"))
	  {
		index = 0;
	  }
	else if (strstr (selector, "TreeView") || strstr (selector, "textview"))
	  {
		index = 1;
	  }

	GtkWidget **slot = &donors[selected ? 1 : 0][index];

	if (!*slot)
	  {
		if (index == 0)
		  *slot = gtk_button_new ();
		else if (index == 1)
		  *slot = gtk_text_view_new ();
		else
		  *slot = gtk_label_new (nullptr);
		if (selected)
		  gtk_widget_set_state_flags (*slot, GTK_STATE_FLAG_SELECTED,
					      TRUE);
		/* intentional leak: style donors live as long as the app */
		g_object_ref_sink (*slot);
	  }

	return *slot;
}

GtkStyleContext *
XAP_GtkStyle_get_style (GtkStyleContext * /*parent*/,
                        const char      *selector)
{
	GtkWidget *w = donor_widget (selector, FALSE);
	return g_object_ref (gtk_widget_get_style_context (w));
}

GtkWidget *
XAP_GtkStyle_get_widget (const char   *selector,
                         GtkStateFlags state_flags)
{
	return donor_widget (selector,
			     (state_flags & GTK_STATE_FLAG_SELECTED) != 0);
}
