#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#ifdef WITH_PLOT
#include <cglm/cglm.h>
#endif

#define COMMA ','
#define COLON ':'

enum { ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE, ENT_STR_LOGMAX };
enum { OPT_TYPE_INT, OPT_TYPE_PAD, OPT_TYPE_INFO, OPT_TYPE_STAT, OPT_TYPE_GRLG, OPT_TYPE_GREX,
#ifdef WITH_PLOT
  OPT_TYPE_PLEL,
#endif
  OPT_TYPE_RECAP
};

gboolean parser_init(void);
void parser_parse(int at, char *input);
char* parser_valid_target(const char *target);
char* parser_str(const char *str, const char *option, int cat);
void parser_whois(char *buff, int sz, char* elem[]);
gboolean parser_mmint(const char *str, const char *option, t_minmax mm, int *re);
gboolean parser_range(char *range, char delim, const char *option, t_minmax *re);
#ifdef WITH_PLOT
gboolean parser_ivec4(char *range, char delim, const char *option, ivec4_t re);
#endif

#endif
