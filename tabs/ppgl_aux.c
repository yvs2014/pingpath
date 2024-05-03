
#include <string.h>
#include <cglm/cglm.h>

#include "ppgl_aux.h"
#include "stat.h"
#include "pinger.h"
#include "series.h"

static const GLfloat PPGL_NAN = -1.01;
static vec3 *surf_vertex;
static t_ppgl_idc surf_vertex_idc;

static inline void vec3_set(vec3 vec, GLfloat a, GLfloat b, GLfloat c) { vec[0] = a; vec[1] = b; vec[2] = c; }
static inline void vec3_xy(vec3 vec, GLfloat a, GLfloat b) { vec[0] = a; vec[1] = b; }

static inline GLfloat rttscaled(GLfloat rtt) { return (rtt < 0) ? PPGL_NAN : rtt / series_datamax * 2 - 1; }

static void grid_params(const vec4 off, int xn, int yn, GLfloat *x0, GLfloat *y0, GLfloat *dx, GLfloat *dy) {
  *x0 = -1 + off[0]; *dx = (2 - (off[0] + off[1])) / xn;
  *y0 = -1 + off[2]; *dy = (2 - (off[2] + off[3])) / yn;
}

#define PPGL_VAL_NAN(val) ((val) == PPGL_NAN)
#define PPGL_RTT_VAL { t_rseq *data = item->data; item = item->next; \
  if (IS_RTT_DATA(data)) val = rttscaled(data->rtt); }
#define PPGL_AVG_VAL(prev, curr) (PPGL_VAL_NAN(prev) ? (curr) : (PPGL_VAL_NAN(curr) ? (prev) : ((curr + prev) / 2)))

static void ppgl_surf_vertex(vec3 *vertex, int xn, int yn, const vec4 off) {
  GLfloat x0, y0, dx, dy; grid_params(off, xn, yn, &x0, &y0, &dx, &dy);
  GLfloat dxs = dx / 2 * (GLfloat)xn / tgtat;
  int xn1 = xn + 1, yn1 = yn + 1; // xn,yn are mandatory even numbers
  int imin = opts.min * 2, imax = SERIES_LIM * 2;
  for (int i = 0, n = 0; i < xn1; i++) {
    GSList *item = SERIES(i / 2);
    gboolean xodd = i % 2, skip = (i < imin) || (i > imax);
    GLfloat x = x0 + i * dxs, prev = PPGL_NAN;
    int ndx = n + yn;
    for (int j = yn; j >= 0; j--, n++, ndx--) {
      GLfloat val = PPGL_NAN;
      if (!skip) {
        GLfloat y = y0 + j * dy;
        vec3_xy(vertex[ndx], y, x);
        if (xodd && item) {
          gboolean yodd = j % 2;
          if (yodd) {
            PPGL_RTT_VAL;
            vertex[ndx + 1][2] = PPGL_AVG_VAL(prev, val); // approximation between 'y-odd' elements
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
        GLfloat up = (nup < max) ? vertex[nup][2] : PPGL_NAN;
        GLfloat down = (ndown < 0) ? PPGL_NAN : vertex[ndown][2];
        vertex[ndx][2] = PPGL_AVG_VAL(up, down);
      }
    }
  }
}

static inline void ppgl_fill_surf(vec3 *vertex, t_ppgl_idc *idc) {
  if (idc && idc->id) ppgl_surf_vertex(vertex, idc->desc.x, idc->desc.y, idc->desc.off);
}

static void keep_surf_coords(vec3 *vertex, int sz, t_ppgl_idc *idc) {
  vec3 *dup = g_memdup2(vertex, sz);
  if (dup) {
    g_free(surf_vertex);
    surf_vertex = dup;
    if (idc) surf_vertex_idc = *idc;
  }
}

static inline gboolean surf_is_vert(int n) {
  if (n >= surf_vertex_idc.count) return false;
  GLfloat d = surf_vertex[n][2]; return ((-1 <= d) && (d <= 1));
}

#define FILL_NDX_TRIA(ndx1, ndx2, ndx3) { int n1 = ndx1, n2 = ndx2, n3 = ndx3; \
  if (surf_is_vert(n1) && surf_is_vert(n2) && surf_is_vert(n3)) { \
    *p++ = n1; *p++ = n2; *p++ = n3; cnt++; }}

static void ppgl_fill_ndx_surf(GLushort *p, t_ppgl_idc *idc) {
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

static void ppgl_fill_ndx_grid(GLushort *p, t_ppgl_idc *idc) {
  if (!p || !idc) return;
  int xn = idc->desc.x, yn = idc->desc.y;
  int xn1 = xn + 1, yn1 = yn + 1, cnt = 0;
  int xmin = 0, xlim = xn1, ymin = 0, ylim = yn1, ystride = PPGL_TIME_RANGE / 10;
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

// vertex x,y,data,alpha
t_ppgl_idc ppgl_aux_vert_init(t_ppgl_vert_desc *desc, gboolean dyn) {
  t_ppgl_idc idc = { .id = 0 };
  if (desc) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      GLfloat filler = desc->surf ? PPGL_NAN : -1;
      int xn = desc->x, yn = desc->y;
      int xn1 = xn + 1, yn1 = yn + 1;
      idc.count = xn1 * yn1;
      idc.stride = sizeof(vec3);
      if (desc->xtick) idc.count += yn1;
      if (desc->ytick) idc.count += xn1;
      vec3 vertex[idc.count]; memset(vertex, 0, sizeof(vertex));
      GLfloat x0, y0, dx, dy; grid_params(desc->off, xn, yn, &x0, &y0, &dx, &dy);
      int n = 0;
      for (int i = 0; i < xn1; i++) for (int j = 0; j < yn1; j++)
        vec3_set(vertex[n++], y0 + j * dy, x0 + i * dx, filler);
      if (desc->surf) { // refill surf related indexes
        ppgl_fill_surf(vertex, &idc);
        keep_surf_coords(vertex, sizeof(vertex), &idc);
      }
      if (desc->xtick) for (int j = 0; j < yn1; j++) vec3_set(vertex[n++], y0 + j * dy, -1, -1);
      if (desc->ytick) for (int i = 0; i < xn1; i++) vec3_set(vertex[n++], 1, x0 + i * dx, -1);
      glBindBuffer(GL_ARRAY_BUFFER, idc.id);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, dyn ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    } else FAIL("glGenBuffers()");
  }
  return idc;
}

void ppgl_aux_vbo_surf_update(t_ppgl_idc *vbo) {
  if (!vbo || !vbo->id) return;
  GLfloat *data = glMapNamedBuffer(vbo->id, GL_READ_WRITE);
  if (data) {
    ppgl_fill_surf((vec3*)data, vbo);
    keep_surf_coords((vec3*)data, vbo->count * sizeof(vec3), vbo);
  }
  glUnmapNamedBuffer(vbo->id);
}

// gridlines
t_ppgl_idc ppgl_aux_grid_init(t_ppgl_vert_desc *desc, gboolean dyn) {
  t_ppgl_idc idc = { .id = 0 };
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
      GLushort data[hold]; memset(data, 0, sizeof(data));
      ppgl_fill_ndx_grid(data, &idc);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idc.id);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(data), data, dyn ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    } else FAIL("glGenBuffers()");
  }
  return idc;
}

void ppgl_aux_dbo_grid_update(t_ppgl_idc *dbo) {
  if (!dbo || !dbo->id) return;
  GLushort *data = glMapNamedBuffer(dbo->id, GL_READ_WRITE);
  if (data) ppgl_fill_ndx_grid(data, dbo);
  glUnmapNamedBuffer(dbo->id);
}

// triangles
t_ppgl_idc ppgl_aux_surf_init(t_ppgl_vert_desc *desc) {
  t_ppgl_idc idc = { .id = 0 };
  if (desc) {
    memcpy(&idc.desc, desc, sizeof(idc.desc));
    glGenBuffers(1, &idc.id);
    if (idc.id) {
      idc.stride = sizeof(GLushort);
      GLushort data[desc->x * desc->y * 6]; memset(data, 0, sizeof(data));
      ppgl_fill_ndx_surf(data, &idc);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idc.id);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);
    } else FAIL("glGenBuffers()");
  }
  return idc;
}

void ppgl_aux_dbo_surf_update(t_ppgl_idc *dbo) {
  if (!dbo || !dbo->id) return;
  GLushort *data = glMapNamedBuffer(dbo->id, GL_READ_WRITE);
  if (data) ppgl_fill_ndx_surf(data, dbo);
  glUnmapNamedBuffer(dbo->id);
}

