#ifndef __COMMON_H
#define __COMMON_H

#include <gtk/gtk.h>
#include "aux.h"

#define APPNAME "pingpath"
#define VERSION "0.1.8"

#define MAXTTL 30
#define COUNT  10
#define TIMEOUT 1

#define BUFF_SIZE 1024

#define LOGGING 1
//#define DEBUGGING 1

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

#define UPD_STR(str, val) { g_free(str); str = g_strdup(val); }

struct procdata;

typedef struct widgets { // aux widgets' references
  GApplication *app;
  GtkWidget *win, *appbar, *menu, *datetime, *stat;
} t_widgets;

typedef struct ping_opts {
  char *target;
  int count, timeout;
  guint timer;        // thread ID of stat-view-area update
  long long _tout_usec; // internal const
} t_ping_opts;

typedef struct procdata {
  GSubprocess *proc;
  bool active;        // process state
  GString *out, *err; // for received input data and errors
  int ndx;            // index in internal list
} t_procdata;

extern t_widgets widgets;

#endif
