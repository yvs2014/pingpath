
#include "pinger.h"
#include "menu.h"

#define BUFF_SIZE 1024
#define PING "/bin/ping"
#define SKIPNAME PING ": "

t_procdata pingproc = { .proc = NULL, .active = false, .opts = { .target = NULL, .ttl = MAXTTL, .count = COUNT}};
GtkWidget *pinglines[MAXTTL];
GtkWidget *errline;
char* ping_errors[MAXTTL];

void on_stdout(GObject *stream, GAsyncResult *re, gpointer data);
void on_stderr(GObject *stream, GAsyncResult *re, gpointer data);

// aux
//
static char* trim_nl(char *s) {
  if (s) {
    int l = strnlen(s, BUFF_SIZE) - 1;
    if ((l >= 0) && (s[l] == '\n')) s[l] = 0;
  }
  return s;
}

static void save_err_at(int n, const char *s) {
  if (ping_errors[n]) free(ping_errors[n]);
  ping_errors[n] = strndup(s, BUFF_SIZE);
}

static char* all_errs(void) {
  static char combined_errs[1024];
  combined_errs[0] = 0;
  for (int i = 0, l = 0; (i < MAXTTL) && (l < BUFF_SIZE); i++)
    if (ping_errors[i]) l += snprintf(combined_errs + l, sizeof(combined_errs) - l, "%s\n", ping_errors[i]);
  return combined_errs;
}

static void set_finish_flag(struct _GObject *a, struct _GAsyncResult *b, gpointer data) {
  t_procdata *p = data;
  if (p) {
    if (p->active) p->active = false;
    update_menu(p);
  }
}

// public
//
void pinger_stop(t_procdata *p, const gchar* reason) {
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
  free_ping_errors();
  update_menu(p);
}
 
void pinger_start(t_procdata *p) {
  if (!p) return;
  if (!p->out) p->out = g_string_sized_new(BUFF_SIZE);
  if (!p->err) p->err = g_string_sized_new(BUFF_SIZE);
  if (!p->out || !p->err) return;
  char sttl[16]; snprintf(sttl, sizeof(sttl), "-t%d", p->opts.ttl);
  char scnt[16]; snprintf(scnt, sizeof(scnt), "-c%d", p->opts.count);
  GError *err = NULL;
  p->proc = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
    &err, PING, scnt, sttl, p->opts.target, NULL);
  if (err) { WARN("create subprocess: %s", err->message); return; }
  g_subprocess_wait_check_async(p->proc, NULL, set_finish_flag, p);
  GInputStream *output = g_subprocess_get_stdout_pipe(p->proc);
  g_input_stream_read_async(output, p->out->str, p->out->allocated_len, G_PRIORITY_DEFAULT, NULL, on_stdout, p);
  GInputStream *errput = g_subprocess_get_stderr_pipe(p->proc);
  g_input_stream_read_async(errput, p->err->str, p->err->allocated_len, G_PRIORITY_DEFAULT, NULL, on_stderr, p);
  p->active = true;
  update_menu(p);
}

void on_stdout(GObject *stream, GAsyncResult *re, gpointer data) {
  static char obuff[BUFF_SIZE];
  char *s = obuff;
  t_procdata *p = data;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); pinger_stop(p, "sz < 0"); return; }
  if (!sz) { pinger_stop(p, "EOF"); return; } // EOF
  snprintf(s, sizeof(obuff), "%*.*s", sz, sz, p->out->str);
  trim_nl(s); LOG("%s", s);
  gtk_label_set_label(GTK_LABEL(pinglines[0]), s);
  if (!gtk_widget_get_visible(pinglines[0])) gtk_widget_set_visible(pinglines[0], true);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->out->str, p->out->allocated_len,
    G_PRIORITY_DEFAULT, NULL, on_stdout, data);
}

void on_stderr(GObject *stream, GAsyncResult *re, gpointer data) {
  static char ebuff[BUFF_SIZE];
  char *s = ebuff;
  t_procdata *p = data;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); return; }
  if (!sz) return; // EOF
  snprintf(s, sizeof(ebuff), "%*.*s", sz, sz, p->err->str);
  { int l = strlen(SKIPNAME); if (!strncmp(s, SKIPNAME, l)) s += l; } // skip program name
  trim_nl(s); LOG("ERROR: %s", s);
  save_err_at(p->opts.ttl - 1, s); s = all_errs(); // save error and get all together
  gtk_label_set_label(GTK_LABEL(errline), s);
  if (!gtk_widget_get_visible(errline)) gtk_widget_set_visible(errline, true);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->err->str, p->err->allocated_len,
    G_PRIORITY_DEFAULT, NULL, on_stderr, data);
}

void clear_errline(void) {
  gtk_label_set_label(GTK_LABEL(errline), NULL);
  if (gtk_widget_get_visible(errline)) gtk_widget_set_visible(errline, false);
  free_ping_errors();
}

void free_ping_errors(void) {
  for (int i = 0; i < MAXTTL; i++) if (ping_errors[i]) { free(ping_errors[i]); ping_errors[i] = NULL; }
}

