
#include "style.h"
#include "common.h"

int style_loaded;

#define PROP_THEME "gtk-theme-name"
#define PROP_PREFER "gtk-application-prefer-dark-theme"

#define DEF_BGROUND_DARK  "#151515"
#define APP_BGROUND_DARK  "#2c2c2c"
#define APP_BGROUND_LIGHT "#f0f0f0"

#define PAD   "8px"
#define PAD4  "4px"
#define PAD6  "6px"
#define PAD16 "16px"

static const gchar *css_dark_colors =
  "." CSS_BGROUND "{background:" DEF_BGROUND_DARK ";}"
  "." CSS_BGONBG "{background:" APP_BGROUND_DARK ";}"
  "popover > arrow, popover > contents {background:" APP_BGROUND_DARK "; box-shadow:none;}"
  "entry, entry.flat {background:" APP_BGROUND_DARK "; text-decoration:underline; border:none;}"
  "headerbar {background:" APP_BGROUND_DARK ";}"
  "headerbar:backdrop {background:" APP_BGROUND_DARK ";}"
  "notebook > header > tabs > tab { background-color:" APP_BGROUND_DARK ";}"
  "." CSS_EXP "{background:" APP_BGROUND_DARK ";}"
  "." CSS_LIGHT_BG "{background:" APP_BGROUND_LIGHT ";}"
;

static const gchar *css_common =
  "notebook > header > tabs {padding-left:0px; padding-right:0px;}"
  "notebook > header > tabs > tab {margin-left:0px; margin-right:0px;}"
  "vertical {padding:" PAD16 ";}"
  ".flat {padding:" PAD4 ";}"
  "." CSS_PAD "{padding:" PAD ";}"
  "." CSS_NOPAD "{min-height:0px;}"
  "." CSS_CHPAD "{padding-right:" PAD ";}"
  "." CSS_PAD6  "{padding-right:" PAD6 ";}"
  "." CSS_NOFRAME "{border:none;}"
  "." CSS_TAB "{padding:" PAD ";}"
  "." CSS_ROUNDED "{border-radius:5px; padding:" PAD16 ";}"
  "#" CSS_ID_DATETIME "{font-weight:500;}"
  "#" CSS_ID_NOTIFIER "{ filter: invert(100%); }"
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
  static gchar css_data[BUFF_SIZE];
  GdkDisplay *display = gdk_display_get_default();
  if (!display) { WARN_("no default display"); return; }
  GtkSettings *settings = gtk_settings_get_default();
  int l = g_snprintf(css_data, sizeof(css_data), "%s", css_common);
  if (!settings) { LOG_("no default settings"); }
  else { // try to prefer dark theme, otherwise leave default
    g_object_set(G_OBJECT(settings), PROP_PREFER, TRUE, NULL);
    int prefer_dark = get_gset_bool_by_key(settings, PROP_PREFER);
    gchar *theme = get_gset_str_by_key(settings, PROP_THEME);
    if ((prefer_dark >= 0) || theme) { // check out preferences
      if (prefer_dark || (theme ? strcasestr(theme, "dark") : false))
        if (l < sizeof(css_data)) g_strlcpy(css_data + l, css_dark_colors, sizeof(css_data) - l);
//      LOG("theme=%s prefer-dark=%d", theme, prefer_dark);
    }
    g_free(theme);
  }
  GtkCssProvider *css_provider = gtk_css_provider_new();
  g_return_if_fail(GTK_IS_CSS_PROVIDER(css_provider));
  CSS_PROVIDER_LOAD_FROM(css_provider, css_data);
  gtk_style_context_add_provider_for_display(display,
    GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css_provider);
  style_loaded = true;
}

