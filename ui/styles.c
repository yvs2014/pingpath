
#include <gtk/gtk.h>
#include "styles.h"

void init_styles(void) {
  GtkCssProvider *css = gtk_css_provider_new();
  gchar *css_data =
    "#" CSS_ID_WIN "{background-color:" DEF_BGROUND ";}"
    "#" CSS_ID_APPBAR "{background-color:" APP_BGROUND ";}"
    "#" CSS_ID_PINGTAB "{padding:" PAD " " PAD2 ";}"
    "#" CSS_ID_DATETIME "{font-weight:500;}"
    "#" CSS_ID_DYNAREA "{background-color:" DEF_BGROUND ";}"
    "#" CSS_ID_HDRAREA "{background-color:" DEF_BGROUND ";}"
    "#" CSS_IDR "{background-color:red;}"
    "#" CSS_IDG "{background-color:green;}"
    "#" CSS_IDB "{background-color:blue;}"
    "#" CSS_IDY "{background-color:yellow;}"
    "#" CSS_IDC "{background-color:cyan;}"
  ;
  gtk_css_provider_load_from_string(css, css_data);
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
    GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);
}

