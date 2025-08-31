#ifndef PARSER_H
#define PARSER_H

#include "common.h"

enum { ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE, ENT_STR_LOGMAX };
enum { OPT_TYPE_INT, OPT_TYPE_PAD, OPT_TYPE_INFO, OPT_TYPE_STAT, OPT_TYPE_GRLG, OPT_TYPE_GREX,
#ifdef WITH_PLOT
  OPT_TYPE_PLEL,
#endif
  OPT_TYPE_RECAP
};

gboolean parser_init(void);
void parser_parse(int at, char *input);
char* parser_valid_target(const char *target, gboolean *spaced);
char* parser_str(const char *str, const char *option, unsigned cat);
void parser_whois(char *buff, t_whois* welem);
gboolean parser_mmint(const char *str, const char *option, t_minmax minmax, int *value);
gboolean parser_range(char *range, char delim, const char *option, t_minmax *minmax);
#ifdef WITH_PLOT
gboolean parser_ivec(char *range, char delim, const char *option, int *dest, unsigned max);
#endif

#endif
