#ifndef PARSER_H
#define PARSER_H

#include "common.h"

enum { ENT_STR_NONE, ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE,
  ENT_STR_LOGMAX, ENT_STR_GRAPH, ENT_STR_GEXTRA, ENT_STR_LEGEND, ENT_STR_THEME, ENT_STR_MAX };
enum { OPT_TYPE_INT, OPT_TYPE_PAD, OPT_TYPE_INFO, OPT_TYPE_STAT, OPT_TYPE_GRLG, OPT_TYPE_GREX, OPT_TYPE_RECAP, OPT_TYPE_MAX };

gboolean parser_init(void);
void parser_parse(int at, char *input);
gchar* parser_valid_target(const gchar *target);
int parser_int(const gchar *str, int type, const gchar *option, t_minmax range);
char* parser_str(const gchar *str, const gchar *option, int cat);
void parser_whois(gchar *buff, int sz, gchar* elem[]);
t_minmax parser_range(gchar *range, const gchar *option);

#endif
