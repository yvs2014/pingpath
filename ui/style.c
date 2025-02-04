
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "style.h"

#define FONT_BOLD "font-weight:500;"

#define PAD   "8px"
#define PAD4  "4px"
#define PAD6  "6px"
#define PAD16 "16px"

#define PROP_THEME       "gtk-theme-name"
#define PROP_PREFER_DARK "gtk-application-prefer-dark-theme"
#define DARK_SFFX "dark"
#define SFFX_DIV1 "-"
#define SFFX_DIV2 ":"

#define LEGEND_BORDER     "#515151"
#define BG_CONTRAST_DARK  "#2c2c2c"
#define BG_CONTRAST_LIGHT "#f5f5f5"

#define STYLE_GET_DISPLAY(re) GdkDisplay *display = gdk_display_get_default(); \
  if (!display) { WARN("no default display"); return re; }

typedef struct style_colors {
  const char *def[2], *app[2], *alt[2], *gry[2];
} t_style_colors;

int style_loaded;
gboolean ub_theme;

//

static const t_style_colors style_color = {
  .def = { "#fdfdfd", "#151515"},
  .app = { BG_CONTRAST_LIGHT, BG_CONTRAST_DARK},
  .alt = { "#f9f9f9", "#222222"},
  .gry = { "#e0e0e0", "#414141"},
};

static const char *css_common = "\n\
notebook > header > tabs {padding-left:0; padding-right:0;}\n\
notebook > header > tabs > tab {margin-left:0; margin-right:0;}\n\
entry {background:transparent; border:none;}\n\
button.toggle {background:transparent;}\n\
vertical {padding:" PAD16 ";}\n\
.flat {padding:" PAD4 ";}\n\
." CSS_NOPAD     "{min-height:0;}\n\
." CSS_PAD       "{padding:"       PAD   ";}\n\
." CSS_PAD4      "{padding:"       PAD4  ";}\n\
." CSS_LPAD      "{padding-left:"  PAD   ";}\n\
." CSS_LPAD_BOLD "{padding-left:"  PAD16 "; " FONT_BOLD "}\n\
." CSS_TPAD      "{padding-top:"   PAD16   ";}\n\
." CSS_RPAD      "{padding-right:" PAD   ";}\n\
." CSS_RPAD_BOLD "{padding-right:" PAD16 "; " FONT_BOLD "}\n\
." CSS_NOFRAME   "{border:none;}\n\
." CSS_ROUNDED   "{border-radius:5px; padding:" PAD16 ";}\n\
." CSS_ROUNDG    "{border-radius:7px; padding:" PAD ";}\n\
." CSS_INVERT    "{filter:invert(100%);}\n\
." CSS_LEGEND    "{border: 1px outset " LEGEND_BORDER "; padding: " PAD4 " " PAD6 " " PAD4 " " PAD4 ";}\n\
." CSS_LEGEND_TEXT "{padding:0;}\n\
." CSS_ROTOR     "{border: 0; padding: " PAD4 " " PAD6 " " PAD4 " " PAD4 ";}\n\
." CSS_BGONDARK  "{background:" BG_CONTRAST_DARK  ";color:" BG_CONTRAST_DARK  ";}\n\
." CSS_BGONLIGHT "{background:" BG_CONTRAST_LIGHT ";color:" BG_CONTRAST_LIGHT ";}\n\
." CSS_ID_DATETIME "{" FONT_BOLD "}";

static void style_css_load(GtkCssProvider *prov, const char *str) {
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_css_provider_load_from_string(prov, str);
#else
  gtk_css_provider_load_from_data(prov, str, -1);
#endif
}

static char* style_extra_css(const char *common) {
  int m = opts.darktheme ? 1 : 0; // main
  int g = opts.darkgraph ? 1 : 0; // graph
#ifdef WITH_PLOT
  int p = opts.darkplot  ? 1 : 0; // plot
#endif
  const char *col = style_color.app[m];
  char* items[] = {
    g_strdup_printf("%s\n", common),
    g_strdup_printf("." CSS_BGROUND " {background:%s;}\n", style_color.def[m]),
    g_strdup_printf("." CSS_DEF_BG " {background:%s; color:%s;}\n", col, col),
    g_strdup_printf("." CSS_INV_BG " {background:%s; color:%s;}\n", style_color.app[!m], style_color.app[!m]),
    g_strdup_printf("." CSS_GRAPH_BG   " {background:%s;}\n", style_color.def[g]),
    g_strdup_printf("." CSS_LEGEND_COL " {background:%s; color:%s;}\n", style_color.alt[g], style_color.alt[!g]),
#ifdef WITH_PLOT
    g_strdup_printf("." CSS_PLOT_BG    " {background:%s; color:%s;}\n", style_color.def[p], style_color.def[!p]),
    g_strdup_printf("." CSS_ROTOR_COL  " {color:%s;}\n", style_color.def[!p]),
    g_strdup_printf("." CSS_ROTOR_COL  ":hover {background:%s;}\n", style_color.gry[p]),
#endif
    NULL};
  char *str = g_strjoinv(NULL, items);
  for (char **ptr = items; *ptr; ptr++) g_free(*ptr);
  return str;
}

static gboolean is_theme_dark(const char *theme) {
  char *str = g_utf8_strdown(theme, -1);
  gboolean dark = g_str_has_suffix(str ? str : theme, SFFX_DIV1 DARK_SFFX);
  if (!dark) dark = g_str_has_suffix(str ? str : theme, SFFX_DIV2 DARK_SFFX);
  g_free(str); return dark;
}

static char* style_basetheme(const char *theme) {
  char *dup = g_strdup(theme);
  if (dup) {
    gboolean is_dark = is_theme_dark(theme);
    if (is_dark) {
      char *sfx = g_strrstr(dup, SFFX_DIV1 DARK_SFFX);
      if (!sfx) sfx = g_strrstr(dup, SFFX_DIV2 DARK_SFFX);
      if (sfx) *sfx = 0; }}
  return dup;
}

static char* style_prefer(gboolean dark) {
  GtkSettings *set = gtk_settings_get_default();
  if (!set) { WARN("%s: %s", ERROR_HDR, "gtk_settings_get_default()"); return NULL; }
  g_object_set(G_OBJECT(set), PROP_PREFER_DARK, dark, NULL);
  char *theme = NULL; g_object_get(set, PROP_THEME, &theme, NULL);
  if (!theme) { WARN("%s: g_object_get(%s)", ERROR_HDR, PROP_THEME); return NULL; }
  char *prefer = style_basetheme(theme); g_free(theme);
  ub_theme = prefer ? (strcasestr(prefer, "Yaru") != NULL) : false; // aux info for icons
  DEBUG("%s: %s", PROP_THEME, prefer);
  DEBUG("%s: %s", PROP_PREFER_DARK, dark ? ON_HDR : OFF_HDR);
  return prefer;
}

static GtkCssProvider *prov_theme, *prov_extra;

static void style_load_theme(GdkDisplay *display, const char *theme, const char *variant) {
  if (!display || !theme) return;
  GtkCssProvider *prov = gtk_css_provider_new(); g_return_if_fail(GTK_IS_CSS_PROVIDER(prov));
  gtk_css_provider_load_named(prov, theme, variant);
  if (prov_theme) gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(prov_theme));
  prov_theme = prov;
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(prov_theme),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void style_load_extra(GdkDisplay *display) {
  style_loaded = false;
  GtkCssProvider *prov = gtk_css_provider_new();
  g_return_if_fail(GTK_IS_CSS_PROVIDER(prov));
  char *data = style_extra_css(css_common);
  if (!data) { WARN("%s: %s", ERROR_HDR, "style_extra_css()"); g_object_unref(prov); return; }
  style_css_load(prov, data); g_free(data);
  if (prov_extra) gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(prov_extra));
  prov_extra = prov;
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(prov_extra),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  style_loaded = true;
}


// pub
//

const char* is_sysicon(const char **icon) {
  static GtkIconTheme *icon_theme;
  STYLE_GET_DISPLAY(NULL);
  if (!icon_theme && display) icon_theme = gtk_icon_theme_get_for_display(display);
  if (GTK_IS_ICON_THEME(icon_theme) && icon)
    for (int i = 0; (i < MAX_ICONS) && *icon; i++, icon++)
      if (gtk_icon_theme_has_icon(icon_theme, *icon)) return *icon;
  return NULL;
}

void style_set(void) {
  STYLE_GET_DISPLAY();
  char *prefer = style_prefer(opts.darktheme);
  if (prefer) { style_load_theme(display, prefer, opts.darktheme ? DARK_SFFX : NULL); g_free(prefer); }
  style_load_extra(display);
}

void style_free(void) {
  if (prov_theme) g_object_unref(prov_theme);
  if (prov_extra) g_object_unref(prov_extra);
}

