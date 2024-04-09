
#include <string.h>
#include <cglm/cglm.h>

#include "ppgl_aux.h"

static inline void vec3_set(vec3 vec, GLfloat a, GLfloat b, GLfloat c) { vec[0] = a; vec[1] = b; vec[2] = c; }

static GLfloat ppgl_datanode(int i, int j) { // TODO
  return -0.99;
}

// vertex x,y,data,alpha
t_ppgl_idcnt ppgl_aux_vert(int xn, int yn, GLfloat xtick, GLfloat ytick, GLfloat dx, GLfloat dy, bool isflat) {
  t_ppgl_idcnt idc = { .id = 0 };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    int xn1 = xn + 1, yn1 = yn + 1;
    idc.count = xn1 * yn1;
    idc.stride = sizeof(vec3);
    if (xtick > 0) idc.count += yn1;
    if (ytick > 0) idc.count += xn1;
    GLfloat x0 = -1 + dx;
    vec3 vertex[idc.count]; memset(vertex, 0, sizeof(vertex));
    GLfloat xlen = 2. - dx, ylen = 2. - dy;
    GLfloat xn2 = xn / xlen, yn2 = yn / ylen;
    int n = 0;
    for (int i = 0; i < xn1; i++) for (int j = 0; j < yn1; j++)
      vec3_set(vertex[n++], -1 + j / yn2, x0 + i / xn2, isflat ? -1 : ppgl_datanode(i, j));
    if (xtick > 0) for (int j = 0; j < yn1; j++) vec3_set(vertex[n++], -1 + j / yn2, -1, -1);
    if (ytick > 0) for (int i = 0; i < xn1; i++) vec3_set(vertex[n++], 1, x0 + i / xn2, -1);
    glBindBuffer(GL_ARRAY_BUFFER, idc.id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, isflat ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
  } else FAIL("glGenBuffers()");
  return idc;
}

t_ppgl_idcnt ppgl_aux_grid(int xn, int yn, bool isxtick, bool isytick, int step) { // index array of gridlines
  t_ppgl_idcnt idc = { .id = 0 };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    int xn1 = xn + 1, yn1 = yn + 1;
    idc.count = (xn * yn1 + xn1 * yn) * 2;
    idc.stride = sizeof(GLushort);
    if (isxtick) idc.count += yn1 * 2;
    if (isytick) idc.count += xn1 * 2;
    GLushort index[idc.count]; memset(index, 0, sizeof(index));
    int i = 0;
    // grid
    for (int x = 0; x < xn1; x++) {
      if ((step > 1) && ((x % step) != 0)) continue;
      int xx = x * yn1;
      for (int y = 0; y < yn; y++) {
        int xy = xx + y; index[i++] = xy++; index[i++] = xy; }
    }
    for (int y = 0; y < yn1; y++) {
      if ((step > 1) && ((y % step) != 0)) continue;
      for (int x = 0; x < xn; x++) {
        int xy = x * yn1 + y; index[i++] = xy; index[i++] = xy + yn1; }
    }
    // ticks
    int n = xn1 * yn1;
    if (isxtick) for (int y = 0, xy = n; y < yn1; y++, n++) {
      index[i++] = y; index[i++] = y + xy; }
    if (isytick) for (int x = 0; x < xn1; x++, n++) {
      index[i++] = n; index[i++] = yn1 * (x + 1) - 1; }
    //
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idc.id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);
  } else FAIL("glGenBuffers()");
  return idc;
}

static void ppgl_tr_clock(GLushort* arr, int i0, int dn, int clk) {
  int in = i0 + dn;
  *arr++ = i0;
  *arr++ = i0 + 1;
  *arr++ = clk ? in : in + 1;
  *arr++ = in + 1;
  *arr++ = in;
  *arr++ = clk ? i0 + 1 : i0;
}

t_ppgl_idcnt ppgl_aux_surf(int xn, int yn) { // index array of triangles
  t_ppgl_idcnt idc = { .id = 0 };
  glGenBuffers(1, &idc.id);
  if (idc.id) {
    idc.count = xn * yn * 6;
    idc.stride = sizeof(GLushort);
    GLushort index[idc.count]; memset(index, 0, sizeof(index));
    int yn1 = yn + 1;
    for (int i = 0, x = 0; x < xn; x++) {
      int clk = x % 2, dn = x * yn1;
      for (int y = 0; y < yn; y++, i += 6)
        ppgl_tr_clock(&index[i], y + dn, yn1, (y % 2) ? clk : !clk);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idc.id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW);
  } else FAIL("glGenBuffers()");
  return idc;
}

