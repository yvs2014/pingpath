
#include "stat.h"
#include "pinger.h"
#include "dns.h"
#include "whois.h"

// stat formats
#define INT_FMT "%d"
#define DBL_FMT "%.*f"
#define DBL_SFX DBL_FMT "%s"
#define ELEM_LEN 5
#define NUM_BUFF_SZ 16

enum { NONE = 0, RX = 1, TX = 2, RXTX = 3 };

int hops_no = MAXTTL;
int visibles = -1;

#define IS_WHOIS_NDX(ndx) ((ELEM_AS <= ndx) && (ndx <= ELEM_RT))

static t_hop hops[MAXTTL];
static t_host_max host_max;
static int whois_max[WHOIS_NDX_MAX];
static const int wndx2endx[WHOIS_NDX_MAX] = {
  [WHOIS_AS_NDX] = ELEM_AS, [WHOIS_CC_NDX] = ELEM_CC, [WHOIS_DESC_NDX] = ELEM_DESC, [WHOIS_RT_NDX] = ELEM_RT };

static void update_hmax(const gchar* addr, const gchar *name) {
  int la = addr ? g_utf8_strlen(addr, MAXHOSTNAME) : 0;
  int ln = name ? g_utf8_strlen(name, MAXHOSTNAME) : 0;
  if (addr) {
    stat_check_hostaddr_max(la);
    stat_check_hostname_max(name ? ln : la);
  } else if (name)
    stat_check_hostname_max(ln);
}

static void update_addrname(int at, t_host *b) { // addr is mandatory, name not
  if (!b) return;
  gboolean done;
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
          update_hmax(NULL, b->name);
          hops[at].cached = hops[at].cached_nl = false;
          LOG("set hostname[%d]: %s", at, b->name);
        } else if (opts.dns) dns_lookup(hop, i); // otherwise run dns lookup
      }
      break;
    }
  }
  if (!done) { // add addrname
    if (vacant < 0) { LOG("no free slots for hop#%d address(%s)", at, b->addr); }
    else {
      t_host *a = &hop->host[vacant];
      UPD_STR(a->addr, b->addr);
      UPD_STR(a->name, b->name);
      update_hmax(b->addr, b->name);
      hops[at].cached = hops[at].cached_nl = false;
      for (int j = 0; j < WHOIS_NDX_MAX; j++) {
        CLR_STR(hop->whois[vacant].elem[j]);
        hops[at].wcached[j] = hops[at].wcached_nl[j] = false;
      }
      LOG("set addrname[%d]: %s %s", at, b->addr, b->name ? b->name : "");
      if (!b->name && opts.dns) dns_lookup(hop, vacant); // run dns lookup
      if (opts.whois) whois_resolv(hop, vacant);         // run whois resolv
    }
  }
  // cleanup dups
  g_free(b->addr);
  g_free(b->name);
}

static void stop_pings_behind(int from, const gchar *reason) {
  for (int i = from; i < MAXTTL; i++) pinger_nth_stop(i, reason);
}

static void set_hops_no(int no, const char *info) {
  hops_no = no;
  stop_pings_behind(no, info);
  pinger_vis_rows(no);
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

#define STAT_AVERAGE(count, avrg, val) { count++; \
  if (count > 0) { if (avrg < 0) avrg = 0; avrg += (val - avrg) / count; }}

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
  if (!hops[at].tout && (visibles < at) && (at < hops_no)) { visibles = at; pinger_vis_rows(at + 1); }
}

static int calc_rtt(int at, t_tseq *mark) {
  if (!hops[at].mark.sec) return -1;
  int rtt = (mark->sec - hops[at].mark.sec) * G_USEC_PER_SEC + (mark->usec - hops[at].mark.usec);
  rtt = (rtt > opts.tout_usec) ? -1 : rtt; // skip timeouted responses
  return rtt;
}

// Note: name[0] is shortcut for "" test instead of STR_NEQ(name, unkn_field)
#define ADDRNAME(addr, name) ((name && name[0]) ? (name) : ADDRONLY(addr))
#define ADDRONLY(addr) ((addr) ? (addr) : unkn_field)
static inline const gchar* addrname(int ndx, t_host *host) {
  return opts.dns ? ADDRNAME(host[ndx].addr, host[ndx].name) : ADDRONLY(host[ndx].addr);
}
static inline const gchar* addr_or_name(int ndx, t_host *host, gboolean num) {
  return num ? host[ndx].addr : (STR_EQ(host[ndx].addr, host[ndx].name) ? NULL : host[ndx].name);
}

static const gchar *info_host(int at) {
  static gchar hostinfo_cache[MAXTTL][BUFF_SIZE];
  t_hop *hop = &hops[at];
  t_host *host = hop->host;
  if (host[0].addr) { // as a marker
    if (hop->cached) return hostinfo_cache[at];
    gchar *s = hostinfo_cache[at];
    int l = g_snprintf(s, BUFF_SIZE, "%s", addrname(0, host));
    for (int i = 1; (i < MAXADDR) && (l < BUFF_SIZE); i++) {
      if (host[i].addr) l += g_snprintf(s + l, BUFF_SIZE - l, "\n%s", addrname(i, host));
      else break;
    }
    if (hop->info && (l < BUFF_SIZE)) l += g_snprintf(s + l, BUFF_SIZE - l, "\n%s", hop->info);
    hop->cached = true;
    LOG("hostinfo cache[%d] updated with %s", at, (s && s[0]) ? s : log_empty);
    return s;
  }
  return NULL;
}

static const gchar *info_host_nl(int at) {
  static gchar hostinfo_nl_cache[MAXTTL][MAXHOSTNAME];
  t_hop *hop = &hops[at];
  t_host *host = hop->host;
  if (host[0].addr) { // as a marker
    if (hop->cached_nl) return hostinfo_nl_cache[at];
    gchar *s = hostinfo_nl_cache[at];
    g_snprintf(s, BUFF_SIZE, "%s", addrname(0, host));
    hop->cached_nl = true;
    LOG("hostinfo_nl cache[%d] updated with %s", at, (s && s[0]) ? s : log_empty);
    return s;
  }
  return NULL;
}

static int info_host_arr(int at, const gchar* arr[MAXADDR]) {
  t_host *host = hops[at].host; int n = 0;
  for (; n < MAXADDR; n++) { if (host[n].addr) arr[n] = addrname(n, host); else break; }
  return n;
}

static int info_addrhost(int at, const gchar* arr[MAXADDR], gboolean num) {
  t_host *host = hops[at].host; int n = 0;
  for (; n < MAXADDR; n++) { if (host[n].addr) arr[n] = addr_or_name(n, host, num); else break; }
  return n;
}

static const gchar *info_whois(int at, int type) {
  static gchar whois_cache[MAXTTL][WHOIS_NDX_MAX][BUFF_SIZE];
  t_whois *whois = hops[at].whois;
  gchar *elem = whois[0].elem[type];
  if (elem) { // as a marker
    gchar *s = whois_cache[at][type];
    if (!hops[at].wcached[type]) {
      int l = g_snprintf(s, BUFF_SIZE, "%s", elem);
      for (int i = 1; (i < MAXADDR) && (l < BUFF_SIZE); i++) {
        elem = whois[i].elem[type];
        if (!elem) break;
        l += g_snprintf(s + l, BUFF_SIZE - l, "\n%s", elem);
      }
      hops[at].wcached[type] = true;
      LOG("whois cache[%d,%d] updated with %s", at, type, (s && s[0]) ? s : log_empty);
    }
    return s;
  }
  return NULL;
}

static const gchar *info_whois_nl(int at, int type) {
  static gchar whois_nl_cache[MAXTTL][WHOIS_NDX_MAX][MAXHOSTNAME];
  t_whois *whois = hops[at].whois;
  gchar *elem = whois[0].elem[type];
  if (elem) { // as a marker
    gchar *s = whois_nl_cache[at][type];
    if (!hops[at].wcached_nl[type]) {
      g_snprintf(s, MAXHOSTNAME, "%s", elem);
      hops[at].wcached_nl[type] = true;
      LOG("whois_nl cache[%d,%d] updated with %s", at, type, (s && s[0]) ? s : log_empty);
    }
    return s;
  }
  return NULL;
}

static int info_whois_arr(int at, int type, const gchar* arr[MAXADDR]) {
  t_whois *whois = hops[at].whois; int n = 0;
  for (; n < MAXADDR; n++) {
    gchar *s = whois[n].elem[type];
    if (s) arr[n] = s; else break;
  }
  return n;
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

static const gchar *stat_hop(int type, t_hop *hop) {
  static gchar buff_loss[NUM_BUFF_SZ];
  static gchar buff_sent[NUM_BUFF_SZ];
  static gchar buff_recv[NUM_BUFF_SZ];
  static gchar buff_last[NUM_BUFF_SZ];
  static gchar buff_best[NUM_BUFF_SZ];
  static gchar buff_wrst[NUM_BUFF_SZ];
  static gchar buff_avrg[NUM_BUFF_SZ];
  static gchar buff_jttr[NUM_BUFF_SZ];
  switch (type) {
    case ELEM_LOSS: return fill_stat_dbl(hop->loss, buff_loss, sizeof(buff_loss), "%", 0);
    case ELEM_SENT: return fill_stat_int(hop->sent, buff_sent, sizeof(buff_sent));
    case ELEM_RECV: return fill_stat_int(hop->recv, buff_recv, sizeof(buff_recv));
    case ELEM_LAST: return fill_stat_rtt(hop->last, buff_last, sizeof(buff_last));
    case ELEM_BEST: return fill_stat_rtt(hop->best, buff_best, sizeof(buff_best));
    case ELEM_WRST: return fill_stat_rtt(hop->wrst, buff_wrst, sizeof(buff_wrst));
    case ELEM_AVRG: return fill_stat_dbl(hop->avrg, buff_avrg, sizeof(buff_avrg), NULL, 1000);
    case ELEM_JTTR: return fill_stat_dbl(hop->jttr, buff_jttr, sizeof(buff_jttr), NULL, 1000);
  }
  return NULL;
}

static void set_initial_maxes(void) {
  host_max.addr = host_max.name = g_utf8_strlen(ELEM_HOST_HDR, MAXHOSTNAME);
  whois_max[WHOIS_AS_NDX]   = g_utf8_strlen(ELEM_AS_HDR,   MAXHOSTNAME);
  whois_max[WHOIS_CC_NDX]   = g_utf8_strlen(ELEM_CC_HDR,   MAXHOSTNAME);
  whois_max[WHOIS_DESC_NDX] = g_utf8_strlen(ELEM_DESC_HDR, MAXHOSTNAME);
  whois_max[WHOIS_RT_NDX]   = g_utf8_strlen(ELEM_RT_HDR,   MAXHOSTNAME);
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
    hops_no = MAXTTL;
    visibles = -1;
    pinger_state.gotdata = false;
    pinger_state.reachable = false;
    memset(hops, 0, sizeof(hops));
  }
  for (int i = 0; i < MAXTTL; i++) {
    if (clean) hops[i].reach = true;
    stat_nth_hop_NA(&hops[i]);
    hops[i].at = i;
  }
  set_initial_maxes();
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
  set_initial_maxes();
}

void stat_clear(gboolean clean) { stat_free(); stat_init(clean); }

void stat_reset_cache(void) { // reset 'cached' flags
  for (int i = 0; i < MAXTTL; i++) hops[i].cached = hops[i].cached_nl = false;
  pinger_set_width(ELEM_HOST, opts.dns ? host_max.name : host_max.addr);
}

static void stat_up_info(int at, const gchar *info) {
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
  if (hops_no > ttl) {
    LOG("target is reached at ttl=%d", ttl);
    set_hops_no(ttl, "behind target");
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
    if (reach) { if (hops_no < ttl) set_hops_no(ttl, "unreachable"); }
    else {
      if (hops_no > ttl) uniq_unreach(at);
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

const gchar *stat_str_elem(int at, int type) {
  switch (type) {
    case ELEM_HOST: return info_host(at);
    case ELEM_AS:   return info_whois(at, WHOIS_AS_NDX);
    case ELEM_CC:   return info_whois(at, WHOIS_CC_NDX);
    case ELEM_DESC: return info_whois(at, WHOIS_DESC_NDX);
    case ELEM_RT:   return info_whois(at, WHOIS_RT_NDX);
    case ELEM_LOSS:
    case ELEM_SENT:
    case ELEM_RECV:
    case ELEM_LAST:
    case ELEM_BEST:
    case ELEM_WRST:
    case ELEM_AVRG:
    case ELEM_JTTR:
      return stat_hop(type, &hops[at]);
  }
  return NULL;
}

static const gchar *stat_strnl_elem(int at, int type) {
  switch (type) {
    case ELEM_HOST: return info_host_nl(at);
    case ELEM_AS:   return info_whois_nl(at, WHOIS_AS_NDX);
    case ELEM_CC:   return info_whois_nl(at, WHOIS_CC_NDX);
    case ELEM_DESC: return info_whois_nl(at, WHOIS_DESC_NDX);
    case ELEM_RT:   return info_whois_nl(at, WHOIS_RT_NDX);
  }
  return NULL;
}

int stat_str_arr(int at, int type, const gchar* arr[MAXADDR]) {
  int n = 0;
  if (arr)  {
    memset(arr, 0, MAXADDR * sizeof(gchar*));
    switch (type) {
      case ELEM_HOST: n = info_host_arr(at, arr); break;
      case ELEM_AS:   n = info_whois_arr(at, WHOIS_AS_NDX, arr); break;
      case ELEM_CC:   n = info_whois_arr(at, WHOIS_CC_NDX, arr); break;
      case ELEM_DESC: n = info_whois_arr(at, WHOIS_DESC_NDX, arr); break;
      case ELEM_RT:   n = info_whois_arr(at, WHOIS_RT_NDX, arr); break;
      case EX_ELEM_ADDR: n = info_addrhost(at, arr, true); break;
      case EX_ELEM_HOST: n = info_addrhost(at, arr, false); break;
    }
  }
  return n;
}

double stat_dbl_elem(int at, int type) {
  double re = -1;
  switch (type) {
    case ELEM_LOSS: re = hops[at].loss; break;
    case ELEM_SENT: re = hops[at].sent; break;
    case ELEM_RECV: re = hops[at].recv; break;
    case ELEM_LAST: re = hops[at].last; break;
    case ELEM_BEST: re = hops[at].best; break;
    case ELEM_WRST: re = hops[at].wrst; break;
    case ELEM_AVRG: re = hops[at].avrg; break;
    case ELEM_JTTR: re = hops[at].jttr; break;
  }
  return re;
}

int stat_int_elem(int at, int type) {
  int re = -1;
  switch (type) {
    case ELEM_SENT: re = hops[at].sent; break;
    case ELEM_RECV: re = hops[at].recv; break;
    case ELEM_LAST: re = hops[at].last; break;
    case ELEM_BEST: re = hops[at].best; break;
    case ELEM_WRST: re = hops[at].wrst; break;
  }
  return re;
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
  data->name = stat_strnl_elem(at, ELEM_HOST);
  data->as = stat_strnl_elem(at, ELEM_AS);
  data->cc = stat_strnl_elem(at, ELEM_CC);
  data->av = stat_str_elem(at, ELEM_AVRG);
  data->jt = stat_str_elem(at, ELEM_JTTR);
}

int stat_elem_max(int type) {
  switch (type) {
    case ELEM_HOST: return opts.dns ? host_max.name : host_max.addr;
    case ELEM_AS:   return whois_max[WHOIS_AS_NDX];
    case ELEM_CC:   return whois_max[WHOIS_CC_NDX];
    case ELEM_DESC: return whois_max[WHOIS_DESC_NDX];
    case ELEM_RT:   return whois_max[WHOIS_RT_NDX];
    case ELEM_FILL: return ELEM_LEN / 2;
  }
  return ELEM_LEN;
}

void stat_check_hostaddr_max(int len) {
  if (host_max.addr < len) {
    host_max.addr = len;
    if (!opts.dns) pinger_set_width(ELEM_HOST, len);
  }
}

void stat_check_hostname_max(int len) {
  if (host_max.name < len) {
    host_max.name = len;
    if (opts.dns) pinger_set_width(ELEM_HOST, len);
  }
}

void stat_check_whois_max(gchar* elem[]) {
  for (int i = 0; i < WHOIS_NDX_MAX; i++) if (elem[i]) {
    int len = g_utf8_strlen(elem[i], MAXHOSTNAME);
    if (len > whois_max[i]) { whois_max[i] = len; pinger_set_width(wndx2endx[i], len); }
  }
}

void stat_whois_enabler(void) {
  gboolean enable = false;
  for (int i = 0; i < G_N_ELEMENTS(pingelem); i++)
    if (IS_WHOIS_NDX(pingelem[i].type) && pingelem[i].enable) { enable = true; break; }
  if (enable != opts.whois) { opts.whois = enable; LOG("whois %sabled", enable ? "en" : "dis"); }
}

void stat_run_whois_resolv(void) {
  for (int at = opts.min; at < opts.lim; at++) {
    t_hop *hop = &hops[at];
    for (int i = 0; i < MAXADDR; i++)
      if (hop->host[i].addr) whois_resolv(hop, i);
  }
}

