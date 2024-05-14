
#include <string.h>
#include <cglm/cglm.h>

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

static void grid_params(const vec4 off, int xn, int yn, GLfloat *x0, GLfloat *y0, GLfloat *dx, GLfloat *dy) {
  *x0 = -1 + off[0]; *dx = (2 - (off[0] + off[1])) / xn;
  *y0 = -1 + off[2]; *dy = (2 - (off[2] + off[3])) / yn;
}

#define PLOT_VAL_NAN(val) ((val) == PLOT_NAN)
#define PLOT_RTT_VAL { t_rseq *data = item->data; item = item->next; \
  if (IS_RTT_DATA(data)) val = rttscaled(data->rtt); }
#define PLOT_AVG_VAL(prev, curr) (PLOT_VAL_NAN(prev) ? (curr) : (PLOT_VAL_NAN(curr) ? (prev) : ((curr + prev) / 2)))

static void plot_surf_vertex(vec3 *vertex, int xn, int yn, const vec4 off) {
  GLfloat x0, y0, dx, dy; grid_params(off, xn, yn, &x0, &y0, &dx, &dy);
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
  GLfloat d = surf_vertex[n][2]; return ((-1 <= d) && (d <= 1));
}

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

#define FILL_NDX_LINE(ndx, offn) { int n1 = (ndx); int n2 = n1 + (offn); \
  gboolean fill = idc->desc.surf ? (surf_is_vert(n1) && surf_is_vert(n2)) : true; \
  if (fill) { *p++ = n1; *p++ = n2; cnt++; }}

static void plot_fill_ndx_grid(GLushort *p, t_plot_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->desc.x, yn = idc->desc.y;
  int xn1 = xn + 1, yn1 = yn + 1, cnt = 0;
  int xmin = 0, xlim = xn1, ymin = 0, ylim = yn1, ystride = PLOT_TIME_RANGE / 10;
  if (idc->desc.surf) {
    xmin = opts.min * 2 + 1; xlim = tgtat * 2;
    if (xlim > xn1) xlim = xn1;
    ymin++; ylim--;
  }
  { // grid
    for (int x = xmin; x < xlim; x++) {
      int xx = x * yn1;
      for (int y = 0; y < yn; y++)
        FILL_NDX_LINE(xx + y, 1);
    }
    for (int y = ymin; y < ylim; y++) if (!idc->desc.surf || ((y % ystride) == 0))
      for (int x = 0; x < xn; x++)
        FILL_NDX_LINE(x * yn1 + y, yn1);
  }
  if (idc->desc.xtick || idc->desc.ytick) { // ticks
    int n = xn1 * yn1;
    if (idc->desc.xtick) { cnt += yn1; for (int y = 0; y < yn1; y++, n++) { *p++ = y; *p++ = n; }}
    if (idc->desc.ytick) { cnt += xn1; for (int x = 1; x <= xn1; x++, n++) { *p++ = n; *p++ = yn1 * x - 1; }}
  }
  if (!cnt) { *p++ = 0; *p++ = 0; cnt = 1; } // at least (0,0) line
  idc->count = cnt * 2;
}


// pub

static void plot_vert_array_init(vec3 *data, t_plot_idc *idc) {
  if (!data || !idc) return;
  int xn = idc->desc.x, yn = idc->desc.y;
  int xn1 = xn + 1, yn1 = yn + 1;
  GLfloat x0, y0, dx, dy; grid_params(idc->desc.off, xn, yn, &x0, &y0, &dx, &dy);
  GLfloat filler = idc->desc.surf ? PLOT_NAN : -1;
  int n = 0;
  for (int i = 0; i < xn1; i++) for (int j = 0; j < yn1; j++)
    vec3_set(data[n++], y0 + j * dy, x0 + i * dx, filler);
  if (idc->desc.surf) plot_fill_vbo_surf(data, idc);
  if (idc->desc.xtick) for (int j = 0; j < yn1; j++) vec3_set(data[n++], y0 + j * dy, -1, -1);
  if (idc->desc.ytick) for (int i = 0; i < xn1; i++) vec3_set(data[n++], 1, x0 + i * dx, -1);
}

// vertex x,y,data,alpha
t_plot_idc plot_aux_vert_init(t_plot_vert_desc *desc, gboolean dyn,
    void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr) {
  t_plot_idc idc = { .id = 0, .typ = GL_ARRAY_BUFFER };
  if (desc) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      idc.stride = sizeof(vec3);
      int xn1 = idc.desc.x + 1, yn1 = idc.desc.y + 1;
      idc.count = xn1 * yn1;
      if (idc.desc.xtick) idc.count += yn1;
      if (idc.desc.ytick) idc.count += xn1;
      vec3 vertex[idc.count]; idc.hold = sizeof(vertex);
      memset(vertex, 0, idc.hold);
      plot_vert_array_init(vertex, &idc);
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

void plot_aux_update(t_plot_idc *vbo, t_plot_idc *surf, t_plot_idc *grid) {
  plot_aux_buff_update(vbo, SURF_VBO);
  plot_aux_buff_update(surf, SURF_DBO);
  plot_aux_buff_update(grid, GRID_DBO);
}

void plot_aux_reset(t_plot_idc *ibo, t_plot_idc *surf, t_plot_idc *grid) {
  plot_aux_buff_update(ibo, VERT_IBO);
  plot_aux_buff_update(surf, SURF_DBO);
  plot_aux_buff_update(grid, GRID_DBO);
}

// gridlines
t_plot_idc plot_aux_grid_init(t_plot_vert_desc *desc, gboolean dyn) {
  t_plot_idc idc = { .id = 0, .typ = GL_ELEMENT_ARRAY_BUFFER };
  if (desc) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      idc.stride = sizeof(GLushort);
      int xn = desc->x, yn = desc->y;
      int xn1 = xn + 1, yn1 = yn + 1;
      int hold = (xn * yn1 + xn1 * yn) * 2;
      if (desc->xtick) hold += yn1 * 2;
      if (desc->ytick) hold += xn1 * 2;
      GLushort data[hold]; idc.hold = sizeof(data);
      memset(data, 0, idc.hold);
      plot_fill_ndx_grid(data, &idc);
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

