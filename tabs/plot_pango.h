#ifndef TABS_PLOT_PANGO_H
#define TABS_PLOT_PANGO_H

#include <epoxy/gl.h>

#include "plot_aux.h"

enum { PLOT_FONT_SIZE = 24 };

gboolean plot_pango_init(void);
void plot_pango_free(void);
GLuint plot_pango_text(const char *str, int size[2]);
t_plot_vo plot_pango_vo_init(GLuint loc);
void plot_pango_drawtex(GLuint id, GLuint vbo, GLuint typ, float x0, float y0, float width, float height);

#endif
