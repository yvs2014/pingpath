#ifndef PARSER_H
#define PARSER_H

#include <gtk/gtk.h>

void init_parser(void);
void parse_input(int at, char *input);
bool test_hchar0(gchar *s);
bool test_hchars(gchar *s);

#endif
