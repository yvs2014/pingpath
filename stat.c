
#include <stdlib.h>

#include "common.h"

#include "stat.h"
#include "pinger.h"
#include "dns.h"
#include "whois.h"

// stat formats
#define INT_FMT "%d"
#define DBL_FMT "%.*f"
#define DBL_SFX DBL_FMT "%s"

enum { ELEM_LEN = 5, NUM_BUFF_SZ = 16 };
enum { NONE = 0, RX = 1, TX = 2, RXTX = 3 };

int tgtat = MAXTTL;
int visibles = -1;

#define IS_WHOIS_NDX(ndx) ((PE_AS <= (ndx)) && ((ndx) <= PE_RT))

static t_hop hops[MAXTTL];

static void update_addrname(int at, t_host *b) { // addr is mandatory, name not
  if (!b) return;
  gboolean done = false;
  int vacant = -1;
  t_hop *hop = &hops[at];
  for (int i = 0; i < MAXADDR; i++) {
    t_host *a = &hop->host[i];
    if ((vacant < 0) && !(a->addr)) vacant = i;
    done = STR_EQ(a->addr, b->addr);
    if (done) {
      if (!a->name) {
        if (b->name) { // add first resolved hostname to not change it back-n-forth
          UPD_STR(a->name, b->name);
          hops[at].cached = hops[at].cached_nl = false;
          LOG("set hostname[%d]: %s", at, b->name);
        } else if (opts.dns) dns_lookup(hop, i); // otherwise run dns lookup
      }
      break;
    }
  }
  if (!done) { // add addrname
    if (vacant < 0) LOG("no free slots for hop#%d address(%s)", at, b->addr);
    else {
      t_host *a = &hop->host[vacant];
      UPD_STR(a->addr, b->addr);
      UPD_STR(a->name, b->name);
      hops[at].cached = hops[at].cached_nl = false;
      for (int j = 0; j < WHOIS_NDX_MAX; j++) {
        CLR_STR(hop->whois[vacant].elem[j]);
        hops[at].wcached[j] = hops[at].wcached_nl[j] = false;
      }
      LOG("set addrname[%d]: %s %s", at, b->addr, b->name ? b->name : "");
      if (!b->name && opts.dns)
        dns_lookup(hop, vacant);   // make dns query
      if (opts.whois)
        whois_resolv(hop, vacant); // make whois query
    }
  }
  // cleanup dups
  g_free(b->addr);
  g_free(b->name);
}

static void stop_pings_behind(int from, const char *reason) {
  for (int i = from; i < MAXTTL; i++) pinger_nth_stop(i, reason);
}

static void set_target_at(int at, const char *info) {
  tgtat = at;
  stop_pings_behind(at, info);
  pinger_vis_rows(at);
}

static void uniq_unreach(int at) {
  const char *addr = hops[at].host[0].addr;
  if (!addr) return;
  int last = at;
  for (; last > 0; last--) {
    if (hops[last - 1].reach) break;
    if (STR_NEQ(addr, hops[last - 1].host[0].addr)) break;
  }
  if (tgtat != (last + 1)) set_target_at(last + 1, "unreachable duplicates");
}

#define STAT_AVERAGE(count, avrg, val) { (count)++; \
  if ((count) > 0) { if ((avrg) < 0) (avrg) = 0; (avrg) += ((val) - (avrg)) / (count); }}

enum { PREV, CURR };

static void update_stat(int at, int rtt, t_tseq *mark, int rxtx) {
  if (rxtx & RX) hops[at].recv++;
  if (rxtx & TX) hops[at].sent++;
  if (rxtx && hops[at].sent) hops[at].loss = (hops[at].sent - hops[at].recv) * 100. / hops[at].sent;
  if (rtt > 0) {
    if ((rtt < hops[at].best) || (hops[at].best < 0)) hops[at].best = rtt;
    if ((rtt > hops[at].wrst) || (hops[at].wrst < 0)) hops[at].wrst = rtt;
    STAT_AVERAGE(hops[at].known_rtts, hops[at].avrg, rtt);
    if (hops[at].prev > 0) STAT_AVERAGE(hops[at].known_jttrs, hops[at].jttr, abs(rtt - hops[at].prev));
    hops[at].prev = hops[at].last;
    hops[at].last = rtt;
  }
  if (mark) {
    hops[at].mark = *mark;
    if (mark->seq != hops[at].rseq[CURR].seq) {
      hops[at].rseq[PREV] = hops[at].rseq[CURR];
      hops[at].rseq[CURR].rtt = rtt; hops[at].rseq[CURR].seq = mark->seq;
    }
  }
  if (!pinger_state.gotdata) pinger_state.gotdata = true;
  { if (rxtx == TX) hops[at].tout = true; else if (rxtx != NONE) hops[at].tout = false; }
  if (!hops[at].tout && (visibles < at) && (at < tgtat)) { visibles = at; pinger_vis_rows(at + 1); }
}

static int calc_rtt(int at, t_tseq *mark) {
  if (!hops[at].mark.sec) return -1;
  int rtt = (mark->sec - hops[at].mark.sec) * G_USEC_PER_SEC + (mark->usec - hops[at].mark.usec);
  rtt = (rtt > opts.tout_usec) ? -1 : rtt; // skip timeouted responses
  return rtt;
}

// Note: name[0] is shortcut for "" test instead of STR_NEQ(name, unkn_field)
#define ADDRNAME(addr, name) (((name) && (name)[0]) ? (name) : ADDRONLY(addr))
#define ADDRONLY(addr) ((addr) ? (addr) : unkn_field)
static inline const char* addrname(int ndx, t_host *host) {
  return opts.dns ? ADDRNAME(host[ndx].addr, host[ndx].name) : ADDRONLY(host[ndx].addr);
}
static inline const char* addr_or_name(int ndx, t_host *host, gboolean num) {
  return num ? host[ndx].addr : (STR_EQ(host[ndx].addr, host[ndx].name) ? NULL : host[ndx].name);
}

static const char *info_host(int at) {
  static char hostinfo_cache[MAXTTL][BUFF_SIZE];
  t_hop *hop = &hops[at];
  t_host *host = hop->host;
  if (host[0].addr) { // as a marker
    if (hop->cached) return hostinfo_cache[at];
    char *str = hostinfo_cache[at];
    int len = g_snprintf(str, BUFF_SIZE, "%s", addrname(0, host));
    for (int i = 1; (i < MAXADDR) && (len < BUFF_SIZE); i++) {
      if (host[i].addr) len += g_snprintf(str + len, BUFF_SIZE - len, "\n%s", addrname(i, host));
      else break;
    }
    if (hop->info && (len < BUFF_SIZE)) g_snprintf(str + len, BUFF_SIZE - len, "\n%s", hop->info);
    hop->cached = true;
    LOG("hostinfo cache[%d] updated with %s", at, (str && str[0]) ? str : log_empty);
    return str;
  }
  return NULL;
}

static const char *info_host_nl(int at) {
  static char hostinfo_nl_cache[MAXTTL][MAXHOSTNAME];
  t_hop *hop = &hops[at];
  t_host *host = hop->host;
  if (host[0].addr) { // as a marker
    if (hop->cached_nl) return hostinfo_nl_cache[at];
    char *str = hostinfo_nl_cache[at];
    g_snprintf(str, BUFF_SIZE, "%s", addrname(0, host));
    hop->cached_nl = true;
    LOG("hostinfo_nl cache[%d] updated with %s", at, (str && str[0]) ? str : log_empty);
    return str;
  }
  return NULL;
}

static int column_host(int at, t_ping_column *column) {
  t_host *host = hops[at].host; int nth = 0;
  if (column) for (; nth < MAXADDR; nth++) {
    if (host[nth].addr) column->cell[nth] = addrname(nth, host); else break; }
  return nth;
}

static int column_addrhost(int at, t_ping_column* column, gboolean num) {
  t_host *host = hops[at].host; int nth = 0;
  if (column) for (; nth < MAXADDR; nth++) {
    if (host[nth].addr) column->cell[nth] = addr_or_name(nth, host, num); else break; }
  return nth;
}

static const char *info_whois(int at, int type) {
  static char whois_cache[MAXTTL][WHOIS_NDX_MAX][BUFF_SIZE];
  t_whois *whois = hops[at].whois;
  char *elem = whois[0].elem[type];
  if (elem) { // as a marker
    char *str = whois_cache[at][type];
    if (!hops[at].wcached[type]) {
      size_t len = g_snprintf(str, BUFF_SIZE, "%s", elem);
      for (int i = 1; (i < MAXADDR) && (len < BUFF_SIZE); i++) {
        elem = whois[i].elem[type];
        if (!elem) break;
        len += g_snprintf(str + len, BUFF_SIZE - len, "\n%s", elem);
      }
      hops[at].wcached[type] = true;
      LOG("whois cache[%d,%d] updated with %s", at, type, (str && str[0]) ? str : log_empty);
    }
    return str;
  }
  return NULL;
}

static const char *info_whois_nl(int at, int type) {
  static char whois_nl_cache[MAXTTL][WHOIS_NDX_MAX][MAXHOSTNAME];
  t_whois *whois = hops[at].whois;
  char *elem = whois[0].elem[type];
  if (elem) { // as a marker
    char *str = whois_nl_cache[at][type];
    if (!hops[at].wcached_nl[type]) {
      g_snprintf(str, MAXHOSTNAME, "%s", elem);
      hops[at].wcached_nl[type] = true;
      LOG("whois_nl cache[%d,%d] updated with %s", at, type, (str && str[0]) ? str : log_empty);
    }
    return str;
  }
  return NULL;
}

static int column_whois(int at, int type, t_ping_column *column) {
  t_whois *whois = hops[at].whois; int nth = 0;
  if (column) for (; nth < MAXADDR; nth++) {
    char *str = whois[nth].elem[type];
    if (str) column->cell[nth] = str; else break;
  }
  return nth;
}

static int prec(double val) { return ((val > 0) && (val < 10)) ? ((val < 0.1) ? 2 : 1) : 0; }

static const char* fill_stat_int(int val, char* buff, int size) {
  if (val >= 0) g_snprintf(buff, size, INT_FMT, val); else buff[0] = 0;
  return buff;
}

static const char* fill_stat_dbl(double val, char* buff, int size, const char *sfx, int factor) {
  if (val < 0) buff[0] = 0; else {
    if (factor) val /= factor;
    int dec = prec(val);
    if (sfx) g_snprintf(buff, size, DBL_SFX, dec, val, sfx);
    else g_snprintf(buff, size, DBL_FMT, dec, val);
  }
  return buff;
}

static const char* fill_stat_rtt(int usec, char* buff, int size) {
  if (usec > 0) {
    double val = usec / 1000.;
    g_snprintf(buff, size, DBL_FMT, prec(val), val);
  } else buff[0] = 0;
  return buff;
}

static const char *stat_hop(int type, t_hop *hop) {
  static char buff_loss[NUM_BUFF_SZ];
  static char buff_sent[NUM_BUFF_SZ];
  static char buff_recv[NUM_BUFF_SZ];
  static char buff_last[NUM_BUFF_SZ];
  static char buff_best[NUM_BUFF_SZ];
  static char buff_wrst[NUM_BUFF_SZ];
  static char buff_avrg[NUM_BUFF_SZ];
  static char buff_jttr[NUM_BUFF_SZ];
  switch (type) {
    case PE_LOSS: return fill_stat_dbl(hop->loss, buff_loss, sizeof(buff_loss), "%", 0);
    case PE_SENT: return fill_stat_int(hop->sent, buff_sent, sizeof(buff_sent));
    case PE_RECV: return fill_stat_int(hop->recv, buff_recv, sizeof(buff_recv));
    case PE_LAST: return fill_stat_rtt(hop->last, buff_last, sizeof(buff_last));
    case PE_BEST: return fill_stat_rtt(hop->best, buff_best, sizeof(buff_best));
    case PE_WRST: return fill_stat_rtt(hop->wrst, buff_wrst, sizeof(buff_wrst));
    case PE_AVRG: return fill_stat_dbl(hop->avrg, buff_avrg, sizeof(buff_avrg), NULL, 1000);
    case PE_JTTR: return fill_stat_dbl(hop->jttr, buff_jttr, sizeof(buff_jttr), NULL, 1000);
    default: break;
  }
  return NULL;
}

static void stat_nth_hop_NA(t_hop *hop) { // NA(<0)
  hop->last = hop->best = hop->wrst = hop->prev = -1;
  hop->loss = hop->avrg = hop->jttr = -1;
  hop->rseq[PREV].rtt = hop->rseq[CURR].rtt = -1;
  hop->rseq[PREV].seq = hop->rseq[CURR].seq = 0;
}


// pub
//

void stat_init(gboolean clean) { // clean start or on reset
  if (clean) {
    tgtat = MAXTTL;
    visibles = -1;
    pinger_state.gotdata = false;
    pinger_state.reachable = false;
    memset(hops, 0, sizeof(hops)); // BUFFNOLINT
  }
  for (int i = 0; i < MAXTTL; i++) {
    if (clean) hops[i].reach = true;
    stat_nth_hop_NA(&hops[i]);
    hops[i].at = i;
  }
  stat_whois_enabler();
}

void stat_free(void) {
  for (int at = 0; at < MAXTTL; at++) { // clear all except of mark and tout
    t_hop *hop = &hops[at];
    // clear statdata
    hop->sent = hop->recv = hop->known_rtts = hop->known_jttrs = 0;
    stat_nth_hop_NA(hop);
    // clear strings
    for (int i = 0; i < MAXADDR; i++) {
      host_free(&hop->host[i]);
      for (int j = 0; j < WHOIS_NDX_MAX; j++) CLR_STR(hops[at].whois[i].elem[j]);
    }
    CLR_STR(hop->info);
  }
}

void stat_clear(gboolean clean) { stat_free(); stat_init(clean); }

void stat_reset_cache(void) { for (int i = 0; i < MAXTTL; i++) hops[i].cached = hops[i].cached_nl = false; }

static void stat_up_info(int at, const char *info) {
  if (STR_EQ(hops[at].info, info)) return;
  UPD_STR(hops[at].info, info);
  hops[at].cached = false;
  LOG("set info[%d]: %s", at, info);
}

void stat_save_success(int at, t_ping_success *data) {
  update_addrname(at, &data->host);
  update_stat(at, data->time, &data->mark, RXTX);
  if (!hops[at].reach) hops[at].reach = true;
  // management
  int ttl = at + 1;
  if (tgtat > ttl) {
    LOG("target is reached at ttl=%d", ttl);
    set_target_at(ttl, "behind target");
  }
  pinger_nth_free_error(at);
  if (hops[at].info) stat_up_info(at, NULL);
  if (!pinger_state.reachable) pinger_state.reachable = true;
}

void stat_save_discard(int at, t_ping_discard *data) {
  update_addrname(at, &data->host);
  if ((data->mark.seq - hops[at].mark.seq) != 1) update_stat(at, -1, NULL, RX);
  else update_stat(at, calc_rtt(at, &data->mark), &data->mark, RXTX);
  // 'unreach' management
  if (data->reason) {
    int ttl = at + 1;
    gboolean reach = !g_strrstr(data->reason, "nreachable");
    if (hops[at].reach != reach) hops[at].reach = reach;
    if (reach) { if (tgtat < ttl) set_target_at(ttl, "unreachable"); }
    else {
      if (tgtat > ttl) uniq_unreach(at);
      stat_up_info(at, data->reason);
    }
    g_free(data->reason);
  }
  pinger_nth_free_error(at);
}

void stat_save_timeout(int at, t_ping_timeout *data) {
  update_stat(at, -1, &data->mark, (hops[at].mark.seq == data->mark.seq) ? NONE : TX);
}

void stat_save_info(int at, t_ping_info *data) {
  update_addrname(at, &data->host);
//  update_stat(at, -1, &data->mark, seq_accord(hops[at].mark.seq, data->mark.seq) ? RXTX : NONE);
  stat_up_info(at, data->info);
}

void stat_last_tx(int at) { // update last 'tx' unless done before
  if (hops[at].tout) { update_stat(at, -1, NULL, TX); hops[at].tout = false; }
}

#define IW_RET(elem, ndx) { return (elem) ? info_whois(at, elem, ndx) : NULL; }

const char *stat_str_elem(int at, int type) {
  switch (type) {
    case PE_HOST: return info_host(at);
    case PE_AS:   return info_whois(at, WHOIS_AS_NDX);
    case PE_CC:   return info_whois(at, WHOIS_CC_NDX);
    case PE_DESC: return info_whois(at, WHOIS_DESC_NDX);
    case PE_RT:   return info_whois(at, WHOIS_RT_NDX);
    case PE_LOSS:
    case PE_SENT:
    case PE_RECV:
    case PE_LAST:
    case PE_BEST:
    case PE_WRST:
    case PE_AVRG:
    case PE_JTTR:
      return stat_hop(type, &hops[at]);
    default: break;
  }
  return NULL;
}

static const char *stat_strnl_elem(int at, int type) {
  switch (type) {
    case PE_HOST: return info_host_nl(at);
    case PE_AS:   return info_whois_nl(at, WHOIS_AS_NDX);
    case PE_CC:   return info_whois_nl(at, WHOIS_CC_NDX);
    case PE_DESC: return info_whois_nl(at, WHOIS_DESC_NDX);
    case PE_RT:   return info_whois_nl(at, WHOIS_RT_NDX);
    default: break;
  }
  return NULL;
}

int stat_ping_column(int at, int type, t_ping_column *column) {
  int ndx = 0;
  if (column)  {
    memset(column, 0, sizeof(*column)); // BUFFNOLINT
    switch (type) {
      case PE_HOST: ndx = column_host(at, column);                  break;
      case PE_AS:   ndx = column_whois(at, WHOIS_AS_NDX, column);   break;
      case PE_CC:   ndx = column_whois(at, WHOIS_CC_NDX, column);   break;
      case PE_DESC: ndx = column_whois(at, WHOIS_DESC_NDX, column); break;
      case PE_RT:   ndx = column_whois(at, WHOIS_RT_NDX, column);   break;
      case PX_ADDR: ndx = column_addrhost(at, column, true);        break;
      case PX_HOST: ndx = column_addrhost(at, column, false);       break;
      default: break;
    }
  }
  return ndx;
}

double stat_dbl_elem(int at, int type) {
  double val = -1;
  switch (type) {
    case PE_LOSS: val = hops[at].loss; break;
    case PE_SENT: val = hops[at].sent; break;
    case PE_RECV: val = hops[at].recv; break;
    case PE_LAST: val = hops[at].last; break;
    case PE_BEST: val = hops[at].best; break;
    case PE_WRST: val = hops[at].wrst; break;
    case PE_AVRG: val = hops[at].avrg; break;
    case PE_JTTR: val = hops[at].jttr; break;
    default: break;
  }
  return val;
}

int stat_int_elem(int at, int type) {
  int val = -1;
  switch (type) {
    case PE_SENT: val = hops[at].sent; break;
    case PE_RECV: val = hops[at].recv; break;
    case PE_LAST: val = hops[at].last; break;
    case PE_BEST: val = hops[at].best; break;
    case PE_WRST: val = hops[at].wrst; break;
    default: break;
  }
  return val;
}


#define RSEQ_FOR_SYNC(which) { data->rtt = hops[at].rseq[which].rtt; data->seq = -hops[at].rseq[which].seq; }

void stat_rseq(int at, t_rseq *data) { // assert('at' within range)
  if (data) {
    if (!data->seq || (data->seq == hops[at].rseq[CURR].seq)) *data = hops[at].rseq[CURR];
    else if (data->seq == hops[at].rseq[PREV].seq) *data = hops[at].rseq[PREV];
    else { /* for sync */
      if (data->seq > hops[at].rseq[CURR].seq) RSEQ_FOR_SYNC(CURR) // rarely, but possible
      else RSEQ_FOR_SYNC(PREV); // unlikely with iputils-ping
   }
  }
}

void stat_legend(int at, t_legend *data) {
  if (!data) return;
  data->name = stat_strnl_elem(at, PE_HOST);
  data->as = stat_strnl_elem(at, PE_AS);
  data->cc = stat_strnl_elem(at, PE_CC);
  data->av = stat_str_elem(at, PE_AVRG);
  data->jt = stat_str_elem(at, PE_JTTR);
}

void stat_whois_enabler(void) {
  gboolean enable = false;
  for (unsigned i = 0; i < G_N_ELEMENTS(pingelem); i++)
    if (IS_WHOIS_NDX(pingelem[i].type) && pingelem[i].enable) { enable = true; break; }
  if (enable != opts.whois) { opts.whois = enable; LOG("whois %sabled", enable ? "en" : "dis"); }
}

void stat_run_whois_resolv(void) {
  for (int at = opts.range.min; at < opts.range.max; at++) {
    t_hop *hop = &hops[at];
    for (int i = 0; i < MAXADDR; i++)
      if (hop->host[i].addr) whois_resolv(hop, i);
  }
}

