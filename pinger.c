
#include "pinger.h"
#include "parser.h"
#include "menu.h"
#include "stat.h"
#include "common.h"

#define PING "/bin/ping"
#define SKIPNAME PING ": "

t_ping_opts ping_opts = { .target = NULL, .count = COUNT, .timeout = TIMEOUT, .finish = true};

t_procdata pingproc[MAXTTL];
GtkWidget *pinglines[MAXTTL];
gchar* ping_errors[MAXTTL];
GtkWidget *errline;

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
  if (ping_errors[n]) g_free(ping_errors[n]);
  ping_errors[n] = g_strndup(s, BUFF_SIZE);
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
    update_menu();
  }
}

// public
//

void init_pinger(void) {
  for (int i = 0; i < MAXTTL; i++) {
    pingproc[i].ndx = -1;
    pingproc[i].active = false;
  }
}

void stop_ping_at(int at, const gchar* reason) {
  t_procdata *p = &(pingproc[at]);
  if (!p->proc) return;
  const gchar *id = g_subprocess_get_identifier(p->proc);
  if (id) {
    LOG("stop[pid=%s] reason: %s", id ? id : "NA", reason);
    g_subprocess_send_signal(p->proc, SIGTERM);
    id = g_subprocess_get_identifier(p->proc);
    if (id) { g_usleep(20 * 1000); id = g_subprocess_get_identifier(p->proc); } // wait a bit
    if (id) {
      g_subprocess_force_exit(p->proc);
      g_usleep(20 * 1000);
      id = g_subprocess_get_identifier(p->proc);
      if (id) { WARN("Cannot stop subprocess[id=%s]", id); return; }
    }
    GError *err = NULL;
    g_subprocess_wait(p->proc, NULL, &err);
    if (err) WARN("pid=%s: %s", id, err->message);
    else LOG("proc[ttl=%d] finished (rc=%d)", p->ndx + 1, g_subprocess_get_status(p->proc));
  }
  p->active = false;
}

void pinger_stop(const gchar* reason) {
  for (int i = 0; i < MAXTTL; i++) stop_ping_at(i, reason);
  free_ping_errors();
  update_menu();
}
 
void pinger_start(void) {
  if (ping_opts.target) for (int i = 0; i < MAXTTL; i++) {
    t_procdata *p = &(pingproc[i]);
    if (!p->out) p->out = g_string_sized_new(BUFF_SIZE);
    if (!p->err) p->err = g_string_sized_new(BUFF_SIZE);
    if (!p->out || !p->err) { WARN("%s failed", "g_string_sized_new()"); break; }
    const gchar** argv = calloc(16, sizeof(gchar*)); int argc = 0;
    argv[argc++] = PING;
    argv[argc++] = "-OD";
    char sttl[16]; snprintf(sttl, sizeof(sttl), "-t%d", i + 1);             argv[argc++] = sttl;
    char scnt[16]; snprintf(scnt, sizeof(scnt), "-c%d", ping_opts.count);   argv[argc++] = scnt;
    char sitm[16]; snprintf(sitm, sizeof(sitm), "-i%d", ping_opts.timeout); argv[argc++] = sitm;
    char sWtm[16]; snprintf(sWtm, sizeof(sitm), "-W%d", ping_opts.timeout); argv[argc++] = sWtm;
//  argv[argc++] = "-n";
    argv[argc++] = "--";
    argv[argc++] = ping_opts.target;
    GError *err = NULL;
    p->proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &err);
    if (err) { WARN("create subprocess: %s", err->message); break; }
    g_subprocess_wait_check_async(p->proc, NULL, set_finish_flag, p);
    GInputStream *output = g_subprocess_get_stdout_pipe(p->proc);
    g_input_stream_read_async(output, p->out->str, p->out->allocated_len, G_PRIORITY_DEFAULT, NULL, on_stdout, p);
    GInputStream *errput = g_subprocess_get_stderr_pipe(p->proc);
    g_input_stream_read_async(errput, p->err->str, p->err->allocated_len, G_PRIORITY_DEFAULT, NULL, on_stderr, p);
    p->active = true;
    p->ndx = i;
  }
  update_menu();
}

void on_stdout(GObject *stream, GAsyncResult *re, gpointer data) {
  static char obuff[BUFF_SIZE];
  t_procdata *p = data;
  if (p->ndx < 0) return;
  char *s = obuff;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); stop_ping_at(p->ndx, "sz < 0"); return; }
  if (!sz) { stop_ping_at(p->ndx, "EOF"); return; } // EOF
  snprintf(s, sizeof(obuff), "%*.*s", sz, sz, p->out->str);
  trim_nl(s); //LOG("%s", s);
  s = parse_input(p->ndx, s);
  GtkWidget* line = pinglines[p->ndx];
  bool vis = gtk_widget_get_visible(line);
  if (p->ndx < hops_no) {
    gtk_label_set_label(GTK_LABEL(line), s);
    if (!vis) gtk_widget_set_visible(line, true);
  } else if (vis) gtk_widget_set_visible(line, false);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->out->str, p->out->allocated_len,
    G_PRIORITY_DEFAULT, NULL, on_stdout, data);
}

void on_stderr(GObject *stream, GAsyncResult *re, gpointer data) {
  static char ebuff[BUFF_SIZE];
  t_procdata *p = data;
  if (p->ndx < 0) return;
  char *s = ebuff;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); return; }
  if (!sz) return; // EOF
  snprintf(s, sizeof(ebuff), "%*.*s", sz, sz, p->err->str);
  { int l = strlen(SKIPNAME); if (!strncmp(s, SKIPNAME, l)) s += l; } // skip program name
  trim_nl(s); LOG("ERROR: %s", s);
  save_err_at(p->ndx, s); s = all_errs(); // save error and get all together
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

void free_ping_error_at(int at) {
  if (ping_errors[at]) { g_free(ping_errors[at]); ping_errors[at] = NULL; }
}

void free_ping_error_from(int from) {
  for (int i = from; i < MAXTTL; i++) free_ping_error_at(i);
}

