#ifndef __COMMON_H
#define __COMMON_H

#include <gtk/gtk.h>
#include "aux.h"

#define VERSION "0.1.5"

#define LOGGING 1
#define DEBUGGING 1

#ifdef LOGGING
#define LOG(fmt, ...) g_print("[%s] " fmt "\n", timestampit(), __VA_ARGS__)
#else
#define LOG(fmt, ...) {}
#endif

#ifdef DEBUGGING
#define DEBUG(fmt, ...) LOG(fmt, __VA_ARGS__)
#else
#define DEBUG(fmt, ...) {}
#endif

#define WARN(fmt, ...) g_warning("%s: " fmt "\n", __func__, __VA_ARGS__)

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
  int count, timeout;
  bool finish; // no running pings
} t_ping_opts;

typedef struct procdata {
  GSubprocess *proc;
  bool active;        // process state
  GString *out, *err; // for received input data and errors
  int ndx;            // index in internal list
} t_procdata;

extern t_widgets widgets;

#endif
