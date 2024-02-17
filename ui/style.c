
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

#if GTK_MAJOR_VERSION == 4
#if GTK_MINOR_VERSION < 12
#define CSS_PROVIDER_LOAD_FROM(prov, str) gtk_css_provider_load_from_data(prov, str, -1)
#else
#define CSS_PROVIDER_LOAD_FROM(prov, str) gtk_css_provider_load_from_string(prov, str)
#endif
#endif

#define STYLE_GET_DISPLAY(re) GdkDisplay *display = gdk_display_get_default(); \
  if (!display) { WARN_("no default display"); return re; }

typedef struct style_colors {
  const char *def[2], *app[2], *gr[2];
} t_style_colors;

int style_loaded;
gboolean ub_theme;

//

static const t_style_colors style_color = {
  .def = { "#fdfdfd", "#151515"},
  .app = { BG_CONTRAST_LIGHT, BG_CONTRAST_DARK},
  .gr  = { "#f9f9f9", "#222222"},
};

static const gchar *css_common = " \
  notebook > header > tabs {padding-left:0; padding-right:0;} \
  notebook > header > tabs > tab {margin-left:0; margin-right:0;} \
  popover {background:transparent;} \
  vertical {padding:" PAD16 ";} \
  .flat {padding:" PAD4 ";} \
  ." CSS_PAD " {padding:" PAD ";} \
  ." CSS_NOPAD " {min-height:0;} \
  ." CSS_CHPAD " {padding-right:" PAD ";} \
  ." CSS_PAD6  " {padding-right:" PAD6 ";} \
  ." CSS_NOFRAME " {border:none;} \
  ." CSS_ROUNDED " {border-radius:5px; padding:" PAD16 ";} \
  ." CSS_ROUNDG " {border-radius:7px; padding:" PAD ";} \
  ." CSS_INVERT " {filter:invert(100%);} \
  ." CSS_LEGEND " {border: 1px outset " LEGEND_BORDER "; padding: " PAD4 " " PAD6 " " PAD4 " " PAD4 ";} \
  ." CSS_LEGEND_TEXT " {padding:0;} \
  ." CSS_BGONDARK  " {background:" BG_CONTRAST_DARK  ";color:" BG_CONTRAST_DARK  ";} \
  ." CSS_BGONLIGHT " {background:" BG_CONTRAST_LIGHT ";color:" BG_CONTRAST_LIGHT ";} \
  #" CSS_ID_DATETIME " {font-weight:500;} \
";

static gchar* style_extra_css(const gchar *common, gboolean dark, gboolean grdark) {
  int ndx = dark ? 1 : 0, grndx = grdark ? 1 : 0;
  const char *def = style_color.def[ndx], *def_gr = style_color.def[grndx],
    *app = style_color.app[ndx], *app_inv = style_color.app[!ndx],
    *lgnd_bg = style_color.gr[grndx], *lgnd_fg = style_color.gr[!grndx];
  gchar* items[] = {
    g_strdup_printf("%s\n", common),
    g_strdup_printf("." CSS_BGROUND " {background:%s;}", def),
    g_strdup_printf("popover > arrow, popover > contents {background:%s; box-shadow:none;}", app),
    g_strdup_printf("entry, entry.flat {background:%s; text-decoration:underline; border:none;}", app),
    g_strdup_printf("headerbar {background:%s;}", app),
    g_strdup_printf("headerbar:backdrop {background:%s;}", app),
    g_strdup_printf("notebook > header > tabs > tab {background-color:%s;}", app),
    g_strdup_printf("." CSS_EXP " {background:%s;}", app),
    g_strdup_printf("." CSS_DEF_BG " {background:%s; color:%s;}", app, app),
    g_strdup_printf("." CSS_INV_BG " {background:%s; color:%s;}", app_inv, app_inv),
    g_strdup_printf("." CSS_GR_BG  " {background:%s;}", def_gr),
    g_strdup_printf("." CSS_LEGEND_COL " {background:%s; color:%s;}", lgnd_bg, lgnd_fg),
    NULL};
  gchar *re = g_strjoinv(NULL, items);
  for (gchar **p = items; *p; p++) g_free(*p);
  return re;
};

static const gboolean is_theme_dark(const gchar *theme) {
  gboolean re = g_str_has_suffix(theme, SFFX_DIV1 DARK_SFFX);
  if (!re) re = g_str_has_suffix(theme, SFFX_DIV2 DARK_SFFX);
  return re;
}

static gchar* style_basetheme(const gchar *theme) {
  gchar *re = g_strdup(theme);
  if (re) {
    gboolean is_dark = is_theme_dark(theme);
    if (is_dark) {
      gchar *sfx = g_strrstr(re, SFFX_DIV1 DARK_SFFX);
      if (!sfx) sfx = g_strrstr(re, SFFX_DIV2 DARK_SFFX);
      if (sfx) *sfx = 0;
    }
  } else FAIL("g_strdup()");
  return re;
}

static gchar* style_prefer(gboolean dark) {
  GtkSettings *set = gtk_settings_get_default();
  if (!set) { WARN_("no default settings"); return NULL; }
  g_object_set(G_OBJECT(set), PROP_PREFER, dark, NULL);
  gchar *theme = NULL; g_object_get(set, PROP_THEME, &theme, NULL);
  if (!theme) { WARN_("No theme"); return NULL; }
  gchar *prefer = style_basetheme(theme); g_free(theme);
  ub_theme = prefer ? (strcasestr(prefer, "Yaru") != NULL) : false; // aux info for icons
  LOG("%stheme='%s' prefer-dark=%d", ub_theme ? "Ubuntu " : "", prefer, dark);
  return prefer;
}

static GtkCssProvider *prov_theme, *prov_extra;

static void style_load_theme(GdkDisplay *display, const gchar *theme, const gchar *variant) {
  if (!display || !theme) return;
  GtkCssProvider *prov = gtk_css_provider_new();
  g_return_if_fail(GTK_IS_CSS_PROVIDER(prov));
  gtk_css_provider_load_named(prov, theme, variant);
  if (prov_theme) gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(prov_theme));
  prov_theme = prov;
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(prov_theme),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void style_load_extra(GdkDisplay *display, gboolean dark, gboolean darkgr) {
  style_loaded = false;
  GtkCssProvider *prov = gtk_css_provider_new();
  g_return_if_fail(GTK_IS_CSS_PROVIDER(prov));
  gchar *data = style_extra_css(css_common, dark, darkgr);
  if (!data) { WARN_("no CSS data"); g_object_unref(prov); return; }
  CSS_PROVIDER_LOAD_FROM(prov, data); g_free(data);
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

void style_set(gboolean dark, gboolean darkgr) {
  STYLE_GET_DISPLAY();
  gchar *prefer = style_prefer(dark);
  if (prefer) { style_load_theme(display, prefer, dark ? "dark" : NULL); g_free(prefer); }
  style_load_extra(display, dark, darkgr);
}

void style_free(void) {
  if (prov_theme) g_object_unref(prov_theme);
  if (prov_extra) g_object_unref(prov_extra);
}

