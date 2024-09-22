#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION < 4
#error GTK4 is required
#endif
#define MIN_GTK_RUNTIME(major, minor, micro) (!gtk_check_version(major, minor, micro))

#define APPNAME "pingpath"
#define VERSION "0.3.27"

#define X_RES 1024
#define Y_RES 720

#define MAXADDR 10

#define MINTTL  1
#define MAXTTL  30

#define REF_MAX         MAXTTL
#define DNS_QUERY_MAX   MAXTTL
#define DNS_CACHE_MAX   MAXTTL
#define WHOIS_QUERY_MAX MAXTTL
#define WHOIS_CACHE_MAX MAXTTL
#define DEF_LOGMAX      500 // max lines in log-tab

#define MAIN_TIMING_SEC 1
#define AUTOHIDE_IN     2   // popup notifications, in seconds

#define MAXHOSTNAME     63  // in chars: must 63, should 255
#define NET_BUFF_SIZE 4096  // suppose it's enough (dns or whois data is usually 200-300 bytes)
#define BUFF_SIZE     1024
#define PAD_SIZE        48

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
#define RECAP_TEXT        't'
#define RECAP_CSV         'c'
#define RECAP_JSON_NUM    'j'
#define RECAP_JSON_PRETTY 'J'

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

#define MAX_ICONS      5
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

#define PING_TAB_TAG  "Trace"
#define PING_TAB_TIP  "Ping path with stats"
#define LOG_TAB_TAG   "Log"
#define LOG_TAB_TIP   "Auxiliary logs"
#define GRAPH_TAB_TAG "Graph"
#define GRAPH_TAB_TIP "Line graph"
#ifdef WITH_PLOT
#define PLOT_TAB_TAG  "3D"
#define PLOT_TAB_TIP  "3D view"
#endif

#define PP_FONT_FAMILY "monospace"
#define PP_RTT0      1000  // 1msec
#define PP_RTT_SCALE 1000. // usec to msec
#define PP_RTT_FMT_LT10  "%.1f"
#define PP_RTT_FMT_GE10  "%.0f"
#define PP_TIME_FMT "%02d:%02d"
#define PP_FMT_LT10(val) ((val < 1e-3f) ? PP_RTT_FMT_GE10 : PP_RTT_FMT_LT10)
#define PP_FMT10(val) ((val < 10) ? PP_FMT_LT10(val) : PP_RTT_FMT_GE10)
#define PP_MARK_MAXLEN 16

#define MARGIN  8
#define ACT_DOT 4 // beyond of "app." or "win."

// verbose & 1:  log to stdout
// verbose & 2:  common debug
// verbose & 4:  dns
// verbose & 8:  whois
// verbose & 16: config
// verbose & 32: reorder
#define LOGGING 1
#define DEBUGGING 1
#define DNS_DEBUGGING 1
#define WHOIS_DEBUGGING 1
#define CONFIG_DEBUGGING 1
#define DNDORD_DEBUGGING 1

#define LOG(fmt, ...) { VERBOSE(fmt, __VA_ARGS__); log_add("[%s] " fmt, timestampit(), __VA_ARGS__); }
#define LOG_(mesg) { VERBOSE("%s", mesg); log_add("[%s] %s", timestampit(), mesg); }

#ifdef LOGGING
#define VERBOSE(fmt, ...) { if (verbose & 1) g_message(fmt, __VA_ARGS__); }
#else
#define VERBOSE(fmt, ...) {}
#endif

#ifdef DEBUGGING
#define DEBUG(fmt, ...) { if (verbose & 2) g_message(fmt, __VA_ARGS__); }
#else
#define DEBUG(fmt, ...) {}
#endif

#ifdef DNDORD_DEBUGGING
#define DNDORD(fmt, ...) { if (verbose & 32) g_message(fmt, __VA_ARGS__); }
#else
#define DNDORD(fmt, ...) {}
#endif

#define WARN(fmt, ...) g_warning("%s: " fmt, __func__, __VA_ARGS__)
#define WARN_(mesg) g_warning("%s: %s", __func__, mesg)
#define ERROR(what) { if (!error) { WARN("%s: %s", what, unkn_error); } \
  else { WARN("%s: rc=%d, %s", what, error->code, error->message); g_error_free(error); }}
#define ERRLOG(what) { if (!error) { LOG("%s: %s", what, unkn_error); } \
  else { LOG("%s: %s: rc=%d, %s\n", __func__, what, error->code, error->message); g_error_free(error); }}
#define FAIL(what) WARN("%s failed", what)
#define FAILX(what, extra) WARN("%s: %s failed", what, extra)

#define STR_EQ(a, b) (!g_strcmp0(a, b))
#define STR_NEQ(a, b) (g_strcmp0(a, b))
#define CLR_STR(str) { if (str) { g_free(str); str = NULL; }}
#define UPD_STR(str, val) { g_free(str); str = g_strdup(val); }
#define UPD_NSTR(str, val, max) { g_free(str); str = g_strndup(val, max); }
#define SET_SA(desc, ndx, cond) {if (G_IS_SIMPLE_ACTION(desc[ndx].sa)) g_simple_action_set_enabled(desc[ndx].sa, cond);}

#define ACT_TOOLTIP    "Command menu"
#define ACT_START_HDR  "Start"
#define ACT_STOP_HDR   "Stop"
#define ACT_PAUSE_HDR  "Pause"
#define ACT_RESUME_HDR "Resume"
#define ACT_RESET_HDR  "Reset"
#define ACT_HELP_HDR   "Help"
#define ACT_QUIT_HDR   "Exit"
#define ACT_LGND_HDR   "Legend"
#define ACT_COPY_HDR   "Copy"
#define ACT_SALL_HDR   "Select all"
#define ACT_UNSALL_HDR "Unselect all"
#ifdef WITH_PLOT
#define ACT_LEFT_K_HDR  "Rotate Left"
#define ACT_RIGHT_K_HDR "Rotate Right"
#define ACT_UP_K_HDR    "Rotate Up"
#define ACT_DOWN_K_HDR  "Rotate Down"
#define ACT_PGUP_K_HDR  "Rotate Clockwise"
#define ACT_PGDN_K_HDR  "Rotate AntiClockwise"
#define ACT_SCUP_K_HDR  "Scale Up"
#define ACT_SCDN_K_HDR  "Scale Down"
#endif

#define ENT_TARGET_HDR "Target"
#define ENT_TSTAMP_HDR "Timestamp"

#define OPT_MAIN_MENU_TIP "Main options"
#define OPT_CYCLES_HDR   "Cycles"
#define OPT_IVAL_HDR     "Interval"
#define OPT_IVAL_HEADER  OPT_IVAL_HDR ", sec"
#define OPT_DNS_HDR      "DNS"
#define OPT_INFO_HDR     "Hop Info"
#define OPT_STAT_HDR     "Statistics"
#define OPT_TTL_HDR      "TTL"
#define OPT_QOS_HDR      "QoS"
#define OPT_PLOAD_HDR    "Payload"
#define OPT_PLOAD_HEADER OPT_PLOAD_HDR ", hex"
#define OPT_PSIZE_HDR    "Size"
#define OPT_IPV_HDR      "IP Version"
#define OPT_IPVA_HDR     "Auto"
#define OPT_IPV4_HDR     "IPv4"
#define OPT_IPV6_HDR     "IPv6"

#define OPT_EXT_MENU_TIP "Auxiliary"
#define OPT_MN_DARK_HDR  "Main dark theme"
#define OPT_MN_DARK_HEADER "Main theme"
#define OPT_GR_DARK_HDR  "Graph dark theme"
#define OPT_GR_DARK_HEADER "Graph theme"
#ifdef WITH_PLOT
#define OPT_PL_DARK_HDR  "Plot dark theme"
#define OPT_PL_DARK_HEADER "Plot theme"
#endif
#define OPT_GRAPH_HDR    "Graph type"
#define OPT_GR_DOT_HDR   "Dots"
#define OPT_GR_LINE_HDR  "Lines"
#define OPT_GR_CURVE_HDR "Splines"
#define OPT_LGND_HDR     "Graph legend"
#define OPT_GRLG_HDR     "Legend fields"
#define OPT_GREX_HDR     "Extra graph elements"
#ifdef WITH_PLOT
#define OPT_PLOT_HDR     "Plot elements"
#define OPT_PLEX_HDR     "Plot parameters"
#define OPT_GRAD_HDR     "Plot gradient"
#define OPT_GLOB_HDR     "Global space"
#define OPT_ROTOR_HDR    "Plot rotation"
#define OPT_SCALE_HDR    "Plot scale"
#define OPT_FOV_HDR      "Field of view"
#endif

#define OPT_ATAB_HDR     "Active tab at start"
#define OPT_LOGMAX_HDR   "LogTab rows"
#define OPT_RECAP_HDR    "Summary"

#define ELEM_HOST_HDR  "Host"
#define ELEM_HOST_TIP  "Hostname or IP-address"
#define ELEM_AS_HDR    "AS"
#define ELEM_AS_TIP    "Autonomous System"
#define ELEM_CC_HDR    "CC"
#define ELEM_CC_TIP    "Registrant Country Code"
#define ELEM_DESC_HDR  "Description"
#define ELEM_DESC_TIP  "Short description in free form"
#define ELEM_RT_HDR    "Route"
#define ELEM_RT_TIP    "Network prefix"
#define ELEM_LOSS_HDR  "Loss"
#define ELEM_LOSS_TIP  "Loss in percentage"
#define ELEM_LOSS_HEADER ELEM_LOSS_HDR ", %"
#define ELEM_SENT_HDR  "Sent"
#define ELEM_SENT_TIP  "Number of pings sent"
#define ELEM_RECV_HDR  "Recv"
#define ELEM_RECV_TIP  "Number of pongs received"
#define ELEM_LAST_HDR  "Last"
#define ELEM_LAST_TIP  "Last delay in milliseconds"
#define ELEM_BEST_HDR  "Best"
#define ELEM_BEST_TIP  "Best known delay in milliseconds"
#define ELEM_WRST_HDR  "Wrst"
#define ELEM_WRST_HEADER "Worst"
#define ELEM_WRST_TIP  "Worst delay in milliseconds"
#define ELEM_AVRG_HDR  "Avrg"
#define ELEM_AVRG_HEADER "Average"
#define ELEM_AVRG_TIP  "Average delay in milliseconds"
#define ELEM_JTTR_HDR  "Jttr"
#define ELEM_JTTR_HEADER "Jitter"
#define ELEM_JTTR_TIP  "Jitter (variation in delay)"

#define GRLG_DASH_HDR    "Color"
#define GRLG_DASH_HEADER "Color dash"
#define GRLG_AVJT_HDR    "RTT±Jitter"
#define GRLG_AVJT_HEADER "Average RTT ± Jitter"
#define GRLG_CCAS_HDR    "CC:ASN"
#define GRLG_CCAS_HEADER "Country Code : AS Number"
#define GRLG_LGHN_HDR    "Hopname"

#define GREX_MEAN_HDR    "Midline"
#define GREX_MEAN_HEADER "Average line"
#define GREX_JRNG_HDR    "Scopes"
#define GREX_JRNG_HEADER "Jitter range"
#define GREX_AREA_HDR    "JArea"
#define GREX_AREA_HEADER "Jitter area"

#ifdef WITH_PLOT
#define PLEL_BACK_HDR    "Backside"
#define PLEL_AXIS_HDR    "Axis"
#define PLEL_GRID_HDR    "Grid"
#define PLEL_ROTR_HDR    "Rotator"
#endif

#define TOGGLE_ON_HDR  "on"
#define TOGGLE_OFF_HDR "off"

#define CYCLES_MIN 1
#define CYCLES_MAX 999999
#define IVAL_MIN   1
#define IVAL_MAX   9999
#define PSIZE_MIN  16
#define PSIZE_MAX  (9000 - 20 - 8)
#define QOS_MAX    255
#define LOGMAX_MAX 999

#define GRAPH_LEFT   60
#define GRAPH_TOP    50
#define GRAPH_RIGHT  40
#define GRAPH_BOTTOM 40

#define SPN_TTL_DELIM  "range"
#define SPN_RCOL_DELIM "red"
#define SPN_GCOL_DELIM "green"
#define SPN_BCOL_DELIM "blue"

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

typedef struct minmax {
  int min, max;
} t_minmax;
#define MM_OKAY(mm, val) (((mm).min <= val) && (val <= (mm).max))

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

typedef int (*type2ndx_fn)(int);

typedef struct elem_desc {
  t_type_elem *elems;
  const t_minmax mm;
  const int cat;
  const char *patt;
  type2ndx_fn t2n, t2e;
} t_elem_desc;

typedef struct th_color { double ondark, onlight; } t_th_color;

extern const char *appver;
extern const char *unkn_error;
extern const char *unkn_field;
extern const char *unkn_whois;
extern const char *log_empty;

extern gboolean cli;
extern int verbose, activetab;

extern t_type_elem pingelem[PE_MAX];
extern t_type_elem graphelem[GX_MAX];
extern t_elem_desc info_desc, stat_desc, grlg_desc, grex_desc;
#ifdef WITH_PLOT
extern t_type_elem plotelem[D3_MAX];
extern t_elem_desc plot_desc;
#endif

extern const double colors[][3];
extern const int n_colors;

const char* onoff(gboolean on);
char* get_nth_color(int i);

int char2ndx(int cat, gboolean ent, char c);
gboolean*  pingelem_enabler(int type);
gboolean* graphelem_enabler(int type);
int  pingelem_type2ndx(int type);
int graphelem_type2ndx(int type);
gboolean is_grelem_enabled(int type);
void clean_elems(int type);
#ifdef WITH_PLOT
gboolean* plotelem_enabler(int type);
int plotelem_type2ndx(int type);
gboolean is_plelem_enabled(int type);
#endif

char* rtver(int ndx);
const char *timestampit(void);
GtkListBoxRow* line_row_new(GtkWidget *child, gboolean visible);

void host_free(t_host *host);
int host_cmp(const void *a, const void *b);
int ref_cmp(const t_ref *a, const t_ref *b);
t_ref* ref_new(t_hop *hop, int ndx);
void print_refs(GSList *refs, const char *prefix);
GSList* list_add_nodup(GSList **list, void *data, GCompareFunc cmp, int max);
GSList* list_add_ref(GSList **list, t_hop *hop, int ndx);
extern void log_add(const char *fmt, ...);

#define UPDATE_LABEL(label, str) { const char *txt = gtk_label_get_text(GTK_LABEL(label)); \
  if (STR_NEQ(txt, str)) gtk_label_set_text(GTK_LABEL(label), str); }

#define IS_INFO_NDX(ndx) ((PE_HOST <= ndx) && (ndx <= PE_RT))
#define IS_STAT_NDX(ndx) ((PE_LOSS <= ndx) && (ndx <= PE_JTTR))
#define IS_GRLG_NDX(ndx) ((GE_AVJT <= ndx) && (ndx <= GE_LGHN))
#define IS_GREX_NDX(ndx) ((GREX_MEAN <= ndx) && (ndx <= GREX_AREA))

#endif
