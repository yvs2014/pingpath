
#include "pinger.h"
#include "menu.h"

const char* PING = "/bin/ping";

t_procdata pingproc = { .proc = NULL, .active = false, .opts = { .target = NULL, .ttl = MAXTTL, .count = COUNT}};
GtkWidget *pinglines[MAXTTL];

static void set_finish_flag(struct _GObject *a, struct _GAsyncResult *b, gpointer data) {
  t_procdata *p = data;
  if (p) {
    if (p->active) p->active = false;
    update_menu(p);
  }
}

void stop_proc(t_procdata *p, const gchar* reason) {
  if (!p || !p->proc) return;
  const gchar *id = g_subprocess_get_identifier(p->proc);
  if (id) {
    LOG("stop[pid=%s] reason: %s", id ? id : "NA", reason);
    g_subprocess_send_signal(p->proc, SIGTERM);
    id = g_subprocess_get_identifier(p->proc);
    if (id) {
      g_usleep(20 * 1000);
      g_subprocess_force_exit(p->proc);
      g_usleep(20 * 1000);
      id = g_subprocess_get_identifier(p->proc);
      if (id) { WARN("Cannot stop subprocess[id=%s]", id); return; }
    }
    GError *err = NULL;
    g_subprocess_wait(p->proc, NULL, &err);
    if (err) { WARN("pid=%s: %s", id, err->message); }
    else {
      int rc = g_subprocess_get_status(p->proc);
      LOG("proc[ttl=%d] finished (rc=%d)", p->opts.ttl, rc);
    }
  }
  // get ready to new run
  p->active = false;
  update_menu(p);
}
 
void start_pings(t_procdata *p, GAsyncReadyCallback reset) {
  if (!p) return;
  if (!p->buff) p->buff = g_string_sized_new(1024);
  char sttl[16]; snprintf(sttl, sizeof(sttl), "-t%d", p->opts.ttl);
  char scnt[16]; snprintf(scnt, sizeof(scnt), "-c%d", p->opts.count);
  GError *err = NULL;
  p->proc = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &err, PING, scnt, sttl, p->opts.target, NULL);
  if (err) { WARN("create subprocess: %s", err->message); return; }
  g_subprocess_wait_check_async(p->proc, NULL, set_finish_flag, p);
  GInputStream *output = g_subprocess_get_stdout_pipe(p->proc);
  g_input_stream_read_async(output, p->buff->str, p->buff->allocated_len, G_PRIORITY_DEFAULT, NULL, reset, p);
  p->active = true;
  update_menu(p);
}

void on_output(GObject *stream, GAsyncResult *re, gpointer data) {
  t_procdata *p = data;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); stop_proc(p, "sz < 0"); return; }
  if (!sz) { stop_proc(p, "EOF"); return; } // EOF
  static char tmpline[1024];
  snprintf(tmpline, sizeof(tmpline), "%*.*s", sz, sz, p->buff->str);
  LOG("\"%s\"", tmpline);
  gtk_label_set_label(GTK_LABEL(pinglines[0]), tmpline);
  if (!gtk_widget_get_visible(pinglines[0])) gtk_widget_set_visible(pinglines[0], true);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->buff->str, p->buff->allocated_len, G_PRIORITY_DEFAULT, NULL, on_output, data);
}

