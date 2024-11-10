#ifndef UI_STYLE_H
#define UI_STYLE_H

#include <gtk/gtk.h>

#include "config.h"

#define CSS_ID_DATETIME "datetime"
#define CSS_BGROUND     "bground"
#define CSS_BGONDARK    "bgondark"
#define CSS_BGONLIGHT   "bgonlight"
#define CSS_DEF_BG      "bgdef"
#define CSS_INV_BG      "bginv"
#define CSS_GRAPH_BG    "bggraph"
#ifdef WITH_PLOT
#define CSS_PLOT_BG     "bgplot"
#define CSS_ROTOR_COL   "rtrcol"
#endif
#define CSS_PAD         "pad"
#define CSS_PAD6        "pad6"
#define CSS_NOPAD       "nopad"
#define CSS_CHPAD       "chpad"
#define CSS_NOFRAME     "noframe"
#define CSS_ROUNDED     "rounded"
#define CSS_ROUNDG      "roundg"
#define CSS_INVERT      "invtxt"
#define CSS_LEGEND      "legend"
#define CSS_LEGEND_COL  "lgndcol"
#define CSS_LEGEND_TEXT "lgndtxt"
#define CSS_ROTOR       "rotor"

void style_set(void);
void style_free(void);
const char* is_sysicon(const char **icon);
extern int style_loaded;
extern gboolean ub_theme;

#endif
