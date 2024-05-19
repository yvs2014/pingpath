
#include <string.h>

#include "plot_aux.h"
#include "stat.h"
#include "pinger.h"
#include "series.h"

int gl_ver;
gboolean gl_named;

static GLenum last_known_gl_error = GL_NO_ERROR;

static void plot_aux_chkerr(void) {
  GLenum err = glGetError();
  if ((err == GL_NO_ERROR) || (err == last_known_gl_error)) return;
  last_known_gl_error = err; g_warning("GL error: 0x%04x", err);
}

static const GLfloat PLOT_NAN = -1.01;
static vec3 *surf_vertex;
static t_plot_idc surf_vertex_idc;

static inline void vec3_set(vec3 vec, GLfloat a, GLfloat b, GLfloat c) { vec[0] = a; vec[1] = b; vec[2] = c; }
static inline void vec3_xy(vec3 vec, GLfloat a, GLfloat b) { vec[0] = a; vec[1] = b; }

static inline GLfloat rttscaled(GLfloat rtt) { return (rtt < 0) ? PLOT_NAN : rtt / series_datamax * 2 - 1; }

#define SETIFDEF(ptr, val) { if (ptr) *ptr = val; }
static void grid_params(const vec4 off, int xn, int yn, GLfloat *x0, GLfloat *x1, GLfloat *y0, GLfloat *y1,
    GLfloat *dx, GLfloat *dy) {
  vec4 r; vec2 d;
  r[0] = -1 + off[0]; r[1] = 1 - off[1]; d[0] = (r[1] - r[0]) / xn;
  r[2] = -1 + off[2]; r[3] = 1 - off[3]; d[1] = (r[3] - r[2]) / yn;
  SETIFDEF(x0, r[0]); SETIFDEF(x1, r[1]);
  SETIFDEF(y0, r[2]); SETIFDEF(y1, r[3]);
  SETIFDEF(dx, d[0]); SETIFDEF(dy, d[1]);
}

#define PLOT_VAL_NAN(val) ((val) == PLOT_NAN)
#define PLOT_RTT_VAL { t_rseq *data = item->data; item = item->next; \
  if (IS_RTT_DATA(data)) val = rttscaled(data->rtt); }
#define PLOT_AVG_VAL(prev, curr) (PLOT_VAL_NAN(prev) ? (curr) : (PLOT_VAL_NAN(curr) ? (prev) : ((curr + prev) / 2)))

static void plot_surf_vertex(vec3 *vertex, int xn, int yn, const vec4 off) {
  GLfloat x0, y0, dx, dy; grid_params(off, xn, yn, &x0, NULL, &y0, NULL, &dx, &dy);
  GLfloat dxs = dx / 2 * (GLfloat)xn;
  { int range = tgtat - opts.min; dxs /= (range > 0) ? range : tgtat; }
  int xn1 = xn + 1, yn1 = yn + 1; // note: xn,yn are mandatory even numbers
  int imin = opts.min * 2, imax = SERIES_LIM * 2;
  for (int i = 0, n = 0; i < xn1; i++) {
    GSList *item = SERIES(i / 2);
    gboolean xodd = i % 2, skip = (i < imin) || (i > imax);
    GLfloat x = x0 + (i - imin) * dxs, prev = PLOT_NAN;
    int ndx = n + yn;
    for (int j = yn; j >= 0; j--, n++, ndx--) {
      GLfloat val = PLOT_NAN;
      if (!skip) {
        GLfloat y = y0 + j * dy;
        vec3_xy(vertex[ndx], y, x);
        if (xodd && item) {
          gboolean yodd = j % 2;
          if (yodd) {
            PLOT_RTT_VAL;
            vertex[ndx + 1][2] = PLOT_AVG_VAL(prev, val); // approximation between 'y-odd' elements
            prev = val;
          }
          vertex[ndx][2] = val;
        }
      }
    }
    if (xodd) vertex[ndx + 1][2] = prev;
  }
  { int max = xn1 * yn1; // approximation between 'x-odd' lines
    for (int i = 0, n = 0; i < xn1; i++) {
      gboolean xeven = !(i % 2), skip = (i < imin) || (i > imax);
      for (int j = 0, ndx = n + yn; j < yn1; j++, n++, ndx--) if (!skip && xeven) {
        int nup = ndx + yn1, ndown = ndx - yn1;
        GLfloat up = (nup < max) ? vertex[nup][2] : PLOT_NAN;
        GLfloat down = (ndown < 0) ? PLOT_NAN : vertex[ndown][2];
        vertex[ndx][2] = PLOT_AVG_VAL(up, down);
      }
    }
  }
}

static void keep_surf_coords(vec3 *vertex, int sz, t_plot_idc *idc) {
  vec3 *dup = g_memdup2(vertex, sz);
  if (dup) {
    g_free(surf_vertex);
    surf_vertex = dup;
    if (idc) surf_vertex_idc = *idc;
  }
}

static inline void plot_fill_vbo_surf(vec3 *data, t_plot_idc *idc) {
  if (!idc || !idc->id) return;
  plot_surf_vertex(data, idc->desc.x, idc->desc.y, idc->desc.off);
  keep_surf_coords(data, idc->hold, idc);
}

static inline gboolean surf_is_vert(int n) {
  if (n >= surf_vertex_idc.count) return false;
  GLfloat d = surf_vertex[n][2]; return ((-1 <= d) && (d <= 1)); }

#define FILL_NDX_TRIA(ndx1, ndx2, ndx3) { int n1 = ndx1, n2 = ndx2, n3 = ndx3; \
  if (surf_is_vert(n1) && surf_is_vert(n2) && surf_is_vert(n3)) { \
    *p++ = n1; *p++ = n2; *p++ = n3; cnt++; }}

static void plot_fill_ndx_surf(GLushort *p, t_plot_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->desc.x, yn = idc->desc.y, cnt = 0;
  for (int i = 0, n = 0, yn1 = yn + 1; i < xn; i++, n += yn1)
    for (int j = 0, y = n, dir = i % 2; j < yn; j++, y++) {
      int x = y + yn1, clk = (j % 2) ? dir : !dir;
      FILL_NDX_TRIA(y, y + 1, clk ? x : x + 1);
      FILL_NDX_TRIA(x + 1, x, clk ? y + 1 : y);
    }
  if (!cnt) { *p++ = 0; *p++ = 0; *p++ = 0; cnt = 1; } // at least one (0,0,0)
  idc->count = cnt * 3;
}

#define FILL_SURF_LINE(ndx, offn) { int n1 = (ndx); int n2 = n1 + (offn); \
  if (surf_is_vert(n1) && surf_is_vert(n2)) { *p++ = n1; *p++ = n2; cnt++; }}

static void plot_fill_ndx_grid(GLushort *p, t_plot_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->desc.x, yn = idc->desc.y;
  int xn1 = xn + 1, yn1 = yn + 1, cnt = 0;
  gboolean plane = idc->desc.typ != VERT_SURF;
  if (plane) { // plane grid
    int lim = (xn1 + yn1) * 2;
    for (int n = 0; n < lim; n += 2) { *p++ = n; *p++ = n + 1; cnt++; }
  } else {     // surface grid
   { int lim = tgtat * 2; if (lim > xn1) lim = xn1;
     for (int x = opts.min * 2 + 1; x < lim; x++) {
       int xx = x * yn1; for (int y = 0; y < yn; y++) FILL_SURF_LINE(xx + y, 1);
     }
   }
   { int stride = PLOT_TIME_RANGE / 10;
    for (int y = 1; y < yn; y++) if ((y % stride) == 0)
      for (int x = 0; x < xn; x++) FILL_SURF_LINE(x * yn1 + y, yn1);
   }
  }
  if (!cnt) { *p++ = 0; *p++ = 0; cnt = 1; } // at least (0,0) line
  idc->count = cnt * 2;
}

static void plot_fill_ndx_axis(GLushort *p, int lim) {
  if (!p || (lim <= 0)) return;
  *p++ = 0; *p++ = lim - 1;
  for (int n = 0; n < lim; n++) { *p++ = n; *p++ = n + lim; }
}

static void plot_vert_array_init(vec3 *p, t_plot_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->desc.x, yn = idc->desc.y;
  int xn1 = xn + 1, yn1 = yn + 1;
  GLfloat x0, y0, dx, dy; grid_params(idc->desc.off, xn, yn, &x0, NULL, &y0, NULL, &dx, &dy);
  gboolean surf = idc->desc.typ == VERT_SURF;
  GLfloat filler = surf ? PLOT_NAN : -1;
  int n = 0;
  if (surf) {
    for (int i = 0; i < xn1; i++) for (int j = 0; j < yn1; j++)
      vec3_set(p[n++], y0 + j * dy, x0 + i * dx, filler);
    plot_fill_vbo_surf(p, idc);
  } else {
    for (int i = 0; i < xn1; i++) {
      GLfloat xi = x0 + i * dx, y1 = y0 + yn * dy;
      vec3_set(p[n++], y0, xi, filler);
      vec3_set(p[n++], y1, xi, filler);
    }
    for (int j = 0; j < yn1; j++) {
      GLfloat yi = y0 + j * dy, x1 = x0 + xn * dx;
      vec3_set(p[n++], yi, x0, filler);
      vec3_set(p[n++], yi, x1, filler);
    }
  }
}

static void plot_vert_axis_init(vec3 *p, int lim, GLfloat off0, GLfloat off1, int fn, GLfloat width, gboolean beginning) {
  if (!p || (lim < 2)) return;
  GLfloat f = -1 + off0, df = (2 - (off0 + off1)) / (lim - 1);
  GLfloat w0 = 1 - width, w1 = 1;
  if (beginning) { w0 = - w0; w1 = - w1; }
  int wn = !fn;
  for (int n = 0, k = lim; n < lim; n++, k++, f += df) {
    p[n][wn] = w0; p[n][fn] = f; p[n][2] = -1;
    p[k][wn] = w1; p[k][fn] = f; p[k][2] = -1;
  }
}

#define VERT_AXIS_INI_CALL(wndx, lndx, rndx, lim, vndx) { \
  GLfloat width = idc.desc.off[wndx], offl = idc.desc.off[lndx], offr = idc.desc.off[rndx]; \
  if (width > 0) plot_vert_axis_init(vertex, lim, offl, offr, vndx, width, !(wndx % 2)); }

// pub

t_plot_idc plot_aux_vert_init(t_plot_vert_desc *desc, void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr) {
  t_plot_idc idc = { .id = 0, .typ = GL_ARRAY_BUFFER };
  if (desc && (desc->typ >= 0) && (desc->typ < VERT_TYPE_MAX)) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      idc.stride = sizeof(vec3);
      int pts = 0, xn1 = idc.desc.x + 1, yn1 = idc.desc.y + 1;
      gboolean dyn = false;
      switch (desc->typ) {
        case VERT_SURF: pts = xn1 * yn1 * 2; dyn = true; break;
        case VERT_GRID: pts = xn1 * yn1; break;
        case VERT_XL_AXIS:
        case VERT_XR_AXIS: pts = yn1 * 2; break;
        case VERT_YL_AXIS:
        case VERT_YR_AXIS: pts = xn1 * 2; break;
      }
      idc.count = pts;
      vec3 vertex[idc.count]; idc.hold = sizeof(vertex);
      memset(vertex, 0, idc.hold);
      switch (desc->typ) {
        case VERT_SURF:
        case VERT_GRID:  plot_vert_array_init(vertex, &idc); break;
        case VERT_XL_AXIS: VERT_AXIS_INI_CALL(0, 2, 3, yn1, 0); break;
        case VERT_XR_AXIS: VERT_AXIS_INI_CALL(1, 2, 3, yn1, 0); break;
        case VERT_YL_AXIS: VERT_AXIS_INI_CALL(2, 0, 1, xn1, 1); break;
        case VERT_YR_AXIS: VERT_AXIS_INI_CALL(3, 0, 1, xn1, 1); break;
      }
      glBindBuffer(idc.typ, idc.id);
      glBufferData(idc.typ, idc.hold, vertex, dyn ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
      if (attr_fn) attr_fn(vao, attr);
      glBindBuffer(idc.typ, 0);
    } else FAIL("glGenBuffers()");
  }
  return idc;
}

void plot_aux_buff_update(t_plot_idc *idc, int typ) {
  if (!idc || !idc->id) return;
  if (!gl_named) glBindBuffer(idc->typ, idc->id);
  void *data = gl_named ? glMapNamedBuffer(idc->id, GL_READ_WRITE)
    : glMapBufferRange(idc->typ, 0, idc->hold, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  if (data) {
    switch (typ) {
      case SURF_DBO: plot_fill_ndx_surf((GLushort*)data, idc); break;
      case GRID_DBO: plot_fill_ndx_grid((GLushort*)data, idc); break;
      case SURF_VBO: plot_fill_vbo_surf((vec3*)data, idc); break;
      case VERT_IBO: plot_vert_array_init((vec3*)data, idc); break;
    }
    GLboolean rc = gl_named ? glUnmapNamedBuffer(idc->id) : glUnmapBuffer(idc->typ); if (!rc) plot_aux_chkerr();
  } else plot_aux_chkerr();
  if (!gl_named) glBindBuffer(idc->typ, 0);
}

void plot_aux_update(t_plot_vo *vo) {
  if (!vo) return;
  plot_aux_buff_update(&vo->vbo,      SURF_VBO);
  plot_aux_buff_update(&vo->dbo.ext,  SURF_DBO);
  plot_aux_buff_update(&vo->dbo.main, GRID_DBO);
}

void plot_aux_reset(t_plot_vo *vo) {
  if (!vo) return;
  plot_aux_buff_update(&vo->vbo,      VERT_IBO);
  plot_aux_buff_update(&vo->dbo.ext,  SURF_DBO);
  plot_aux_buff_update(&vo->dbo.main, GRID_DBO);
}

void plot_aux_free(void) {
  if (surf_vertex) { g_free(surf_vertex); surf_vertex = NULL; }
  memset(&surf_vertex_idc, 0, sizeof(surf_vertex_idc));
}

// gridlines
t_plot_idc plot_aux_grid_init(t_plot_vert_desc *desc) {
  t_plot_idc idc = { .id = 0, .typ = GL_ELEMENT_ARRAY_BUFFER };
  if (desc && (desc->typ >= 0) && (desc->typ < VERT_TYPE_MAX)) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      idc.stride = sizeof(GLushort);
      int cnt = 0, xn1 = desc->x + 1, yn1 = desc->y + 1;
      gboolean dyn = false;
      switch (desc->typ) {
        case VERT_SURF: cnt = 2 * xn1 * yn1 - (xn1 + yn1); dyn = true; break;
        case VERT_GRID: cnt = xn1 * yn1; break;
        case VERT_XL_AXIS:
        case VERT_XR_AXIS: cnt = yn1 + 1; break;
        case VERT_YL_AXIS:
        case VERT_YR_AXIS: cnt = xn1 + 1; break;
      }
      idc.count = cnt * 2;
      GLushort data[idc.count]; idc.hold = sizeof(data);
      memset(data, 0, idc.hold);
      switch (desc->typ) {
        case VERT_SURF:
        case VERT_GRID: plot_fill_ndx_grid(data, &idc); break;
        case VERT_XL_AXIS:
        case VERT_XR_AXIS:
        case VERT_YL_AXIS:
        case VERT_YR_AXIS: plot_fill_ndx_axis(data, cnt - 1); break;
      }
      glBindBuffer(idc.typ, idc.id);
      glBufferData(idc.typ, idc.hold, data, dyn ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
      glBindBuffer(idc.typ, 0);
    } else FAIL("glGenBuffers()");
  }
  return idc;
}

// triangles
t_plot_idc plot_aux_surf_init(t_plot_vert_desc *desc) {
  t_plot_idc idc = { .id = 0, .typ = GL_ELEMENT_ARRAY_BUFFER };
  if (desc) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      idc.stride = sizeof(GLushort);
      GLushort data[desc->x * desc->y * 6]; idc.hold = sizeof(data);
      memset(data, 0, idc.hold);
      plot_fill_ndx_surf(data, &idc);
      glBindBuffer(idc.typ, idc.id);
      glBufferData(idc.typ, idc.hold, data, GL_DYNAMIC_DRAW);
      glBindBuffer(idc.typ, 0);
    } else FAIL("glGenBuffers()");
  }
  return idc;
}

