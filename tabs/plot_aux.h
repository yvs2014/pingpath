#ifndef TABS_PLOT_AUX_H
#define TABS_PLOT_AUX_H

#include <epoxy/gl.h>
#include <cglm/cglm.h>
#include "common.h"

#define MIN_GL_VER 30

#define PLOT_TIME_RANGE 60 // in intervals

typedef int ivec2_t[2]; // not defined in cglm < 0.9

typedef struct plot_vert_desc {
  int x, y, typ;
  vec4 off; // x,y offsets from both sides
} t_plot_vert_desc;

typedef struct plot_idc {
  GLuint id; GLsizei count, stride, hold, typ;
  t_plot_vert_desc desc;
} t_plot_idc;

typedef struct plot_dbo {
  t_plot_idc main, ext;
} t_plot_dbo;

typedef struct plot_vo { GLuint vao; t_plot_idc vbo; t_plot_dbo dbo; } t_plot_vo;

enum { SURF_VBO, SURF_DBO, GRID_DBO, VERT_IBO };
enum { VERT_SURF, VERT_GRID, VERT_XL_AXIS, VERT_XR_AXIS, VERT_YL_AXIS, VERT_YR_AXIS, VERT_TYPE_MAX };

t_plot_idc plot_aux_vert_init(t_plot_vert_desc *desc,
  void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr);
t_plot_idc plot_aux_grid_init(t_plot_vert_desc *desc);
t_plot_idc plot_aux_surf_init(t_plot_vert_desc *desc);
void plot_aux_update(t_plot_vo *vo);
void plot_aux_reset(t_plot_vo *vo);
void plot_aux_free(void);

extern int gl_ver;
extern gboolean gl_named;

#endif
