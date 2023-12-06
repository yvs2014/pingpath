#ifndef COMMON_H
#define COMMON_H

#include <gtk/gtk.h>
#include "aux.h"

#define APPNAME "pingpath"
#define VERSION "0.1.21"

#define MAXTTL 30
#define COUNT 50
#define TIMEOUT 1

#define MAXHOSTNAME 63 // in chars: must 63, should 255
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

#endif
