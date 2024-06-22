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
void stat_reset_cache(void);
void stat_save_success(int at, t_ping_success *data);
void stat_save_discard(int at, t_ping_discard *data);
void stat_save_timeout(int at, t_ping_timeout *data);
void stat_save_info(int at, t_ping_info *data);
void stat_last_tx(int at);
const char *stat_str_elem(int at, int type);
int stat_str_arr(int at, int type, const char* arr[MAXADDR]);
double stat_dbl_elem(int at, int type);
int stat_int_elem(int at, int type);
void stat_rseq(int at, t_rseq *data);
void stat_legend(int at, t_legend *data);
void stat_whois_enabler(void);
void stat_run_whois_resolv(void);

extern int tgtat /*target at*/, visibles;
extern t_type_elem pingelem[PE_MAX]; // map ping-tab indexes to elems

#endif
