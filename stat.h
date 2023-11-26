#ifndef __STAT_H
#define __STAT_H

#include <gtk/gtk.h>


typedef struct ping_part_mark {
  long long sec;
  int usec;
  int seq;
} t_ping_part_mark;

typedef struct ping_part_host {
  gchar *name, *addr;
} t_ping_part_host;

typedef struct ping_success {
  t_ping_part_mark mark;
  t_ping_part_host host;
  int ttl, time;
} t_ping_success;

 
typedef struct ping_discard {
  t_ping_part_mark mark;
  t_ping_part_host host;
  gchar* reason;                                                                              
} t_ping_discard;                                                                             

typedef struct ping_timeout {
  t_ping_part_mark mark;
} t_ping_timeout;

typedef struct ping_info {
  t_ping_part_mark mark;
  t_ping_part_host host;
  int ttl;
  gchar* info;                                                                              
} t_ping_info;
 
void init_stat(void);
void save_success_data(int at, t_ping_success *data);
void save_discard_data(int at, t_ping_discard *data);
void save_timeout_data(int at, t_ping_timeout *data);
void save_info_data(int at, t_ping_info *data);

extern int hops_no;

#endif
