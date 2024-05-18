
#include <cglm/cglm.h>
#include <cglm/mat4.h>

#include "common.h"
#include "plot.h"
#include "plot_aux.h"
#include "plot_pango.h"
#include "series.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"

#if GTK_CHECK_VERSION(4, 13, 4)
#define PP_GLSL_VERSION "300 es"
#define PP_GLSL_MEDIUMP "precision mediump float"
#else
#define PP_GLSL_VERSION "400 core"
#define PP_GLSL_MEDIUMP ""
#endif

#define PP_VEC3(a, b, c) (vec3){a, b, c}

#define PLOT_SETERR(fmt, ...) g_set_error(err, g_quark_from_static_string(""), -1, fmt, __VA_ARGS__)

#define PLOT_GET_LOC(id, name, exec, fn, type) { id = fn(exec, name); \
  if (id < 0) { PLOT_SETERR("Could not bind %s '%s'", type, name); return false; }}
#define PLOT_GET_ATTR(prog, loc, name) { PLOT_GET_LOC((prog).loc, name, (prog).exec, glGetAttribLocation, "attribute"); }
#define PLOT_GET_FORM(prog, loc, name) { PLOT_GET_LOC((prog).loc, name, (prog).exec, glGetUniformLocation, "uniform"); }

// x - ttl, y - time, z - delay
#define PLANE_XN 8
#define PLANE_YN 10
#define PLANE_ZN 10
#define SURF_XN MAXTTL
#define SURF_YN PLOT_TIME_RANGE
#define TTL_AXIS_TITLE "TTL"

#define PLOT_VTR  "vtr"
#define PLOT_CRD  "crd"
#define PLOT_COL1 "col1"
#define PLOT_COL2 "col2"

#define PLOT_FRAG_BACKCOL "0.25"
#define PLOT_FRAG_FACE_F  "0.4"
#define PLOT_FRAG_BACK_F  "0.9"

#define XTICK 0.04
#define YTICK ((XTICK * Y_RES) / X_RES)
#define VIEW_ANGLE        -20
#define PERSPECTIVE_ANGLE 45
#define PERSPECTIVE_NEAR  0.1
#define PERSPECTIVE_FAR   10
#define LOOKAT_EYE    PP_VEC3(0, -3.7, 1.5)
#define LOOKAT_CENTER PP_VEC3(0, -0.7, 0)
#define LOOKAT_UP     PP_VEC3(0, 0, 1)

#define RTT_GRADIENT "4.0"
#define TTL_GRADIENT "2.0"

enum { VO_SURF = 0, VO_TEXT, VO_XY, VO_LEFT, VO_BACK, VO_TTL, VO_TIME, VO_RTT, VO_MAX };

typedef struct plot_trans {
  mat4 xy, left, back, text;
  vec4 ttl[PLANE_XN / 2 + 1];
  vec4 time[PLANE_YN / 2 + 1];
  vec4 rtt[PLANE_ZN / 2 + 1];
} t_plot_trans;

typedef struct plot_prog {
  GLuint exec, vs, fs;
  GLint vtr, colo1, colo2, coord;
  const char *vert, *frag;
} t_plot_prog;

typedef struct plot_res {
  t_plot_prog plot, text;
  t_plot_vo vo[VO_MAX];
  gboolean base;
} t_plot_res;

typedef struct plot_char {
  unsigned tid;   // texture id
  vec2 size;
} t_plot_char;

enum { GLSL_VERT, GLSL_FRAG };
static const GLchar *plot_glsl[] = {
[GLSL_VERT] =
  PP_GLSL_MEDIUMP ";\n"
  "uniform mat4 " PLOT_VTR ";\n"
  "uniform vec4 " PLOT_COL1 ";\n"
  "uniform vec4 " PLOT_COL2 ";\n"
  "in vec3 " PLOT_CRD ";\n"
  "out vec4 face_col;\n"
  "out vec4 back_col;\n"
  "const vec3 bc = vec3(" PLOT_FRAG_BACKCOL ", " PLOT_FRAG_BACKCOL ", " PLOT_FRAG_BACKCOL ");\n"
  "void main() {\n"
  "  float c1 = 1.0 / " RTT_GRADIENT " + crd.z / " RTT_GRADIENT ";\n"
  "  float c2 = 1.0 - c1;\n"
  "  float c3 = 1.0 / " TTL_GRADIENT " + crd.y / " TTL_GRADIENT ";\n"
  "  float c4 = 1.0 - c3;\n"
  "  vec4 col1 = vec4(c1 + c2 * " PLOT_COL1 ".rgb, " PLOT_COL1 ".a);\n"
  "  vec4 col2 = vec4(c1 + c2 * " PLOT_COL2 ".rgb, " PLOT_COL2 ".a);\n"
  "  face_col = c4 * col1 + c3 * col2;\n"
  "  back_col = vec4(c1 + c2 * bc, 1);\n"
  "  gl_Position = " PLOT_VTR " * vec4(" PLOT_CRD ".xyz, 1.0); }",
[GLSL_FRAG] =
  PP_GLSL_MEDIUMP ";\n"
  "in vec4 face_col;\n"
  "in vec4 back_col;\n"
  "out vec4 color;\n"
  "void main() { color = gl_FrontFacing ? face_col : back_col; }"
};

static const GLchar *text_glsl[] = {
[GLSL_VERT] =
  PP_GLSL_MEDIUMP ";\n"
  "uniform mat4 " PLOT_VTR ";\n"
  "in vec4 " PLOT_CRD "; // .xy for position, .zw for texture\n"
  "out vec2 tcrd;\n"
  "void main() {\n"
  "  tcrd = " PLOT_CRD ".zw;\n"
  "  gl_Position = " PLOT_VTR " * vec4(" PLOT_CRD ".xy, 0.0, 1.0); }",
[GLSL_FRAG] =
  PP_GLSL_MEDIUMP ";\n"
  "in vec2 tcrd;\n"
  "out vec4 color;\n"
  "uniform sampler2D text;\n"
  "uniform vec4 " PLOT_COL1 ";\n"
  "void main() { color = " PLOT_COL1 " * vec4(1.0, 1.0, 1.0, texture(text, tcrd).r); }"
};

static t_tab plottab = { .self = &plottab, .name = "plot-tab",
  .tag = PLOT_TAB_TAG, .tip = PLOT_TAB_TIP, .ico = {PLOT_TAB_ICON, PLOT_TAB_ICOA, PLOT_TAB_ICOB, PLOT_TAB_ICOC},
};

static GtkWidget *plot_base_area, *plot_dyn_area;
static t_plot_res plot_base_res = { .base = true }, plot_dyna_res;
static t_plot_trans plot_trans;

static const float scale = 1;
static const vec4 surf_colo1 = { 0.3, 0.9, 0.3, 1 };
static const vec4 surf_colo2 = { 0.3, 0.3, 0.9, 1 };
static const vec4 grid_color = { 0.75, 0.75, 0.75, 1 };
static const t_th_color text_val = { .ondark = 1, .onlight = 0 };
static const float text_alpha = 1;

static t_plot_char char_table[255];

const int plot_api_req =
#if GTK_CHECK_VERSION(4, 13, 4)
  GDK_GL_API_GLES
#else
#if GTK_CHECK_VERSION(4, 6, 0)
  GDK_GL_API_GL
#else
  1
#endif
#endif
;

static gboolean plot_res_ready;
static int draw_plot_at;

//

#define MIN_ASCII_CCHAR 0x20
#define LIM_ASCII_CCHAR 0x7f

static void plot_init_char_tables(void) {
  static gboolean plot_chars_ready;
  if (!plot_pango_init() || plot_chars_ready) return;;
  plot_chars_ready = true;
  t_plot_char c; memset(&c, 0, sizeof(c));
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (unsigned char i = MIN_ASCII_CCHAR; i < LIM_ASCII_CCHAR; i++) {
    ivec2_t sz;
    if (!(c.tid = plot_pango_text(i, sz))) { plot_chars_ready = false; break; }
    c.size[0] = sz[0]; c.size[1] = sz[1]; char_table[i] = c;
  }
}

static void plot_free_char_table(void) {
  for (unsigned char i = MIN_ASCII_CCHAR; i < LIM_ASCII_CCHAR; i++) if (char_table[i].tid) {
    glDeleteTextures(1, &char_table[i].tid); char_table[i].tid = 0; }
}

static gboolean plot_api_set(GtkGLArea *area, int req) {
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

static void plot_set_shader_err(GLuint obj, GError **err) {
  if (!glIsShader(obj) || !err) return;
  GLint len = 0; glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len); if (len <= 0) return;
  char *msg = g_malloc0(len); if (!msg) return;
  glGetShaderInfoLog(obj, len, NULL, msg);
  PLOT_SETERR("Compilation failed: %s", msg);
  g_free(msg);
}

static void plot_set_prog_err(GLuint obj, GError **err, const char *what) {
  if (!glIsProgram(obj) || !err) return;
  GLint len = 0; glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len); if (len <= 0) return;
  char *msg = g_malloc0(len); if (!msg) return;
  glGetProgramInfoLog(obj, len, NULL, msg);
  PLOT_SETERR("%s failed: %s", what, msg);
  g_free(msg);
}

static GLuint plot_compile_shader(const GLchar* src, GLenum type, GError **err) {
  GLuint shader = glCreateShader(type);
  if (shader) {
    const GLchar* sources[] = { "#version " PP_GLSL_VERSION "\n", src };
    glShaderSource(shader, G_N_ELEMENTS(sources), sources, NULL);
    glCompileShader(shader);
    GLint okay = GL_FALSE; glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay) {
      if (err) plot_set_shader_err(shader, err);
      glDeleteShader(shader); shader = 0;
    }
  } else WARN("glCreateShader(%d) failed", type);
  return shader;
}

static void plot_del_res_vao(t_plot_res *res) {
  if (res) for (int i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    GLuint *pvao = &(res->vo[i].vao);
    if (*pvao > 0) { glDeleteVertexArrays(1, pvao); *pvao = 0; }
  }
}

static void plot_del_vbo(GLuint *pid) {
  if (pid && *pid) { glDeleteBuffers(1, pid); *pid = 0; }
}

static void plot_del_res_vbo_dbo(t_plot_res *res) {
  if (res) for (int i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    plot_del_vbo(&(res->vo[i].vbo.id));
    plot_del_vbo(&(res->vo[i].dbo.main.id));
    plot_del_vbo(&(res->vo[i].dbo.ext.id));
  }
}

static void plot_del_prog_glsl(t_plot_prog *prog) {
  if (!prog) return;
  if (prog->exec) {
    if (prog->vs) glDetachShader(prog->exec, prog->vs);
    if (prog->fs) glDetachShader(prog->exec, prog->fs);
    glDeleteProgram(prog->exec); prog->exec = 0;
  }
  if (prog->vs) { glDeleteShader(prog->vs); prog->vs = 0; }
  if (prog->fs) { glDeleteShader(prog->fs); prog->fs = 0; }
}

static gboolean plot_compile_glsl(t_plot_prog *prog, GError **err) {
  if (!prog || !prog->vert || !prog->frag) return false;
  prog->exec = glCreateProgram(); if (!prog->exec) return false;
  GLint okay = GL_FALSE;
  prog->vs = plot_compile_shader(prog->vert, GL_VERTEX_SHADER, err);
  if (prog->vs) {
    glAttachShader(prog->exec, prog->vs);
    prog->fs = plot_compile_shader(prog->frag, GL_FRAGMENT_SHADER, err);
    if (prog->fs) {
      glAttachShader(prog->exec, prog->fs);
      glLinkProgram(prog->exec); glGetProgramiv(prog->exec, GL_LINK_STATUS, &okay);
      if (!okay && err) plot_set_prog_err(prog->exec, err, "Linking");
    }
  }
  if (!okay) plot_del_prog_glsl(prog);
  return okay;
}

static gboolean plot_compile_res(t_plot_res *res, GError **err) {
  if (!res) return false;
  if (!plot_compile_glsl(&res->plot, err)) return false;
  if (!plot_compile_glsl(&res->text, err)) return false;
  return true;
}

#define PLOT_GLSL_PROG_SETUP(prog, glsl) { (prog).vtr = (prog).coord = (prog).colo1 = (prog).colo2 = -1; \
  if (setup) { (prog).vert = (glsl)[GLSL_VERT]; (prog).frag = (glsl)[GLSL_FRAG]; }}

static inline void plot_glsl_reset(t_plot_res *res, gboolean setup) {
  if (res) {
    PLOT_GLSL_PROG_SETUP(res->plot, plot_glsl);
    PLOT_GLSL_PROG_SETUP(res->text, text_glsl);
  }
}

#define PLOT_GLSL_BASE_ATTRIBS(prog) { \
  PLOT_GET_ATTR(prog, coord, PLOT_CRD); \
  PLOT_GET_FORM(prog, vtr, PLOT_VTR); \
  PLOT_GET_FORM(prog, colo1, PLOT_COL1); }
#define PLOT_GLSL_EXT_ATTRIBS(prog) { PLOT_GLSL_BASE_ATTRIBS(prog); PLOT_GET_FORM(prog, colo2, PLOT_COL2); }

#define PLOT_VERT_INIT(obj) { \
  (obj).vbo = plot_aux_vert_init(&desc, plot_plane_vert_attr, (obj).vao, res->plot.coord); \
  (obj).dbo.main = plot_aux_grid_init(&desc); }

static void plot_plane_vert_attr(GLuint vao, GLint coord) {
  if (!vao || (coord < 0)) return;
  glBindVertexArray(vao);
  glEnableVertexAttribArray(coord);
  glVertexAttribPointer(coord, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glBindVertexArray(0);
  glDisableVertexAttribArray(coord);
}

static gboolean plot_res_init(t_plot_res *res, GError **err) {
  if (!res) return false;
  if (!plot_compile_res(res, err)) return false;
  for (int i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    GLuint *pvao = &(res->vo[i].vao);
    glGenVertexArrays(1, pvao);
    if (*pvao) continue;
    WARN_("glGenVertexArrays()");
    plot_del_prog_glsl(&res->plot);
    plot_del_prog_glsl(&res->text);
    plot_del_res_vao(res);
    return false;
  }
  // plot related
  PLOT_GLSL_EXT_ATTRIBS(res->plot);
  if (res->base) {
    { // x,y plane
      t_plot_vert_desc desc = { .x = PLANE_XN, .y = PLANE_YN, .typ = VERT_GRID, .off = {XTICK, 0, 0, YTICK} };
      PLOT_VERT_INIT(res->vo[VO_XY]);
      desc.typ = VERT_XL_AXIS; PLOT_VERT_INIT(res->vo[VO_TIME]);
      desc.typ = VERT_YR_AXIS; PLOT_VERT_INIT(res->vo[VO_TTL]);
    }
    { // left plane
      t_plot_vert_desc desc = { .x = PLANE_XN, .y = PLANE_ZN, .typ = VERT_GRID, .off = {XTICK, 0, 0, 0} };
      PLOT_VERT_INIT(res->vo[VO_LEFT]);
      desc.typ = VERT_XL_AXIS; PLOT_VERT_INIT(res->vo[VO_RTT]);
    }
    { // back plane
      t_plot_vert_desc desc = { .x = PLANE_ZN, .y = PLANE_YN, .typ = VERT_GRID, .off = {0, 0, 0, YTICK} };
      PLOT_VERT_INIT(res->vo[VO_BACK]);
    }
  } else { // surface
    t_plot_vert_desc desc = { .x = SURF_XN * 2, .y = SURF_YN * 2, .typ = VERT_SURF, .off = {XTICK, 0, 0, YTICK} };
    PLOT_VERT_INIT(res->vo[VO_SURF]);
    res->vo[VO_SURF].dbo.ext = plot_aux_surf_init(&desc); // triangles
  }
  // text related
  PLOT_GLSL_BASE_ATTRIBS(res->text);
  res->vo[VO_TEXT] = plot_pango_vo_init(res->text.coord);    // text vao-n-vbo
  return true;
}

static void plot_res_free(t_plot_res *res) {
  if (res) {
    plot_del_prog_glsl(&(res->plot));
    plot_del_res_vao(res);
    plot_del_res_vbo_dbo(res);
    plot_glsl_reset(res, false);
  }
  glDisable(GL_DEPTH_TEST); // glDisable(GL_BLEND);
  if (!plot_res_ready) return;
  plot_res_ready = false;
  plot_pango_free();
  plot_free_char_table();
}

static inline void plot_projview(mat4 projview) {
  mat4 proj, view;
  glm_perspective(glm_rad(PERSPECTIVE_ANGLE), (float)X_RES / Y_RES, PERSPECTIVE_NEAR, PERSPECTIVE_FAR, proj);
  glm_lookat(LOOKAT_EYE, LOOKAT_CENTER, LOOKAT_UP, view);
  glm_mul(proj, view, projview);
}

static void plot_mat_rotate(mat4 model, mat4 projview, GLfloat angle, vec3 axis, mat4 dest) {
  mat4 m; glm_mat4_copy(model, m);
  glm_rotate(m, glm_rad(angle), axis);
  glm_mul(projview, m, dest);
}

static void plot_mat_axis(vec4 src[], int n, int* c0, int* c1, int *c2, vec4 dst[]) {
  GLfloat x = -1, dx = 2.; if (n != 1) dx /= n - 1;
  for (int i = 0; i < n; i++, x += dx) {
    vec4 axis, crd = { c0 ? *c0 : x, c1 ? *c1 : x, c2 ? *c2 : x, 1 };
    glm_mat4_mulv(src, crd, axis);
    glm_vec4_divs(axis, axis[3], dst[i]);
  }
}

static void plot_transform_init(void) {
  mat4 model = GLM_MAT4_IDENTITY_INIT, projview;
  glm_scale(model, PP_VEC3(scale, scale, scale));
  glm_rotate(model, glm_rad(VIEW_ANGLE), GLM_ZUP);
  plot_projview(projview);
  { // plot related
    glm_mul(projview, model, plot_trans.xy); // surface and bottom plane
    plot_mat_rotate(model, projview, 90, GLM_YUP, plot_trans.left); // left plane
    plot_mat_rotate(model, projview, 90, GLM_XUP, plot_trans.back); // back plane
  }
  { // text related
    mat4 model = GLM_MAT4_IDENTITY_INIT;
    glm_scale(model, PP_VEC3(scale, scale, scale));
    glm_mat4_copy(model, plot_trans.text); // text plane
    int min = -1, max = 1;
    plot_mat_axis(plot_trans.left, G_N_ELEMENTS(plot_trans.rtt), NULL, &min, &min, plot_trans.rtt);  //  rtt axis (left)
    plot_mat_axis(plot_trans.xy,  G_N_ELEMENTS(plot_trans.time), NULL, &min, &min, plot_trans.time); // time axis (bottom)
    plot_mat_axis(plot_trans.xy,   G_N_ELEMENTS(plot_trans.ttl), &max, NULL, &min, plot_trans.ttl);  //  ttl axis (right)
  }
}

static void area_init(GtkGLArea *area, t_plot_res *res) {
  if (!GTK_IS_GL_AREA(area) || !res) return;
  gtk_gl_area_make_current(area);
  if (gtk_gl_area_get_error(area)) return;
  if (!gl_ver) {
    gl_ver = epoxy_gl_version(); gl_named = gl_ver >= 45;
    LOG("GL: %s", glGetString(GL_VERSION));
    if (gl_ver < MIN_GL_VER) {
      char *mesg = g_strdup_printf("GL: incompat version %d.%d (min %d.%d)",
        gl_ver / 10, gl_ver % 10, MIN_GL_VER / 10, MIN_GL_VER % 10);
      WARN_(mesg); LOG("%s", mesg); g_free(mesg);
    }
  }
  { GError *error = NULL; if (!plot_res_init(res, &error)) { gtk_gl_area_set_error(area, error); ERROR("init resources"); }}
  if (!gtk_gl_area_get_has_depth_buffer(area)) gtk_gl_area_set_has_depth_buffer(area, true);
  glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  plot_init_char_tables();
  plot_res_ready = true;
}

static void area_free(GtkGLArea *area, t_plot_res *res) {
  plot_res_free(res);
}

static void plot_draw_elems(GLuint vao, t_plot_idc buff, GLint vtr,
    const vec4 colo1, GLint loc1, const vec4 colo2, GLint loc2, const mat4 mat, GLenum mode) {
  if (!vao || !buff.id) return;
  glBindVertexArray(vao);
  gboolean off = (mode == GL_TRIANGLES);
  if (off) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(off, off); }
  glUniformMatrix4fv(vtr, 1, GL_FALSE, (const GLfloat*)mat);
  if (colo1) glUniform4fv(loc1, 1, colo1);
  if (colo2) glUniform4fv(loc2, 1, colo2);
  glBindBuffer(buff.typ, buff.id);
  glDrawElements(mode, buff.count, GL_UNSIGNED_SHORT, 0);
  if (off) { glPolygonOffset(0, 0); glDisable(GL_POLYGON_OFFSET_FILL); }
  glBindBuffer(buff.typ, 0);
  glBindVertexArray(0);
}

#define FONT_WINH_PERCENT 1.5

static void plot_draw_text(t_plot_res *res, const char *text, const vec4 color,
    GLfloat xn, GLfloat yn, GLuint wx, GLuint hy, gboolean lr, GLfloat dw, GLfloat dh) {
  if (!res || !text) return;
  GLuint vao = res->vo[VO_TEXT].vao, vbo = res->vo[VO_TEXT].vbo.id, typ = res->vo[VO_TEXT].vbo.typ;
  if (!vao || !vbo || !typ) return;
  char *dir = lr ? g_strdup(text) : g_strreverse(g_strdup(text));
  if (!dir) return;
  glUniformMatrix4fv(res->text.vtr, 1, GL_FALSE, (const GLfloat*)plot_trans.text);
  if (color) glUniform4fv(res->text.colo1, 1, color);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(vao);
  GLfloat f = hy * FONT_WINH_PERCENT / 100. / PLOT_FONT_SIZE / 2;
  GLfloat x0 = xn, y0 = yn, ws = 6. / wx * f, hs = 6. / hy * f;
  for (const char *p = dir; *p; p++) {
    t_plot_char c = char_table[(unsigned char)*p];
    if (!c.tid) continue;
    GLfloat w = c.size[0] * ws;
    GLfloat h = c.size[1] * hs;
    GLfloat x = dw ? x0 + dw * w : x0;
    GLfloat y = dh ? y0 + dh * h : y0;
    plot_pango_drawtex(c.tid, vbo, typ, x, y, w, h);
    x0 += lr ? w : -w;
  }
  g_free(dir);
}

#define PLOT_DRAWELEMS(ndx, dbotype, glcolo1, glcolo2, matrix, gltype) { plot_draw_elems(res->vo[ndx].vao, \
  res->vo[ndx].dbo.dbotype, res->plot.vtr, glcolo1, res->plot.colo1, glcolo2, res->plot.colo2, plot_trans.matrix, gltype); }
#define PLOT_AXIS_TITLE(arr, atx, aty, title, lr, dx, dy) vec4 *ax = arr; int n = G_N_ELEMENTS(arr); \
  plot_draw_text(res, title, text_color, atx, aty, w, h, lr, dx, dy);
#define PLOT_IS_BASE_GLSL_VALID(prog) ((prog).exec && ((prog).vtr >= 0) && \
  ((prog).colo1 >= 0) && ((prog).coord >= 0))
#define PLOT_IS_EXT_GLSL_VALID(prog) (PLOT_IS_BASE_GLSL_VALID(prog) && ((prog).colo2 >= 0))
#define PLOT_TEXTCOL(thcol) (opts.darkplot ? thcol.ondark : thcol.onlight)

static gboolean area_draw(GtkGLArea *area, GdkGLContext *context, t_plot_res *res) {
  if (!GTK_IS_GL_AREA(area) || !GDK_IS_GL_CONTEXT(context) || !res) return false;
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (PLOT_IS_EXT_GLSL_VALID(res->plot)) {
    glUseProgram(res->plot.exec);
    if (res->base) {
      PLOT_DRAWELEMS(VO_XY,   main, grid_color, grid_color, xy,   GL_LINES);     // plane: bottom
      PLOT_DRAWELEMS(VO_LEFT, main, grid_color, grid_color, left, GL_LINES);     // plane: left
      PLOT_DRAWELEMS(VO_BACK, main, grid_color, grid_color, back, GL_LINES);     // plane: back
      PLOT_DRAWELEMS(VO_RTT,  main, grid_color, grid_color, left, GL_LINES);     // axis: rtt
      PLOT_DRAWELEMS(VO_TIME, main, grid_color, grid_color, xy,   GL_LINES);     // axis: time
      PLOT_DRAWELEMS(VO_TTL,  main, grid_color, grid_color, xy,   GL_LINES);     // axis: ttl
    } else if (pinger_state.gotdata) {
      PLOT_DRAWELEMS(VO_SURF, main, grid_color, grid_color, xy,   GL_LINES);     // surface gridlines
      PLOT_DRAWELEMS(VO_SURF, ext,  surf_colo1, surf_colo2, xy,   GL_TRIANGLES); // surface
    }
    glFlush();
    glUseProgram(0);
  }
  if (PLOT_IS_BASE_GLSL_VALID(res->text)) {
    glUseProgram(res->text.exec);
    char buff[16];
    int w = gtk_widget_get_width(GTK_WIDGET(area)), h = gtk_widget_get_height(GTK_WIDGET(area));
    GLfloat tcol = PLOT_TEXTCOL(text_val);
    vec4 text_color = { tcol, tcol, tcol, text_alpha };
    if (res->base) {
      { // left axis
        PLOT_AXIS_TITLE(plot_trans.rtt, ax[0][0], ax[0][1], Y_AXIS_TITLE, true, strlen(Y_AXIS_TITLE) / -2., 2);
        GLfloat val = 0, step = series_datamax / PP_RTT_SCALE * 2 / PLANE_ZN;
        for (int i = n - 1; i >= 0; i--, val += step) {
          snprintf(buff, sizeof(buff), PP_RTT_AXIS_FMT, val);
          plot_draw_text(res, buff, text_color, ax[i][0], ax[i][1], w, h, false, -2, -0.2);
        }}
      { // right axis
        PLOT_AXIS_TITLE(plot_trans.ttl, (ax[0][0] + ax[n - 1][0]) / 2, (ax[0][1] + ax[n - 1][1]) / 2,
          TTL_AXIS_TITLE, true, 5, -1);
        int printed = -1;
        GLfloat val = opts.min + 1;
        GLfloat step = (tgtat - val) / (n - 1);
        for (int i = 0; i < n; i++, val += step) {
          if (i && (i != (n - 1)) && ((int)val == printed)) continue; else printed = val;
          float r = fmodf(val, 1);
          snprintf(buff, sizeof(buff), "%.0f%s", val, r ? ((r < 0.5) ? "-" : "+") : "");
          plot_draw_text(res, buff, text_color, ax[i][0], ax[i][1], w, h, true, 1.5, -0.3);
      }}
    } else { // bottom axis
      PLOT_AXIS_TITLE(plot_trans.time, (ax[0][0] + ax[n - 1][0]) / 2, (ax[0][1] + ax[n - 1][1]) / 2,
        X_AXIS_TITLE, false, -4, -3.5);
      if (!draw_plot_at || (pinger_state.run && !pinger_state.pause)) draw_plot_at = time(NULL) % 3600;
      int dt = PLOT_TIME_RANGE * 2 / PLANE_YN;
      for (int i = 0, t = draw_plot_at - PLOT_TIME_RANGE; i < n; i++, t += dt) {
        LIMVAL(t, 3600);
        snprintf(buff, sizeof(buff), PP_TIME_AXIS_FMT, t / 60, t % 60);
        plot_draw_text(res, buff, text_color, ax[i][0], ax[i][1], w, h, false, 0.3, -1.5);
    }}
    glUseProgram(0);
  }
  return true;
}

static GtkWidget* plot_init_glarea(t_plot_res *res) {
  GtkWidget *area = gtk_gl_area_new();
  g_return_val_if_fail(area, NULL);
  if (!plot_api_set(GTK_GL_AREA(area), plot_api_req)) {
    WARN("Cannot set reguired GL API: %d", plot_api_req);
    g_object_ref_sink(area);
    return NULL;
  }
  gtk_widget_set_hexpand(area, true);
  gtk_widget_set_vexpand(area, true);
  g_signal_connect(area, EV_PLOT_INIT, G_CALLBACK(area_init), res);
  g_signal_connect(area, EV_PLOT_FREE, G_CALLBACK(area_free), res);
  g_signal_connect(area, EV_PLOT_DRAW, G_CALLBACK(area_draw), res);
  return area;
}


// pub
//

t_tab* plottab_init(GtkWidget* win) {
  { int units = 1;
    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &units);
    if (!units) { WARN_("No support for texture lookups in the vertex shader"); return NULL; }}
  plot_transform_init();
  plot_glsl_reset(&plot_base_res, true);
  plot_glsl_reset(&plot_dyna_res, true);
  //
  TW_TW(plottab.lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2), CSS_PAD, NULL);
  g_return_val_if_fail(plottab.lab.w, NULL);
  TW_TW(plottab.dyn, gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), NULL, CSS_PLOT_BG);
  g_return_val_if_fail(plottab.dyn.w, NULL);
  // create layers
  plot_base_area = plot_init_glarea(&plot_base_res); g_return_val_if_fail(plot_base_area, NULL);
  plot_dyn_area  = plot_init_glarea(&plot_dyna_res); g_return_val_if_fail(plot_dyn_area,  NULL);
  // merge layers
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(over, NULL);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), plot_base_area);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), plot_dyn_area);
  gtk_overlay_set_child(GTK_OVERLAY(over), plottab.dyn.w);
  // wrap scrolling
  plottab.tab.w = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), over);
  // put into tab
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(plottab.tab.w), scroll);
  series_reg_on_scale(plot_base_area);
  series_min(PLOT_TIME_RANGE);
  return &plottab;
}

inline void plottab_free(void) { plot_res_free(&plot_dyna_res); plot_res_free(&plot_base_res); plot_aux_free(); }

void plottab_restart(void) {
  draw_plot_at = 0;
  plot_aux_reset(&plot_dyna_res.vo[VO_SURF]);
  plottab_refresh();
}

void plottab_update(void) {
  plot_aux_update(&plot_dyna_res.vo[VO_SURF]);
  if (GTK_IS_WIDGET(plot_dyn_area)) gtk_gl_area_queue_render(GTK_GL_AREA(plot_dyn_area));
}

void plottab_refresh(void) {
  if (GTK_IS_WIDGET(plot_base_area)) gtk_gl_area_queue_render(GTK_GL_AREA(plot_base_area));
  if (GTK_IS_WIDGET(plot_dyn_area)) gtk_gl_area_queue_render(GTK_GL_AREA(plot_dyn_area));
}

