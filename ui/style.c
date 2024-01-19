
#include "style.h"
#include "common.h"

int style_loaded;

#define PROP_THEME "gtk-theme-name"
#define PROP_PREFER "gtk-application-prefer-dark-theme"

#define DEF_BGROUND_DARK  "#151515"
#define APP_BGROUND_DARK  "#2c2c2c"
#define APP_BGROUND_LIGHT "#f5f5f5"
#define LEGEND_BORDER     "#515151"

#define PAD   "8px"
#define PAD4  "4px"
#define PAD6  "6px"
#define PAD16 "16px"

static const gchar *css_dark_colors = " \
  ." CSS_BGROUND " {background:" DEF_BGROUND_DARK ";} \
  ." CSS_BGONBG " {background:" APP_BGROUND_DARK ";} \
  popover > arrow, popover > contents {background:" APP_BGROUND_DARK "; box-shadow:none;} \
  entry, entry.flat {background:" APP_BGROUND_DARK "; text-decoration:underline; border:none;} \
  headerbar {background:" APP_BGROUND_DARK ";} \
  headerbar:backdrop {background:" APP_BGROUND_DARK ";} \
  notebook > header > tabs > tab {background-color:" APP_BGROUND_DARK ";} \
  ." CSS_EXP " {background:" APP_BGROUND_DARK ";} \
  ." CSS_DARK_BG " {background:" APP_BGROUND_DARK "; color:" APP_BGROUND_DARK ";} \
  ." CSS_LIGHT_BG " {background:" APP_BGROUND_LIGHT "; color:" APP_BGROUND_LIGHT ";} \
  "
;

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
  ." CSS_INVERT " {filter:invert(100%);} \
  ." CSS_LEGEND " {background:" APP_BGROUND_LIGHT "; color:" APP_BGROUND_DARK \
    "; border: 1px outset " LEGEND_BORDER "; padding: " PAD4 " " PAD6 " " PAD4 " " PAD4 ";} \
  ." CSS_LEGEND_TEXT " {background:" APP_BGROUND_LIGHT "; color:" APP_BGROUND_DARK "; padding: 0;} \
  #" CSS_ID_DATETIME " {font-weight:500;} \
"
;

static GValue* get_gval(GtkSettings *gset, const char *key, GValue *val) {
  GParamSpec *prop = g_object_class_find_property(G_OBJECT_GET_CLASS(gset), key);
  if (!prop) return NULL;
  g_value_init(val, prop->value_type);
  g_object_get_property(G_OBJECT(gset), prop->name, val);
  return val;
}

static int get_gset_bool_by_key(GtkSettings *sets, const char *key) {
  GValue data = {0};
  GValue *val = get_gval(sets, key, &data);
  return (val && G_VALUE_HOLDS_BOOLEAN(val)) ? g_value_get_boolean(val) : -1;
}

static gchar* get_gset_str_by_key(GtkSettings *sets, const char *key) {
  GValue data = {0};
  GValue *val = get_gval(sets, key, &data);
  return (val && G_VALUE_HOLDS_STRING(val)) ? g_strdup_value_contents(val) : NULL;
}

#if GTK_MAJOR_VERSION == 4
#if GTK_MINOR_VERSION < 12
#define CSS_PROVIDER_LOAD_FROM(prov, str) gtk_css_provider_load_from_data(prov, str, -1)
#else
#define CSS_PROVIDER_LOAD_FROM(prov, str) gtk_css_provider_load_from_string(prov, str)
#endif
#endif

void style_init(void) {
  GdkDisplay *display = gdk_display_get_default();
  if (!display) { WARN_("no default display"); return; }
  GtkSettings *settings = gtk_settings_get_default();
  gchar** cs = g_new0(gchar*, 3);
  if (cs) {
    cs[0] = g_strdup(css_common);
    if (!settings) { cs[1] = g_strdup(css_dark_colors); LOG_("no default settings"); }
    else { // try to prefer dark theme, otherwise leave default
      g_object_set(G_OBJECT(settings), PROP_PREFER, TRUE, NULL);
      int prefer_dark = get_gset_bool_by_key(settings, PROP_PREFER);
      gchar *theme = get_gset_str_by_key(settings, PROP_THEME);
      if ((prefer_dark >= 0) || theme) { // check out preferences
        if (prefer_dark || (theme ? strcasestr(theme, "dark") : false)) cs[1] = g_strdup(css_dark_colors);
        LOG("theme=%s prefer-dark=%d", theme, prefer_dark);
      }
      g_free(theme);
    }
  }
  gchar *css_data = g_strjoinv("\n", cs); g_strfreev(cs);
  if (css_data) {
    GtkCssProvider *css_provider = gtk_css_provider_new();
    g_return_if_fail(GTK_IS_CSS_PROVIDER(css_provider));
    CSS_PROVIDER_LOAD_FROM(css_provider, css_data); g_free(css_data);
    gtk_style_context_add_provider_for_display(display,
      GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    style_loaded = true;
  } else WARN("no CSS data: %s failed", "g_strjoinv()");
}

