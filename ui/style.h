#ifndef UI_STYLE_H
#define UI_STYLE_H

#include "common.h"

#define CSS_ID_DATETIME "datetime"
#define CSS_BGROUND     "bground"
#define CSS_BGONBG      "bgonbg"
#define CSS_DARK_BG     "bgdark"
#define CSS_LIGHT_BG    "bglight"
#define CSS_EXP         "expander"
#define CSS_PAD         "pad"
#define CSS_PAD6        "pad6"
#define CSS_NOPAD       "nopad"
#define CSS_CHPAD       "chpad"
#define CSS_NOFRAME     "noframe"
#define CSS_ROUNDED     "rounded"
#define CSS_ROUNDG      "roundg"
#define CSS_INVERT      "invtxt"
#define CSS_LEGEND      "legend"
#define CSS_LEGEND_TEXT "lgndtxt"

void style_init(void);
const char* is_sysicon(const char **icon);
extern int style_loaded;
extern gboolean ub_theme;

#endif
