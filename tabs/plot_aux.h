#ifndef TABS_PLOT_AUX_H
#define TABS_PLOT_AUX_H

#include <gtk/gtk.h>
#include <epoxy/gl.h>

enum { MIN_GL_VER = 30 };
enum { PLOT_TIME_RANGE = 60 }; // in intervals

typedef struct plot_idc {
  GLuint id; GLsizei count, stride, hold, typ;
  int x, y, plo;
} t_plot_idc;

typedef struct plot_dbo {
  t_plot_idc main, ext;
} t_plot_dbo;

typedef struct plot_vo { GLuint vao; t_plot_idc vbo; t_plot_dbo dbo; } t_plot_vo;

enum { SURF_VBO, SURF_DBO, GRID_DBO, VERT_IBO };
enum { PLO_SURF, PLO_GRID, PLO_AXIS_XL, PLO_AXIS_XR, PLO_AXIS_YL, PLO_AXIS_YR/*, PLO_MAX*/ };

t_plot_idc plot_aux_vert_init_plane(int x, int y, int plo,
  void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr);
t_plot_idc plot_aux_vert_init_axis(int x, int y, int plo, float width,
  void (*attr_fn)(GLuint vao, GLint attr), GLuint vao, GLint attr);
t_plot_idc plot_aux_grid_init(int x, int y, int plo);
t_plot_idc plot_aux_surf_init(int x, int y, int plo);
void plot_aux_update(t_plot_vo *vo);
void plot_aux_reset(t_plot_vo *vo);
void plot_aux_free(void);

extern int gl_ver;
extern gboolean gl_named;

#endif
