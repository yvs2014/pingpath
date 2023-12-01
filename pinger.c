
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "ui/appbar.h"
#include "tabs/ping.h"

#define PING "/bin/ping"
#define SKIPNAME PING ": "

t_ping_opts ping_opts = { .target = NULL, .count = COUNT, .timeout = TIMEOUT, .dns = true };

gchar* ping_errors[MAXTTL];

void on_stdout(GObject *stream, GAsyncResult *re, gpointer data);
void on_stderr(GObject *stream, GAsyncResult *re, gpointer data);

// aux
//
static t_procdata pingproc[MAXTTL];

static gchar* last_error(void) {
  static gchar last_error_buff[BUFF_SIZE];
  for (int i = MAXTTL; i > 0; i--) {
    gchar *err = ping_errors[i - 1];
    if (err) {
      snprintf(last_error_buff, sizeof(last_error_buff), "%s", err);
      return last_error_buff;
    }
  }
  return NULL;
}

void free_ping_errors(void) {
  for (int i = 0; i < MAXTTL; i++) UPD_STR(ping_errors[i], NULL);
}

static bool pings_active(void) {
  for (int i = 0; i < MAXTTL; i++) if (pingproc[i].active) return true;
  return false;
}

static void view_updater(bool reset) {
  if (ping_opts.timer) { g_source_remove(ping_opts.timer); ping_opts.timer = 0; }
  if (reset) ping_opts.timer = g_timeout_add(ping_opts.timeout * 1000, update_dynarea, NULL);
}

static void set_finish_flag(struct _GObject *a, struct _GAsyncResult *b, gpointer data) {
  t_procdata *p = data;
  if (p) {
    if (p->active) p->active = false;
    if (!pings_active()) {
      update_actions();
      view_updater(false);
      update_dynarea(NULL);
    }
  }
}
// EO-stat-viewer


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
    else LOG("ping[ttl=%d] finished (rc=%d)", p->ndx + 1, g_subprocess_get_status(p->proc));
  }
  if (!id) {
    set_finish_flag(NULL, NULL, p);
    p->proc = NULL;
  }
}

void pinger_stop(const gchar* reason) {
  for (int i = 0; i < MAXTTL; i++) stop_ping_at(i, reason);
  view_updater(false);
}

static bool create_pingproc(int at, t_procdata *p) {
  if (!p->out) p->out = g_string_sized_new(BUFF_SIZE);
  if (!p->err) p->err = g_string_sized_new(BUFF_SIZE);
  if (!p->out || !p->err) { WARN("%s failed", "g_string_sized_new()"); return false; }
  const gchar** argv = calloc(16, sizeof(gchar*)); int argc = 0;
  argv[argc++] = PING;
  argv[argc++] = "-OD";
  if (!ping_opts.dns) argv[argc++] = "-n";
  char sttl[16]; snprintf(sttl, sizeof(sttl), "-t%d", at + 1);            argv[argc++] = sttl;
  char scnt[16]; snprintf(scnt, sizeof(scnt), "-c%d", ping_opts.count);   argv[argc++] = scnt;
  char sitm[16]; snprintf(sitm, sizeof(sitm), "-i%d", ping_opts.timeout); argv[argc++] = sitm;
  char sWtm[16]; snprintf(sWtm, sizeof(sitm), "-W%d", ping_opts.timeout); argv[argc++] = sWtm;
  argv[argc++] = "--";
  argv[argc++] = ping_opts.target;
  GError *err = NULL;
  p->proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &err);
  if (err) { WARN("create subprocess: %s", err->message); return false; }
  g_subprocess_wait_check_async(p->proc, NULL, set_finish_flag, p);
  GInputStream *output = g_subprocess_get_stdout_pipe(p->proc);
  g_input_stream_read_async(output, p->out->str, p->out->allocated_len, G_PRIORITY_DEFAULT, NULL, on_stdout, p);
  GInputStream *errput = g_subprocess_get_stderr_pipe(p->proc);
  g_input_stream_read_async(errput, p->err->str, p->err->allocated_len, G_PRIORITY_DEFAULT, NULL, on_stderr, p);
  p->active = true;
  p->ndx = at;
  LOG("ping[ttl=%d] started", at + 1);
  return p->active;
}

void pinger_start(void) {
  if (!ping_opts.target) return;
  ping_opts.tout_usec = ping_opts.timeout * 1000000;
  clear_stat();
  for (int i = 0; i < MAXTTL; i++)
    if (!create_pingproc(i, &pingproc[i])) break;
  if (pings_active()) {
    pinger_clear_data();
    view_updater(true);
  }
}

void pinger_clear_data(void) {
  free_ping_errors();
  clear_stat();
  hops_no = MAXTTL;
  clear_dynarea();
}

void pinger_free(void) {
  free_ping_errors();
  free_stat();
  g_free(ping_opts.target); ping_opts.target = NULL;
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
  parse_input(p->ndx, s);
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
  s = g_strstrip(s); LOG("ERROR: %s", s);
  UPD_STR(ping_errors[p->ndx], s); s = last_error(); // save error and display last one
  set_errline(s);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->err->str, p->err->allocated_len,
    G_PRIORITY_DEFAULT, NULL, on_stderr, data);
}

