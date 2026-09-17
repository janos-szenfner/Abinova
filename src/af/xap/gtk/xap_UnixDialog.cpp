/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* AbiWord
 * Copyright (C) 2019 Hubert Figuière
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


#include "xap_UnixDialog.h"
#include "xap_UnixDialogHelper.h"

static gboolean s_delete_clicked(GtkWindow *, gpointer)
{
  /* GTK4: returning FALSE lets the default close-request handler
   * destroy the window; modeless dialogs install their own
   * close-request handler that runs the framework cleanup path
   * (the GTK3 "destroy" signal does not exist anymore). */
  return FALSE;
}

void XAP_UnixDialog::connectBasicSignals()
{
  g_signal_connect(G_OBJECT(m_windowMain),
                   "close-request",
                   G_CALLBACK(s_delete_clicked),
                   static_cast<gpointer>(this));
}
