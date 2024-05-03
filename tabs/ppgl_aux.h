#ifndef TABS_PPGL_AUX_H
#define TABS_PPGL_AUX_H

#include <epoxy/gl.h>
#include <cglm/cglm.h>
#include "common.h"

#define PPGL_TIME_RANGE 60 // in intervals

typedef struct ppgl_vert_desc {
  int x, y/*, step*/;
  gboolean xtick, ytick, surf;
  vec4 off; // x,y offsets from both sides
} t_ppgl_vert_desc;

typedef struct ppgl_idc {
  GLuint id; GLsizei count, stride;
  t_ppgl_vert_desc desc;
} t_ppgl_idc;

typedef struct ppgl_vo { GLuint vao; t_ppgl_idc vbo; } t_ppgl_vo;

t_ppgl_idc ppgl_aux_vert_init(t_ppgl_vert_desc *desc, gboolean dyn);
t_ppgl_idc ppgl_aux_grid_init(t_ppgl_vert_desc *desc, gboolean dyn);
t_ppgl_idc ppgl_aux_surf_init(t_ppgl_vert_desc *desc);
void ppgl_aux_vbo_surf_update(t_ppgl_idc *vbo);
void ppgl_aux_dbo_grid_update(t_ppgl_idc *dbo);
void ppgl_aux_dbo_surf_update(t_ppgl_idc *dbo);

#endif
