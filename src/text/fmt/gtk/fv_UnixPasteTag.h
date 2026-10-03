/* Abinova - unix impl of the post-paste options smart tag
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

#ifndef FV_UNIXPASTETAG_H
#define FV_UNIXPASTETAG_H

#include "ut_types.h"
#include "pt_Types.h"

#include <gtk/gtk.h>

class FV_View;

/* Word-parity "paste options" smart tag: a small clipboard button
 * overlaid on the document canvas just past the end of the pasted
 * range.  The widget lives in the GtkOverlay that wraps the drawing
 * area - the same mechanism FV_UnixSelectionHandles uses - and is
 * created lazily on first use because the view can exist before the
 * frame's widget tree (and with no frame at all on headless convert
 * paths, where every call becomes a no-op).
 */
class ABI_EXPORT FV_UnixPasteTag
{
public:
	FV_UnixPasteTag(FV_View * pView);
	~FV_UnixPasteTag();

	void hide(void);
	void popup(void);
	void setPosition(PT_DocPosition pos);
	bool isVisible(void) const { return m_bVisible; }

private:
	bool _getPositionCoords(PT_DocPosition pos, UT_sint32& x, UT_sint32& y,
							UT_uint32& height) const;
	void _ensureButton(void);
	void _optionPicked(int opt);
	void _showPasteSpecial(void);
	static void s_option_clicked(GtkWidget * w, gpointer data);

	FV_View *   m_pView;
	GtkWidget * m_pOverlay;   /* raw; only dereferenced while m_pButton lives */
	GtkWidget * m_pButton;    /* GtkMenuButton; owned by the overlay, weak-ref'd */
	GtkWidget * m_pPopover;   /* child of m_pButton, weak-ref'd */
	bool        m_bVisible;
};

#endif /* FV_UNIXPASTETAG_H */
