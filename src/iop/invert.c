/*
    This file is part of darktable,
    Copyright (C) 2011-2022 darktable developers.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gtk/gtk.h>
#include <stdlib.h>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
#include "common/colorspaces.h"
#include "control/control.h"
#include "develop/imageop.h"
#include "develop/imageop_math.h"
#include "dtgtk/button.h"
#include "dtgtk/resetlabel.h"
#include "gui/color_picker_proxy.h"

#include "gui/gtk.h"
#include "iop/iop_api.h"

DT_MODULE_INTROSPECTION(2, dt_iop_invert_params_t)

typedef struct dt_iop_invert_params_t
{
  float color[4]; // $DEFAULT: 1.0 color of film material
} dt_iop_invert_params_t;

typedef struct dt_iop_invert_gui_data_t
{
  GtkWidget *colorpicker;
  GtkDarktableResetLabel *label;
  GtkBox *pickerbuttons;
  GtkWidget *picker;
  double RGB_to_CAM[4][3];
  double CAM_to_RGB[3][4];
} dt_iop_invert_gui_data_t;

typedef struct dt_iop_invert_global_data_t
{
  int kernel_invert_1f;
  int kernel_invert_4f;
} dt_iop_invert_global_data_t;

typedef struct dt_iop_invert_data_t
{
  float color[4]; // color of film material
} dt_iop_invert_data_t;

int legacy_params(dt_iop_module_t *self, const void *const old_params, const int old_version, void *new_params,
                  const int new_version)
{
  if(old_version == 1 && new_version == 2)
  {
    typedef struct dt_iop_invert_params_v1_t
    {
      float color[3]; // color of film material
    } dt_iop_invert_params_v1_t;

    dt_iop_invert_params_v1_t *o = (dt_iop_invert_params_v1_t *)old_params;
    dt_iop_invert_params_t *n = (dt_iop_invert_params_t *)new_params;

    n->color[0] = o->color[0];
    n->color[1] = o->color[1];
    n->color[2] = o->color[2];
    n->color[3] = NAN;

    if(self->dev && self->dev->image_storage.flags & DT_IMAGE_4BAYER)
    {
      double RGB_to_CAM[4][3];

      // Get and store the matrix to go from camera to RGB for 4Bayer images (used for spot WB)
      if(!dt_colorspaces_conversion_matrices_rgb(self->dev->image_storage.adobe_XYZ_to_CAM,
                                                 RGB_to_CAM, NULL,
                                                 self->dev->image_storage.d65_color_matrix, NULL))
      {
        const char *camera = self->dev->image_storage.camera_makermodel;
        fprintf(stderr, "[invert] `%s' color matrix not found for 4bayer image\n", camera);
        dt_control_log(_("`%s' color matrix not found for 4bayer image"), camera);
      }
      else
      {
        dt_colorspaces_rgb_to_cygm(n->color, 1, RGB_to_CAM);
      }
    }

    return 0;
  }
  return 1;
}


const char *name()
{
  return _("invert");
}

const char *deprecated_msg()
{
  return _("this module is deprecated. please use the negadoctor module instead.");
}

const char **description(struct dt_iop_module_t *self)
{
  return dt_iop_set_description(self, _("invert film negatives"),
                                      _("corrective"),
                                      _("linear, raw, display-referred"),
                                      _("linear, raw"),
                                      _("linear, raw, display-referred"));
}


int default_group()
{
  return IOP_GROUP_FILM;
}

int flags()
{
  return IOP_FLAGS_ONE_INSTANCE | IOP_FLAGS_DEPRECATED;
}

int default_colorspace(dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  return IOP_CS_RAW;
}

static void gui_update_from_coeffs(dt_iop_module_t *self)
{
  dt_iop_invert_gui_data_t *g = (dt_iop_invert_gui_data_t *)self->gui_data;
  dt_iop_invert_params_t *p = (dt_iop_invert_params_t *)self->params;

  GdkRGBA color = (GdkRGBA){.red = p->color[0], .green = p->color[1], .blue = p->color[2], .alpha = 1.0 };

  const dt_image_t *img = &self->dev->image_storage;
  if(img->flags & DT_IMAGE_4BAYER)
  {
    dt_aligned_pixel_t rgb;
    for_four_channels(k) rgb[k] = p->color[k];

    dt_colorspaces_cygm_to_rgb(rgb, 1, g->CAM_to_RGB);

    color.red = rgb[0];
    color.green = rgb[1];
    color.blue = rgb[2];
  }

  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(g->colorpicker), &color);
}

void color_picker_apply(dt_iop_module_t *self, GtkWidget *picker, dt_dev_pixelpipe_iop_t *piece)
{
  static dt_aligned_pixel_t old = { 0.0f, 0.0f, 0.0f, 0.0f };

  const float *grayrgb = self->picked_color;

  if(grayrgb[0] == old[0] && grayrgb[1] == old[1] && grayrgb[2] == old[2] && grayrgb[3] == old[3]) return;

  for_four_channels(k) old[k] = grayrgb[k];

  dt_iop_invert_params_t *p = self->params;
  for_four_channels(k) p->color[k] = grayrgb[k];

  ++darktable.gui->reset;
  gui_update_from_coeffs(self);
  --darktable.gui->reset;

  dt_dev_add_history_item(darktable.develop, self, TRUE);
  dt_control_queue_redraw_widget(self->widget);
}

static void colorpicker_callback(GtkColorButton *widget, dt_iop_module_t *self)
{
  if(darktable.gui->reset) return;
  dt_iop_invert_gui_data_t *g = (dt_iop_invert_gui_data_t *)self->gui_data;
  dt_iop_invert_params_t *p = (dt_iop_invert_params_t *)self->params;

  dt_iop_color_picker_reset(self, TRUE);

  GdkRGBA c;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &c);
  p->color[0] = c.red;
  p->color[1] = c.green;
  p->color[2] = c.blue;

  const dt_image_t *img = &self->dev->image_storage;
  if(img->flags & DT_IMAGE_4BAYER)
  {
    dt_colorspaces_rgb_to_cygm(p->color, 1, g->RGB_to_CAM);
  }
  else if(dt_image_is_monochrome(img))
  { // Just to make sure the monochrome stays monochrome we take the luminosity of the chosen color on all channels
    p->color[0] = p->color[1] = p->color[2] = 0.21f*c.red + 0.72f*c.green + 0.07f*c.blue ;
  }
  dt_dev_add_history_item(darktable.develop, self, TRUE);
}

void process(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, const void *const ivoid,
             void *const ovoid, const dt_iop_roi_t *const roi_in, const dt_iop_roi_t *const roi_out)
{
  const dt_iop_invert_data_t *const d = (dt_iop_invert_data_t *)piece->data;

  const float *const m = piece->pipe->dsc.processed_maximum;

  const dt_aligned_pixel_t film_rgb_f
      = { d->color[0] * m[0], d->color[1] * m[1], d->color[2] * m[2], d->color[3] * m[3] };

  // FIXME: it could be wise to make this a NOP when picking colors. not sure about that though.
  //   if(self->request_color_pick){
  // do nothing
  //   }

  const uint32_t filters = piece->pipe->dsc.filters;
  const uint8_t(*const xtrans)[6] = (const uint8_t(*const)[6])piece->pipe->dsc.xtrans;

  const float *const in = (const float *const)ivoid;
  float *const out = (float *const)ovoid;

  if(filters == 9u)
  { // xtrans float mosaiced
#ifdef _OPENMP
#pragma omp parallel for SIMD() default(none) \
    dt_omp_firstprivate(film_rgb_f, in, out, roi_out, xtrans) \
    schedule(static) \
    collapse(2)
#endif
    for(int j = 0; j < roi_out->height; j++)
    {
      for(int i = 0; i < roi_out->width; i++)
      {
        const size_t p = (size_t)j * roi_out->width + i;
        out[p] = CLAMP(film_rgb_f[FCxtrans(j, i, roi_out, xtrans)] - in[p], 0.0f, 1.0f);
      }
    }

    for(int k = 0; k < 4; k++) piece->pipe->dsc.processed_maximum[k] = 1.0f;
  }
  else if(filters)
  { // bayer float mosaiced

#ifdef _OPENMP
#pragma omp parallel for SIMD() default(none) \
    dt_omp_firstprivate(film_rgb_f, filters, in, out, roi_out) \
    schedule(static) \
    collapse(2)
#endif
    for(int j = 0; j < roi_out->height; j++)
    {
      for(int i = 0; i < roi_out->width; i++)
      {
        const size_t p = (size_t)j * roi_out->width + i;
        out[p] = CLAMP(film_rgb_f[FC(j + roi_out->y, i + roi_out->x, filters)] - in[p], 0.0f, 1.0f);
      }
    }

    for(int k = 0; k < 4; k++) piece->pipe->dsc.processed_maximum[k] = 1.0f;
  }
  else
  { // non-mosaiced
    const int ch = piece->colors;

#ifdef _OPENMP
#pragma omp parallel for SIMD() default(none) \
    dt_omp_firstprivate(ch, d, in, out, roi_out) \
    schedule(static) \
    collapse(2)
#endif
    for(size_t k = 0; k < (size_t)ch * roi_out->width * roi_out->height; k += ch)
    {
      for(int c = 0; c < 3; c++)
      {
        const size_t p = (size_t)k + c;
        out[p] = d->color[c] - in[p];
      }
    }

    if(piece->pipe->mask_display & DT_DEV_PIXELPIPE_DISPLAY_MASK) dt_iop_alpha_copy(ivoid, ovoid, roi_out->width, roi_out->height);
  }
}

#if defined(__SSE__)
void process_sse2(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, const void *const ivoid,
                  void *const ovoid, const dt_iop_roi_t *const roi_in, const dt_iop_roi_t *const roi_out)
{
  dt_iop_invert_data_t *d = (dt_iop_invert_data_t *)piece->data;

  const float *const m = piece->pipe->dsc.processed_maximum;

  const dt_aligned_pixel_t film_rgb_f
      = { d->color[0] * m[0], d->color[1] * m[1], d->color[2] * m[2], d->color[3] * m[3] };

  // FIXME: it could be wise to make this a NOP when picking colors. not sure about that though.
  //   if(self->request_color_pick){
  // do nothing
  //   }

  const uint32_t filters = piece->pipe->dsc.filters;
  const uint8_t(*const xtrans)[6] = (const uint8_t(*const)[6])piece->pipe->dsc.xtrans;

  if(filters == 9u)
  { // xtrans float mosaiced

    const __m128 val_min = _mm_setzero_ps();
    const __m128 val_max = _mm_set1_ps(1.0f);

#ifdef _OPENMP
#pragma omp parallel for default(none) \
    dt_omp_firstprivate(film_rgb_f, ivoid, ovoid, roi_out, val_max, val_min, xtrans) \
    schedule(static)
#endif
    for(int j = 0; j < roi_out->height; j++)
    {
      const float *in = ((float *)ivoid) + (size_t)j * roi_out->width;
      float *out = ((float *)ovoid) + (size_t)j * roi_out->width;

      int i = 0;

      int alignment = ((4 - (j * roi_out->width & (4 - 1))) & (4 - 1));

      // process unaligned pixels
      for(; i < alignment && i < roi_out->width; i++, out++, in++)
        *out = CLAMP(film_rgb_f[FCxtrans(j, i, roi_out, xtrans)] - *in, 0.0f, 1.0f);

      const __m128 film[3] = { _mm_set_ps(film_rgb_f[FCxtrans(j, i + 3, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 2, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 1, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 0, roi_out, xtrans)]),
                               _mm_set_ps(film_rgb_f[FCxtrans(j, i + 7, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 6, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 5, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 4, roi_out, xtrans)]),
                               _mm_set_ps(film_rgb_f[FCxtrans(j, i + 11, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 10, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 9, roi_out, xtrans)],
                                          film_rgb_f[FCxtrans(j, i + 8, roi_out, xtrans)]) };

      // process aligned pixels with SSE
      for(int c = 0; c < 3 && i < roi_out->width - (4 - 1); c++, i += 4, in += 4, out += 4)
      {
        __m128 v;

        v = _mm_load_ps(in);
        v = _mm_sub_ps(film[c], v);
        v = _mm_min_ps(v, val_max);
        v = _mm_max_ps(v, val_min);
        _mm_stream_ps(out, v);
      }

      // process the rest
      for(; i < roi_out->width; i++, out++, in++)
        *out = CLAMP(film_rgb_f[FCxtrans(j, i, roi_out, xtrans)] - *in, 0.0f, 1.0f);
    }
    _mm_sfence();

    for(int k = 0; k < 4; k++) piece->pipe->dsc.processed_maximum[k] = 1.0f;
  }
  else if(filters)
  { // bayer float mosaiced

    const __m128 val_min = _mm_setzero_ps();
    const __m128 val_max = _mm_set1_ps(1.0f);

#ifdef _OPENMP
#pragma omp parallel for default(none) \
    dt_omp_firstprivate(film_rgb_f, filters, ivoid, ovoid, roi_out, val_max, val_min) \
    schedule(static)
#endif
    for(int j = 0; j < roi_out->height; j++)
    {
      const float *in = ((float *)ivoid) + (size_t)j * roi_out->width;
      float *out = ((float *)ovoid) + (size_t)j * roi_out->width;

      int i = 0;
      int alignment = ((4 - (j * roi_out->width & (4 - 1))) & (4 - 1));

      // process unaligned pixels
      for(; i < alignment && i < roi_out->width; i++, out++, in++)
        *out = CLAMP(film_rgb_f[FC(j + roi_out->y, i + roi_out->x, filters)] - *in, 0.0f, 1.0f);

      const __m128 film = _mm_set_ps(film_rgb_f[FC(j + roi_out->y, roi_out->x + i + 3, filters)],
                                     film_rgb_f[FC(j + roi_out->y, roi_out->x + i + 2, filters)],
                                     film_rgb_f[FC(j + roi_out->y, roi_out->x + i + 1, filters)],
                                     film_rgb_f[FC(j + roi_out->y, roi_out->x + i, filters)]);

      // process aligned pixels with SSE
      for(; i < roi_out->width - (4 - 1); i += 4, in += 4, out += 4)
      {
        const __m128 input = _mm_load_ps(in);
        const __m128 subtracted = _mm_sub_ps(film, input);
        _mm_stream_ps(out, _mm_max_ps(_mm_min_ps(subtracted, val_max), val_min));
      }

      // process the rest
      for(; i < roi_out->width; i++, out++, in++)
        *out = CLAMP(film_rgb_f[FC(j + roi_out->y, i + roi_out->x, filters)] - *in, 0.0f, 1.0f);
    }
    _mm_sfence();

    for(int k = 0; k < 4; k++) piece->pipe->dsc.processed_maximum[k] = 1.0f;
  }
  else
  { // non-mosaiced
    const int ch = piece->colors;

    const __m128 film = _mm_set_ps(1.0f, d->color[2], d->color[1], d->color[0]);

#ifdef _OPENMP
#pragma omp parallel for default(none) \
    dt_omp_firstprivate(ch, film, ivoid, ovoid, roi_out) \
    schedule(static)
#endif
    for(int k = 0; k < roi_out->height; k++)
    {
      const float *in = ((float *)ivoid) + (size_t)ch * k * roi_out->width;
      float *out = ((float *)ovoid) + (size_t)ch * k * roi_out->width;
      for(int j = 0; j < roi_out->width; j++, in += ch, out += ch)
      {
        const __m128 input = _mm_load_ps(in);
        const __m128 subtracted = _mm_sub_ps(film, input);
        _mm_stream_ps(out, subtracted);
      }
    }
    _mm_sfence();

    if(piece->pipe->mask_display & DT_DEV_PIXELPIPE_DISPLAY_MASK) dt_iop_alpha_copy(ivoid, ovoid, roi_out->width, roi_out->height);
  }
}
#endif

#ifdef HAVE_OPENCL
int process_cl(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, cl_mem dev_in, cl_mem dev_out,
               const dt_iop_roi_t *const roi_in, const dt_iop_roi_t *const roi_out)
{
  dt_iop_invert_data_t *d = (dt_iop_invert_data_t *)piece->data;
  dt_iop_invert_global_data_t *gd = (dt_iop_invert_global_data_t *)self->global_data;

  const int devid = piece->pipe->devid;
  const uint32_t filters = piece->pipe->dsc.filters;
  cl_mem dev_color = NULL;
  cl_int err = -999;
  int kernel = -1;

  dt_aligned_pixel_t film_rgb_f = { d->color[0], d->color[1], d->color[2], d->color[3] };

  if(filters)
  {
    kernel = gd->kernel_invert_1f;

    const float *const m = piece->pipe->dsc.processed_maximum;
    for(int c = 0; c < 4; c++) film_rgb_f[c] *= m[c];
  }
  else
  {
    kernel = gd->kernel_invert_4f;
  }

  dev_color = dt_opencl_copy_host_to_device_constant(devid, sizeof(float) * 3, film_rgb_f);
  if(dev_color == NULL) goto error;

  const int width = roi_in->width;
  const int height = roi_in->height;

  size_t sizes[] = { ROUNDUPDWD(width, devid), ROUNDUPDHT(height, devid), 1 };
  dt_opencl_set_kernel_arg(devid, kernel, 0, sizeof(cl_mem), (void *)&dev_in);
  dt_opencl_set_kernel_arg(devid, kernel, 1, sizeof(cl_mem), (void *)&dev_out);
  dt_opencl_set_kernel_arg(devid, kernel, 2, sizeof(int), (void *)&width);
  dt_opencl_set_kernel_arg(devid, kernel, 3, sizeof(int), (void *)&height);
  dt_opencl_set_kernel_arg(devid, kernel, 4, sizeof(cl_mem), (void *)&dev_color);
  dt_opencl_set_kernel_arg(devid, kernel, 5, sizeof(uint32_t), (void *)&filters);
  dt_opencl_set_kernel_arg(devid, kernel, 6, sizeof(uint32_t), (void *)&roi_out->x);
  dt_opencl_set_kernel_arg(devid, kernel, 7, sizeof(uint32_t), (void *)&roi_out->y);
  err = dt_opencl_enqueue_kernel_2d(devid, kernel, sizes);
  if(err != CL_SUCCESS) goto error;

  dt_opencl_release_mem_object(dev_color);
  for(int k = 0; k < 4; k++) piece->pipe->dsc.processed_maximum[k] = 1.0f;
  return TRUE;

error:
  dt_opencl_release_mem_object(dev_color);
  dt_print(DT_DEBUG_OPENCL, "[opencl_invert] couldn't enqueue kernel! %d\n", err);
  return FALSE;
}
#endif

void reload_defaults(dt_iop_module_t *self)
{
  dt_iop_invert_gui_data_t *const g = (dt_iop_invert_gui_data_t*)self->gui_data;

  if (g)
  {
    if(dt_image_is_monochrome(&self->dev->image_storage))
    {
      // Here we could provide more for monochrome special cases. As no monochrome camera
      // has a bayer sensor we don't need g->RGB_to_CAM and g->CAM_to_RGB corrections
      dtgtk_reset_label_set_text(g->label, _("brightness of film material"));
    }
    else
    {
      dtgtk_reset_label_set_text(g->label, _("color of film material"));

      if(self->dev->image_storage.flags & DT_IMAGE_4BAYER)
      {
        // Get and store the matrix to go from camera to RGB for 4Bayer images (used for spot WB)
        if(!dt_colorspaces_conversion_matrices_rgb(self->dev->image_storage.adobe_XYZ_to_CAM,
                                                   g->RGB_to_CAM, g->CAM_to_RGB,
                                                   self->dev->image_storage.d65_color_matrix, NULL))
        {
          const char *camera = self->dev->image_storage.camera_makermodel;
          fprintf(stderr, "[invert] `%s' color matrix not found for 4bayer image\n", camera);
          dt_control_log(_("`%s' color matrix not found for 4bayer image"), camera);
        }
      }
    }
  }
}

void init_global(dt_iop_module_so_t *module)
{
  const int program = 2; // basic.cl, from programs.conf
  module->data = malloc(sizeof(dt_iop_invert_global_data_t));

  dt_iop_invert_global_data_t *gd = module->data;
  gd->kernel_invert_1f = dt_opencl_create_kernel(program, "invert_1f");
  gd->kernel_invert_4f = dt_opencl_create_kernel(program, "invert_4f");
}

void cleanup_global(dt_iop_module_so_t *module)
{
  dt_iop_invert_global_data_t *gd = (dt_iop_invert_global_data_t *)module->data;
  dt_opencl_free_kernel(gd->kernel_invert_4f);
  dt_opencl_free_kernel(gd->kernel_invert_1f);
  free(module->data);
  module->data = NULL;
}

void commit_params(struct dt_iop_module_t *self, dt_iop_params_t *params, dt_dev_pixelpipe_t *pipe,
                   dt_dev_pixelpipe_iop_t *piece)
{
  dt_iop_invert_params_t *p = (dt_iop_invert_params_t *)params;
  dt_iop_invert_data_t *d = (dt_iop_invert_data_t *)piece->data;

  for(int k = 0; k < 4; k++) d->color[k] = p->color[k];

  // x-trans images not implemented in OpenCL yet
  if(pipe->image.buf_dsc.filters == 9u) piece->process_cl_ready = 0;

  // 4Bayer images not implemented in OpenCL yet
  if(self->dev->image_storage.flags & DT_IMAGE_4BAYER) piece->process_cl_ready = 0;

  if(self->hide_enable_button) piece->enabled = 0;
}

void init_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  piece->data = g_malloc0(sizeof(dt_iop_invert_data_t));
  piece->data_size = sizeof(dt_iop_invert_data_t);
}

void cleanup_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  g_free(piece->data);
  piece->data = NULL;
}

void gui_update(dt_iop_module_t *self)
{
  gui_update_from_coeffs(self);
}

void gui_init(dt_iop_module_t *self)
{
  dt_iop_invert_gui_data_t *g = IOP_GUI_ALLOC(invert);
  dt_iop_invert_params_t *p = (dt_iop_invert_params_t *)self->params;

  self->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g->label = DTGTK_RESET_LABEL(dtgtk_reset_label_new("", self, &p->color, sizeof(float) * 4));
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(g->label), TRUE, TRUE, 0);

  g->pickerbuttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(g->pickerbuttons), TRUE, TRUE, 0);

  GdkRGBA color = (GdkRGBA){.red = p->color[0], .green = p->color[1], .blue = p->color[2], .alpha = 1.0 };
  g->colorpicker = gtk_color_button_new_with_rgba(&color);
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(g->colorpicker), FALSE);
  gtk_color_button_set_title(GTK_COLOR_BUTTON(g->colorpicker), _("select color of film material"));
  g_signal_connect(G_OBJECT(g->colorpicker), "color-set", G_CALLBACK(colorpicker_callback), self);
  gtk_box_pack_start(GTK_BOX(g->pickerbuttons), GTK_WIDGET(g->colorpicker), TRUE, TRUE, 0);

  g->picker = dt_color_picker_new(self, DT_COLOR_PICKER_AREA, GTK_WIDGET(g->pickerbuttons));
}

// clang-format off
// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.py
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
// clang-format on
