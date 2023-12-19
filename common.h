#ifndef COMMON_H
#define COMMON_H

#include <gtk/gtk.h>

#define APPNAME "pingpath"
#define VERSION "0.1.34"

#define MAXTTL  30
#define MAXADDR 10

#define REF_MAX         MAXTTL
#define DNS_QUERY_MAX   MAXTTL
#define DNS_CACHE_MAX   MAXTTL
#define WHOIS_QUERY_MAX MAXTTL
#define WHOIS_CACHE_MAX MAXTTL

#define MAXHOSTNAME     63 // in chars: must 63, should 255
#define NET_BUFF_SIZE 4096 // suppose it's enough (dns or whois data is usually 200-300 bytes)
#define BUFF_SIZE     1024
#define PAD_SIZE        48

#define EV_ACTIVE     "activate"
#define EV_TOGGLE     "toggled"
#define EV_VAL_CHANGE "value-changed"
#define EV_TAB_SWITCH "switch-page"

#define ICON_PROP     "icon-name"
#define ACT_MENU_ICON "open-menu-symbolic"
#define OPT_MENU_ICON "document-properties-symbolic"
#define GO_UP_ICON    "go-up-symbolic"
#define GO_DOWN_ICON  "go-down-symbolic"
#define GO_LEFT_ICON  "go-previous-symbolic"
#define GO_RIGHT_ICON "go-next-symbolic"
#define PING_TAB_ICON "format-justify-left-symbolic"
#define LOG_TAB_ICON  "weather-clear-symbolic"

#define PING_TAB_TAG "Trace"
#define LOG_TAB_TAG  "Log"

#define MARGIN 8

#define LOGGING 1
//#define DEBUGGING 1
//#define DNS_DEBUGGING 1
//#define WHOIS_DEBUGGING 1

#ifdef LOGGING
#define LOG(fmt, ...) g_print("[%s] " fmt "\n", timestampit(), __VA_ARGS__)
extern const char *log_empty;
#else
#define LOG(fmt, ...) {}
#endif

#ifdef DEBUGGING
#define DEBUG(fmt, ...) LOG(fmt, __VA_ARGS__)
#else
#define DEBUG(fmt, ...) {}
#endif

#define WARN(fmt, ...) g_warning("%s: " fmt "\n", __func__, __VA_ARGS__)
#define ERROR(at) { g_warning("%s: %s: %s\n", __func__, at, error ? error->message : unkn_error); g_error_free(error); }
#define ERRLOG(what) { LOG("%s: %s", what, error ? error->message : unkn_error); g_error_free(error); }

#define STR_EQ(a, b) (!g_strcmp0(a, b))
#define STR_NEQ(a, b) (g_strcmp0(a, b))
#define UPD_STR(str, val) { g_free(str); str = g_strdup(val); }
#define UPD_NSTR(str, val, max) { g_free(str); str = g_strndup(val, max); }

#define ACT_START_HDR  "Start"
#define ACT_STOP_HDR   "Stop"
#define ACT_PAUSE_HDR  "Pause"
#define ACT_RESUME_HDR "Resume"
#define ACT_RESET_HDR  "Reset"
#define ACT_HELP_HDR   "Help"
#define ACT_QUIT_HDR   "Exit"

#define OPT_CYCLES_HDR "Cycles"
#define OPT_IVAL_HDR   "Interval"
#define OPT_IVAL_HDRL  OPT_IVAL_HDR ", sec"
#define OPT_DNS_HDR    "DNS"
#define OPT_INFO_HDR   "Hop Info"
#define OPT_STAT_HDR   "Statistics"
#define OPT_TTL_HDR    "TTL"
#define OPT_QOS_HDR    "QoS"
#define OPT_PLOAD_HDR  "Payload"
#define OPT_PLOAD_HDRL OPT_IVAL_HDR ", hex"
#define OPT_PSIZE_HDR  "Size"
#define OPT_IPV_HDR    "IP Version"
#define OPT_IPVA_HDR   "Auto"
#define OPT_IPV4_HDR   "IPv4"
#define OPT_IPV6_HDR   "IPv6"

#define ELEM_HOST_HDR  "Host"
#define ELEM_AS_HDR    "AS"
#define ELEM_CC_HDR    "CC"
#define ELEM_DESC_HDR  "Description"
#define ELEM_RT_HDR    "Route"
#define ELEM_LOSS_HDR  "Loss"
#define ELEM_LOSS_HDRL ELEM_LOSS_HDR ", %"
#define ELEM_SENT_HDR  "Sent"
#define ELEM_RECV_HDR  "Recv"
#define ELEM_LAST_HDR  "Last"
#define ELEM_BEST_HDR  "Best"
#define ELEM_WRST_HDR  "Wrst"
#define ELEM_WRST_HDRL "Worst"
#define ELEM_AVRG_HDR  "Avrg"
#define ELEM_AVRG_HDRL "Average"
#define ELEM_JTTR_HDR  "Jttr"
#define ELEM_JTTR_HDRL "Jitter"

enum { TAB_PING_NDX, TAB_LOG_NDX, TAB_NDX_MAX };

enum { ENT_BOOL_NONE, ENT_BOOL_DNS, ENT_BOOL_HOST, ENT_BOOL_AS, ENT_BOOL_CC, ENT_BOOL_DESC, ENT_BOOL_RT,
  ENT_BOOL_LOSS, ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST,
  ENT_BOOL_AVRG, ENT_BOOL_JTTR, ENT_BOOL_MAX };

enum { ELEM_NO, ELEM_HOST, ELEM_AS, ELEM_CC, ELEM_DESC, ELEM_RT, ELEM_FILL,
  ELEM_LOSS, ELEM_SENT, ELEM_RECV, ELEM_LAST, ELEM_BEST, ELEM_WRST, ELEM_AVRG, ELEM_JTTR, ELEM_MAX };

enum { WHOIS_AS_NDX, WHOIS_CC_NDX, WHOIS_DESC_NDX, WHOIS_RT_NDX, WHOIS_NDX_MAX };

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
  t_host host[MAXADDR]; bool cached;
  t_whois whois[MAXADDR]; bool wcached[WHOIS_NDX_MAX];
  int sent, recv, last, prev, best, wrst;
  double loss, avrg, jttr;
  t_tseq mark;
  bool reach;
  bool tout; // flag of timeouted seq
  gchar* info;
  int at;    // for DEBUG
} t_hop;

typedef struct ref { t_hop *hop; int ndx; } t_ref;
typedef struct tab { GtkWidget *tab, *lab, *dyn, *hdr; const char *ico, *tag; } t_tab;

extern const char *appver;
extern const char *unkn_error;
extern const char *unkn_field;
extern const char *unkn_whois;

const char *timestampit(void);
GtkListBoxRow* line_row_new(GtkWidget *child, bool visible);
void tab_setup(t_tab *tab);

void host_free(t_host *host);
int host_cmp(const void *a, const void *b);
int ref_cmp(const t_ref *a, const t_ref *b);
t_ref* ref_new(t_hop *hop, int ndx);
void print_refs(GSList *refs, const gchar *prefix);
GSList* list_add_nodup(GSList **list, gpointer data, GCompareFunc cmp, int max);

#define ADD_REF_OR_RET(refs) { \
  if (!list_add_nodup(refs, ref_new(hop, ndx), (GCompareFunc)ref_cmp, REF_MAX)) { \
    WARN("%s: add reference failed", addr); return NULL; }}

#endif
