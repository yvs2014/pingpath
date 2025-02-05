
#include <stdlib.h>
#include <cglm/cglm.h>

#include "common.h"

#include "plot_aux.h"
#include "stat.h"
#include "series.h"

int gl_ver;
gboolean gl_named;

static GLenum last_known_gl_error = GL_NO_ERROR;

static void plot_aux_chkerr(void) {
  GLenum err = glGetError();
  if ((err == GL_NO_ERROR) || (err == last_known_gl_error)) return;
  last_known_gl_error = err; g_warning("GL %s: 0x%04x", ERROR_HDR, err);
}

static const float PLOT_NAN = -1.01F;
static vec3 *surf_vertex;
static t_plot_idc surf_vertex_idc;

static inline void vec3_set(vec3 vec, float a, float b, float c) { vec[0] = a; vec[1] = b; vec[2] = c; }
static inline void vec3_xy(vec3 vec, float a, float b) { vec[0] = a; vec[1] = b; }

static inline float rttscaled(float rtt) { return (rtt < 0) ? PLOT_NAN : rtt / series_datamax * 2 - 1; }

#define SETIFDEF(ptr, val) { if (ptr) *(ptr) = val; }
static void grid_params(int xn, int yn, float *x0, float *x1, float *y0, float *y1, float *dx, float *dy) {
  vec4 r = { -1, 1, -1, 1 }; vec2 d = { 2. / xn, 2. / yn };
  SETIFDEF(x0, r[0]); SETIFDEF(x1, r[1]);
  SETIFDEF(y0, r[2]); SETIFDEF(y1, r[3]);
  SETIFDEF(dx, d[0]); SETIFDEF(dy, d[1]);
}

#define PLOT_VAL_NAN(val) ((val) == PLOT_NAN)
#define PLOT_RTT_VAL { t_rseq *data = item->data; item = item->next; \
  if (IS_RTT_DATA(data)) val = rttscaled(data->rtt); }
#define PLOT_AVG_VAL(prev, curr) (PLOT_VAL_NAN(prev) ? (curr) : (PLOT_VAL_NAN(curr) ? (prev) : (((curr) + (prev)) / 2)))

static void plot_surf_vertex(vec3 *vertex, int xn, int yn) {
  float x0 = NAN, y0 = NAN, x1 = NAN, y1 = NAN, dx = NAN, dy = NAN;
  grid_params(xn, yn, &x0, &x1, &y0, &y1, &dx, &dy);
  float dxs = dx / 2 * (float)xn;
  { int range = tgtat - opts.range.min; dxs /= (range > 0) ? range : tgtat; }
  int xn1 = xn + 1, yn1 = yn + 1; // note: xn,yn are mandatory even numbers
  int imin = opts.range.min * 2, imax = SERIES_LIM * 2;
  float x = x0 - imin * dxs;
  for (int i = 0, n = 0; i < xn1; i++) {
    GSList *item = SERIES(i / 2);
    gboolean xodd = i % 2, skip = (i < imin) || (i > imax);
    float prev = PLOT_NAN, y = y1;
    int ndx = n + yn;
    for (int j = yn; j >= 0; j--, n++, ndx--) {
      float val = PLOT_NAN;
      if (!skip) {
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
      y -= dy;
    }
    if (xodd) vertex[ndx + 1][2] = prev;
    x += dxs;
  }
  { int max = xn1 * yn1; // approximation between 'x-odd' lines
    for (int i = 0, n = 0; i < xn1; i++) {
      gboolean xeven = !(i % 2), skip = (i < imin) || (i > imax);
      for (int j = 0, ndx = n + yn; j < yn1; j++, n++, ndx--) if (!skip && xeven) {
        int nup = ndx + yn1, ndown = ndx - yn1;
        float up = (nup < max) ? vertex[nup][2] : PLOT_NAN;
        float down = (ndown < 0) ? PLOT_NAN : vertex[ndown][2];
        vertex[ndx][2] = PLOT_AVG_VAL(up, down);
      }
    }
  }
}

static void keep_surf_coords(vec3 *vertex, int size, t_plot_idc *idc) {
  vec3 *dup = g_memdup2(vertex, size);
  if (dup) {
    g_free(surf_vertex);
    surf_vertex = dup;
    if (idc) surf_vertex_idc = *idc;
  }
}

static inline void plot_fill_vbo_surf(vec3 *data, t_plot_idc *idc) {
  if (!idc || !idc->id) return;
  plot_surf_vertex(data, idc->x, idc->y);
  keep_surf_coords(data, idc->hold, idc);
}

static inline gboolean surf_is_vert(int n) {
  if (n >= surf_vertex_idc.count) return false;
  float d = surf_vertex[n][2]; return ((-1 <= d) && (d <= 1)); }

#define FILL_NDX_TRIA(ndx1, ndx2, ndx3) { int n1 = (ndx1), n2 = (ndx2), n3 = (ndx3); \
  if (surf_is_vert(n1) && surf_is_vert(n2) && surf_is_vert(n3)) { \
    *p++ = n1; *p++ = n2; *p++ = n3; cnt++; }}

static void plot_fill_ndx_surf(GLushort *p, t_plot_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->x, yn = idc->y, cnt = 0;
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

#define PLO_IS_FLAT(typ)   ((typ) != PLO_SURF)
#define PLO_IS_YAXIS(typ) (((typ) == PLO_AXIS_YL) || ((typ) == PLO_AXIS_YR))
#define PLO_IS_LAXIS(typ) (((typ) == PLO_AXIS_XL) || ((typ) == PLO_AXIS_YL))
#define PLO_IS_AXIS(typ)  (((typ) == PLO_AXIS_XL) || ((typ) == PLO_AXIS_XR) || PLO_IS_YAXIS(typ))

static void plot_fill_ndx_grid(GLushort *p, t_plot_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->x, yn = idc->y;
  int xn1 = xn + 1, yn1 = yn + 1, cnt = 0;
  gboolean flat = PLO_IS_FLAT(idc->plo);
  if (flat) { // flat grid
    int lim = (xn1 + yn1) * 2;
    for (int n = 0; n < lim; n += 2) { *p++ = n; *p++ = n + 1; cnt++; }
  } else {    // surface grid
   { int lim = tgtat * 2; if (lim > xn1) lim = xn1;
     for (int x = opts.range.min * 2 + 1; x < lim; x++) {
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
  int xn = idc->x, yn = idc->y;
  int xn1 = xn + 1, yn1 = yn + 1;
  float x0 = NAN, y0 = NAN, x1 = NAN, y1 = NAN, dx = NAN, dy = NAN;
  grid_params(xn, yn, &x0, &x1, &y0, &y1, &dx, &dy);
  gboolean flat = PLO_IS_FLAT(idc->plo);
  float filler = flat ? -1 : PLOT_NAN;
  int n = 0;
  if (flat) {
    { float xi = x0; for (int i = 0; i < xn1; i++) {
      vec3_set(p[n++], y0, xi, filler);
      vec3_set(p[n++], y1, xi, filler);
      xi += dx;
    }}
    { float yi = y0; for (int j = 0; j < yn1; j++) {
      vec3_set(p[n++], yi, x0, filler);
      vec3_set(p[n++], yi, x1, filler);
      yi += dy;
    }}
  } else {
    for (int i = 0; i < xn1; i++) for (int j = 0; j < yn1; j++)
      vec3_set(p[n++], y0 + j * dy, x0 + i * dx, filler);
    plot_fill_vbo_surf(p, idc);
  }
}

static void plot_vert_axis_init(vec3 *p, int lim, gboolean is_y, float width, gboolean beginning) {
  if (!p || (lim < 2)) return;
  float f = -1, df = 2. / (lim - 1);
  float w0 = 1, w1 = 1 + width;
  if (beginning) { w0 = - w0; w1 = - w1; }
  int i0 = !is_y, i1 = is_y, i2 = 2;
  for (int n = 0, k = lim; n < lim; n++, k++) {
    p[n][i0] = w0; p[n][i1] = f; p[n][i2] = -1;
    p[k][i0] = w1; p[k][i1] = f; p[k][i2] = -1;
    f += df;
  }
}


// pub

t_plot_idc plot_aux_vert_init_plane(int x, int y, int plo,
    void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr) {
  t_plot_idc idc = { .id = 0, .typ = GL_ARRAY_BUFFER, .x = x, .y = y, .plo = plo };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    idc.stride = sizeof(vec3);
    int xn1 = idc.x + 1, yn1 = idc.y + 1;
    gboolean surf = !PLO_IS_FLAT(idc.plo);
    idc.count = xn1 * yn1; if (surf) idc.count *= 2;
    vec3 vertex[idc.count];
    memset(vertex, 0, sizeof(vertex)); // BUFFNOLINT
    idc.hold = sizeof(vertex);
    plot_vert_array_init(vertex, &idc);
    glBindBuffer(idc.typ, idc.id);
    glBufferData(idc.typ, idc.hold, vertex, surf ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    if (attr_fn) attr_fn(vao, attr);
    glBindBuffer(idc.typ, 0);
  } else FAIL("glGenBuffers()");
  return idc;
}

t_plot_idc plot_aux_vert_init_axis(int x, int y, int plo, float width,
    void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr) {
  t_plot_idc idc = { .id = 0, .typ = GL_ARRAY_BUFFER, .x = x, .y = y, .plo = plo };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    idc.stride = sizeof(vec3);
    int xn1 = idc.x + 1, yn1 = idc.y + 1;
    gboolean is_y = PLO_IS_YAXIS(idc.plo);
    gboolean is_l = PLO_IS_LAXIS(idc.plo);
    int n = is_y ? xn1 : yn1;
    idc.count = n * 2; // edge-n-tick
    vec3 vertex[idc.count];
    memset(vertex, 0, sizeof(vertex)); // BUFFNOLINT
    idc.hold = sizeof(vertex);
    plot_vert_axis_init(vertex, n, is_y, width, is_l);
    glBindBuffer(idc.typ, idc.id);
    glBufferData(idc.typ, idc.hold, vertex, GL_STATIC_DRAW);
    if (attr_fn) attr_fn(vao, attr);
    glBindBuffer(idc.typ, 0);
  } else FAIL("glGenBuffers()");
  return idc;
}

static void plot_aux_buff_update(t_plot_idc *idc, int datatyp) {
  if (!idc || !idc->id) return;
  if (!gl_named) glBindBuffer(idc->typ, idc->id);
  void *data = gl_named ? glMapNamedBuffer(idc->id, GL_READ_WRITE)
    : glMapBufferRange(idc->typ, 0, idc->hold, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  if (data) {
    switch (datatyp) {
      case SURF_DBO: plot_fill_ndx_surf((GLushort*)data, idc); break;
      case GRID_DBO: plot_fill_ndx_grid((GLushort*)data, idc); break;
      case SURF_VBO: plot_fill_vbo_surf((vec3*)data, idc); break;
      case VERT_IBO: plot_vert_array_init((vec3*)data, idc); break;
      default: break;
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
  memset(&surf_vertex_idc, 0, sizeof(surf_vertex_idc)); // BUFFNOLINT
}

// gridlines
t_plot_idc plot_aux_grid_init(int x, int y, int plo) {
  t_plot_idc idc = { .id = 0, .typ = GL_ELEMENT_ARRAY_BUFFER, .x = x, .y = y, .plo = plo };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    idc.stride = sizeof(GLushort);
    int xn1 = idc.x + 1, yn1 = idc.y + 1, cnt = 0;
    gboolean is_surf = !PLO_IS_FLAT(idc.plo), is_axis = PLO_IS_AXIS(idc.plo);
    if (is_axis) { cnt = PLO_IS_YAXIS(idc.plo) ? xn1 : yn1; cnt++; }
    else { cnt = xn1 * yn1; if (is_surf) { cnt *= 2; cnt -= xn1 + yn1; }}
    if (!cnt) cnt = 1;
    idc.count = cnt * 2;
    GLushort data[idc.count];
    memset(data, 0, sizeof(data)); // BUFFNOLINT
    idc.hold = sizeof(data);
    is_axis ? plot_fill_ndx_axis(data, cnt - 1) : plot_fill_ndx_grid(data, &idc);
    glBindBuffer(idc.typ, idc.id);
    glBufferData(idc.typ, idc.hold, data, is_surf ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glBindBuffer(idc.typ, 0);
  } else FAIL("glGenBuffers()");
  return idc;
}

// triangles
t_plot_idc plot_aux_surf_init(int x, int y, int plo) {
  t_plot_idc idc = { .id = 0, .typ = GL_ELEMENT_ARRAY_BUFFER, .x = x, .y = y, .plo = plo };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    idc.stride = sizeof(GLushort);
    GLushort data[idc.x * idc.y * 6];
    memset(data, 0, sizeof(data)); // BUFFNOLINT
    idc.hold = sizeof(data);
    plot_fill_ndx_surf(data, &idc);
    glBindBuffer(idc.typ, idc.id);
    glBufferData(idc.typ, idc.hold, data, GL_DYNAMIC_DRAW);
    glBindBuffer(idc.typ, 0);
  } else FAIL("glGenBuffers()");
  return idc;
}

