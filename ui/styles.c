
#include <gtk/gtk.h>

#include "styles.h"
#include "common.h"

int css_loaded;

#define PROP_THEME "gtk-theme-name"
#define PROP_PREFER "gtk-application-prefer-dark-theme"

#define DEF_BGROUND_DARK "#151515"
#define APP_BGROUND_DARK "#2c2c2c"

static const gchar *css_dark_colors =
  "#" CSS_ID_WIN "{background-color:" DEF_BGROUND_DARK ";}"
  "#" CSS_ID_APPBAR "{background-color:" APP_BGROUND_DARK ";}"
  "#" CSS_ID_DYNAREA "{background-color:" DEF_BGROUND_DARK ";}"
  "#" CSS_ID_HDRAREA "{background-color:" DEF_BGROUND_DARK ";}"
;

static const gchar *css_common =
  "#" CSS_ID_PINGTAB "{padding:" PAD " " PAD2 ";}"
  "#" CSS_ID_DATETIME "{font-weight:500;}"
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

void init_css_styles(void) {
  static gchar css_data[BUFF_SIZE];
  GdkDisplay *display = gdk_display_get_default();
  if (!display) { WARN("%s", "no default display"); return; }
  GtkSettings *settings = gtk_settings_get_default();
  int l = g_snprintf(css_data, sizeof(css_data), "%s", css_common);
  if (!settings) { LOG("%s", "no default settings"); }
  else {
    int prefer_dark = get_gset_bool_by_key(settings, PROP_PREFER);
    gchar *theme = get_gset_str_by_key(settings, PROP_THEME);
    if ((prefer_dark >= 0) || theme) { // check out preferences
      if (prefer_dark || strcasestr(theme, "dark"))
        g_snprintf(css_data + l, sizeof(css_data) - l, "%s", css_dark_colors);
      LOG("theme=%s prefer-dark=%d", theme, prefer_dark);
    }
    g_free(theme);
  }
  GtkCssProvider *css_provider = gtk_css_provider_new();
  g_return_if_fail(GTK_CSS_PROVIDER(css_provider));
  gtk_css_provider_load_from_string(css_provider, css_data);
  gtk_style_context_add_provider_for_display(display,
    GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css_provider);
  css_loaded = true;
}

