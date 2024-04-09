
#include <cglm/cglm.h>
#include <cglm/mat4.h>

#include "common.h"
#include "ppgl.h"
#include "ppgl_aux.h"
#include "ppgl_pango.h"
#include "pinger.h"
#include "ui/style.h"

#define PP_GLSL_VERSION "420 core"

#define PP_VEC3(a, b, c) (vec3){a, b, c}

#define PPGL_SETERR(fmt, ...) g_set_error(err, g_quark_from_static_string(""), -1, fmt, __VA_ARGS__)
#define PPGL_RETFAIL(fmt, ...) { PPGL_SETERR(fmt, __VA_ARGS__); return false; }

#define PPGL_GET_LOCATION(id, name, exec, fn, type) { id = fn(exec, name); \
  if (id < 0) PPGL_RETFAIL("Could not bind %s '%s'", type, name); }
#define PPGL_GET_ATTR(id, name, exec) PPGL_GET_LOCATION(id, name, exec, glGetAttribLocation, "attribute")
#define PPGL_GET_FORM(id, name, exec) PPGL_GET_LOCATION(id, name, exec, glGetUniformLocation, "uniform")
#define PPGL_GET_ATTR2(prog, loc, name) { PPGL_GET_ATTR((prog).loc, name, (prog).exec); }
#define PPGL_GET_FORM2(prog, loc, name) { PPGL_GET_FORM((prog).loc, name, (prog).exec); }

#define PPGL_TIME_RANGE 60 // in seconds
// x - ttl, y - time, z - delay
#define PLANE_XN 8
#define PLANE_YN 10
#define PLANE_ZN 10
#define SURF_XN MAXTTL
#define SURF_YN PPGL_TIME_RANGE
#define SURF_ZN 10
#define TTL_AXIS_TITLE "TTL"

#define PPGL_VTR "vtr"
#define PPGL_COL "col"
#define PPGL_CRD "crd"

#define PPGL_FRAG_BACKCOL "0.25"
#define PPGL_FRAG_FACE_F  "0.4"
#define PPGL_FRAG_BACK_F  "0.9"

#define XTICK 0.04
#define YTICK ((XTICK * Y_RES) / X_RES)
#define VIEW_ANGLE        -20
#define PERSPECTIVE_ANGLE 45
#define PERSPECTIVE_NEAR  0.1
#define PERSPECTIVE_FAR   10
#define LOOKAT_EYE    PP_VEC3(0, -3.7, 1.5)
#define LOOKAT_CENTER PP_VEC3(0, -0.7, 0)
#define LOOKAT_UP     PP_VEC3(0, 0, 1)

enum { VO_SURF = 0, VO_XY, VO_LEFT, VO_BACK, VO_TEXT, VO_MAX };

typedef struct ppgl_mat {
  mat4 xy, left, back, text;
} t_ppgl_mat;

typedef struct ppgl_axs {
  vec4 ttl[PLANE_XN / 2 + 1];
  vec4 time[PLANE_YN / 2 + 1];
  vec4 rtt[PLANE_ZN / 2 + 1];
} t_ppgl_axs;

typedef struct ppgl_dbo {
  t_ppgl_idcnt surf, grid, xy, left, back;
} t_ppgl_dbo;

typedef struct ppgl_prog {
  GLuint exec, vs, fs;
  GLint vtr, color, coord;
} t_ppgl_prog;

typedef struct ppgl_res {
  t_ppgl_prog plot, text;
  t_ppgl_vo vo[VO_MAX];
  t_ppgl_mat mat;
  t_ppgl_axs axis;
  t_ppgl_dbo dbo;
} t_ppgl_res;

typedef struct ppgl_char {
  unsigned tid;   // texture id
  vec2 size;
} t_ppgl_char;

static const GLchar *ppgl_vert_plot =
"uniform mat4 " PPGL_VTR ";\n"
"uniform vec4 " PPGL_COL ";\n"
"in vec3 " PPGL_CRD ";\n"
"out vec4 vert_col;\n"
"void main() {\n"
"  vert_col = vec4(" PPGL_COL ".rgb, (1 - crd.z / 2) * " PPGL_COL ".a);\n"
"  gl_Position = " PPGL_VTR " * vec4(" PPGL_CRD ".xyz, 1);\n"
"}";

static const GLchar *ppgl_frag_plot =
"in vec4 vert_col;\n"
"out vec4 color;\n"
"out int face;\n"
"const vec2 factor = {" PPGL_FRAG_FACE_F ", " PPGL_FRAG_BACK_F "};\n"
"const vec3 back_col = vec3(" PPGL_FRAG_BACKCOL ", " PPGL_FRAG_BACKCOL ", " PPGL_FRAG_BACKCOL ");\n"
"void main() {\n"
"  color = gl_FrontFacing ? (factor[0] * vert_col) :\n"
"   (factor[1] * vec4(back_col, 1 - (1 - vert_col.w) * 0.5));\n"
"}";

static const GLchar *ppgl_vert_text =
"uniform mat4 " PPGL_VTR ";\n"
"in vec4 " PPGL_CRD "; // <vec2 pos, vec2 tex>\n"
"out vec2 tex_crd2;\n"
"uniform mat4 projection;\n"
"void main() {\n"
"  tex_crd2 = " PPGL_CRD ".zw;\n"
"  gl_Position = " PPGL_VTR " * vec4(" PPGL_CRD ".xy, 0, 1);\n"
"}";

static const GLchar *ppgl_frag_text =
"in vec2 tex_crd2;\n"
"out vec4 color;\n"
"uniform sampler2D text;\n"
"uniform vec4 " PPGL_COL ";\n"
"void main() {\n"
"  vec4 sampled = vec4(1, 1, 1, texture(text, tex_crd2).r);\n"
"  color = " PPGL_COL " * sampled;\n"
"}";

static t_tab ppgltab = { .self = &ppgltab, .name = "ppgl-tab",
  .tag = GL3D_TAB_TAG, .tip = GL3D_TAB_TIP, .ico = {GL3D_TAB_ICON, GL3D_TAB_ICOA, GL3D_TAB_ICOB},
};

static GtkWidget *ppgl_area;
static t_ppgl_res ppgl_res;

static const float scale = 1;
static const vec4 surf_color = { 1., 0, 0, 1 };
static const vec4 grid_color = { 0, 0, 0, 0.4 };
static const vec4 text_color = { 0, 0, 0, 0.6 };

static t_ppgl_char ppgl_char_table[255];

const int ppgl_api_req =
#if GTK_CHECK_VERSION(4, 6, 0)
  GDK_GL_API_GL
#else
  1
#endif
;

//

#define MIN_ASCII_CCHAR 0x20
#define LIM_ASCII_CCHAR 0x7f

static void ppgl_init_char_table(void) {
  if (!ppgl_pango_init()) return;;
  t_ppgl_char c; memset(&c, 0, sizeof(c));
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (unsigned char i = MIN_ASCII_CCHAR; i < LIM_ASCII_CCHAR; i++) {
    ivec2 sz;
    if (!(c.tid = ppgl_pango_text(i, sz))) break;
    c.size[0] = sz[0]; c.size[1] = sz[1]; ppgl_char_table[i] = c;
  }
}

static void ppgl_free_char_table(void) {
  for (unsigned char i = MIN_ASCII_CCHAR; i < LIM_ASCII_CCHAR; i++) if (ppgl_char_table[i].tid) {
    glDeleteTextures(1, &ppgl_char_table[i].tid);
    ppgl_char_table[i].tid = 0;
  }
}

static gboolean ppgl_api_set(GtkGLArea *area, int req) {
  if (!GTK_IS_GL_AREA(area)) return false;
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_gl_area_set_allowed_apis(area, req);
  int api = gtk_gl_area_get_allowed_apis(area);
  DEBUG("GL API: requested %d, got %d", req, api);
  return (api == req);
#else
  gboolean reqes = req & GDK_GL_API_GLES;
  gtk_gl_area_set_use_es(area, reqes);
  gboolean es = gtk_gl_area_get_use_es(area);
  DEBUG("GLES API: requested '%s', got '%s'", onoff(reqes), onoff(es));
  return (es == reqes);
#endif
}

static void ppgl_set_shader_err(GLuint obj, GError **err) {
  if (!glIsShader(obj) || !err) return;
  GLint len = 0; glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len); if (len <= 0) return;
  gchar *msg = g_malloc0(len); if (!msg) return;
  glGetShaderInfoLog(obj, len, NULL, msg);
  PPGL_SETERR("Compilation failed: %s", msg);
  g_free(msg);
}

static void ppgl_set_prog_err(GLuint obj, GError **err, const char *what) {
  if (!glIsProgram(obj) || !err) return;
  GLint len = 0; glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len); if (len <= 0) return;
  gchar *msg = g_malloc0(len); if (!msg) return;
  glGetProgramInfoLog(obj, len, NULL, msg);
  PPGL_SETERR("%s failed: %s", what, msg);
  g_free(msg);
}

static GLuint ppgl_compile_shader(const GLchar* src, GLenum type, GError **err) {
  GLuint shader = glCreateShader(type);
  if (shader) {
    const GLchar* sources[] = { "#version " PP_GLSL_VERSION "\n", src };
    glShaderSource(shader, G_N_ELEMENTS(sources), sources, NULL);
    glCompileShader(shader);
    GLint okay = GL_FALSE; glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay) {
      if (err) ppgl_set_shader_err(shader, err);
      glDeleteShader(shader); shader = 0;
    }
  } else WARN("glCreateShader(%d) failed", type);
  return shader;
}

static void ppgl_del_res_vao(t_ppgl_res *res) {
  if (res) for (int i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    GLuint *pvao = &(res->vo[i].vao);
    if (*pvao > 0) { glDeleteVertexArrays(1, pvao); *pvao = 0; }
  }
}

static void ppgl_del_vbo(GLuint *pid) {
  if (pid && *pid) { glDeleteBuffers(1, pid); *pid = 0; }
}

static void ppgl_del_res_vbo_dbo(t_ppgl_res *res) {
  if (res) for (int i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    ppgl_del_vbo(&(res->vo[i].vbo.id));
  }
  ppgl_del_vbo(&(res->dbo.surf.id));
  ppgl_del_vbo(&(res->dbo.grid.id));
  ppgl_del_vbo(&(res->dbo.xy.id));
  ppgl_del_vbo(&(res->dbo.left.id));
  ppgl_del_vbo(&(res->dbo.back.id));
}

static void ppgl_del_prog_glsl(t_ppgl_prog *prog) {
  if (!prog) return;
  if (prog->exec) {
    if (prog->vs) glDetachShader(prog->exec, prog->vs);
    if (prog->fs) glDetachShader(prog->exec, prog->fs);
    glDeleteProgram(prog->exec); prog->exec = 0;
  }
  if (prog->vs) { glDeleteShader(prog->vs); prog->vs = 0; }
  if (prog->fs) { glDeleteShader(prog->fs); prog->fs = 0; }
}

static gboolean ppgl_compile_glsl(t_ppgl_prog *prog, const char *vert_glsl, const char *frag_glsl, GError **err) {
  if (!prog) return false;
  prog->exec = glCreateProgram(); if (!prog->exec) return false;
  GLint okay = GL_FALSE;
  prog->vs = ppgl_compile_shader(vert_glsl, GL_VERTEX_SHADER, err);
  if (prog->vs) {
    glAttachShader(prog->exec, prog->vs);
    prog->fs = ppgl_compile_shader(frag_glsl, GL_FRAGMENT_SHADER, err);
    if (prog->fs) {
      glAttachShader(prog->exec, prog->fs);
      glLinkProgram(prog->exec); glGetProgramiv(prog->exec, GL_LINK_STATUS, &okay);
      if (!okay && err) ppgl_set_prog_err(prog->exec, err, "Linking");
    }
  }
  if (!okay) ppgl_del_prog_glsl(prog);
  return okay;
}

static gboolean ppgl_compile_res(t_ppgl_res *res, GError **err) {
  if (!res) return false;
  if (!ppgl_compile_glsl(&res->plot, ppgl_vert_plot, ppgl_frag_plot, err)) return false;
  if (!ppgl_compile_glsl(&res->text, ppgl_vert_text, ppgl_frag_text, err)) return false;
  return true;
}

static void ppgl_plane_vert_init(t_ppgl_res *res, t_ppgl_vo *pvo, int xn, int yn,
    GLfloat xtick, GLfloat ytick, GLfloat dx, GLfloat dy, bool isflat) {
  if (!res || !pvo) return;
  pvo->vbo = ppgl_aux_vert(xn, yn, xtick, ytick, dx, dy, isflat);
  glBindVertexArray(pvo->vao);
  glEnableVertexAttribArray(res->plot.coord);
  glVertexAttribPointer(res->plot.coord, 3, GL_FLOAT, GL_FALSE, 0, 0);
}

static gboolean ppgl_res_init(t_ppgl_res *res, GError **err) {
  if (!res) return false;
  if (!ppgl_compile_res(res, err)) return false;
  for (int i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    GLuint *pvao = &(res->vo[i].vao);
    glGenVertexArrays(1, pvao);
    if (*pvao) continue;
    WARN_("glGenVertexArrays()");
    ppgl_del_prog_glsl(&res->plot);
    ppgl_del_prog_glsl(&res->text);
    ppgl_del_res_vao(res);
    return false;
  }
  //
  PPGL_GET_FORM2(res->plot, vtr, PPGL_VTR);
  PPGL_GET_FORM2(res->plot, color, PPGL_COL);
  PPGL_GET_ATTR2(res->plot, coord, PPGL_CRD);
  ppgl_plane_vert_init(res, &(res->vo[VO_SURF]), SURF_XN,  SURF_YN,  // surface
    0, 0, XTICK, YTICK, false);
  ppgl_plane_vert_init(res, &(res->vo[VO_XY]),   PLANE_XN, PLANE_YN, // x,y plane
    XTICK, YTICK, XTICK, YTICK, true);
  ppgl_plane_vert_init(res, &(res->vo[VO_LEFT]), PLANE_XN, PLANE_ZN, // left plane
    XTICK, 0, XTICK, 0, true);
  ppgl_plane_vert_init(res, &(res->vo[VO_BACK]), PLANE_ZN, PLANE_YN, // back plane
    0, 0, 0, YTICK, true);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glDisableVertexAttribArray(res->plot.coord);
  res->dbo.surf = ppgl_aux_surf(SURF_XN,  SURF_YN);     // triangles
  res->dbo.grid = ppgl_aux_grid(SURF_XN,  SURF_YN,  false,     false,     2); // surface grid
  res->dbo.xy   = ppgl_aux_grid(PLANE_XN, PLANE_YN, XTICK > 0, YTICK > 0, 1); // x,y plane grid
  res->dbo.left = ppgl_aux_grid(PLANE_XN, PLANE_ZN, XTICK > 0, false,     1); // left plane grid
  res->dbo.back = ppgl_aux_grid(PLANE_ZN, PLANE_YN, false,     false,     1); // back plane grid
  //
  PPGL_GET_FORM2(res->text, vtr, PPGL_VTR);
  PPGL_GET_ATTR2(res->text, coord, PPGL_CRD);
  PPGL_GET_FORM2(res->text, color, PPGL_COL);
  res->vo[VO_TEXT] = ppgl_pango_vo_init(res->text.coord); // text vao-n-vbo
  ppgl_init_char_table();
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glDisableVertexAttribArray(res->text.coord);
  return true;
}

static inline void ppgl_projview(mat4 projview) {
  mat4 proj, view;
  glm_perspective(glm_rad(PERSPECTIVE_ANGLE), (float)X_RES / Y_RES, PERSPECTIVE_NEAR, PERSPECTIVE_FAR, proj);
  glm_lookat(LOOKAT_EYE, LOOKAT_CENTER, LOOKAT_UP, view);
  glm_mul(proj, view, projview);
}

static void ppgl_mat_rotate(mat4 model, mat4 projview, GLfloat angle, vec3 axis, mat4 dest) {
  mat4 m; glm_mat4_copy(model, m);
  glm_rotate(m, glm_rad(angle), axis);
  glm_mul(projview, m, dest);
}

static void ppgl_mat_axis(vec4 src[], int n, int* c0, int* c1, int *c2, vec4 dst[]) {
  GLfloat x = -1, dx = 2.; if (n != 1) dx /= n - 1;
  for (int i = 0; i < n; i++, x += dx) {
    vec4 axis, crd = { c0 ? *c0 : x, c1 ? *c1 : x, c2 ? *c2 : x, 1 };
    glm_mat4_mulv(src, crd, axis);
    glm_vec4_divs(axis, axis[3], dst[i]);
  }
}

static void ppgl_matvec_init(t_ppgl_res *res) {
  if (!res) return;
  mat4 model = GLM_MAT4_IDENTITY_INIT, projview;
  glm_scale(model, PP_VEC3(scale, scale, scale));
  glm_rotate(model, glm_rad(VIEW_ANGLE), GLM_ZUP);
  ppgl_projview(projview);
  if (res->plot.vtr >= 0) { // plot related
    glm_mul(projview, model, res->mat.xy); // surface and bottom plane
    ppgl_mat_rotate(model, projview, 90, GLM_YUP, res->mat.left); // left plane
    ppgl_mat_rotate(model, projview, 90, GLM_XUP, res->mat.back); // back plane
  }
  if (res->text.vtr >= 0) { // text related
    mat4 model = GLM_MAT4_IDENTITY_INIT;
    glm_scale(model, PP_VEC3(scale, scale, scale));
    glm_mat4_copy(model, res->mat.text); // text plane
    int min = -1, max = 1;
    ppgl_mat_axis(res->mat.left, G_N_ELEMENTS(res->axis.rtt), NULL, &min, &min, res->axis.rtt);  //  rtt axis (left)
    ppgl_mat_axis(res->mat.xy,  G_N_ELEMENTS(res->axis.time), NULL, &min, &min, res->axis.time); // time axis (bottom)
    ppgl_mat_axis(res->mat.xy,   G_N_ELEMENTS(res->axis.ttl), &max, NULL, &min, res->axis.ttl);  //  ttl axis (right)
  }
}

static void ppgl_plot_init(GtkGLArea *area, t_ppgl_res *res) {
  static gboolean ppgl_already_logged;
  if (!GTK_IS_GL_AREA(area) || !res) return;
  gtk_gl_area_make_current(area);
  if (gtk_gl_area_get_error(area)) return;
  if (!ppgl_already_logged) {
    const char *ver = (const char*)glGetString(GL_VERSION);
    if (ver) { LOG("GL: %s", ver); ppgl_already_logged = true; }
  }
  if (!gtk_gl_area_get_has_depth_buffer(area)) gtk_gl_area_set_has_depth_buffer(area, true);
  GError *error = NULL;
  if (!ppgl_res_init(res, &error)) { gtk_gl_area_set_error(area, error); ERROR("init resources"); }
  glEnable(GL_DEPTH_TEST); //glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  ppgl_matvec_init(res);
}

static void ppgl_glsl_loc_reset(t_ppgl_res *res) {
  if (!res) return;
  res->plot.vtr = res->plot.coord = res->plot.color = -1;
  res->text.vtr = res->text.coord = res->text.color = -1;
}

static void ppgl_plot_free(GtkGLArea *area, t_ppgl_res *res) {
  ppgl_pango_free();
  ppgl_free_char_table();
  glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
  if (!res) return;
  ppgl_del_prog_glsl(&(res->plot));
  ppgl_del_res_vao(res);
  ppgl_del_res_vbo_dbo(res);
  ppgl_glsl_loc_reset(res);
}

static void ppgl_draw_elems(GLuint vao, t_ppgl_idcnt buff, GLint vtr,
    const vec4 color, GLint loc, const mat4 mat, GLenum mode) {
  if (vao && buff.id) {
    glBindVertexArray(vao);
    gboolean off = (mode == GL_TRIANGLES);
    if (off) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(off, off); }
    glUniformMatrix4fv(vtr, 1, GL_FALSE, (const GLfloat*)mat);
    if (color) glUniform4fv(loc, 1, color);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buff.id);
    glDrawElements(mode, buff.count, GL_UNSIGNED_SHORT, 0);
    if (off) { glPolygonOffset(0, 0); glDisable(GL_POLYGON_OFFSET_FILL); }
  }
}

#define PPGL_DRAWELEMS(ndx, dbon, ecol, matn, gltype) ppgl_draw_elems(res->vo[ndx].vao, (res->dbo).dbon, \
  res->plot.vtr, ecol, res->plot.color, (res->mat).matn, gltype);

#define FONT_WINH_PERCENT 1.5

static void ppgl_draw_text(t_ppgl_res *res, const char *text, const vec4 color,
    GLfloat xn, GLfloat yn, GLuint wx, GLuint hy, gboolean lr, GLfloat dw, GLfloat dh) {
  if (!text || !res) return;
  GLuint vao = res->vo[VO_TEXT].vao, vbo = res->vo[VO_TEXT].vbo.id;
  if (!vao || !vbo) return;
  char *p0 = lr ? g_strdup(text) : g_strreverse(g_strdup(text));
  if (!p0) return;
  glUniformMatrix4fv(res->text.vtr, 1, GL_FALSE, (const GLfloat*)res->mat.text);
  if (color) glUniform4fv(res->text.color, 1, color);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(vao);
  GLfloat f = hy * FONT_WINH_PERCENT / 100. / PPGL_FONT_SIZE / 2;
  GLfloat x0 = xn, y0 = yn, ws = 6. / wx * f, hs = 6. / hy * f;
  for (const char *p = p0; *p; p++) {
    t_ppgl_char c = ppgl_char_table[(unsigned char)*p];
    if (!c.tid) continue;
    GLfloat w = c.size[0] * ws;
    GLfloat h = c.size[1] * hs;
    GLfloat x = dw ? x0 + dw * w : x0;
    GLfloat y = dh ? y0 + dh * h : y0;
    ppgl_pango_drawtex(c.tid, vbo, x, y, w, h);
    x0 += lr ? w : -w;
  }
  g_free(p0);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

#define PPGL_IS_GLSL_VALID(prog) ((prog).exec && ((prog).vtr >= 0) && ((prog).color >= 0) && ((prog).coord >= 0))
#define PPGL_AXIS_TITLE(arr, atx, aty, title, lr, dx, dy) vec4 *ax = arr; int n = G_N_ELEMENTS(arr); \
  ppgl_draw_text(res, title, text_color, atx, aty, w, h, lr, dx, dy);

static gboolean ppgl_plot_draw(GtkGLArea *area, GdkGLContext *context, t_ppgl_res *res) {
  if (!GTK_IS_GL_AREA(area) || !GDK_IS_GL_CONTEXT(context) || !res) return false;
  if (PPGL_IS_GLSL_VALID(res->plot)) {
    glUseProgram(res->plot.exec);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    PPGL_DRAWELEMS(VO_XY,   xy,   grid_color, xy,   GL_LINES);     // bottom plane
    PPGL_DRAWELEMS(VO_LEFT, left, grid_color, left, GL_LINES);     // left plane
    PPGL_DRAWELEMS(VO_BACK, back, grid_color, back, GL_LINES);     // back plane
    PPGL_DRAWELEMS(VO_SURF, grid, grid_color, xy,   GL_LINES);     // surface gridlines
    PPGL_DRAWELEMS(VO_SURF, surf, surf_color, xy,   GL_TRIANGLES); // surface
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glFlush();
    glUseProgram(0);
  }
  if (PPGL_IS_GLSL_VALID(res->text)) {
    glUseProgram(res->text.exec);
    char buff[16];
    int w = gtk_widget_get_width(GTK_WIDGET(area)), h = gtk_widget_get_height(GTK_WIDGET(area));
    { // left axis
      PPGL_AXIS_TITLE(res->axis.rtt, ax[0][0], ax[0][1], Y_AXIS_TITLE, true, strlen(Y_AXIS_TITLE) / -2., 2);
      GLfloat val = PP_RTT0 / PP_RTT_SCALE;
      GLfloat step = - val * 2 / PLANE_ZN;
      for (int i = 0; i < n; i++, val += step) {
        snprintf(buff, sizeof(buff), PP_RTT_AXIS_FMT, val);
        ppgl_draw_text(res, buff, text_color, ax[i][0], ax[i][1], w, h, false, -2, -0.2);
    }}
    { // bottom axis
      PPGL_AXIS_TITLE(res->axis.time, (ax[0][0] + ax[n - 1][0]) / 2, (ax[0][1] + ax[n - 1][1]) / 2,
        X_AXIS_TITLE, false, -4, -3.5);
      int val = (time(NULL) - PPGL_TIME_RANGE) % 3600;
      int step = PPGL_TIME_RANGE * 2 / PLANE_YN;
      for (int i = 0; i < n; i++, val += step) {
        if (val < 0) val += 3600;
        int mm = val / 60, ss = val % 60;
        snprintf(buff, sizeof(buff), PP_TIME_AXIS_FMT, mm, ss);
        ppgl_draw_text(res, buff, text_color, ax[i][0], ax[i][1], w, h, false, 0.3, -1.5);
    }}
    { // right axis
      PPGL_AXIS_TITLE(res->axis.ttl, (ax[0][0] + ax[n - 1][0]) / 2, (ax[0][1] + ax[n - 1][1]) / 2,
        TTL_AXIS_TITLE, true, 5, -1);
      GLfloat val = 1;
      GLfloat step = (float)(MAXTTL - 1) / (n - 1);
      for (int i = 0; i < n; i++, val += step) {
        float r = fmodf(val, 1);
        snprintf(buff, sizeof(buff), "%.0f%s", val, r ? ((r < 0.5) ? "-" : "+") : "");
        ppgl_draw_text(res, buff, text_color, ax[i][0], ax[i][1], w, h, true, 1.5, -0.3);
    }}
    glUseProgram(0);
  }
  return true;
}


// pub
//

t_tab* ppgltab_init(GtkWidget* win) {
  { int units = 1;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &units);
    if (!units) { WARN_("No support for texture lookups in the vertex shader"); return NULL; }}
  ppgl_glsl_loc_reset(&ppgl_res);
  ppgl_area = gtk_gl_area_new();
  if (!ppgl_api_set(GTK_GL_AREA(ppgl_area), ppgl_api_req)) {
    WARN("Cannot set reguired GL API: %d", ppgl_api_req);
    g_object_ref_sink(ppgl_area);
    return NULL;
  }
  gtk_widget_set_hexpand(ppgl_area, true);
  gtk_widget_set_vexpand(ppgl_area, true);
  g_signal_connect(ppgl_area, EV_PPGL_INIT, G_CALLBACK(ppgl_plot_init), &ppgl_res);
  g_signal_connect(ppgl_area, EV_PPGL_FREE, G_CALLBACK(ppgl_plot_free), &ppgl_res);
  g_signal_connect(ppgl_area, EV_PPGL_DRAW, G_CALLBACK(ppgl_plot_draw), &ppgl_res);
  //
  TW_TW(ppgltab.lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2), CSS_PAD, NULL);
  TW_TW(ppgltab.dyn, gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), NULL, CSS_GR_BG);
  // merge layers
  GtkWidget *over = gtk_overlay_new();
  gtk_overlay_add_overlay(GTK_OVERLAY(over), ppgl_area);
  gtk_overlay_set_child(GTK_OVERLAY(over), ppgltab.dyn.w);
  // wrap in scroll (not necessary yet)
  ppgltab.tab.w = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), over);
  // put into tab
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(ppgltab.tab.w), scroll);
  return &ppgltab;
}

void ppgl_update(gboolean retrieve) {
  if (!pinger_state.run) return;
//  if (retrieve) ppgl_data_update();
  if (GTK_IS_WIDGET(ppgl_area)) gtk_widget_queue_draw(ppgl_area);
}

void ppgl_final_update(void) {
//  ppgl_data_update();
  if (GTK_IS_WIDGET(ppgl_area)) gtk_widget_queue_draw(ppgl_area);
}

