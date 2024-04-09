#ifndef TABS_PPGL_PANGO_H
#define TABS_PPGL_PANGO_H

#include <epoxy/gl.h>
#include <cglm/mat4.h>

#include "common.h"
#include "ppgl_aux.h"

#define PPGL_FONT_SIZE 24

gboolean ppgl_pango_init(void);
void ppgl_pango_free(void);
GLuint ppgl_pango_text(char ch, ivec2 size);
t_ppgl_vo ppgl_pango_vo_init(GLuint loc);
void ppgl_pango_drawtex(GLuint tid, GLuint vbo, GLfloat x0, GLfloat y0, GLfloat w, GLfloat h);

#endif
