/*
    This file is part of Ansel,
    Copyright (C) 2023 Aurélien Pierre.

    Ansel is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ansel is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Ansel.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/darktable.h"
#include "common/debug.h"
#include "control/conf.h"
#include "control/control.h"
#include "develop/develop.h"
#include "dtgtk/button.h"
#include "gui/gtk.h"
#include "libs/lib.h"
#include "libs/lib_api.h"
#ifdef GDK_WINDOWING_QUARTZ
#include "osx/osx.h"
#endif

DT_MODULE(1)

typedef struct dt_lib_menubuttons_t
{
  GtkWidget *home;
  GtkWidget *iconify;
  GtkWidget *maximize;
  GtkWidget *close;
} dt_lib_menubuttons_t;

const char *name(struct dt_lib_module_t *self)
{
  return _("Menu buttons");
}

const char **views(dt_lib_module_t *self)
{
  static const char *v[] = {"lighttable", "darkroom", "map", "print", NULL};
  return v;
}

uint32_t container(dt_lib_module_t *self)
{
  return DT_UI_CONTAINER_PANEL_TOP_FIRST_ROW;
}

int expandable(dt_lib_module_t *self)
{
  return 0;
}

int position()
{
  return 0;
}

void _home_callback()
{
  dt_ctl_switch_mode_to("lighttable");
}

void _close_callback()
{
  gtk_window_close(GTK_WINDOW(dt_ui_main_window(darktable.gui->ui)));
}

void _iconify_callback()
{
  gtk_window_iconify(GTK_WINDOW(dt_ui_main_window(darktable.gui->ui)));
}

void _maximize_callback()
{
  if(gtk_window_is_maximized(GTK_WINDOW(dt_ui_main_window(darktable.gui->ui))))
    gtk_window_unmaximize(GTK_WINDOW(dt_ui_main_window(darktable.gui->ui)));
  else
    gtk_window_maximize(GTK_WINDOW(dt_ui_main_window(darktable.gui->ui)));
}

void gui_init(dt_lib_module_t *self)
{
  dt_lib_menubuttons_t *d = (dt_lib_menubuttons_t *)g_malloc0(sizeof(dt_lib_menubuttons_t));
  self->data = (void *)d;

  /* Init container widget */
  self->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign(self->widget, GTK_ALIGN_END);
  gtk_widget_set_hexpand(self->widget, FALSE);

  d->home = gtk_button_new_from_icon_name("go-home", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text(d->home, _("Go back to lighttable"));
  g_signal_connect(G_OBJECT(d->home), "clicked", _home_callback, NULL);
  dt_gui_add_class(d->home, "window-button");
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(d->home), FALSE, FALSE, 0);

  d->iconify = gtk_button_new_from_icon_name("window-minimize", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text(d->iconify, _("Iconify window"));
  g_signal_connect(G_OBJECT(d->iconify), "clicked", _iconify_callback, NULL);
  dt_gui_add_class(d->iconify, "window-button");
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(d->iconify), FALSE, FALSE, 0);

  d->maximize = gtk_button_new_from_icon_name("window-restore", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text(d->maximize, _("Maximize window"));
  g_signal_connect(G_OBJECT(d->maximize), "clicked", _maximize_callback, NULL);
  dt_gui_add_class(d->maximize, "window-button");
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(d->maximize), FALSE, FALSE, 0);

  d->close = gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text(d->close, _("Close the application"));
  g_signal_connect(G_OBJECT(d->close), "clicked", _close_callback, NULL);
  dt_gui_add_class(d->close, "window-button");
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(d->close), FALSE, FALSE, 0);
}

void gui_cleanup(dt_lib_module_t *self)
{
  g_free(self->data);
  self->data = NULL;
}

// clang-format off
// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.py
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
// clang-format on
