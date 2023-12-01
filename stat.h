#ifndef STAT_H
#define STAT_H

#include <gtk/gtk.h>

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
 
void init_stat(void);
void free_stat(void);
void clear_stat(void);
void save_success_data(int at, t_ping_success *data);
void save_discard_data(int at, t_ping_discard *data);
void save_timeout_data(int at, t_ping_timeout *data);
void save_info_data(int at, t_ping_info *data);
const gchar *stat_elem(int at, int ndx);

extern int hops_no;
extern int host_addr_max;
extern int host_name_max;
extern int stat_all_max;

#endif
