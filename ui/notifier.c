
#include "notifier.h"
#include "gtk/gtkrevealer.h"
#include "style.h"

typedef struct notifier {
  GtkWidget *label, *box, *reveal;
  guint autohide;
} t_notifier;

static t_notifier notifier;
static gboolean nt_bglight;

static gboolean nt_autohide(t_notifier *nt) {
  if (nt) {
    if (GTK_IS_REVEALER(nt->reveal)) gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), false);
    nt->autohide = 0;
  }
  return G_SOURCE_REMOVE;
}

static void nt_inform(const gchar *mesg) {
  if (mesg) {
    LOG_(mesg);
    if (GTK_IS_LABEL(notifier.label) && GTK_IS_REVEALER(notifier.reveal)) {
      if (notifier.autohide) nt_autohide(&notifier);
      gtk_label_set_text(GTK_LABEL(notifier.label), mesg);
      if (nt_bglight == bg_light) {
        nt_bglight = !bg_light;
        gtk_widget_remove_css_class(notifier.box, bg_light ? CSS_LIGHT_BG : CSS_DARK_BG);
        gtk_widget_add_css_class(notifier.box, nt_bglight ? CSS_LIGHT_BG : CSS_DARK_BG);
      }
      gtk_revealer_set_reveal_child(GTK_REVEALER(notifier.reveal), true);
      notifier.autohide = g_timeout_add(AUTOHIDE_IN, (GSourceFunc)nt_autohide, &notifier);
    }
  }
}


// pub
//

GtkWidget* notifier_init(GtkWidget *base) {
  if (!GTK_IS_WIDGET(base)) return NULL;
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(GTK_IS_OVERLAY(over), NULL);
  notifier.box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(notifier.box), NULL);
  notifier.label = gtk_label_new(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(notifier.label), NULL);
  if (style_loaded) {
    nt_bglight = !bg_light;
    gtk_widget_add_css_class(notifier.box, nt_bglight ? CSS_LIGHT_BG : CSS_DARK_BG);
    gtk_widget_add_css_class(notifier.box, CSS_ROUNDED);
    gtk_widget_set_name(notifier.label, CSS_ID_NOTIFIER);
  }
  gtk_widget_set_visible(notifier.label, true);
  gtk_widget_set_can_focus(notifier.label, false);
  gtk_widget_set_hexpand(notifier.label, true);
  gtk_widget_set_halign(notifier.label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(notifier.label, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(notifier.box), notifier.label);
  //
  notifier.reveal = gtk_revealer_new();
  g_return_val_if_fail(GTK_IS_REVEALER(notifier.reveal), NULL);
  gtk_widget_set_visible(notifier.reveal, true);
  gtk_widget_set_can_focus(notifier.reveal, false);
  gtk_widget_set_hexpand(notifier.reveal, true);
  gtk_widget_set_halign(notifier.reveal, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(notifier.reveal, GTK_ALIGN_CENTER);
  gtk_revealer_set_child(GTK_REVEALER(notifier.reveal), notifier.box);
  gtk_revealer_set_reveal_child(GTK_REVEALER(notifier.reveal), false);
  gtk_revealer_set_transition_type(GTK_REVEALER(notifier.reveal), GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), notifier.reveal);
  gtk_overlay_set_child(GTK_OVERLAY(over), base);
  return over;
}

void notifier_inform(const gchar *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  gchar *str = g_strdup_vprintf(fmt, ap);
  if (str) { cli ? CLILOG(str) : nt_inform(str); g_free(str); }
  va_end(ap);
}

