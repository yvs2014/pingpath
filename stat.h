#ifndef STAT_H
#define STAT_H

#include "common.h"

typedef struct ping_success {
  t_tseq mark;
  t_host host;
  int ttl, time;
} t_ping_success;

typedef struct ping_discard {
  t_tseq mark;
  t_host host;
  char* reason;
} t_ping_discard;

typedef struct ping_timeout {
  t_tseq mark;
} t_ping_timeout;

typedef struct ping_info {
  t_tseq mark;
  t_host host;
  int ttl;
  char* info;
} t_ping_info;

void stat_init(gboolean clean);
void stat_free(void);
void stat_clear(gboolean clean);
void stat_reset_dns_cache(void);
void stat_save_success(int at, t_ping_success *data);
void stat_save_discard(int at, t_ping_discard *data);
void stat_save_timeout(int at, t_ping_timeout *data);
void stat_save_info(int at, t_ping_info *data);
void stat_last_tx(int at);
double stat_dbl_elem(int at, int type);
int stat_int_elem(int at, int type);
void stat_rseq(int at, t_rseq *data);
t_legend stat_legend(int at);
void stat_whois_enabler(void);
void stat_run_whois_resolv(void);
void stat_col_addrhost(int at, t_ping_column* column, gboolean num); // NONNULL(2)

extern int tgtat /*target at*/, visibles;

#endif
