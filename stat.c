
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
#define INT_FMT "%5d"
#define DBL_FMT "%3.2f"
#define NUM_BUFF_SZ 16

typedef struct hop {
  t_host host[MAX_ADDRS]; int pos;
  int sent, recv, last, prev, best, wrst;
  double loss, avrg, jttr;
  t_tseq start;
  bool reach;
  gchar* info;                                                                              
} t_hop;

int hops_no = MAXTTL;
t_hop hops[MAXTTL];
int host_addr_max;
int host_name_max;
int stat_all_max;

// aux
//

static void update_hmax(const gchar* s, bool addr) {
  int *max = addr ? &host_addr_max : &host_name_max;
  if (s) {
    int l = g_utf8_strlen(s, MAXHOSTNAME);
    if (l > *max) {
      *max = l;
      if (addr && !ping_opts.dns) update_elem_width(host_addr_max, NDX_HOST);
      if (!addr && ping_opts.dns) update_elem_width(host_name_max, NDX_HOST);
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
  for (int i = from; i < MAXTTL; i++) stop_ping_at(i, reason);
}

static void set_hops_no(int no, const char *info) {
  hops_no = no;
  stop_pings_behind(no, info);
  set_visible_lines(no);
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

static void update_stat(int at, int rtt) {
  hops[at].sent++;
  hops[at].recv++;
  if (hops[at].sent) hops[at].loss = (hops[at].sent - hops[at].recv) / (hops[at].sent * 100.);
  if (rtt >= 0) {
    if (rtt < hops[at].best) hops[at].best = rtt;
    if (rtt > hops[at].wrst) hops[at].wrst = rtt;
    hops[at].avrg += (rtt - hops[at].avrg) / hops[at].recv;
    if ((hops[at].prev > 0) && (hops[at].recv > 1))
      hops[at].jttr += (abs(rtt - hops[at].prev) - hops[at].jttr) / (hops[at].recv - 1);
    hops[at].prev = hops[at].last;
    hops[at].last = rtt;
  }
  // update max [TODO: fixed stat width]
  const gchar *s = stat_elem(at, NDX_STAT);
  int l = g_utf8_strlen(s, MAXHOSTNAME);
  if (l > stat_all_max) { stat_all_max = l; update_elem_width(l, NDX_STAT); }
}

static int calc_rtt(int at, t_tseq *mark) {
  int rtt = ((mark->seq != (hops[at].start.seq + 1)) || (hops[at].start.seq < 0)) ? -1 :
    (mark->sec - hops[at].start.sec) * 1000000 + (mark->usec - hops[at].start.usec);
  rtt = (rtt > ping_opts.tout_usec) ? -1 : rtt; // skip timeouted responses
  return rtt;
}


// pub
//
void init_stat(void) {
  for (int i = 0; i < MAXTTL; i++) hops[i].reach = true;
  host_addr_max = host_name_max = g_utf8_strlen(elem_hdr[NDX_HOST], MAXHOSTNAME);
  stat_all_max = g_utf8_strlen(elem_hdr[NDX_STAT], MAXHOSTNAME);
}

void free_stat(void) {
  for (int at = 0; at < MAXTTL; at++) {
    for (int i = 0; i < MAX_ADDRS; i++) {
      UPD_STR(hops[at].host[i].addr, NULL);
      UPD_STR(hops[at].host[i].name, NULL);
    }
    UPD_STR(hops[at].info, NULL);
  }
  host_addr_max = host_name_max = g_utf8_strlen(elem_hdr[NDX_HOST], MAXHOSTNAME);
  stat_all_max = g_utf8_strlen(elem_hdr[NDX_STAT], MAXHOSTNAME);
}

void clear_stat(void) {
  free_stat();
  memset(hops, 0, sizeof(hops));
  init_stat();
}

void save_success_data(int at, t_ping_success *data) {
  update_addrname(at, &(data->host));
  update_stat(at, data->time);
  if (!hops[at].reach) hops[at].reach = true;
  // management
  int ttl = at + 1;
  if (hops_no > ttl) {
    LOG("target is reached at ttl=%d", ttl);
    set_hops_no(ttl, "behind target");
  }
  UPD_STR(ping_errors[at], NULL);
  UPD_STR(hops[at].info, NULL);
}

void save_discard_data(int at, t_ping_discard *data) {
  if (at < hops_no) {
    update_addrname(at, &(data->host));
    int rtt = calc_rtt(at, &(data->mark));
    update_stat(at, rtt);
    if (rtt > 0) hops[at].start.seq = -1; // clear marker
  }
  // management
  if (data->reason) {
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
  UPD_STR(ping_errors[at], NULL);
}

void save_timeout_data(int at, t_ping_timeout *data) {
  // for RTT calculation of messages without 'time' field
  hops[at].start.seq = data->mark.seq; // marker of sent packet
  hops[at].start.sec = data->mark.sec;
  hops[at].start.usec = data->mark.usec;
}

void save_info_data(int at, t_ping_info *data) {
  update_addrname(at, &(data->host));
  update_stat(at, -1);
  UPD_STR(hops[at].info, data->info);
}

static void fill_stat_int(int val, char* buff, int size) {
  if (val >= 0) snprintf(buff, size, INT_FMT, val); else buff[0] = 0;
}

static void fill_stat_dbl(double val, char* buff, int size) {
  if (val >= 0) snprintf(buff, size, DBL_FMT, val); else buff[0] = 0;
}

static void fill_stat_msec(int usec, char* buff, int size) {
  if (usec > 0) snprintf(buff, size, DBL_FMT, usec / 1000.); else buff[0] = 0;
}

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

static const gchar *stat_stat(int at) {
  static gchar stat_stat_buff[MAXTTL][BUFF_SIZE];
  t_hop *hop = &(hops[at]);
  t_host *host = hop->host;
  if (host[0].addr) {
    char loss[NUM_BUFF_SZ]; fill_stat_dbl(hop->loss, loss, sizeof(loss));
    char sent[NUM_BUFF_SZ]; fill_stat_int(hop->sent, sent, sizeof(sent));
    char last[NUM_BUFF_SZ]; fill_stat_msec(hop->last, last, sizeof(last));
    gchar *s = stat_stat_buff[at];
    g_snprintf(s, BUFF_SIZE, "%s%% %s %s", loss, sent, last);
    return s;
  }
  return NULL;
}

const gchar *stat_elem(int at, int ndx) {
  switch (ndx) {
    case NDX_HOST: return stat_host(at);
    case NDX_STAT: return stat_stat(at);
  }
  return NULL;
}

