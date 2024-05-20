#ifndef PARSER_H
#define PARSER_H

#include "common.h"

#define COMMA ','
#define COLON ':'
#define TTL_RANGE_DELIM COMMA
#define COL_GRAD_DELIM  COLON

enum { ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE,
  ENT_STR_LOGMAX, ENT_STR_GRAPH, ENT_STR_GEXTRA, ENT_STR_LEGEND, ENT_STR_THEME };
enum { OPT_TYPE_INT, OPT_TYPE_PAD, OPT_TYPE_INFO, OPT_TYPE_STAT, OPT_TYPE_GRLG, OPT_TYPE_GREX,
#ifdef WITH_PLOT
  OPT_TYPE_PLEL,
#endif
  OPT_TYPE_RECAP
};

gboolean parser_init(void);
void parser_parse(int at, char *input);
char* parser_valid_target(const char *target);
int parser_int(const char *str, int type, const char *option, t_minmax range);
char* parser_str(const char *str, const char *option, int cat);
void parser_whois(char *buff, int sz, char* elem[]);
t_minmax parser_range(char *range, char delim, const char *option);

#endif
