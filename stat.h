#ifndef STAT_H
#define STAT_H

#include "common.h"

typedef struct host_max {
  int addr, name;
} t_host_max;

typedef struct ping_success {
  t_tseq mark;
  t_host host;
  int ttl, time;
} t_ping_success;

typedef struct ping_discard {
  t_tseq mark;
  t_host host;
  gchar* reason;
} t_ping_discard;

typedef struct ping_timeout {
  t_tseq mark;
} t_ping_timeout;

typedef struct ping_info {
  t_tseq mark;
  t_host host;
  int ttl;
  gchar* info;
} t_ping_info;

void stat_init(gboolean clean);
void stat_free(void);
void stat_clear(gboolean clean);
void stat_reset_cache(void);
void stat_save_success(int at, t_ping_success *data);
void stat_save_discard(int at, t_ping_discard *data);
void stat_save_timeout(int at, t_ping_timeout *data);
void stat_save_info(int at, t_ping_info *data);
void stat_last_tx(int at);
const gchar *stat_str_elem(int at, int type);
int stat_str_arr(int at, int type, const gchar* arr[MAXADDR]);
double stat_dbl_elem(int at, int type);
int stat_int_elem(int at, int type);
void stat_rseq(int at, t_rseq *data);
void stat_legend(int at, t_legend *data);
int stat_elem_max(int type);
void stat_check_hostaddr_max(int l);
void stat_check_hostname_max(int l);
void stat_check_whois_max(gchar* elem[]);
void stat_whois_enabler(void);
void stat_run_whois_resolv(void);
void stat_clean_elems(int type);

extern int hops_no, visibles;
extern t_type_elem pingelem[ELEM_MAX]; // map ping-tab indexes to elems

#endif
