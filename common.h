#ifndef COMMON_H
#define COMMON_H

#include <gtk/gtk.h>
#include "aux.h"

#define APPNAME "pingpath"
#define VERSION "0.1.25"

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

#define EV_ACTIVE "activate"
#define EV_TOGGLE "toggled"
#define EV_SPIN   "value-changed"

#define ACT_MENU_ICON      "open-menu-symbolic"
#define OPT_MENU_ICON      "document-properties-symbolic"
#define SUB_MENU_ICON_UP   "go-up-symbolic"
#define SUB_MENU_ICON_DOWN "go-down-symbolic"
#define SUB_MENU_PROP      "icon-name"
#define TO_LEFT_ICON       "go-previous-symbolic"
#define TO_RIGHT_ICON      "go-next-symbolic"

enum { ENT_BOOL_NONE, ENT_BOOL_DNS, ENT_BOOL_HOST, ENT_BOOL_LOSS, ENT_BOOL_SENT, ENT_BOOL_RECV,
  ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST, ENT_BOOL_AVRG, ENT_BOOL_JTTR, ENT_BOOL_MAX };
enum { ELEM_NO, ELEM_HOST, ELEM_LOSS, ELEM_SENT, ELEM_RECV, ELEM_LAST, ELEM_BEST, ELEM_WRST,
  ELEM_AVRG, ELEM_JTTR, ELEM_MAX };

#endif
