#ifndef PARSER_H
#define PARSER_H

#include <gtk/gtk.h>

bool parser_init(void);
void parser_parse(int at, char *input);
bool parser_valid_char0(gchar *str);
bool parser_valid_host(gchar *host);

#endif
