#ifndef PARSER_H
#define PARSER_H

#include <gtk/gtk.h>

enum { ENT_STR_NONE, ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE, ENT_STR_MAX };

typedef int (*parser_str_fn)(const gchar* str, int ndx, const char *option);

bool parser_init(void);
void parser_parse(int at, char *input);
bool parser_valid_char0(gchar *str);
bool parser_valid_host(gchar *host);
int parser_int(const gchar *str, int typ, const gchar *option);

#endif
