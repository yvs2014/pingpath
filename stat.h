#ifndef STAT_H
#define STAT_H

#include <gtk/gtk.h>
#include "common.h"

typedef struct tseq {
  int seq;
  long long sec;
  int usec;
} t_tseq;

typedef struct host {
  gchar *addr, *name;
} t_host;

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

void stat_init(void);
void stat_free(void);
void stat_clear(void);
void stat_set_nopong(const gchar *mesg);
void stat_save_success(int at, t_ping_success *data);
void stat_save_discard(int at, t_ping_discard *data);
void stat_save_timeout(int at, t_ping_timeout *data);
void stat_save_info(int at, t_ping_info *data);
void stat_last_tx(int at);
const gchar *stat_elem(int at, int typ);
int stat_elem_max(int typ);

typedef struct t_stat_elem {
  bool enable;
  gchar* name;
} t_stat_elem;

extern int hops_no;
extern t_stat_elem statelem[ELEM_MAX]; // map indexes to elems

#endif
