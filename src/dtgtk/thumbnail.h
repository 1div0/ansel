/*
    This file is part of darktable,
    Copyright (C) 2019-2021 darktable developers.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
/** this is the thumbnail class for the lighttable module.  */

#pragma once

#include "common/darktable.h"
#include "common/debug.h"

#include <glib.h>
#include <gtk/gtk.h>

#define MAX_STARS 5

struct dt_thumbtable_t;

typedef enum dt_thumbnail_border_t
{
  DT_THUMBNAIL_BORDER_NONE = 0,
  DT_THUMBNAIL_BORDER_LEFT = 1 << 0,
  DT_THUMBNAIL_BORDER_TOP = 1 << 1,
  DT_THUMBNAIL_BORDER_RIGHT = 1 << 2,
  DT_THUMBNAIL_BORDER_BOTTOM = 1 << 3,
} dt_thumbnail_border_t;

typedef enum dt_thumbnail_overlay_t
{
  DT_THUMBNAIL_OVERLAYS_NONE,
  DT_THUMBNAIL_OVERLAYS_HOVER_NORMAL,
  DT_THUMBNAIL_OVERLAYS_ALWAYS_NORMAL,
  DT_THUMBNAIL_OVERLAYS_LAST
} dt_thumbnail_overlay_t;

typedef struct
{
  int32_t imgid, rowid, groupid;
  int width, height;         // current thumb size (with the background and the border)
  int x, y;                  // current position at screen
  int img_width, img_height; // current image size (can be greater than the image box in case of zoom)

  gboolean mouse_over;
  gboolean selected;

  int rating;
  int colorlabels;
  gchar *filename;
  gchar *info_line;
  gboolean is_altered;
  gboolean has_audio;
  gboolean is_grouped;
  gboolean is_bw;
  gboolean is_bw_flow;
  gboolean is_hdr;
  gboolean has_localcopy;

  // all widget components
  GtkWidget *widget;               // GtkEventbox -- parent of all others
  GtkWidget *w_main;               // GtkOverlay --
  GtkWidget *w_ext;                // GtkLabel -- thumbnail extension

  GtkWidget *w_image;        // GtkDrawingArea -- thumbnail image
  GtkBorder *img_margin;     // in percentage of the main widget size
  cairo_surface_t *img_surf; // cached surface at exact dimensions to speed up redraw

  GtkWidget *w_cursor;    // GtkDrawingArea -- triangle to show current image(s) in filmstrip
  GtkWidget *w_bottom_eb; // GtkEventBox -- background of the bottom infos area (contains w_bottom)
  GtkWidget *w_bottom;    // GtkLabel -- text of the bottom infos area, just with #thumb-bottom
  GtkWidget *w_reject;    // GtkDarktableThumbnailBtn -- Reject icon
  GtkWidget *w_stars[MAX_STARS];  // GtkDarktableThumbnailBtn -- Stars icons
  GtkWidget *w_color;     // GtkDarktableThumbnailBtn -- Colorlabels "flower" icon

  GtkWidget *w_local_copy; // GtkDarktableThumbnailBtn -- localcopy triangle
  GtkWidget *w_altered;    // GtkDarktableThumbnailBtn -- Altered icon
  GtkWidget *w_group;      // GtkDarktableThumbnailBtn -- Grouping icon
  GtkWidget *w_audio;      // GtkDarktableThumbnailBtn -- Audio sidecar icon

  GtkWidget *w_alternative; // alternative overlay

  dt_thumbnail_border_t group_borders; // which group borders should be drawn

  gboolean disable_mouseover;             // do we allow to change mouseoverid by mouse move
  gboolean disable_actions;               // do we allow to change rating/etc...

  dt_thumbnail_overlay_t over;  // type of overlays

  // difference between the global zoom values and the value to apply to this specific thumbnail
  double zoomx;   // zoom panning of the image
  double zoomy;   //
  double drag_x_start;
  double drag_y_start;
  gboolean dragging;

  struct dt_thumbtable_t *table; // convenience reference to the parent

  // Set FALSE when the thumbnail size changed, set TRUE when we have a Cairo image surface for that size
  gboolean image_inited;

  gboolean alternative_mode;
  float iso;
  float aperture;
  float speed;
  float exposure_bias;
  float focal;
  float focus_distance;
  char datetime[200];
  char camera[128];
  char lens[128];
  char folder[PATH_MAX];

  GtkWidget *w_exposure;
  GtkWidget *w_exposure_bias;
  GtkWidget *w_camera;
  GtkWidget *w_filename;
  GtkWidget *w_datetime;
  GtkWidget *w_lens;
  GtkWidget *w_focal;
  GtkWidget *w_folder;

  gboolean busy; // should we show the busy message ?
  gboolean drawn; // image already drawn, nothing more to do

} dt_thumbnail_t;

dt_thumbnail_t *dt_thumbnail_new(int32_t imgid, int rowid, int32_t groupid, dt_thumbnail_overlay_t over, struct dt_thumbtable_t *table);
int dt_thumbnail_destroy(dt_thumbnail_t *thumb);
GtkWidget *dt_thumbnail_create_widget(dt_thumbnail_t *thumb);
void dt_thumbnail_resize(dt_thumbnail_t *thumb, int width, int height, gboolean force);
void dt_thumbnail_set_group_border(dt_thumbnail_t *thumb, dt_thumbnail_border_t border);
void dt_thumbnail_set_mouseover(dt_thumbnail_t *thumb, gboolean over);
void dt_thumbnail_set_overlay(dt_thumbnail_t *thumb, dt_thumbnail_overlay_t mode);

// set if the thumbnail should react (mouse_over) to drag and drop
// note that it's just cosmetic as dropping occurs in thumbtable in any case
void dt_thumbnail_set_drop(dt_thumbnail_t *thumb, gboolean accept_drop);

// update the information of the image and update icons accordingly
void dt_thumbnail_update_infos(dt_thumbnail_t *thumb);

// check if the image is selected and set its state and background
void dt_thumbnail_update_selection(dt_thumbnail_t *thumb, gboolean selected);

// force image recomputing
int dt_thumbnail_image_refresh_real(dt_thumbnail_t *thumb);
#define dt_thumbnail_image_refresh(thumb) DT_DEBUG_TRACE_WRAPPER(DT_DEBUG_LIGHTTABLE, dt_thumbnail_image_refresh_real, (thumb))

// force reloading image infos
void dt_thumbnail_reload_infos(dt_thumbnail_t *thumb);

void dt_thumbnail_alternative_mode(dt_thumbnail_t *thumb, gboolean nable);

static inline dt_thumbnail_overlay_t sanitize_overlays(dt_thumbnail_overlay_t overlays)
{
  return (dt_thumbnail_overlay_t)MIN(overlays, DT_THUMBNAIL_OVERLAYS_LAST - 1);
}

// clang-format off
// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.py
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
// clang-format on
