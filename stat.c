
#include "stat.h"
#include "pinger.h"
#include "tabs/ping.h"
#include "common.h"

#define MAX_ADDRS 10

#define STR_EQ(a, b) (!g_strcmp0(a, b))
#define STR_NEQ(a, b) (g_strcmp0(a, b))
#define ADDRNAME(addr, name) ((name) ? (name) : ((addr) ? (addr) : UNKN))

// stat formats
#define UNKN    "???"
#define INT_FMT "%d"
#define DBL_FMT "%.*f"
#define DBL_SFX DBL_FMT "%s"
#define ELEM_LEN 5
#define NUM_BUFF_SZ 16

enum { NONE = 0, RX = 1, TX = 2, RXTX = 3 };

typedef struct hop {
  t_host host[MAX_ADDRS]; int pos;
  int sent, recv, last, prev, best, wrst;
  double loss, avrg, jttr;
  t_tseq mark;
  bool reach;
  bool tout; // at last update
  gchar* info;
} t_hop;

int hops_no = MAXTTL;
static t_hop hops[MAXTTL];
static int host_addr_max;
static int host_name_max;

#define ELEM_INFO_HDR "Host"
#define ELEM_LOSS_HDR "Loss"
#define ELEM_SENT_HDR "Sent"
#define ELEM_LAST_HDR "Last"
#define ELEM_BEST_HDR "Best"
#define ELEM_WRST_HDR "Wrst"
#define ELEM_AVRG_HDR "Avrg"
#define ELEM_JTTR_HDR "Jttr"

t_stat_elems stat_elems = { // TODO: editable from option menu
  [ELEM_NO]   = "",
  [ELEM_INFO] = ELEM_INFO_HDR,
  [ELEM_LOSS] = ELEM_LOSS_HDR,
  [ELEM_SENT] = ELEM_SENT_HDR,
  [ELEM_LAST] = ELEM_LAST_HDR,
  [ELEM_BEST] = ELEM_BEST_HDR,
  [ELEM_WRST] = ELEM_WRST_HDR,
  [ELEM_AVRG] = ELEM_AVRG_HDR,
  [ELEM_JTTR] = ELEM_JTTR_HDR,
};


// aux
//

static void update_hmax(const gchar* s, bool addr) {
  int *max = addr ? &host_addr_max : &host_name_max;
  if (s) {
    int l = g_utf8_strlen(s, MAXHOSTNAME);
    if (l > *max) {
      *max = l;
      if (addr && !ping_opts.dns) pingtab_update_width(host_addr_max, ELEM_INFO);
      if (!addr && ping_opts.dns) pingtab_update_width(host_name_max, ELEM_INFO);
    }
  }
  if (addr && (host_name_max < host_addr_max)) host_name_max = host_addr_max;
}

static void update_addrname(int at, t_host *b) {
  // addr is mandatory, name not
  bool done;
  int vacant = -1;
  for (int i = 0; i < MAX_ADDRS; i++) {
    t_host *a = &(hops[at].host[i]);
    if ((vacant < 0) && !(a->addr)) vacant = i;
    done = STR_EQ(a->addr, b->addr);
    if (done) {
      hops[at].pos = i;
      if (STR_NEQ(a->name, b->name)) { // add|update NAME
        UPD_STR(a->name, b->name);
        update_hmax(b->name, false);
      }
      break;
    }
  }
  if (!done) { // add addrname
    if (vacant < 0) { LOG("no free slots for hop#%d address(%s)", at, b->addr); }
    else {
      hops[at].pos = vacant;
      t_host *a = &(hops[at].host[vacant]);
      UPD_STR(a->addr, b->addr);
      update_hmax(b->addr, true);
      UPD_STR(a->name, b->name);
      update_hmax(b->name, false);
    }
  }
  // cleanup dups
  g_free(b->addr);
  g_free(b->name);
}

static void stop_pings_behind(int from, const gchar *reason) {
  for (int i = from; i < MAXTTL; i++) pinger_stop_nth(i, reason);
}

static void set_hops_no(int no, const char *info) {
  hops_no = no;
  stop_pings_behind(no, info);
  pingtab_set_visible(no);
}

static void uniq_unreach(int at) {
  const gchar *addr = hops[at].host[0].addr;
  if (!addr) return;
  int i;
  for (i = at; i > 0; i--) {
    if (hops[i - 1].reach) break;
    if (STR_NEQ(addr, hops[i - 1].host[0].addr)) break;
  }
  if (hops_no != (i + 1)) set_hops_no(i + 1, "unreachable duplicates");
}

static void update_stat(int at, int rtt, t_tseq *mark, int rxtx) {
  if (rxtx & RX) hops[at].recv++;
  if (rxtx & TX) hops[at].sent++;
  if (rxtx && hops[at].sent) hops[at].loss = (hops[at].sent - hops[at].recv) * 100. / hops[at].sent;
  if (rtt >= 0) {
    if ((rtt < hops[at].best) || (hops[at].best < 0)) hops[at].best = rtt;
    if ((rtt > hops[at].wrst) || (hops[at].wrst < 0)) hops[at].wrst = rtt;
    if (hops[at].avrg < 0) hops[at].avrg = 0; // initiate avrg
    hops[at].avrg += (rtt - hops[at].avrg) / hops[at].recv;
    if ((hops[at].prev > 0) && (hops[at].recv > 1)) {
      if (hops[at].jttr < 0) hops[at].jttr = 0; // initiate jttr
      hops[at].jttr += (abs(rtt - hops[at].prev) - hops[at].jttr) / (hops[at].recv - 1);
    }
    hops[at].prev = hops[at].last;
    hops[at].last = rtt;
  }
  if (mark) hops[at].mark = *mark;
  // update timeouted flag
  if (rxtx == TX) hops[at].tout = true;
  else if (rxtx != NONE) hops[at].tout = false;
  // update global gotdata flag
  if (!pinger_state.gotdata && !hops[at].tout) {
    pinger_state.gotdata = true;
    pingtab_set_visible(at + 1);
  }
}

static int calc_rtt(int at, t_tseq *mark) {
  if (!hops[at].mark.sec) return -1;
  int rtt = (mark->sec - hops[at].mark.sec) * 1000000 + (mark->usec - hops[at].mark.usec);
  rtt = (rtt > ping_opts.tout_usec) ? -1 : rtt; // skip timeouted responses
  return rtt;
}

static inline bool seq_accord(int prev, int curr) { return ((curr - prev) == 1); }

static const gchar *stat_host(int at) {
  static gchar stat_host_buff[MAXTTL][BUFF_SIZE];
  t_hop *hop = &(hops[at]);
  t_host *host = hop->host;
  if (host[0].addr) {
    gchar *s = stat_host_buff[at];
    int l = g_snprintf(s, BUFF_SIZE, "%s", ADDRNAME(host[0].addr, host[0].name));
    for (int i = 1; (i < MAX_ADDRS) && host[i].addr; i++)
      if (host[i].addr) g_snprintf(s + l, BUFF_SIZE - l, "\n%s", ADDRNAME(host[i].addr, host[i].name));
    if (hop->info) snprintf(s + l, BUFF_SIZE - l, "\n%s", hop->info);
    return s;
  }
  return NULL;
}

static int prec(double v) { return ((v > 0) && (v < 10)) ? (v < 0.1 ? 2 : 1) : 0; }

static const gchar* fill_stat_int(int val, gchar* buff, int size) {
  if (val >= 0) g_snprintf(buff, size, INT_FMT, val); else buff[0] = 0;
  return buff;
}

static const gchar* fill_stat_dbl(double val, gchar* buff, int size, const char *sfx, int factor) {
  if (val < 0) buff[0] = 0; else {
    if (factor) val /= factor;
    int n = prec(val);
    if (sfx) g_snprintf(buff, size, DBL_SFX, n, val, sfx);
    else g_snprintf(buff, size, DBL_FMT, n, val);
  }
  return buff;
}

static const gchar* fill_stat_rtt(int usec, gchar* buff, int size) {
  if (usec > 0) {
    double val = usec / 1000.;
    g_snprintf(buff, size, DBL_FMT, prec(val), val);
  } else buff[0] = 0;
  return buff;
}

static const gchar *stat_hop(int typ, t_hop *hop) {
  static gchar loss[NUM_BUFF_SZ];
  static gchar sent[NUM_BUFF_SZ];
  static gchar last[NUM_BUFF_SZ];
  static gchar best[NUM_BUFF_SZ];
  static gchar wrst[NUM_BUFF_SZ];
  static gchar avrg[NUM_BUFF_SZ];
  static gchar jttr[NUM_BUFF_SZ];
  switch (typ) {
    case ELEM_LOSS: return fill_stat_dbl(hop->loss, loss, sizeof(loss), "%", 0);
    case ELEM_SENT: return fill_stat_int(hop->sent, sent, sizeof(sent));
    case ELEM_LAST: return fill_stat_rtt(hop->last, last, sizeof(last));
    case ELEM_BEST: return fill_stat_rtt(hop->best, best, sizeof(best));
    case ELEM_WRST: return fill_stat_rtt(hop->wrst, wrst, sizeof(wrst));
    case ELEM_AVRG: return fill_stat_dbl(hop->avrg, avrg, sizeof(avrg), NULL, 1000);
    case ELEM_JTTR: return fill_stat_dbl(hop->jttr, jttr, sizeof(jttr), NULL, 1000);
  }
  return NULL;
}


// pub
//

void stat_init(void) {
  for (int i = 0; i < MAXTTL; i++) {
    hops[i].reach = true;
    hops[i].last = hops[i].best = hops[i].wrst = -1;
    hops[i].avrg = hops[i].jttr = -1; // <0 unknown
  }
  host_addr_max = host_name_max = g_utf8_strlen(ELEM_INFO_HDR, MAXHOSTNAME);
  pinger_state.gotdata = false;
  pinger_state.reachable = false;
}

void stat_free(void) {
  for (int at = 0; at < MAXTTL; at++) {
    for (int i = 0; i < MAX_ADDRS; i++) {
      UPD_STR(hops[at].host[i].addr, NULL);
      UPD_STR(hops[at].host[i].name, NULL);
    }
    UPD_STR(hops[at].info, NULL);
  }
  host_addr_max = host_name_max = g_utf8_strlen(ELEM_INFO_HDR, MAXHOSTNAME);
}

void stat_clear(void) {
  stat_free();
  memset(hops, 0, sizeof(hops));
  stat_init();
  pingtab_set_error(NULL);
}

void stat_set_nopong(const gchar *mesg) {
  bool info = false;
  for (int i = 0; i < hops_no; i++) if (hops[i].info) { info = true; break; }
  if (!info) pingtab_set_error(mesg);
}

void stat_save_success(int at, t_ping_success *data) {
  update_addrname(at, &data->host);
  update_stat(at, data->time, &data->mark, RXTX);
  if (!hops[at].reach) hops[at].reach = true;
  // management
  int ttl = at + 1;
  if (hops_no > ttl) {
    LOG("target is reached at ttl=%d", ttl);
    set_hops_no(ttl, "behind target");
  }
  pinger_free_nth_error(at);
  UPD_STR(hops[at].info, NULL);
  if (!pinger_state.reachable) pinger_state.reachable = true;
}

void stat_save_discard(int at, t_ping_discard *data) {
  update_addrname(at, &data->host);
  seq_accord(hops[at].mark.seq, data->mark.seq)
    ? update_stat(at, calc_rtt(at, &data->mark), &data->mark, RXTX)
    : update_stat(at, -1, NULL, RX);
  if (data->reason) { // 'unreach' management
    int ttl = at + 1;
    bool reach = !g_strrstr(data->reason, "nreachable");
    if (hops[at].reach != reach) hops[at].reach = reach;
    if (reach) { if (hops_no < ttl) set_hops_no(ttl, "unreachable"); }
    else {
      if (hops_no > ttl) uniq_unreach(at);
      UPD_STR(hops[at].info, data->reason);
    }
    g_free(data->reason);
  }
  pinger_free_nth_error(at);
}

void stat_save_timeout(int at, t_ping_timeout *data) {
  update_stat(at, -1, &data->mark, (hops[at].mark.seq == data->mark.seq) ? NONE : TX);
}

void stat_save_info(int at, t_ping_info *data) {
  update_addrname(at, &data->host);
//  update_stat(at, -1, &data->mark, seq_accord(hops[at].mark.seq, data->mark.seq) ? RXTX : NONE);
  UPD_STR(hops[at].info, data->info);
}

void stat_last_tx(int at) { // update last 'tx' unless done before
  if (hops[at].tout) { update_stat(at, -1, NULL, TX); hops[at].tout = false; }
}

const gchar *stat_elem(int at, int typ) {
  switch (typ) {
    case ELEM_INFO: return stat_host(at);
    case ELEM_LOSS:
    case ELEM_SENT:
    case ELEM_LAST:
    case ELEM_BEST:
    case ELEM_WRST:
    case ELEM_AVRG:
    case ELEM_JTTR:
      return stat_hop(typ, &hops[at]);
  }
  return NULL;
}

int stat_elem_max(int typ) {
  return (typ != ELEM_INFO) ? ELEM_LEN : (ping_opts.dns ? host_name_max : host_addr_max);
}

