#ifndef TABS_PLOT_AUX_H
#define TABS_PLOT_AUX_H

#include <epoxy/gl.h>
#include <cglm/cglm.h>
#include "common.h"

#define PLOT_TIME_RANGE 60 // in intervals

typedef struct plot_vert_desc {
  int x, y/*, step*/;
  gboolean xtick, ytick, surf;
  vec4 off; // x,y offsets from both sides
} t_plot_vert_desc;

typedef struct plot_idc {
  GLuint id; GLsizei count, stride;
  t_plot_vert_desc desc;
} t_plot_idc;

typedef struct plot_vo { GLuint vao; t_plot_idc vbo; } t_plot_vo;

t_plot_idc plot_aux_vert_init(t_plot_vert_desc *desc, gboolean dyn);
t_plot_idc plot_aux_grid_init(t_plot_vert_desc *desc, gboolean dyn);
t_plot_idc plot_aux_surf_init(t_plot_vert_desc *desc);
void plot_aux_vbo_surf_update(t_plot_idc *vbo);
void plot_aux_dbo_grid_update(t_plot_idc *dbo);
void plot_aux_dbo_surf_update(t_plot_idc *dbo);

#endif
