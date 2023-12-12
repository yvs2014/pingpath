
#include "pinger.h"
#include "common.h"
#include "parser.h"
#include "stat.h"
#include "ui/appbar.h"
#include "tabs/ping.h"

#define PING "/bin/ping"
#define SKIPNAME PING ": "

t_opts opts = { .target = NULL, .dns = true, .count = DEF_COUNT, .qos = DEF_QOS, .size = DEF_PSIZE,
  .min = 0, .lim = MAXTTL, .timeout = DEF_TOUT, .tout_usec = DEF_TOUT * 1000000 };
t_pinger_state pinger_state;
guint stat_timer;

typedef struct proc {
  GSubprocess *proc;
  bool active;        // process state
  GString *out, *err; // for received input data and errors
  int ndx;            // index in internal list
} t_proc;

static t_proc pings[MAXTTL];
static gchar* ping_errors[MAXTTL];

// aux
//

static gchar* last_error(void) {
  static gchar last_error_buff[BUFF_SIZE];
  for (int i = opts.lim; i > opts.min; i--) {
    gchar *err = ping_errors[i - 1];
    if (err) {
      g_strlcpy(last_error_buff, err, sizeof(last_error_buff));
      return last_error_buff;
    }
  }
  return NULL;
}

static void view_updater(bool reset) {
  if (stat_timer) { g_source_remove(stat_timer); stat_timer = 0; }
  if (reset) stat_timer = g_timeout_add(opts.timeout * 1000, pingtab_update, NULL);
}

static bool pinger_active(void) {
  bool active = false;
  for (int i = opts.min; i < opts.lim; i++) if (pings[i].active) { active = true; break; }
  if (pinger_state.run != active) {
    pinger_state.run = active;
    view_updater(active);
    appbar_update();
  }
  return active;
}

static void set_finish_flag(struct _GObject *a, struct _GAsyncResult *b, gpointer data) {
  static gchar *nodata_mesg = "No data";
  int a_state = -1;
  t_proc *p = data;
  if (p) {
    if (p->active) p->active = false;
    a_state = pinger_active();
    if (!a_state) pingtab_wrap_update(); // final update
  }
  if (!pinger_state.gotdata) {
    if (a_state < 0) a_state = pinger_active();
    if (!a_state && (!info_mesg || (info_mesg == notyet_mesg))) pingtab_set_error(nodata_mesg);
  }
}

static void on_stdout(GObject *stream, GAsyncResult *re, gpointer data) {
  static char obuff[BUFF_SIZE];
  t_proc *p = data;
  if (p->ndx < 0) return;
  char *s = obuff;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); pinger_stop_nth(p->ndx, "sz < 0"); return; }
  if (!sz) { pinger_stop_nth(p->ndx, "EOF"); stat_last_tx(p->ndx); return; } // EOF
  g_snprintf(s, sizeof(obuff), "%*.*s", sz, sz, p->out->str);
  parser_parse(p->ndx, s);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->out->str, p->out->allocated_len,
    G_PRIORITY_DEFAULT, NULL, on_stdout, data);
}

static void on_stderr(GObject *stream, GAsyncResult *re, gpointer data) {
  static char ebuff[BUFF_SIZE];
  t_proc *p = data;
  if (p->ndx < 0) return;
  char *s = ebuff;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { WARN("stream read: %s", err ? err->message : ""); return; }
  if (!sz) return; // EOF
  g_snprintf(s, sizeof(ebuff), "%*.*s", sz, sz, p->err->str);
  { int l = strlen(SKIPNAME); if (!strncmp(s, SKIPNAME, l)) s += l; } // skip program name
  s = g_strstrip(s); LOG("ERROR: %s", s);
  UPD_STR(ping_errors[p->ndx], s); s = last_error(); // save error and display last one
  pingtab_set_error(s);
  g_input_stream_read_async(G_INPUT_STREAM(stream), p->err->str, p->err->allocated_len,
    G_PRIORITY_DEFAULT, NULL, on_stderr, data);
}

#define MAX_ARGC 32

static bool create_ping(int at, t_proc *p) {
  if (!p->out) p->out = g_string_sized_new(BUFF_SIZE);
  if (!p->err) p->err = g_string_sized_new(BUFF_SIZE);
  if (!p->out || !p->err) { WARN("%s failed", "g_string_sized_new()"); return false; }
  const gchar** argv = calloc(MAX_ARGC, sizeof(gchar*)); int argc = 0;
  argv[argc++] = PING;
  argv[argc++] = "-OD";
  if (!opts.dns) argv[argc++] = "-n";
  char sttl[16]; g_snprintf(sttl, sizeof(sttl), "-t%d", at + 1);       argv[argc++] = sttl;
  char scnt[16]; g_snprintf(scnt, sizeof(scnt), "-c%d", opts.count);   argv[argc++] = scnt;
  char sitm[16]; g_snprintf(sitm, sizeof(sitm), "-i%d", opts.timeout); argv[argc++] = sitm;
  char sWtm[16]; g_snprintf(sWtm, sizeof(sitm), "-W%d", opts.timeout); argv[argc++] = sWtm;
  char sqos[16]; if (opts.qos != DEF_QOS) {
    g_snprintf(sqos, sizeof(sqos), "-Q%d",   opts.qos);  argv[argc++] = sqos; }
  char spad[64]; if (strncmp(opts.pad, DEF_PPAD, sizeof(opts.pad))) {
    g_snprintf(spad, sizeof(spad), "-p%s", opts.pad);  argv[argc++] = spad; }
  char spsz[16]; if (opts.size != DEF_PSIZE) {
    g_snprintf(spsz, sizeof(spsz), "-s%d",   opts.size); argv[argc++] = spsz; }
  char sipv[4];  if ((opts.ipv == 4) || (opts.ipv == 6)) {
    g_snprintf(sipv, sizeof(sipv), "-%d",    opts.ipv);  argv[argc++] = sipv; }
  argv[argc++] = "--";
  argv[argc++] = opts.target;
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
#ifdef DEBUGGING
  gchar* deb_argv[MAX_ARGC];
  memcpy(deb_argv, argv, sizeof(deb_argv));
  gchar *deb_argv_s = g_strjoinv(", ", deb_argv);
  DEBUG("ping[ttl=%d]: argv[%s]", at + 1, deb_argv_s);
  g_free(deb_argv_s);
#endif
  LOG("ping[ttl=%d] started", at + 1);
  return p->active;
}


// pub
//

void pinger_init(void) {
  g_strlcpy(opts.pad, DEF_PPAD, sizeof(opts.pad));
  for (int i = opts.min; i < opts.lim; i++) { pings[i].ndx = -1; pings[i].active = false; }
}

void pinger_start(void) {
  if (!opts.target) return;
  stat_clear(true);
  for (int i = opts.min; i < opts.lim; i++) if (!create_ping(i, &pings[i])) break;
  if (pinger_active()) pinger_clear_data(true);
}

void pinger_stop_nth(int nth, const gchar* reason) {
  t_proc *p = &(pings[nth]);
  if (p->proc) {
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
    if (!id) { set_finish_flag(NULL, NULL, p); p->proc = NULL; }
  }
}

void pinger_stop(const gchar* reason) {
  for (int i = opts.min; i < opts.lim; i++) pinger_stop_nth(i, reason);
  pinger_active();
}

void pinger_free(void) {
  pinger_free_errors();
  stat_free();
  g_free(opts.target); opts.target = NULL;
}

void pinger_free_errors(void) { for (int i = opts.min; i < opts.lim; i++) pinger_free_nth_error(i); }

inline void pinger_free_nth_error(int nth) { UPD_STR(ping_errors[nth], NULL); }

void pinger_clear_data(bool clean) {
  pinger_free_errors();
  stat_clear(clean);
  hops_no = opts.lim;
  pingtab_clear();
}

bool pinger_within_range(int min, int max, int got) { // 1..MAXTTL
  if (min > max) return false;
  if ((min < 1) || (min > MAXTTL)) return false;
  if ((max < 1) || (max > MAXTTL)) return false;
  return ((min <= got) && (got <= max));
}

