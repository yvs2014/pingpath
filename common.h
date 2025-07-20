#ifndef COMMON_H
#define COMMON_H

#include <gtk/gtk.h>
#include <locale.h>

#include "text.h"

#if GTK_MAJOR_VERSION < 4
#error GTK4 is required
#endif
#define MIN_GTK_RUNTIME(major, minor, micro) (!gtk_check_version(major, minor, micro))

#define APPNAME "pingpath"
#define VERSION "0.3.70"
#define APPVER  APPNAME "-" VERSION

extern locale_t locale, localeC;

#define REF_MAX         MAXTTL
#define DNS_QUERY_MAX   MAXTTL
#define DNS_CACHE_MAX   MAXTTL
#define WHOIS_QUERY_MAX MAXTTL
#define WHOIS_CACHE_MAX MAXTTL

enum {
  X_RES  = 1024, Y_RES = 720,
  MINTTL = 1,   MAXTTL = 30,
  MAXADDR         = 10,
  DEF_LOGMAX      =  500, // max lines in log-tab
  MAIN_TIMING_SEC =    1,
  AUTOHIDE_IN     =    2, // popup notifications, in seconds
  MAXHOSTNAME     =   63, // in chars: must 63, should 255
  NET_BUFF_SIZE   = 4096, // suppose it's enough (dns or whois data is usually 200-300 bytes)
  BUFF_SIZE       = 1024,
  PAD_SIZE        =   48,
  MAX_ICONS       =    5,
  PP_MARK_MAXLEN  =   16,
  MARGIN          =    8,
  ACT_DOT         =    4, // beyond of "app." or "win."
};

#define UNKN_FIELD "" /* "?" "???" */

#define INFO_PATT    "hacdr"
#define STAT_PATT    "lsrmbwaj"
#define GRLG_PATT    "dch"
#define GREX_PATT    "lra"
#ifdef WITH_PLOT
#define PLEL_PATT    "bagr"
#endif

#ifdef WITH_JSON
#define RECAP_PATT   "tcjJ"
#else
#define RECAP_PATT   "tc"
#endif
enum { RECAP_TEXT = 't', RECAP_CSV = 'c', RECAP_JSON_NUM = 'j', RECAP_JSON_PRETTY = 'J' };

#define EV_ACTIVE     "activate"
#define EV_CLOSEQ     "close-request"
#define EV_TOGGLE     "toggled"
#define EV_PRESS      "pressed"
#define EV_CLICK      "clicked"
#define EV_KEY        "key-released"
#define EV_VAL_CHANGE "value-changed"
#define EV_ROW_CHANGE "selected-rows-changed"
#define EV_ROW_ACTIVE "row-activated"
#define EV_TAB_SWITCH "switch-page"
#define EV_GET_POS    "get-child-position"
#define EV_DND_DRAG   "prepare"
#define EV_DND_ICON   "drag-begin"
#define EV_DND_DROP   "drop"
#define EV_DND_MOVE   "motion"
#ifdef WITH_PLOT
#define EV_PLOT_INIT   "realize"
#define EV_PLOT_FREE   "unrealize"
#define EV_PLOT_DRAW   "render"
#endif

#define ICON_PROP      "icon-name"
#define ACT_MENU_ICON  "open-menu-symbolic"
#define ACT_MENU_ICOA  "view-more-symbolic"
#define OPT_MAIN_MENU_ICON "emblem-system-symbolic"
#define OPT_MAIN_MENU_ICOA "application-x-executable-symbolic"
#define OPT_EXT_MENU_ICON  "media-playlist-consecutive-symbolic"
#define OPT_EXT_MENU_ICOA  "go-next-symbolic"
#define GO_UP_ICON     "go-up-symbolic"
#define GO_DOWN_ICON   "go-down-symbolic"
#define GO_LEFT_ICON   "go-previous-symbolic"
#define GO_RIGHT_ICON  "go-next-symbolic"
#define PING_TAB_ICON  "format-justify-left-symbolic"
#define PING_TAB_ICOA  "view-continuous-symbolic"
#define PING_TAB_ICOB  "view-list-symbolic"
#define GRAPH_TAB_ICON "media-playlist-shuffle-symbolic"
#define GRAPH_TAB_ICOA "utilities-system-monitor-symbolic"
#define GRAPH_TAB_ICOB "media-playlist-repeat-symbolic"
#ifdef WITH_PLOT
#define PLOT_TAB_ICON  "application-x-appliance-symbolic"
#define PLOT_TAB_ICOA  "network-cellular-signal-excellent-symbolic"
#define PLOT_TAB_ICOB  "zoom-original-symbolic"
#define PLOT_TAB_ICOC  "edit-select-all-symbolic"
#define RTR_LEFT_ICON  "go-previous-symbolic"
#define RTR_LEFT_ICOA  "pan-start-symbolic"
#define RTR_LEFT_ICOB  "go-first-symbolic"
#define RTR_RIGHT_ICON "go-next-symbolic"
#define RTR_RIGHT_ICOA "pan-end-symbolic"
#define RTR_RIGHT_ICOB "go-last-symbolic"
#define RTR_UP_ICON    "go-up-symbolic"
#define RTR_UP_ICOA    "pan-up-symbolic"
#define RTR_UP_ICOB    "go-top-symbolic"
#define RTR_DOWN_ICON  "go-down-symbolic"
#define RTR_DOWN_ICOA  "pan-down-symbolic"
#define RTR_DOWN_ICOB  "go-bottom-symbolic"
#define RTR_ROLL_ICON  "view-refresh-symbolic"
#define RTR_ROLL_ICOA  "object-rotate-right-symbolic"
#define RTR_ROLL_ICOB  "system-reboot-symbolic"
#endif
#define LOG_TAB_ICON   "system-search-symbolic"
#define LOG_TAB_ICOA   "edit-find-symbolic"

#define PP_FONT_FAMILY "monospace"
#define PP_RTT0      1000  // 1msec
#define PP_RTT_SCALE 1000. // usec to msec
#define PP_RTT_FMT_LT10  "%.1f"
#define PP_RTT_FMT_GE10  "%.0f"
#define PP_TIME_FMT "%02d:%02d"
#define PP_FMT_LT10(val) (((val) < 1e-3f) ? PP_RTT_FMT_GE10 : PP_RTT_FMT_LT10)
#define PP_FMT10(val) (((val) < 10) ? PP_FMT_LT10(val) : PP_RTT_FMT_GE10)

typedef struct verbose {
  unsigned
    verbose:1,
    debug  :1,
    dns    :1,
    whois  :1,
    config :1,
    dnd    :1;
} t_verbose;
extern t_verbose verbose;

#define NOOP ((void)0)

#define LOG_WITH_STAMP(fmt, ...) do {                           \
  char ts[64];                                                  \
  log_add("[%s] " fmt, timestamp(ts, sizeof(ts)), __VA_ARGS__); \
} while (0)

#define LOG(fmt, ...) do {          \
  VERBOSE(fmt, __VA_ARGS__);        \
  LOG_WITH_STAMP(fmt, __VA_ARGS__); \
} while (0)

#define WARNLOG(fmt, ...) do {      \
  g_warning(fmt, __VA_ARGS__);      \
  LOG_WITH_STAMP(fmt, __VA_ARGS__); \
} while (0)

#define VERB_LEVEL(level, fmt, ...) do {    \
  if (verbose.level) g_message((fmt), __VA_ARGS__); \
} while (0)

#define VERBOSE(fmt, ...)     VERB_LEVEL(verbose, (fmt), __VA_ARGS__)
#define DEBUG(fmt, ...)       VERB_LEVEL(debug,   (fmt), __VA_ARGS__)
#define DNS_DEBUG(fmt, ...)   VERB_LEVEL(dns,     "%s " fmt, DNS_HDR,    __VA_ARGS__)
#define WHOIS_DEBUG(fmt, ...) VERB_LEVEL(whois,   "%s " fmt, WHOIS_HDR,  __VA_ARGS__)
#define CONF_DEBUG(fmt, ...)  VERB_LEVEL(config,  "%s " fmt, CONFIG_HDR, __VA_ARGS__)
#define DNDORD(fmt, ...)      VERB_LEVEL(dnd,     (fmt), __VA_ARGS__)

#if (__GNUC__ >= 8) || (__clang_major__ >= 6) || (__STDC_VERSION__ >= 202311L)
#define WARN(fmt, ...) g_warning("%s: " fmt, __func__ __VA_OPT__(,) __VA_ARGS__)
#else
#define WARN(fmt, ...) g_warning("%s: " fmt, __func__, ##__VA_ARGS__)
#endif

#define ERROR(what) do {                                      \
  if (error) {                                                \
    WARN("%s: rc=%d, %s", what, error->code, error->message); \
    g_error_free(error);                                      \
  } else WARN("%s: %s", what, UNKN_ERR);                      \
} while (0)

#define FAIL(what) do {                        \
  WARN("%s: %s", ERROR_HDR, (what));           \
  LOG_WITH_STAMP("%s: %s", ERROR_HDR, (what)); \
} while (0)

#define FAILX(what, extra) do {                             \
  WARN("%s: %s: %s", ERROR_HDR, (what), (extra));           \
  LOG_WITH_STAMP("%s: %s: %s", ERROR_HDR, (what), (extra)); \
} while (0)

#define STR_EQ(a, b) (!g_strcmp0((a), (b)))
#define STR_NEQ(a, b) g_strcmp0((a), (b))
#define CLR_STR(str) do { if (str) { g_free(str); (str) = NULL; }} while (0)
#define UPD_STR(str, val) do { g_free(str); (str) = g_strdup(val); } while (0)
#define UPD_NSTR(str, val, max) do { g_free(str); (str) = g_strndup(val, max); } while (0)
#define SET_SA(desc, ndx, cond) do { if (G_IS_SIMPLE_ACTION((desc)[ndx].sa)) \
  g_simple_action_set_enabled((desc)[ndx].sa, cond); } while (0)

enum {
  DEF_CYCLES = 100,
  CYCLES_MIN = 1,  CYCLES_MAX = 999999,
  //
  DEF_TOUT   = 1,
  IVAL_MIN   = 1,  IVAL_MAX   = 9999,
  //
  DEF_PSIZE  = 56,
  PSIZE_MIN  = 16, PSIZE_MAX  = (9000 - 20 - 8),
  //
  DEF_QOS      = 0,
  QOS_MAX      = 255,
  //
  GRAPH_LEFT   = 60,
  GRAPH_TOP    = 50,
  GRAPH_RIGHT  = 40,
  GRAPH_BOTTOM = 40,
  //
#ifdef WITH_PLOT
  DEF_RCOL_FROM =  80, DEF_RCOL_TO =  80,
  DEF_GCOL_FROM = 230, DEF_GCOL_TO =  80,
  DEF_BCOL_FROM =  80, DEF_BCOL_TO = 230,
  GL_ANGX = 0, GL_ANGY = -20, GL_ANGZ = 0,
  DEF_ANGSTEP  = 5,
  DEF_FOV      = 45,
#endif
  //
  LOGMAX_MAX   = 999,
};

#define DEF_DNS    true
#define DEF_WHOIS  true
#define DEF_PPAD  "00"

#define DEF_LEGEND     true
#define DEF_DARK_MAIN  true
#define DEF_DARK_GRAPH false

#ifdef WITH_PLOT
#define DEF_DARK_PLOT false
#define DEF_RGLOBAL   true
#endif

enum { GLIB_STRV, GTK_STRV, CAIRO_STRV, PANGO_STRV };

enum { TAB_PING_NDX, TAB_GRAPH_NDX,
#ifdef WITH_PLOT
  TAB_PLOT_NDX,
#endif
  TAB_LOG_NDX, TAB_NDX_MAX };
#define ATAB_MIN TAB_PING_NDX
#ifdef WITH_PLOT
#define ATAB_MAX TAB_PLOT_NDX
#else
#define ATAB_MAX TAB_GRAPH_NDX
#endif

enum { ENT_EXP_INFO, ENT_EXP_STAT, ENT_EXP_LGFL, ENT_EXP_GREX,
#ifdef WITH_PLOT
  ENT_EXP_PLEL,
#endif
};

enum { ENT_BOOL_DNS, ENT_BOOL_HOST, ENT_BOOL_AS, ENT_BOOL_CC, ENT_BOOL_DESC, ENT_BOOL_RT,
  ENT_BOOL_LOSS, ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST, ENT_BOOL_AVRG, ENT_BOOL_JTTR,
  ENT_BOOL_MN_DARK, ENT_BOOL_GR_DARK, ENT_BOOL_LGND,
#ifdef WITH_PLOT
  ENT_BOOL_PL_DARK,
#endif
  ENT_BOOL_AVJT, ENT_BOOL_CCAS, ENT_BOOL_LGHN, ENT_BOOL_MEAN, ENT_BOOL_JRNG, ENT_BOOL_AREA,
#ifdef WITH_PLOT
  ENT_BOOL_PLBK, ENT_BOOL_PLAX, ENT_BOOL_PLGR, ENT_BOOL_PLRR, ENT_BOOL_GLOB,
#endif
};

enum { PE_NO, PE_HOST, PE_AS, PE_CC, PE_DESC, PE_RT, PE_FILL, // PingElement
  PE_LOSS, PE_SENT, PE_RECV, PE_LAST, PE_BEST, PE_WRST, PE_AVRG, PE_JTTR, PE_MAX,
  PX_ADDR, PX_HOST, PX_MAX };

enum { GE_NO, GE_DASH, GE_AVJT, GE_CCAS, GE_LGHN, GX_MEAN, GX_JRNG, GX_AREA, GX_MAX };
#define GE_MIN GE_AVJT
#define GE_MAX (GE_LGHN + 1)

#ifdef WITH_PLOT
enum { D3_BACK, D3_AXIS, D3_GRID, D3_ROTR, D3_MAX };
#endif

enum { INFO_CHAR, STAT_CHAR, GRLG_CHAR, GREX_CHAR,
#ifdef WITH_PLOT
  PLEL_CHAR,
#endif
};

enum { WHOIS_AS_NDX, WHOIS_CC_NDX, WHOIS_DESC_NDX, WHOIS_RT_NDX, WHOIS_NDX_MAX };

enum { POP_MENU_NDX_COPY, POP_MENU_NDX_SALL, POP_MENU_NDX_MAX };

enum { GRAPH_TYPE_NONE, GRAPH_TYPE_DOT, GRAPH_TYPE_LINE, GRAPH_TYPE_CURVE, GRAPH_TYPE_MAX };

typedef struct minmax { int min, max; } t_minmax;
#define MM_OKAY(mm, val) (((mm).min <= (val)) && ((val) <= (mm).max))

typedef struct opts {
  char *target;
  t_minmax range;     // TTL range
  gboolean dns, whois, legend, darktheme, darkgraph;
  int cycles, timeout, qos, size, ipv, graph;
  char pad[PAD_SIZE]; // 16 x "00."
  char recap;         // type of summary at exit
  int tout_usec;      // internal: timeout in usecs
  int logmax;         // internal: max lines in log tab
#ifdef WITH_PLOT
  gboolean plot, darkplot;
  t_minmax rcol, gcol, bcol; // color range
  gboolean rglob;            // rotation space: global (xyz) or local (attitude)
  int orient[3];             // plot orientation
  int angstep;               // rotation step
  int fov;                   // field of view
#endif
} t_opts;
extern t_opts opts;

typedef struct host {
  char *addr, *name;
} t_host;

typedef struct whois {
  char* elem[WHOIS_NDX_MAX];
} t_whois;

typedef struct whoaddr {
  char *addr;
  t_whois whois;
} t_whoaddr;

typedef struct tseq {
  int seq;
  long long sec;
  int usec;
} t_tseq;

typedef struct t_rseq {
  int rtt, seq;
} t_rseq;

typedef struct hop {
  int sent, recv, last, best, wrst;
  double loss, avrg, jttr;
  // Note: 'jttr' used here is the average distance between consecutive points
  //       which depicts the range in which RTT varies
  t_host host[MAXADDR];
  gboolean cached, cached_nl;
  t_whois whois[MAXADDR];
  gboolean wcached[WHOIS_NDX_MAX], wcached_nl[WHOIS_NDX_MAX];
  // internal
  int at, prev, known_rtts, known_jttrs;
  gboolean reach, tout;
  t_tseq mark;
  t_rseq rseq[2];
  char* info;
} t_hop;

typedef struct ref { t_hop *hop; int ndx; } t_ref;

typedef struct t_type_elem {
  int type;
  gboolean enable;
  char *name, *tip;
} t_type_elem;

typedef struct t_legend { const char *name, *as, *cc, *av, *jt; } t_legend;

typedef unsigned (*type2ndx_fn)(int);
typedef int (*type2ent_fn)(int);

typedef struct elem_desc {
  t_type_elem *elems;
  const t_minmax mm;
  const int cat;
  const char *patt;
  type2ndx_fn t2n;
  type2ent_fn t2e;
} t_elem_desc;

typedef struct th_color { double ondark, onlight; } t_th_color;

extern gboolean cli;
extern int activetab;

extern t_type_elem pingelem[PE_MAX];
extern t_type_elem graphelem[GX_MAX];
extern t_elem_desc info_desc, stat_desc, grlg_desc, grex_desc;
#ifdef WITH_PLOT
extern t_type_elem plotelem[D3_MAX];
extern t_elem_desc plot_desc;
#endif

typedef struct ping_column { const char* cell[MAXADDR]; } t_ping_column;

extern const double colors[][3];
extern const int n_colors;
char* get_nth_color(int nth);

void init_elem_links(void);

int char2ndx(int cat, gboolean ent, char ch);
gboolean*  pingelem_enabler(int type);
gboolean* graphelem_enabler(int type);
unsigned  pingelem_type2ndx(int type);
unsigned graphelem_type2ndx(int type);
gboolean is_grelem_enabled(int type);
void clean_elems(int type);
#ifdef WITH_PLOT
gboolean* plotelem_enabler(int type);
unsigned plotelem_type2ndx(int type);
gboolean is_plelem_enabled(int type);
#endif

char* rtver(int ndx);
const char *timestamp(char *ts, size_t size);
const char *mnemo(const char *str);
GtkListBoxRow* line_row_new(GtkWidget *child, gboolean visible);

void host_free(t_host *host);
int host_cmp(const void *a, const void *b);
int ref_cmp(const t_ref *a, const t_ref *b);
t_ref* ref_new(t_hop *hop, int ndx);
#if defined(DNS_EXTRA_DEBUG) || defined(WHOIS_EXTRA_DEBUG)
void print_refs(GSList *refs, const char *prefix);
#endif
GSList* list_add_nodup(GSList **list, void *data, GCompareFunc cmp, unsigned max);
GSList* list_add_ref(GSList **list, t_hop *hop, int ndx);
extern void log_add(const char *fmt, ...);

#define UPDATE_LABEL(label, str) { const char *txt = gtk_label_get_text(GTK_LABEL(label)); \
  if (STR_NEQ(txt, str)) gtk_label_set_text(GTK_LABEL(label), str); }

#define IS_INFO_NDX(ndx) ((PE_HOST <= (ndx)) && ((ndx) <= PE_RT))
#define IS_STAT_NDX(ndx) ((PE_LOSS <= (ndx)) && ((ndx) <= PE_JTTR))
#define IS_GRLG_NDX(ndx) ((GE_AVJT <= (ndx)) && ((ndx) <= GE_LGHN))
#define IS_GREX_NDX(ndx) ((GREX_MEAN <= (ndx)) && ((ndx) <= GREX_AREA))

#endif
