
#include "plot_pango.h"

static PangoFontDescription *plot_font;

static cairo_t* plot_create_cairo(void) {
  cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
  g_return_val_if_fail(surface, NULL);
  cairo_t *cairo = NULL;
  int status = cairo_surface_status(surface);
  if (status == CAIRO_STATUS_SUCCESS) cairo = cairo_create(surface);
  else WARN("Status of creating cairo surface: %d", status);
  cairo_surface_destroy(surface);
  return cairo;
}

static cairo_t* plot_create_cairo_from_buff(ivec2 size, cairo_format_t format, cairo_surface_t **psurf, unsigned char **pbuff) {
  if (!pbuff || !psurf) return NULL;
  int stride = cairo_format_stride_for_width(format, size[0]);
  *pbuff = g_malloc0_n(size[1], stride);
  g_return_val_if_fail(*pbuff, NULL);
  *psurf = cairo_image_surface_create_for_data(*pbuff, CAIRO_FORMAT_ARGB32, size[0], size[1], stride);
  int status = *psurf ? cairo_surface_status(*psurf) : CAIRO_STATUS_NO_MEMORY;
  if (status == CAIRO_STATUS_SUCCESS) return cairo_create(*psurf);
  WARN("image error(%d): %s", status, cairo_status_to_string(status));
  g_free(*pbuff); *pbuff = NULL;
  return NULL;
}

static GLuint plot_create_texture(ivec2 size, const void *pixels) {
  GLuint id = 0;
  glGenTextures(1, &id);
  if (id) {
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size[0], size[1], 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
  } else FAIL("glGenTextures()");
  return id;
}

static inline void vec4_set(vec4 vec, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
  vec[0] = a; vec[1] = b; vec[2] = c; vec[3] = d; }


// pub

gboolean plot_pango_init(void) {
  if (!plot_font) {
    plot_font = pango_font_description_new();
    if (plot_font) {
      pango_font_description_set_family(plot_font, PP_FONT_FAMILY);
      pango_font_description_set_absolute_size(plot_font, PLOT_FONT_SIZE * PANGO_SCALE);
    } else WARN_("Cannot allocate pango font");
  }
  return (plot_font != NULL);
}

void plot_pango_free(void) {
  if (plot_font) { pango_font_description_free(plot_font); plot_font = NULL; }
}

GLuint plot_pango_text(char ch, ivec2 size) {
  if (!plot_font) return 0;
  GLuint tid = 0;
  cairo_t *cairo = plot_create_cairo();
  if (cairo) {
    PangoLayout *layout = pango_cairo_create_layout(cairo);
    if (layout) {
      char text[2] = { ch, 0 };
      pango_layout_set_text(layout, text, -1);
      pango_layout_set_font_description(layout, plot_font);
      pango_layout_get_size(layout, &size[0], &size[1]);
      size[0] /= PANGO_SCALE; size[1] /= PANGO_SCALE;
      cairo_surface_t *surface = NULL; unsigned char *data = NULL;
      cairo_t *render = plot_create_cairo_from_buff(size, CAIRO_FORMAT_ARGB32, &surface, &data);
      if (render) {
        cairo_set_source_rgba(render, 1, 1, 1, 1);
        pango_cairo_show_layout(render, layout);
        tid = plot_create_texture(size, data);
        cairo_destroy(render);
      }
      if (surface) cairo_surface_destroy(surface);
      g_free(data);
      g_object_unref(layout);
    } else FAIL("pango_cairo_create_layout()");
    cairo_destroy(cairo);
  }
  return tid;
}

t_plot_vo plot_pango_vo_init(GLuint loc) {
  t_plot_vo vo = { .vao = 0, .vbo = { .id = 0, .count = 6, .stride = sizeof(vec4) }};
  glGenVertexArrays(1, &vo.vao);
  if (vo.vao) {
    glBindVertexArray(vo.vao);
    glGenBuffers(1, &vo.vbo.id);
    if (vo.vbo.id) {
      glBindBuffer(GL_ARRAY_BUFFER, vo.vbo.id);
      glBufferData(GL_ARRAY_BUFFER, vo.vbo.stride * vo.vbo.count, NULL, GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(loc);
      glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, vo.vbo.stride, 0);
    } else FAIL("glGenBuffers");
  } else FAIL("glGenVertexArrays()");
  return vo;
}

void plot_pango_drawtex(GLuint tid, GLuint vbo, GLfloat x0, GLfloat y0, GLfloat w, GLfloat h) {
  if (!tid || !vbo) return;
  const int points = 6;
  vec4 vertex[points]; memset(vertex, 0, sizeof(vertex));
  GLfloat x1 = x0 + w, y1 = y0 + h;
  vec4_set(vertex[0], x0, y1, 0, 0);
  vec4_set(vertex[1], x0, y0, 0, 1);
  vec4_set(vertex[2], x1, y0, 1, 1);
  vec4_set(vertex[3], x0, y1, 0, 0);
  vec4_set(vertex[4], x1, y0, 1, 1);
  vec4_set(vertex[5], x1, y1, 1, 0);
  glBindTexture(GL_TEXTURE_2D, tid);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDrawArrays(GL_TRIANGLES, 0, points);
}

