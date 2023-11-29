
#include <gtk/gtk.h>
#include "styles.h"

void init_styles(void) {
  GtkCssProvider *css = gtk_css_provider_new();
  gchar *css_data =
    "#" CSS_ID_WIN "{background-color:#151515;}"
    "#" CSS_ID_APPBAR "{background-color:#2c2c2c;}"
    "#" CSS_ID_DATETIME "{font-weight:500;}";
  gtk_css_provider_load_from_string(css, css_data);
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
    GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);
}

