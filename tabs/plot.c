
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <cglm/cglm.h>

#include "common.h"
#include "text.h"
#include "aux.h"

#include "plot.h"
#include "plot_aux.h"
#include "plot_pango.h"
#include "series.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/notifier.h"

#if GTK_CHECK_VERSION(4, 13, 4)
#define PP_GLSL_VERSION "300 es"
#define PP_GLSL_MEDIUMP "precision mediump float"
#else
#define PP_GLSL_VERSION "400 core"
#define PP_GLSL_MEDIUMP ""
#endif

#define PLOT_SETERR(fmt, ...) g_set_error(err, g_quark_from_static_string(""), -1, fmt, __VA_ARGS__)

#define TIM_AXIS_TITLE "Time"
#define RTT_AXIS_TITLE "Delay"
#define TTL_AXIS_TITLE "TTL"

// x - ttl, y - time, z - delay
enum { PLANE_XN = 8, PLANE_YN = 10, PLANE_ZN = 10 };
#define SURF_XN MAXTTL
#define SURF_YN PLOT_TIME_RANGE

#define AXIS_TTL_ELEMS (PLANE_XN / 2 + 1)
#define AXIS_TIM_ELEMS (PLANE_YN / 2 + 1)
#define AXIS_RTT_ELEMS (PLANE_ZN / 2 + 1)
#define AXIS_MAX_ELEMS AXIS_TIM_ELEMS

#define PLOT_VTR  "vtr"
#define PLOT_CRD  "crd"
#define PLOT_COL1 "col1"
#define PLOT_COL2 "col2"

#define PLOT_FRAG_BACKCOL "0.25"
#define PLOT_FRAG_FACE_F  "0.4"
#define PLOT_FRAG_BACK_F  "0.9"

#define TICK 0.035
#define PERSPECTIVE_NEAR  0.01
#define PERSPECTIVE_FAR   100.
#define LOOKAT_EYE    (vec3){0, -4.8, 0}
#define LOOKAT_CENTER (vec3){0,  0,   0}
#define LOOKAT_UP     (vec3){0,  0,   1}
#define SCALE3        (vec3){scale, scale, scale}

#define RTT_GRADIENT "4.0"
#define TTL_GRADIENT "2.0"

static const unsigned char MIN_ASCII_CCHAR = 0x20;
static const unsigned char LIM_ASCII_CCHAR = 0x7f;

#define FONT_HEIGHT_PERCENT 1.5

// locations: VO (virtual object), PO (plane object), AN (axis object)
enum { VO_SURF = 0, VO_TEXT, PO_BOT, PO_LOR, PO_FON,
 A1_RTT, A2_RTT, A1_TIM, A2_TIM, A1_TTL, A2_TTL, VO_MAX };

enum { SHIFT_TITLE, SHIFT_MARKS, SHIFT_MAX };    // shifts
enum { AXIS_RTT, AXIS_TIM, AXIS_TTL, AXIS_MAX }; // axes

// lor,fon,bot: left-o-right, far-o-near, bottom-o-top
typedef struct plot_trans { mat4 data, lor, fon, bot; } t_plot_trans;

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
  unsigned tid; // texture id
  vec2 size;
} t_plot_char;

typedef struct mark_text { int len; char text[PP_MARK_MAXLEN]; } t_mark_text;
typedef struct axis_mark_overlap { gboolean cached, over[AXIS_MAX_ELEMS]; } t_axis_mark_overlap;

typedef struct plot_axis_params {
  int ndx;       // A1|A2 vo index
  const int rno; // mark_rect's ndx
  const vec2 pad;
  vec2 at, shift;
  const int crd_len; vec4 crd[AXIS_MAX_ELEMS];
  const int title_len; const char *title;
  void (*fill)(t_mark_text mark[], int n);
  t_axis_mark_overlap overlap;
} t_plot_axis_params;

typedef struct plot_plane_at { gboolean           bot, lor, fon; } t_plot_plane_at;

typedef struct plot_axes_set { t_plot_axis_params rtt, tim, ttl; } t_plot_axes_set;

typedef struct plparams {
  int cube[3];
  t_plot_plane_at at; // planes' position
  t_plot_axes_set axes;
} t_plparam;

typedef struct v3s { float a, b, c; } t_v3s;

mat4 rmat = GLM_MAT4_IDENTITY_INIT; // rotation_matrix

//

static const float scale = 1;

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
  .ico = {PLOT_TAB_ICON, PLOT_TAB_ICOA, PLOT_TAB_ICOB, PLOT_TAB_ICOC} };

static GtkWidget *plot_base_area, *plot_dyn_area;
static t_plot_res plot_base_res = { .base = true }, plot_dyna_res;
static t_plot_trans plot_trans;
static mat4 scaled = GLM_MAT4_IDENTITY_INIT;

static const vec4 grid_color = { 0.75, 0.75, 0.75, 1 };
static const t_th_color text_val = { .ondark = 1, .onlight = 0 };
static const float text_alpha = 1;

static const int plot_api_req =
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

static vec4 surf_colo1;
static vec4 surf_colo2;

static t_plot_char char_table[255];

static gboolean plot_res_ready;
static int draw_plot_at;

static void plot_rtt_marks(t_mark_text mark[], int max);
static void plot_tim_marks(t_mark_text mark[], int max);
static void plot_ttl_marks(t_mark_text mark[], int max);

enum { RNO_RTT, RNO_TIM, RNO_TTL, RNO_MAX };

static t_plparam plparam = { .cube = { 90, 90, 0 }, .axes = {
  .rtt = { .rno = RNO_RTT, .pad = { 7, 9}, .crd_len = AXIS_RTT_ELEMS,
    .title_len = strlen(RTT_AXIS_TITLE), .title = RTT_AXIS_TITLE, .fill = plot_rtt_marks },
  .tim = { .rno = RNO_TIM, .pad = {10, 9}, .crd_len = AXIS_TIM_ELEMS,
    .title_len = strlen(TIM_AXIS_TITLE), .title = TIM_AXIS_TITLE, .fill = plot_tim_marks },
  .ttl = { .rno = RNO_TTL, .pad = { 7, 9}, .crd_len = AXIS_TTL_ELEMS,
    .title_len = strlen(TTL_AXIS_TITLE), .title = TTL_AXIS_TITLE, .fill = plot_ttl_marks },
}};

static gboolean not_cached_yet = true;
static vec4 axrect1[RNO_MAX], axrectN[RNO_MAX]; // firs-n-last axis marks

static const vec2 plot_mrk_pad = { 2, 3};

static const float hfontf = 6 * (FONT_HEIGHT_PERCENT / 100.) / (2 * PLOT_FONT_SIZE);
static vec2 char0sz, char0nm;
static int area_size[2];

//

static void plot_set_char_norm(float width, float height) {
  char0nm[0] = char0sz[0] * hfontf * height / width;
  char0nm[1] = char0sz[1] * hfontf;
}

static void plot_init_char_tables(void) {
  static gboolean plot_chars_ready;
  if (!plot_pango_init() || plot_chars_ready) return;;
  plot_chars_ready = true;
  t_plot_char ch = {0};
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (unsigned char i = MIN_ASCII_CCHAR; i < LIM_ASCII_CCHAR; i++) {
    int size[2] = {0};
    ch.tid = plot_pango_text(i, size);
    if (!ch.tid) { plot_chars_ready = false; break; }
    ch.size[0] = size[0]; ch.size[1] = size[1]; char_table[i] = ch;
  }
  char0sz[0] = char_table['0'].size[0];
  char0sz[1] = char_table['0'].size[1];
  plot_set_char_norm(X_RES, Y_RES);
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
  if (res) for (unsigned i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    GLuint *pvao = &(res->vo[i].vao);
    if (*pvao > 0) { glDeleteVertexArrays(1, pvao); *pvao = 0; }
  }
}

static void plot_del_vbo(GLuint *pid) { if (pid && *pid) { glDeleteBuffers(1, pid); *pid = 0; }}

static void plot_del_res_vbo_dbo(t_plot_res *res) {
  if (res) for (unsigned i = 0; i < G_N_ELEMENTS(res->vo); i++) {
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

static inline void plot_glsl_reset(t_plot_res *res, gboolean setup) {
#define PLOT_SET_PROG(prog, glsl) { (prog).vtr = (prog).coord = (prog).colo1 = (prog).colo2 = -1; \
    if (setup) { (prog).vert = (glsl)[GLSL_VERT]; (prog).frag = (glsl)[GLSL_FRAG]; }}
  if (res) {
    PLOT_SET_PROG(res->plot, plot_glsl);
    PLOT_SET_PROG(res->text, text_glsl);
  }
#undef PLOT_SET_PROG
}

static void plot_plane_vert_attr(GLuint vao, GLint coord) {
  if (!vao || (coord < 0)) return;
  glBindVertexArray(vao);
  glEnableVertexAttribArray(coord);
  glVertexAttribPointer(coord, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glBindVertexArray(0);
  glDisableVertexAttribArray(coord);
}

static gboolean plot_res_init(t_plot_res *res, GError **err) {
#define PLOT_FLAT_VERT(obj, typ) { \
  (obj).vbo = plot_aux_vert_init_plane(dim[0], dim[1], typ, plot_plane_vert_attr, (obj).vao, res->plot.coord); \
  (obj).dbo.main = plot_aux_grid_init(dim[0], dim[1], typ); }
#define PLOT_AXIS_VERT(obj, typ, width) { \
  (obj).vbo = plot_aux_vert_init_axis(dim[0], dim[1], typ, width, plot_plane_vert_attr, (obj).vao, res->plot.coord); \
  (obj).dbo.main = plot_aux_grid_init(dim[0], dim[1], typ); }
//
#define PLOT_GET_LOC(id, name, exec, fn, type) { (id) = fn(exec, name); \
  if ((id) < 0) { PLOT_SETERR("Could not bind %s '%s'", type, name); return false; }}
#define PLOT_GET_ATTR(prog, loc, name) { PLOT_GET_LOC((prog).loc, name, (prog).exec, glGetAttribLocation, "attribute"); }
#define PLOT_GET_FORM(prog, loc, name) { PLOT_GET_LOC((prog).loc, name, (prog).exec, glGetUniformLocation, "uniform"); }
//
#define PLOT_BASE_ATTR(prog) { \
  PLOT_GET_ATTR(prog, coord, PLOT_CRD); \
  PLOT_GET_FORM(prog, vtr,   PLOT_VTR); \
  PLOT_GET_FORM(prog, colo1, PLOT_COL1); }
#define PLOT_EXT_ATTR(prog) { PLOT_BASE_ATTR(prog); PLOT_GET_FORM(prog, colo2, PLOT_COL2); }
  if (!res) return false;
  if (!plot_compile_res(res, err)) return false;
  for (unsigned i = 0; i < G_N_ELEMENTS(res->vo); i++) {
    GLuint *pvao = &(res->vo[i].vao);
    glGenVertexArrays(1, pvao);
    if (*pvao) continue;
    WARN("glGenVertexArrays()");
    plot_del_prog_glsl(&res->plot);
    plot_del_prog_glsl(&res->text);
    plot_del_res_vao(res);
    return false;
  }
  // plot related
  PLOT_EXT_ATTR(res->plot);
  if (res->base) {
    { // flat: bottom-o-top
      int dim[2] = { PLANE_XN, PLANE_YN };
      PLOT_FLAT_VERT(res->vo[PO_BOT], PLO_GRID);
      PLOT_AXIS_VERT(res->vo[A1_TIM], PLO_AXIS_XL, TICK);
      PLOT_AXIS_VERT(res->vo[A2_TIM], PLO_AXIS_XR, TICK);
      PLOT_AXIS_VERT(res->vo[A1_TTL], PLO_AXIS_YR, TICK);
      PLOT_AXIS_VERT(res->vo[A2_TTL], PLO_AXIS_YL, TICK);
    }
    { // flat: left-o-right
      int dim[2] = { PLANE_XN, PLANE_ZN };
      PLOT_FLAT_VERT(res->vo[PO_LOR], PLO_GRID);
      PLOT_AXIS_VERT(res->vo[A1_RTT], PLO_AXIS_XL, TICK);
      PLOT_AXIS_VERT(res->vo[A2_RTT], PLO_AXIS_XR, TICK);
    }
    { // flat: far-o-near
      int dim[2] = { PLANE_ZN, PLANE_YN };
      PLOT_FLAT_VERT(res->vo[PO_FON], PLO_GRID);
    }
  } else { // surface
    int dim[2] = { SURF_XN * 2, SURF_YN * 2 };
    PLOT_FLAT_VERT(res->vo[VO_SURF], PLO_SURF);
    res->vo[VO_SURF].dbo.ext = plot_aux_surf_init(dim[0], dim[1], PLO_SURF); // triangles
  }
  // text related
  PLOT_BASE_ATTR(res->text);
  res->vo[VO_TEXT] = plot_pango_vo_init(res->text.coord); // text vao-n-vbo
  return true;
#undef PLOT_FLAT_VERT
#undef PLOT_AXIS_VERT
#undef PLOT_GET_LOC
#undef PLOT_GET_ATTR
#undef PLOT_GET_FORM
#undef PLOT_BASE_ATTR
#undef PLOT_EXT_ATTR
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
      WARN("%s", mesg); LOG("%s", mesg); g_free(mesg);
    }
  }
  { GError *error = NULL; if (!plot_res_init(res, &error)) {
    gtk_gl_area_set_error(area, error); ERROR("init resources"); }}
  if (!gtk_gl_area_get_has_depth_buffer(area)) gtk_gl_area_set_has_depth_buffer(area, true);
  glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  plot_init_char_tables();
  plot_res_ready = true;
}

static void area_free(GtkWidget *self G_GNUC_UNUSED, t_plot_res *res) { plot_res_free(res); }

static void plot_draw_elems(GLuint vao, t_plot_idc buff, GLint vtr,
    const vec4 colo1, GLint loc1, const vec4 colo2, GLint loc2, const mat4 mat, GLenum mode) {
  if (!vao || !buff.id) return;
  glBindVertexArray(vao);
  gboolean off = (mode == GL_TRIANGLES);
  if (off) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(off, off); }
  glUniformMatrix4fv(vtr, 1, GL_FALSE, (float*)mat);
  if (colo1) glUniform4fv(loc1, 1, colo1);
  if (colo2) glUniform4fv(loc2, 1, colo2);
  glBindBuffer(buff.typ, buff.id);
  glDrawElements(mode, buff.count, GL_UNSIGNED_SHORT, 0);
  if (off) { glPolygonOffset(0, 0); glDisable(GL_POLYGON_OFFSET_FILL); }
  glBindBuffer(buff.typ, 0);
  glBindVertexArray(0);
}

static float plot_mark_shift(float chsz, float cw, int len, float pad, float cs) {
  return cw * pad * cs - (1 - cs) * (chsz * len) / 2;
}

static bool plot_draw_text(t_plot_res *res, const char *text, int len,
    const vec4 color, const vec2 crd, const vec2 shift, const vec2 pad, vec4 *rect) {
#define CENTER_TEXT(ptr, a, b, c, d) { float dx = (c) / 2, dy = (d) / 2; \
  (ptr)[0] = (a) - dx; (ptr)[1] = (b) - dy; (ptr)[2] = (a) + dx; (ptr)[3] = (b) + dy; }
#define MARKMAXLEN 64
  if (!(len > 0) || !res || !text) return false;
  GLuint vao = res->vo[VO_TEXT].vao, vbo = res->vo[VO_TEXT].vbo.id, typ = res->vo[VO_TEXT].vbo.typ;
  if (!vao || !vbo || !typ) return false;
  glUniformMatrix4fv(res->text.vtr, 1, GL_FALSE, (float*)scaled);
  if (color) glUniform4fv(res->text.colo1, 1, color);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(vao);
  float cw = char0nm[0], ch = char0nm[1];
  float x = crd[0] + plot_mark_shift(cw, cw, len, pad[0], shift[0]);
  float y = crd[1] + plot_mark_shift(ch, cw, 1,   pad[1], shift[1]);
  if (rect) CENTER_TEXT(*rect, x, y, (len + 1) * cw, ch);
  int cnt = 0;
  for (const char *p = text; *p && (cnt < MARKMAXLEN); p++, cnt++) {
    unsigned tid = char_table[(unsigned char)*p].tid;
    if (tid) plot_pango_drawtex(tid, vbo, typ, x, y, cw, ch);
    x += cw;
  }
  return true;
#undef CENTER_TEXT
#undef MARKMAXLEN
}

static void plot_rtt_marks(t_mark_text mark[], int max) {
  float val = series_datamax / PP_RTT_SCALE;
  float step = (max > 1) ? (val / (max - 1)) : 0;
  val = 0;
  for (int i = 0; i < max; i++) {
    mark[i].len = snprintf(mark[i].text, sizeof(mark[i].text), PP_FMT10(val), val); // BUFFNOLINT
    val += step;
  }
}

static void plot_tim_marks(t_mark_text mark[], int max) {
  int dt = PLOT_TIME_RANGE * 2 / PLANE_YN; if (opts.timeout > 0) dt *= opts.timeout;
  if (!draw_plot_at || (pinger_state.run && !pinger_state.pause)) draw_plot_at = time(NULL) % 3600;
  for (int i = 0, t = draw_plot_at - PLOT_TIME_RANGE; i < max; i++, t += dt) {
    LIMVAL(t, 3600);
    mark[i].len = snprintf(mark[i].text, sizeof(mark[i].text), PP_TIME_FMT, t / 60, t % 60); // BUFFNOLINT
  }
}

static void plot_ttl_marks(t_mark_text mark[], int max) {
  float val = opts.min + 1;
  float step = (max > 1) ? ((tgtat - val) / (max - 1)) : 0;
  for (int i = 0, printed = -1; i < max; i++) {
    if ((i != (max - 1)) && ((int)val == printed)) mark[i].len = 0;
    else {
      printed = val;
      float r = fmodf(val, 1);
      mark[i].len = snprintf(mark[i].text, sizeof(mark[i].text), "%.0f%s", val, r ? ((r < 0.5) ? "-" : "+") : ""); // BUFFNOLINT
    }
    val += step;
  }
}

static inline gboolean rect_overlapped(vec4 curr, vec4 prev) {
  gboolean  over = (glm_min(prev[2], curr[2]) - glm_max(prev[0], curr[0])) > 0;
  if (over) over = (glm_min(prev[3], curr[3]) - glm_max(prev[1], curr[1])) > 0;
  return over;
}


static inline gboolean plot_cache_overlapping(int rno, vec4 curr, vec4 prev, vec4 trec, gboolean first) {
  gboolean   over = rect_overlapped(curr, prev);
  if (!over) over = rect_overlapped(curr, trec);
  if (rno == RNO_TIM) {
    if (!over) over = rect_overlapped(curr, axrect1[RNO_RTT]);
    if (!over) over = rect_overlapped(curr, axrectN[RNO_RTT]);
    if (!over) over = rect_overlapped(curr, axrect1[RNO_TTL]);
    if (!over) over = rect_overlapped(curr, axrectN[RNO_TTL]);
  }
  if (!over) {
    glm_vec4_copy(curr, prev);
    if (first) glm_vec4_copy(curr, axrect1[rno]);
  }
  return over;
}

static void glsl_draw_axis(t_plot_res *res, vec4 color, gboolean name_only, t_plot_axis_params *axis) {
#define PLOT_DRAW_MARK(rect)  plot_draw_text(res, mark[i].text, mark[i].len, \
    color, axis->crd[i], axis->shift, plot_mrk_pad, rect)
#define PLOT_DRAW_TITLE(rect) plot_draw_text(res, axis->title,  axis->title_len, \
    color, axis->at,     axis->shift, axis->pad,    rect)
  if (name_only) { PLOT_DRAW_TITLE(NULL); return; }
  vec4 trec; gboolean cached = axis->overlap.cached;
  if (!PLOT_DRAW_TITLE(cached ? NULL : &trec)) return;
  int n = axis->crd_len;
  t_mark_text mark[n]; axis->fill(mark, n);
  gboolean* over = axis->overlap.over;
  if (cached) for (int i = n - 1; i >= 0; i--) {
    if (over[i]) continue;
    PLOT_DRAW_MARK(NULL);
  } else { // cache overlapping
    int rno = axis->rno; gboolean first = true; vec4 prev = {0};
    for (int i = n - 1; i >= 0; i--) {
      vec4 curr; if (!PLOT_DRAW_MARK(&curr)) continue;
      over[i] = plot_cache_overlapping(rno, curr, prev, trec, first);
      if (first && !over[i]) first = false;
    }
    glm_vec4_copy(prev, axrectN[rno]);
    axis->overlap.cached = true;
  }
#undef PLOT_DRAW_MARK
#undef PLOT_DRAW_TITLE
}

static void glsl_draw_text(t_plot_res *res) {
  glUseProgram(res->text.exec);
  float rgb = opts.darkplot ? text_val.ondark : text_val.onlight;
  vec4 color = {rgb, rgb, rgb, text_alpha};
  gboolean name_only = !is_plelem_enabled(D3_AXIS);
  if (res->base) {
    glsl_draw_axis(res, color, name_only, &plparam.axes.rtt);
    glsl_draw_axis(res, color, name_only, &plparam.axes.ttl);
  } else
    glsl_draw_axis(res, color, name_only, &plparam.axes.tim);
  glUseProgram(0);
}

static void glsl_draw_plot(t_plot_res *res) {
#define AXIS_VO(axis) (plparam.axes.axis.ndx)
#define PO_RTT AXIS_VO(rtt)
#define PO_TIM AXIS_VO(tim)
#define PO_TTL AXIS_VO(ttl)
#define PO_DRAWELEM(pondx, matrix) { PLOT_DRAWELEMS(pondx, main, grid_color, grid_color, matrix, GL_LINES); }
#define PLOT_DRAWELEMS(ndx, dbotype, glcolo1, glcolo2, matrix, gltype) { plot_draw_elems(res->vo[ndx].vao, \
  res->vo[ndx].dbo.dbotype, res->plot.vtr, glcolo1, res->plot.colo1, glcolo2, res->plot.colo2, plot_trans.matrix, gltype); }
  glUseProgram(res->plot.exec);
  if (res->base) {
    if (is_plelem_enabled(D3_BACK)) {
      PO_DRAWELEM(PO_BOT, bot); // flat: bottom-o-top
      PO_DRAWELEM(PO_LOR, lor); // flat: left-o-right
      PO_DRAWELEM(PO_FON, fon); // flat: far-o-near
    }
    if (is_plelem_enabled(D3_AXIS)) {
      PO_DRAWELEM(PO_RTT, lor); // axis: rtt
      PO_DRAWELEM(PO_TIM, bot); // axis: time
      PO_DRAWELEM(PO_TTL, bot); // axis: ttl
    }
  } else if (pinger_state.gotdata) {
    PLOT_DRAWELEMS(VO_SURF,  ext, surf_colo1, surf_colo2, data, GL_TRIANGLES); // surface
    if (is_plelem_enabled(D3_GRID))
      PLOT_DRAWELEMS(VO_SURF, main, grid_color, grid_color, data,   GL_LINES); // gridlines
  }
  glFlush();
  glUseProgram(0);
#undef AXIS_VO
#undef PO_RTT
#undef PO_TIM
#undef PO_TTL
#undef PO_DRAWELEM
#undef PLOT_DRAWELEMS
}

static inline void plot_clear_area(void) { glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

static gboolean area_draw(GtkGLArea *area, GdkGLContext *context, t_plot_res *res) {
#define BASE_PROG_VALID(prog) ((prog).exec && ((prog).vtr >= 0) && ((prog).colo1 >= 0) && ((prog).coord >= 0))
#define EXT_PROG_VALID(prog) (BASE_PROG_VALID(prog) && ((prog).colo2 >= 0))
  if (!GTK_IS_GL_AREA(area) || !GDK_IS_GL_CONTEXT(context) || !res) return false;
  plot_clear_area();
  { int width  = gtk_widget_get_width (GTK_WIDGET(area));
    int height = gtk_widget_get_height(GTK_WIDGET(area));
    if (((width != area_size[0]) || (height != area_size[1])) && (width > 0) && (height > 0))
      plot_set_char_norm(width, height);
  }
  if (BASE_PROG_VALID(res->text)) {
    if (not_cached_yet) { glsl_draw_text(res); plot_clear_area(); if (!res->base) not_cached_yet = false; }
    glsl_draw_text(res);
  }
  if (EXT_PROG_VALID(res->plot)) glsl_draw_plot(res);
  return true;
#undef BASE_PROG_VALID
#undef EXT_PROG_VALID
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

static void plot_form_axis(mat4 src, t_plot_axis_params *param, gboolean ndx, gboolean atmax, gboolean rev) {
#define MIDDLE_POINT(arr) { ((arr)[0][0] + (arr)[n - 1][0]) / 2, ((arr)[0][1] + (arr)[n - 1][1]) / 2 }
  if (!param) return;
  int n = param->crd_len; vec4 *dst = param->crd;
  float at = atmax ? 1 : -1;
  vec4 aux[n]; float x = -1, dx = 2; if (n != 1) dx /= n - 1;
  for (int i = 0; i < n; i++) {
    vec4 crd = { [2] = -1, [3] = 1 }; crd[ndx] = at; crd[!ndx] = rev ? -x : x;
    vec4 axs; glm_mat4_mulv(src, crd, axs); glm_vec4_divs(axs, axs[3], dst[i]);
    vec4 dir; glm_vec4_copy(crd, dir); dir[ndx] *= 2;
    glm_mat4_mulv(src, dir, axs);
    glm_vec4_divs(axs, axs[3], aux[i]);
    x += dx;
  }
  vec2 p1 = MIDDLE_POINT(dst), p2 = MIDDLE_POINT(aux);
  glm_vec2_sub(p2, p1, param->shift);
  glm_vec2_normalize(param->shift);
  glm_vec2_copy(p1, param->at);
  param->overlap.cached = false;
  memset(param->overlap.over, 0, sizeof(param->overlap.over)); // BUFFNOLINT
#undef MIDDLE_POINT
}

static inline void plot_trans_axes(t_plot_plane_at at) {
  plot_form_axis(plot_trans.lor, &plparam.axes.rtt, 1, at.fon != at.lor, true );
  plot_form_axis(plot_trans.bot, &plparam.axes.tim, 1, at.fon != at.bot, false);
  plot_form_axis(plot_trans.bot, &plparam.axes.ttl, 0, at.lor,           false);
  not_cached_yet = true;
}

static void plot_rotate_copy(mat4 projview, mat4 model, int angle, vec3 axis, vec3 *mirror, mat4 dest) {
  mat4 dup; glm_mat4_copy(model, dup);
  if (mirror) angle += 180;
  if (angle) glm_rotate(dup, glm_rad(angle), axis);
  if (mirror) glm_rotate(dup, glm_rad(180), *mirror);
  glm_mul(projview, dup, dest);
}

static inline void plot_trans_planes(mat4 projview, int cube[3], t_plot_plane_at at) {
  plot_rotate_copy(projview, rmat, cube[0], GLM_YUP, at.lor ? NULL : &GLM_ZUP, plot_trans.lor);
  plot_rotate_copy(projview, rmat, cube[1], GLM_XUP, at.fon ? NULL : &GLM_ZUP, plot_trans.fon);
  plot_rotate_copy(projview, rmat, cube[2], GLM_YUP, at.bot ? NULL : &GLM_ZUP, plot_trans.bot);
}

static inline void plottab_on_color_opts(void) {
#define COL4_CMPNSET(dst, mm) { vec4 c = { opts.rcol.mm / 255., opts.gcol.mm / 255., opts.bcol.mm / 255., 1 }; \
    if (((dst)[0] != c[0]) || ((dst)[1] != c[1]) || ((dst)[2] != c[2])) glm_vec4_copy(c, dst); }
  COL4_CMPNSET(surf_colo1, min);
  COL4_CMPNSET(surf_colo2, max);
#undef COL4_CMPNSET
}

static float in_view_dir_value(mat4 R, vec3 axis) {
  vec3 ijk; glm_vec3_rotate_m4(R, axis, ijk);
  vec3 prj; glm_vec3_proj(ijk, GLM_YUP, prj);
  return prj[1];
}

static inline gboolean in_view_direction(mat4 R, vec3 axis) { return in_view_dir_value(R, axis) <= GLM_FLT_EPSILON; }

static inline t_plot_plane_at plot_planes_at(mat4 base) {
  t_plot_plane_at at;
  mat4 R; glm_mat4_copy(base, R);
  at.lor =  in_view_direction(R, GLM_XUP);
  at.fon = !in_view_direction(R, GLM_YUP);
  at.bot =  in_view_direction(R, GLM_ZUP);
  return at;
}

static inline void plot_projview(mat4 matrix) {
  mat4 proj, view;
  glm_perspective(glm_rad(opts.fov), (float)X_RES / Y_RES, PERSPECTIVE_NEAR, PERSPECTIVE_FAR, proj);
  glm_lookat(LOOKAT_EYE, LOOKAT_CENTER, LOOKAT_UP, view);
  glm_mul(proj, view, matrix);
}

static inline void plot_axis_at2ndx(t_plot_plane_at at, t_plot_axes_set *ax) {
  ax->rtt.ndx = at.fon == at.lor ? A1_RTT : A2_RTT;
  ax->tim.ndx = at.fon == at.bot ? A1_TIM : A2_TIM;
  ax->ttl.ndx = at.lor           ? A1_TTL : A2_TTL;
}

static void pl_qrotor(mat4 mat, int q[4]) {
  vec4 quat; glm_quat(quat, glm_rad(q[3]), q[0], q[2], q[1]);
  glm_quat_rotate(mat, quat, mat);
}

static void pl_on_rotation(int delta[4]) {
  mat4 curr; glm_mat4_copy(scaled, curr); pl_qrotor(curr, delta);
  if (opts.rglob) glm_mat4_mul(curr, rmat, rmat);
  else            glm_mat4_mul(rmat, curr, rmat);
}

static void pl_post_rotation(void) {
  t_plot_plane_at at = plot_planes_at(rmat); plparam.at = at;
  mat4 projview; plot_projview(projview);
  glm_mul(projview, rmat, plot_trans.data); // data plane
  plot_trans_planes(projview, plparam.cube, at);
  plot_trans_axes(at);
  plot_axis_at2ndx(at, &plparam.axes);
}

static inline void pl_init_orientation(void) {
  for (int i = 0; i < 3; i++) if (opts.orient[i]) {
    int quat[4] = {0, 0, 0, opts.orient[i]}; quat[i] = 1;
    pl_on_rotation(quat);
  }
  pl_post_rotation();
}

static void plottab_on_opts(unsigned flags) {
  if (flags & PL_PARAM_COLOR) plottab_on_color_opts();
  if (flags & PL_PARAM_AT) { draw_plot_at = 0; plot_aux_reset(&plot_dyna_res.vo[VO_SURF]); }
  if (flags & PL_PARAM_FOV) pl_post_rotation();
}


// pub
//

t_tab* plottab_init(void) {
  plottab.tag = PLOT_TAB_TAG;
  plottab.tip = PLOT_TAB_TIP;
#define PL_INIT_LAYER(widget, descr) { (widget) = plot_init_glarea(descr); \
    g_return_val_if_fail(widget, NULL); layers = g_slist_append(layers, widget); }
  { int units = 1; glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &units);
    if (!units) { WARN("No support for texture lookups in the vertex shader"); return NULL; }}
  { glm_scale(scaled, SCALE3);
    pl_init_orientation();
    plottab_on_opts(PL_PARAM_ALL);
    plot_glsl_reset(&plot_base_res, true);
    plot_glsl_reset(&plot_dyna_res, true); }
  { GSList *layers = NULL;
    PL_INIT_LAYER(plot_base_area, &plot_base_res);
    PL_INIT_LAYER(plot_dyn_area,  &plot_dyna_res);
    if (!drawtab_init(&plottab, CSS_PLOT_BG, layers, NT_ROTOR_NDX)) return NULL;
    g_slist_free(layers); }
  { series_reg_on_scale(plot_base_area); series_min_no(PLOT_TIME_RANGE); }
  return &plottab;
#undef PL_INIT_LAYER
}

void plottab_free(void) { plot_res_free(&plot_dyna_res); plot_res_free(&plot_base_res); plot_aux_free(); }

void plottab_update(void) {
  plot_aux_update(&plot_dyna_res.vo[VO_SURF]);
  if (GTK_IS_WIDGET(plot_dyn_area)) gtk_gl_area_queue_render(GTK_GL_AREA(plot_dyn_area));
}

void plottab_redraw(void) {
  if (GTK_IS_WIDGET(plot_base_area)) gtk_gl_area_queue_render(GTK_GL_AREA(plot_base_area));
  if (GTK_IS_WIDGET(plot_dyn_area))  gtk_gl_area_queue_render(GTK_GL_AREA(plot_dyn_area));
}

void plottab_refresh(gboolean flags) { if (flags) plottab_on_opts(flags); plottab_redraw(); }

inline void plottab_on_trans_opts(int quat[4]) { pl_on_rotation(quat); pl_post_rotation(); }

