#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION < 4
#error GTK4 is required
#endif

#define APPNAME "pingpath"
#define VERSION "0.1.53"

#define MAXTTL  30
#define MAXADDR 10

#define REF_MAX         MAXTTL
#define DNS_QUERY_MAX   MAXTTL
#define DNS_CACHE_MAX   MAXTTL
#define WHOIS_QUERY_MAX MAXTTL
#define WHOIS_CACHE_MAX MAXTTL
#define DEF_LOGMAX      500 // max lines in log-tab

#define MAXHOSTNAME     63  // in chars: must 63, should 255
#define NET_BUFF_SIZE 4096  // suppose it's enough (dns or whois data is usually 200-300 bytes)
#define BUFF_SIZE     1024
#define PAD_SIZE        48

#define INFO_PATT    "hacdr"
#define STAT_PATT    "lsrmbwaj"

#define AUTOHIDE_IN 2000    // popup notifications, in milliseconds

#define EV_ACTIVE     "activate"
#define EV_TOGGLE     "toggled"
#define EV_CLICK      "pressed"
#define EV_VAL_CHANGE "value-changed"
#define EV_ROW_CHANGE "selected-rows-changed"
#define EV_TAB_SWITCH "switch-page"

#define ICON_PROP      "icon-name"
#define ACT_MENU_ICON  "open-menu-symbolic"
#define OPT_MENU_ICON  "document-properties-symbolic"
#define GO_UP_ICON     "go-up-symbolic"
#define GO_DOWN_ICON   "go-down-symbolic"
#define GO_LEFT_ICON   "go-previous-symbolic"
#define GO_RIGHT_ICON  "go-next-symbolic"
#define PING_TAB_ICON  "format-justify-left-symbolic"
#define LOG_TAB_ICON   "weather-clear-symbolic"
#define GRAPH_TAB_ICON "media-playlist-shuffle-symbolic"

#define PING_TAB_TAG  "Trace"
#define PING_TAB_TIP  "Ping path with stats"
#define LOG_TAB_TAG   "Log"
#define LOG_TAB_TIP   "Auxiliary logs"
#define GRAPH_TAB_TAG "Graph"
#define GRAPH_TAB_TIP "Line graph"

#define MARGIN  8
#define ACT_DOT 4 // beyond of "app." or "win."

// verbose > 0: log to stdout
// verbose & 2: common debug
// verbose & 4: dns debug
// verbose & 8: whois debug
#define LOGGING 1
#define DEBUGGING 1
#define DNS_DEBUGGING 1
#define WHOIS_DEBUGGING 1

#define LOG(fmt, ...) { GLIB_PR(fmt, __VA_ARGS__); log_add("[%s] " fmt, timestampit(), __VA_ARGS__); }
#define LOG_(mesg) { GLIB_PR("%s", mesg); log_add("[%s] %s", timestampit(), mesg); }

#define NOLOG(fmt, ...) GLIB_PR(fmt, __VA_ARGS__)
#define CLILOG(mesg) g_message("%s", mesg)

#ifdef LOGGING
#define GLIB_PR(fmt, ...) { if (verbose) g_message(fmt, __VA_ARGS__); }
#else
#define GLIB_PR(fmt, ...) {}
#endif

#ifdef DEBUGGING
#define DEBUG(fmt, ...) { if (verbose & 2) LOG(fmt, __VA_ARGS__); }
#else
#define DEBUG(fmt, ...) {}
#endif

#define WARN(fmt, ...) g_warning("%s: " fmt "\n", __func__, __VA_ARGS__)
#define WARN_(mesg) g_warning("%s: %s\n", __func__, mesg)
#define ERROR(what) { if (!error) { g_warning("%s: %s: %s\n", __func__, what, unkn_error); } \
  else { g_warning("%s: %s: rc=%d, %s\n", __func__, what, error->code, error->message); g_error_free(error); }}
#define ERRLOG(what) { if (!error) { LOG("%s: %s", what, unkn_error); } \
  else { LOG("%s: %s: rc=%d, %s\n", __func__, what, error->code, error->message); g_error_free(error); }}

#define STR_EQ(a, b) (!g_strcmp0(a, b))
#define STR_NEQ(a, b) (g_strcmp0(a, b))
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
#define ACT_COPY_HDR   "Copy"
#define ACT_SALL_HDR   "Select all"
#define ACT_UNSALL_HDR "Unselect all"

#define ENT_TARGET_HDR "Target"

#define OPT_TOOLTIP    "Customize options"
#define OPT_CYCLES_HDR "Cycles"
#define OPT_IVAL_HDR   "Interval"
#define OPT_IVAL_HDRL  OPT_IVAL_HDR ", sec"
#define OPT_DNS_HDR    "DNS"
#define OPT_INFO_HDR   "Hop Info"
#define OPT_STAT_HDR   "Statistics"
#define OPT_TTL_HDR    "TTL"
#define OPT_QOS_HDR    "QoS"
#define OPT_PLOAD_HDR  "Payload"
#define OPT_PLOAD_HDRL OPT_PLOAD_HDR ", hex"
#define OPT_PSIZE_HDR  "Size"
#define OPT_IPV_HDR    "IP Version"
#define OPT_IPVA_HDR   "Auto"
#define OPT_IPV4_HDR   "IPv4"
#define OPT_IPV6_HDR   "IPv6"
#define OPT_LOGMAX_HDR "Log lines"
#define OPT_GRAPH_HDR  "Graph"

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
#define ELEM_LOSS_HDRL ELEM_LOSS_HDR ", %"
#define ELEM_SENT_HDR  "Sent"
#define ELEM_SENT_TIP  "Number of pings sent"
#define ELEM_RECV_HDR  "Recv"
#define ELEM_RECV_TIP  "Number of pongs received"
#define ELEM_LAST_HDR  "Last"
#define ELEM_LAST_TIP  "Last delay in milliseconds"
#define ELEM_BEST_HDR  "Best"
#define ELEM_BEST_TIP  "Best known delay in milliseconds"
#define ELEM_WRST_HDR  "Wrst"
#define ELEM_WRST_HDRL "Worst"
#define ELEM_WRST_TIP  "Worst delay in milliseconds"
#define ELEM_AVRG_HDR  "Avrg"
#define ELEM_AVRG_HDRL "Average"
#define ELEM_AVRG_TIP  "Average delay in milliseconds"
#define ELEM_JTTR_HDR  "Jttr"
#define ELEM_JTTR_HDRL "Jitter"
#define ELEM_JTTR_TIP  "Ping jitter (variation in delay)"

#define TOGGLE_ON_HDR  "on"
#define TOGGLE_OFF_HDR "off"

#define MAIN_TIMING_MSEC 1000

#define CYCLES_MIN 1
#define CYCLES_MAX 999999
#define IVAL_MIN   1
#define IVAL_MAX   9999
#define PSIZE_MIN  16
#define PSIZE_MAX  (9000 - 20 - 8)
#define QOS_MAX    255
#define LOGMAX_MAX 999

enum { TAB_PING_NDX, TAB_GRAPH_NDX, TAB_LOG_NDX, TAB_NDX_MAX };

enum { ENT_EXP_NONE, ENT_EXP_INFO, ENT_EXP_STAT, ENT_EXP_MAX };

enum { ENT_BOOL_NONE, ENT_BOOL_DNS, ENT_BOOL_HOST, ENT_BOOL_AS, ENT_BOOL_CC, ENT_BOOL_DESC, ENT_BOOL_RT,
  ENT_BOOL_LOSS, ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST,
  ENT_BOOL_AVRG, ENT_BOOL_JTTR, ENT_BOOL_MAX };

enum { ELEM_NO, ELEM_HOST, ELEM_AS, ELEM_CC, ELEM_DESC, ELEM_RT, ELEM_FILL,
  ELEM_LOSS, ELEM_SENT, ELEM_RECV, ELEM_LAST, ELEM_BEST, ELEM_WRST, ELEM_AVRG, ELEM_JTTR, ELEM_MAX };

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

typedef struct hop {
  t_host host[MAXADDR]; gboolean cached;
  t_whois whois[MAXADDR]; gboolean wcached[WHOIS_NDX_MAX];
  int sent, recv, last, prev, best, wrst;
  double loss, avrg, jttr;
  t_tseq mark;
  gboolean reach;
  gboolean tout; // flag of timeouted seq
  gchar* info;
  int at;        // useful back reference
} t_hop;

typedef struct ref { t_hop *hop; int ndx; } t_ref;

typedef struct t_act_desc {
  GSimpleAction* sa;
  const char *name;
  const char *const *shortcut;
} t_act_desc;

typedef struct tab {
  struct tab *self;
  const char *name;
  GtkWidget *tab, *lab, *dyn, *hdr, *info;
  const char *ico, *tag, *tip;
  GMenu *menu;       // menu template
  GtkWidget *pop;    // popover menu
  gboolean sel;      // flag of selection
  t_act_desc desc[POP_MENU_NDX_MAX];
  GActionEntry act[POP_MENU_NDX_MAX];
} t_tab;

extern const char *appver;
extern const char *unkn_error;
extern const char *unkn_field;
extern const char *unkn_whois;
extern const char *log_empty;

extern gboolean cli, bg_light;
extern gint verbose, start_page;

const char *timestampit(void);
GtkListBoxRow* line_row_new(GtkWidget *child, gboolean visible);
void tab_setup(t_tab *tab, const char *css);

void host_free(t_host *host);
int host_cmp(const void *a, const void *b);
int ref_cmp(const t_ref *a, const t_ref *b);
t_ref* ref_new(t_hop *hop, int ndx);
void print_refs(GSList *refs, const gchar *prefix);
GSList* list_add_nodup(GSList **list, gpointer data, GCompareFunc cmp, int max);
extern void log_add(const gchar *fmt, ...);

#define ADD_REF_OR_RET(refs) { \
  if (!list_add_nodup(refs, ref_new(hop, ndx), (GCompareFunc)ref_cmp, REF_MAX)) { \
    WARN("%s: add reference failed", addr); return NULL; }}

#endif
