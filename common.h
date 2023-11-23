#ifndef __COMMON_H
#define __COMMON_H

#include <gtk/gtk.h>

#define VERSION "0.1.2"

#define DEBUG 1

#define WARN(fmt, ...) g_warning("[WARN] %s: " fmt "\n", __func__, __VA_ARGS__)
#ifdef DEBUG
#define LOG(fmt, ...) g_print("[LOG] " fmt "\n", __VA_ARGS__)
#if DEBUG > 1
#define LOG2(fmt, ...) LOG(fmt, __VA_ARGS__)
#else
#define LOG2(fmt, ...) {}
#endif
#else
#define LOG(fmt, ...) {}
#endif

struct procdata;

typedef struct widgets { // aux widgets' references
  GApplication *app;
  GtkWidget *win, *appbar, *menu, *dyn;//, *label, *button_startstop;
} t_widgets;

typedef struct ping_opts {
  char *target;
  int count;
  int ttl;
} t_ping_opts;

typedef struct procdata {
  GSubprocess *proc;
  bool active;      // process state
  t_ping_opts opts; // ping options
  GString *buff;    // buffer for received strings
} t_procdata;

extern t_widgets widgets;

//const char* PING = "/bin/ping";
extern const int MAXTTL;
extern const int COUNT;

#endif
