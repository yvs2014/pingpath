#ifndef PARSER_H
#define PARSER_H

#include "common.h"

enum { ENT_STR_NONE, ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE,
  ENT_STR_LOGMAX, ENT_STR_GRAPH, ENT_STR_MAX };
enum { STR_RX_INT, STR_RX_PAD, STR_RX_INFO, STR_RX_STAT, STR_RX_MAX };

gboolean parser_init(void);
void parser_parse(int at, char *input);
gchar* parser_valid_target(const gchar *target);
int parser_int(const gchar *str, int typ, const gchar *option, t_minmax range);
const char* parser_str(const gchar *str, const gchar *option, int buff_sz, int rx_ndx);
void parser_whois(gchar *buff, int sz, gchar* elem[]);
t_minmax parser_range(gchar *range, const gchar *option);

#endif
