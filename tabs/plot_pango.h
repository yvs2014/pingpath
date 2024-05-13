#ifndef TABS_PLOT_PANGO_H
#define TABS_PLOT_PANGO_H

#include <epoxy/gl.h>
#include <cglm/mat4.h>

#include "common.h"
#include "plot_aux.h"

#define PLOT_FONT_SIZE 24

typedef int ivec2_t[2]; // not defined in cglm < 0.9

gboolean plot_pango_init(void);
void plot_pango_free(void);
GLuint plot_pango_text(char ch, ivec2_t size);
t_plot_vo plot_pango_vo_init(GLuint loc);
void plot_pango_drawtex(GLuint tid, GLuint vbo, GLfloat x0, GLfloat y0, GLfloat w, GLfloat h);

#endif
