#ifndef PARSER_H
#define PARSER_H

#include <gtk/gtk.h>

enum { ENT_STR_NONE, ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE, ENT_STR_LOGMAX, ENT_STR_MAX };

bool parser_init(void);
void parser_parse(int at, char *input);
gchar* parser_valid_target(const gchar *target);
int parser_int(const gchar *str, int typ, const gchar *option);
const char* parser_pad(const gchar *str, const gchar *option);
void parser_whois(gchar *buff, int sz, gchar* elem[]);

#endif
