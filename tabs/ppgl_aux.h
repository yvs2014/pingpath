#ifndef TABS_PPGL_AUX_H
#define TABS_PPGL_AUX_H

#include <epoxy/gl.h>
#include "common.h"

typedef struct ppgl_idsz { GLuint id; GLsizei count, stride; } t_ppgl_idcnt;
typedef struct ppgl_vo { GLuint vao; t_ppgl_idcnt vbo; } t_ppgl_vo;

t_ppgl_idcnt ppgl_aux_vert(int xn, int yn, GLfloat xtick, GLfloat ytick, GLfloat dx, GLfloat dy, bool isflat);
t_ppgl_idcnt ppgl_aux_grid(int xn, int yn, bool isxtick, bool isytick, int step);
t_ppgl_idcnt ppgl_aux_surf(int xn, int yn);

#endif
