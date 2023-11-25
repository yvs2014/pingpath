#ifndef __COMMON_H
#define __COMMON_H

#include <gtk/gtk.h>
#include "aux.h"

#define VERSION "0.1.4"

#define DEBUG 1

#define WARN(fmt, ...) g_warning("%s: " fmt "\n", __func__, __VA_ARGS__)
#ifdef DEBUG
#define LOG(fmt, ...) g_print("[%s] " fmt "\n", timestampit(), __VA_ARGS__)
#if DEBUG > 1
#define LOG2(fmt, ...) LOG(fmt, __VA_ARGS__)
#else
#define LOG2(fmt, ...) {}
#endif
#else
#define LOG(fmt, ...) {}
#endif

#define BUFF_SIZE 1024

#define MAXTTL 30
#define COUNT  5
#define TIMEOUT 1

struct procdata;

typedef struct widgets { // aux widgets' references
  GApplication *app;
  GtkWidget *win, *appbar, *menu, *dyn;//, *label, *button_startstop;
} t_widgets;

typedef struct ping_opts {
  char *target;
  int count;
  int ttl;
  int timeout;
} t_ping_opts;

typedef struct procdata {
  GSubprocess *proc;
  bool active;      // process state
  t_ping_opts opts; // ping options
  GString *out;     // buffer for received input data
  GString *err;     // buffer for received errors
} t_procdata;

extern t_widgets widgets;

#endif
