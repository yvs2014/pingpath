
#include "stat.h"
#include "pinger.h"
#include "common.h"

#define MAX_ADDRS 10

typedef struct hop {
  int sent, recv, last, prev, best, wrst;
  double avrg, jttr;
  GSList *addr; int apos;
  GSList *name; int npos;
  t_ping_part_mark start;
  bool reach;
  gchar* info;                                                                              
} t_hop;

int hops_no = MAXTTL, last_ttl = MAXTTL;
t_hop hops[MAXTTL];

// aux
//
static GSList* add2list(GSList *list, const char *str, int *pos) {
  GSList *elem = g_slist_find_custom(list, str, (GCompareFunc)g_strcmp0);
  if (elem) { if (pos) *pos = g_slist_position(list, elem); } // update position
  else { // add str-copy at the beginning
    list = g_slist_prepend(list, g_strdup(str));
    while (g_slist_length(list) > MAX_ADDRS) // limit it
      list = g_slist_delete_link(list, g_slist_last(list));
    return list;
  }
  return NULL;
}

static void update_addrname(int at, t_ping_part_host *host) {
  // addr is mandatory, name not
  GSList* head = add2list(hops[at].addr, host->addr, &(hops[at].apos));
  if (head != hops[at].addr) hops[at].addr = head;
  if (host->name) {
    head = add2list(hops[at].name, host->name, &(hops[at].npos));
    if (head != hops[at].name) hops[at].name = head;
  }
  // cleanup strdups
  g_free(host->addr);
  g_free(host->name);
}

static void free_info_at(int at) {
  if (hops[at].info) { g_free(hops[at].info); hops[at].info = NULL; }
}

static void stop_pings_behind(int n, const gchar *reason) {
  for (int i = n; i < MAXTTL; i++) stop_ping_at(n, reason);
}

static void uniq_unreach(int at) {
  const gchar *addr = hops[at].addr->data;
  if (!addr) return;
  int i;
  for (i = at - 1; i >= 0; i--) {
    if (hops[i].reach || !hops[i].addr || !hops[i].addr->data) break;
    if (g_strcmp0(hops[i].addr->data, addr)) break;
  }
  hops_no = i + 1;
  stop_pings_behind(hops_no, "behind unreachable");
}

static void update_stat(int at, int rtt) {
  hops[at].sent++;
  hops[at].recv++;
  if (rtt < hops[at].best) hops[at].best = rtt;
  if (rtt > hops[at].wrst) hops[at].wrst = rtt;
  hops[at].avrg += (rtt - hops[at].avrg) / hops[at].recv;
  if ((hops[at].prev > 0) && (hops[at].recv > 1))
    hops[at].jttr += (abs(rtt - hops[at].prev) - hops[at].jttr) / (hops[at].recv - 1);
  hops[at].prev = hops[at].last;
  hops[at].last = rtt;
}

static int calc_rtt(int at, t_ping_part_mark *ts) {
  return (ts->seq != hops[at].start.seq) ? -1 :
    (ts->sec - hops[at].start.sec) * 1000000 + (ts->usec - hops[at].start.usec);
}


// pub
//
void init_stat(void) {
  for (int i = 0; i < MAXTTL; i++) hops[i].reach = true;
}

void save_success_data(int at, t_ping_success *data) {
  update_addrname(at, &(data->host));
  update_stat(at, data->time);
  if (!hops[at].reach) hops[at].reach = true;
  // management
  if (hops_no > at) {
    hops_no = at + 1;
    LOG("target is reached at ttl=%d", hops_no);
    stop_pings_behind(hops_no, "behind target");
  }
  free_ping_error_at(at);
  free_info_at(at);
}

void save_discard_data(int at, t_ping_discard *data) {
  if (at < hops_no) {
    update_addrname(at, &(data->host));
    int rtt = calc_rtt(at, &(data->mark));
    if (rtt > 0) update_stat(at, rtt);
  }
  // management
  if (data->reason) {
    int ttl = at + 1;
    bool reach = !g_strrstr(data->reason, "nreachable");
    if (hops[at].reach != reach) hops[at].reach = reach;
    if (reach) { if (hops_no < ttl) hops_no = ttl; }
    else { if (hops_no > ttl) uniq_unreach(at); }
    g_free(data->reason);
  }
  free_ping_error_at(at);
  free_info_at(at);
}

void save_timeout_data(int at, t_ping_timeout *data) {
  if (hops[at].start.seq != data->mark.seq) hops[at].sent++;
  // for RTT calculation of messages without 'time' field
  hops[at].start.seq = data->mark.seq + 1;
  hops[at].start.sec = data->mark.sec;
  hops[at].start.usec = data->mark.usec;
}

void save_info_data(int at, t_ping_info *data) {
  update_addrname(at, &(data->host));
  // update RTT with truncated message? So far, do not
  hops[at].sent++;
  hops[at].recv++;
  hops[at].info = g_strdup(data->info); g_free(data->info);
//  if (hops[at].info) { // rewind back to not duplicate the same message
//    int max = (last_ttl < hops_no) ? last_ttl : hops_no;
//    if ((max == (at + 1)) && (max > 0)) hops_no = first_uniq(at) + 1;
//  }
}

