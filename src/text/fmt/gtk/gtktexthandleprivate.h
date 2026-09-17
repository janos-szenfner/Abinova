/* GTK - The GIMP Toolkit
 * Copyright © 2012 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define FV_TYPE_TEXT_HANDLE           (_fv_text_handle_get_type ())
#define FV_TEXT_HANDLE(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), FV_TYPE_TEXT_HANDLE, FvTextHandle))
#define FV_TEXT_HANDLE_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), FV_TYPE_TEXT_HANDLE, FvTextHandleClass))
#define FV_IS_TEXT_HANDLE(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), FV_TYPE_TEXT_HANDLE))
#define FV_IS_TEXT_HANDLE_CLASS(o)    (G_TYPE_CHECK_CLASS_TYPE ((o), FV_TYPE_TEXT_HANDLE))
#define FV_TEXT_HANDLE_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), FV_TYPE_TEXT_HANDLE, FvTextHandleClass))

enum FvTextHandlePosition: uint8_t
{
  FV_TEXT_HANDLE_POSITION_CURSOR,
  FV_TEXT_HANDLE_POSITION_SELECTION_START,
  FV_TEXT_HANDLE_POSITION_SELECTION_END = FV_TEXT_HANDLE_POSITION_CURSOR
};

enum FvTextHandleMode: uint8_t
{
  FV_TEXT_HANDLE_MODE_NONE,
  FV_TEXT_HANDLE_MODE_CURSOR,
  FV_TEXT_HANDLE_MODE_SELECTION
};

struct FvTextHandlePrivate;

struct FvTextHandle
{
  GObject parent_instance;
  FvTextHandlePrivate* priv;
};

struct FvTextHandleClass
{
  GObjectClass parent_class;

  void (* handle_dragged) (FvTextHandle         *handle,
                           FvTextHandlePosition  pos,
                           gint                  x,
                           gint                  y);
  void (* drag_finished)  (FvTextHandle         *handle,
                           FvTextHandlePosition  pos);
};

GType           _fv_text_handle_get_type     (void) G_GNUC_CONST;

FvTextHandle *  _fv_text_handle_new          (GtkWidget             *parent);

void            _fv_text_handle_set_mode     (FvTextHandle          *handle,
                                              FvTextHandleMode       mode);
FvTextHandleMode
                _fv_text_handle_get_mode     (FvTextHandle          *handle);
void            _fv_text_handle_set_position (FvTextHandle          *handle,
                                              FvTextHandlePosition   pos,
                                              GdkRectangle          *rect);
void            _fv_text_handle_set_visible  (FvTextHandle          *handle,
                                              FvTextHandlePosition   pos,
                                              gboolean               visible);

void            _fv_text_handle_set_relative_to (FvTextHandle  *handle,
                                                 GdkWindow     *window);

gboolean        _fv_text_handle_get_is_dragged (FvTextHandle         *handle,
                                                FvTextHandlePosition  pos);

G_END_DECLS
