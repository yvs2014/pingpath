#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION < 4
#error GTK4 is required
#endif

#define APPNAME "pingpath"
#define VERSION "0.2.2"

#define X_RES 1024
#define Y_RES 720

#define MAXTTL  30
#define MAXADDR 10

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
#define EV_DESTROY    "destroy"
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

#define MAX_ICONS      4
#define ICON_PROP      "icon-name"
#define ACT_MENU_ICON  "open-menu-symbolic"
#define ACT_MENU_ICOA  "view-more-symbolic"
#define OPT_PING_MENU_ICON  "emblem-system-symbolic"
#define OPT_PING_MENU_ICOA  "application-x-executable-symbolic"
#define OPT_GRAPH_MENU_ICON "media-playlist-consecutive-symbolic"
#define OPT_GRAPH_MENU_ICOA "go-next-symbolic"
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
#define LOG_TAB_ICON   "system-search-symbolic"
#define LOG_TAB_ICOA   "edit-find-symbolic"

#define PING_TAB_TAG  "Trace"
#define PING_TAB_TIP  "Ping path with stats"
#define LOG_TAB_TAG   "Log"
#define LOG_TAB_TIP   "Auxiliary logs"
#define GRAPH_TAB_TAG "Graph"
#define GRAPH_TAB_TIP "Line graph"

#define MARGIN  8
#define ACT_DOT 4 // beyond of "app." or "win."

// verbose & 1:  log to stdout
// verbose & 2:  common debug
// verbose & 4:  dns
// verbose & 8:  whois
// verbose & 16: config
#define LOGGING 1
#define DEBUGGING 1
#define DNS_DEBUGGING 1
#define WHOIS_DEBUGGING 1
#define CONFIG_DEBUGGING 1

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

#define ENT_TARGET_HDR "Target"
#define ENT_TSTAMP_HDR "Timestamp"

#define OPT_MAIN_TOOLTIP "Main options"
#define OPT_CYCLES_HDR   "Cycles"
#define OPT_IVAL_HDR     "Interval"
#define OPT_IVAL_HEADER  OPT_IVAL_HDR ", sec"
#define OPT_DNS_HDR      "DNS"
#define OPT_INFO_HDR     "Hop"
#define OPT_INFO_HEADER  "Hop Info"
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

#define OPT_AUX_TOOLTIP  "Auxiliary"
#define OPT_MN_DARK_HDR  "Main dark theme"
#define OPT_MN_DARK_HEADER "Main theme"
#define OPT_GR_DARK_HDR  "Graph dark theme"
#define OPT_GR_DARK_HEADER "Graph theme"
#define OPT_GRAPH_HDR    "Graph type"
#define OPT_GR_NONE_HDR  "None"
#define OPT_GR_DOT_HDR   "Dots"
#define OPT_GR_LINE_HDR  "Lines"
#define OPT_GR_CURVE_HDR "Splines"
#define OPT_LGND_HDR     "Graph legend"
#define OPT_GRLG_HDR     "Legend fields"
#define OPT_GREX_HDR     "Extra graph elements"

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
#define ELEM_JTTR_TIP  "Ping jitter (variation in delay)"

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

// graph related
#define CELL_SIZE    50
#define GRAPH_LEFT   60
#define GRAPH_TOP    50
#define GRAPH_RIGHT  40
#define GRAPH_BOTTOM 40

enum { GLIB_STRV, GTK_STRV, CAIRO_STRV, PANGO_STRV };

enum { TAB_PING_NDX, TAB_GRAPH_NDX, TAB_LOG_NDX, TAB_NDX_MAX };

enum { ENT_EXP_NONE, ENT_EXP_INFO, ENT_EXP_STAT, ENT_EXP_LGFL, ENT_EXP_GREX, ENT_EXP_MAX };

enum { ENT_BOOL_NONE, ENT_BOOL_DNS, ENT_BOOL_HOST, ENT_BOOL_AS, ENT_BOOL_CC, ENT_BOOL_DESC, ENT_BOOL_RT,
  ENT_BOOL_LOSS, ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST, ENT_BOOL_AVRG, ENT_BOOL_JTTR,
  ENT_BOOL_MN_DARK, ENT_BOOL_GR_DARK, ENT_BOOL_LGND, ENT_BOOL_AVJT, ENT_BOOL_CCAS, ENT_BOOL_LGHN,
  ENT_BOOL_MEAN, ENT_BOOL_JRNG, ENT_BOOL_AREA, ENT_BOOL_MAX };

enum { ELEM_NO, ELEM_HOST, ELEM_AS, ELEM_CC, ELEM_DESC, ELEM_RT, ELEM_FILL,
  ELEM_LOSS, ELEM_SENT, ELEM_RECV, ELEM_LAST, ELEM_BEST, ELEM_WRST, ELEM_AVRG, ELEM_JTTR, ELEM_MAX,
  EX_ELEM_ADDR, EX_ELEM_HOST, EX_ELEM_MAX };

enum { GRLG_NO, GRLG_DASH, GRLG_AVJT, GRLG_CCAS, GRLG_LGHN, GRLG_MAX, GREL_MEAN, GREL_JRNG, GREL_AREA, GREL_MAX };

enum { WHOIS_AS_NDX, WHOIS_CC_NDX, WHOIS_DESC_NDX, WHOIS_RT_NDX, WHOIS_NDX_MAX };

enum { POP_MENU_NDX_COPY, POP_MENU_NDX_SALL, POP_MENU_NDX_MAX };

enum { GRAPH_TYPE_NONE, GRAPH_TYPE_DOT, GRAPH_TYPE_LINE, GRAPH_TYPE_CURVE, GRAPH_TYPE_MAX };

typedef struct minmax {
  int min, max;
} t_minmax;

typedef struct host {
  gchar *addr, *name;
} t_host;

typedef struct whois {
  gchar* elem[WHOIS_NDX_MAX];
} t_whois;

typedef struct whoaddr {
  gchar *addr;
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
  gchar* info;
} t_hop;

typedef struct ref { t_hop *hop; int ndx; } t_ref;

typedef struct t_act_desc {
  GSimpleAction* sa;
  const char *name;
  const char *const *shortcut;
  const gboolean invisible;
} t_act_desc;

typedef struct tab_widget { // widget and its dark/light css
  GtkWidget *w; const char *css, *col;
} t_tab_widget;

typedef struct tab {
  struct tab *self;
  const char *name;
  t_tab_widget tab, lab, dyn, hdr, info;
  const char *tag, *tip, *ico[MAX_ICONS];
  GMenu *menu;       // menu template
  GtkWidget *pop;    // popover menu
  gboolean sel;      // flag of selection
  t_act_desc desc[POP_MENU_NDX_MAX];
  GActionEntry act[POP_MENU_NDX_MAX];
} t_tab;

typedef struct t_stat_elem {
  gboolean enable;
  gchar *name, *tip;
} t_stat_elem;

typedef struct t_legend {
  const gchar *name, *as, *cc, *av, *jt;
} t_legend;

extern const char *appver;
extern const char *unkn_error;
extern const char *unkn_field;
extern const char *unkn_whois;
extern const char *log_empty;

extern gboolean cli;
extern gint verbose, start_page;

extern t_tab* nb_tabs[TAB_NDX_MAX];
extern const double colors[][3];
extern const int n_colors;
extern t_stat_elem graphelem[GREL_MAX];

gchar* get_nth_color(int i);

char* rtver(int ndx);
const char *timestampit(void);
GtkListBoxRow* line_row_new(GtkWidget *child, gboolean visible);
void tab_setup(t_tab *tab);
void tab_color(t_tab *tab);
void tab_reload_theme(void);

void host_free(t_host *host);
int host_cmp(const void *a, const void *b);
int ref_cmp(const t_ref *a, const t_ref *b);
t_ref* ref_new(t_hop *hop, int ndx);
void print_refs(GSList *refs, const gchar *prefix);
GSList* list_add_nodup(GSList **list, gpointer data, GCompareFunc cmp, int max);
GSList* list_add_ref(GSList **list, t_hop *hop, int ndx);
extern void log_add(const gchar *fmt, ...);

#define UPDATE_LABEL(label, str) { const gchar *txt = gtk_label_get_text(GTK_LABEL(label)); \
  if (STR_NEQ(txt, str)) gtk_label_set_text(GTK_LABEL(label), str); }

#define TW_TW(tw, widget, perm, dyn) { (tw).w = widget; (tw).css = perm; (tw).col = dyn; }

#endif
