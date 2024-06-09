
#include "style.h"

#define LEGEND_BORDER "#515151"

#define PAD   "8px"
#define PAD4  "4px"
#define PAD6  "6px"
#define PAD16 "16px"

#define PROP_PREFER "gtk-application-prefer-dark-theme"
#define PROP_THEME  "gtk-theme-name"
#define DARK_SFFX   "dark"
#define SFFX_DIV1   "-"
#define SFFX_DIV2   ":"

#define BG_CONTRAST_DARK  "#2c2c2c"
#define BG_CONTRAST_LIGHT "#f5f5f5"

#define STYLE_GET_DISPLAY(re) GdkDisplay *display = gdk_display_get_default(); \
  if (!display) { WARN_("no default display"); return re; }

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
popover {background:transparent;}\n\
vertical {padding:" PAD16 ";}\n\
.flat {padding:" PAD4 ";}\n\
." CSS_PAD " {padding:" PAD ";}\n\
." CSS_NOPAD " {min-height:0;}\n\
." CSS_CHPAD " {padding-right:" PAD ";}\n\
." CSS_PAD6  " {padding-right:" PAD6 ";}\n\
." CSS_NOFRAME " {border:none;}\n\
." CSS_ROUNDED " {border-radius:5px; padding:" PAD16 ";}\n\
." CSS_ROUNDG " {border-radius:7px; padding:" PAD ";}\n\
." CSS_INVERT " {filter:invert(100%);}\n\
." CSS_LEGEND " {border: 1px outset " LEGEND_BORDER "; padding: " PAD4 " " PAD6 " " PAD4 " " PAD4 ";}\n\
." CSS_LEGEND_TEXT " {padding:0;}\n\
." CSS_ROTOR  " {border: 0; padding: " PAD4 " " PAD6 " " PAD4 " " PAD4 ";}\n\
." CSS_BGONDARK  " {background:" BG_CONTRAST_DARK  ";color:" BG_CONTRAST_DARK  ";}\n\
." CSS_BGONLIGHT " {background:" BG_CONTRAST_LIGHT ";color:" BG_CONTRAST_LIGHT ";}\n\
#" CSS_ID_DATETIME " {font-weight:500;}\n";

static void style_css_load(GtkCssProvider *prov, const char *str) {
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_css_provider_load_from_string(prov, str);
#else
  gtk_css_provider_load_from_data(prov, str, -1);
#endif
}

static char* style_extra_css(const char *common, unsigned darkbits) {
  int n0 = (darkbits >> 0) & 0x1; // main
  int n1 = (darkbits >> 1) & 0x1; // graph
#ifdef WITH_PLOT
  int n2 = (darkbits >> 2) & 0x1; // plot
#endif
  const char *col = style_color.app[n0];
  char* items[] = {
    g_strdup_printf("%s\n", common),
    g_strdup_printf("." CSS_BGROUND " {background:%s;}\n", style_color.def[n0]),
    g_strdup_printf("popover > arrow, popover > contents {background:%s; box-shadow:none;}\n", col),
    g_strdup_printf("entry, entry.flat {background:%s; text-decoration:underline; border:none;}\n", col),
    g_strdup_printf("headerbar {background:%s;}\n", col),
    g_strdup_printf("headerbar:backdrop {background:%s;}\n", col),
    g_strdup_printf("notebook > header > tabs > tab {background-color:%s;}\n", col),
    g_strdup_printf("." CSS_EXP " {background:%s;}\n", col),
    g_strdup_printf("." CSS_DEF_BG " {background:%s; color:%s;}\n", col, col),
    g_strdup_printf("." CSS_INV_BG " {background:%s; color:%s;}\n", style_color.app[!n0], style_color.app[!n0]),
    g_strdup_printf("." CSS_GRAPH_BG   " {background:%s;}\n", style_color.def[n1]),
    g_strdup_printf("." CSS_LEGEND_COL " {background:%s; color:%s;}\n", style_color.alt[n1], style_color.alt[!n1]),
#ifdef WITH_PLOT
    g_strdup_printf("." CSS_PLOT_BG    " {background:%s; color:%s;}\n", style_color.def[n2], style_color.def[!n2]),
    g_strdup_printf("." CSS_ROTOR_COL  " {color:%s;}\n", style_color.def[!n2]),
    g_strdup_printf("." CSS_ROTOR_COL  ":hover {background:%s;}\n", style_color.gry[n2]),
#endif
    NULL};
  char *re = g_strjoinv(NULL, items);
  for (char **p = items; *p; p++) g_free(*p);
  return re;
#undef MN_NDX
#undef GR_NDX
#ifdef WITH_PLOT
#undef PL_NDX
#endif
};

static const gboolean is_theme_dark(const char *theme) {
  gboolean re = g_str_has_suffix(theme, SFFX_DIV1 DARK_SFFX);
  if (!re) re = g_str_has_suffix(theme, SFFX_DIV2 DARK_SFFX);
  return re;
}

static char* style_basetheme(const char *theme) {
  char *re = g_strdup(theme);
  if (re) {
    gboolean is_dark = is_theme_dark(theme);
    if (is_dark) {
      char *sfx = g_strrstr(re, SFFX_DIV1 DARK_SFFX);
      if (!sfx) sfx = g_strrstr(re, SFFX_DIV2 DARK_SFFX);
      if (sfx) *sfx = 0;
    }
  } else FAIL("g_strdup()");
  return re;
}

static char* style_prefer(gboolean dark) {
  GtkSettings *set = gtk_settings_get_default();
  if (!set) { WARN_("no default settings"); return NULL; }
  g_object_set(G_OBJECT(set), PROP_PREFER, dark, NULL);
  char *theme = NULL; g_object_get(set, PROP_THEME, &theme, NULL);
  if (!theme) { WARN_("No theme"); return NULL; }
  char *prefer = style_basetheme(theme); g_free(theme);
  ub_theme = prefer ? (strcasestr(prefer, "Yaru") != NULL) : false; // aux info for icons
  LOG("%stheme='%s' prefer-dark=%d", ub_theme ? "Ubuntu " : "", prefer, dark);
  return prefer;
}

static GtkCssProvider *prov_theme, *prov_extra;

static void style_load_theme(GdkDisplay *display, const char *theme, const char *variant) {
  if (!display || !theme) return;
  GtkCssProvider *prov = gtk_css_provider_new();
  g_return_if_fail(GTK_IS_CSS_PROVIDER(prov));
  gtk_css_provider_load_named(prov, theme, variant);
  if (prov_theme) gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(prov_theme));
  prov_theme = prov;
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(prov_theme),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void style_load_extra(GdkDisplay *display, gboolean dark, gboolean darkgraph, gboolean darkplot) {
  style_loaded = false;
  GtkCssProvider *prov = gtk_css_provider_new();
  g_return_if_fail(GTK_IS_CSS_PROVIDER(prov));
  char *data = style_extra_css(css_common, dark | (darkgraph << 1)
#ifdef WITH_PLOT
    | (darkplot << 2)
#endif
  );
  if (!data) { WARN_("no CSS data"); g_object_unref(prov); return; }
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

void style_set(gboolean darkmain, gboolean darkgraph, gboolean darkplot) {
  STYLE_GET_DISPLAY();
  char *prefer = style_prefer(darkmain);
  if (prefer) { style_load_theme(display, prefer, darkmain ? "dark" : NULL); g_free(prefer); }
  style_load_extra(display, darkmain, darkgraph, darkplot);
}

void style_free(void) {
  if (prov_theme) g_object_unref(prov_theme);
  if (prov_extra) g_object_unref(prov_extra);
}

