
#include "pinger.h"
#include "common.h"
#include "parser.h"
#include "stat.h"
#include "dns.h"
#include "whois.h"
#include "ui/appbar.h"
#include "ui/notifier.h"
#include "tabs/ping.h"
#include "tabs/graph.h"

#define PING "/bin/ping"
#define SKIPNAME PING ": "

#define RESET_ISTREAM(ins, buff, cb, data) g_input_stream_read_async(G_INPUT_STREAM(ins), \
  (buff), BUFF_SIZE, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)(cb), (data))

#define CLEAR_G_OBJECT(obj) g_clear_object(obj) // NOLINT(bugprone-sizeof-expression)

t_opts opts = { .target = NULL, .dns = true, .cycles = DEF_CYCLES, .qos = DEF_QOS, .size = DEF_PSIZE,
  .min = 0, .lim = MAXTTL, .timeout = DEF_TOUT, .tout_usec = DEF_TOUT * 1000000, .logmax = DEF_LOGMAX,
  .graph = GRAPH_TYPE_CURVE, .legend = true };
t_pinger_state pinger_state;
guint stat_timer;

typedef struct proc {
  GSubprocess *proc;
  gboolean active; // process state
  int sig;         // signal of own stop
  int ndx;         // index in internal list
  char *out, *err; // stdout, stderr buffers
} t_proc;

static t_proc pings[MAXTTL];
static gchar* ping_errors[MAXTTL];

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

static void tab_updater(gboolean reset) {
  if (stat_timer) { g_source_remove(stat_timer); stat_timer = 0; }
  if (reset) stat_timer = g_timeout_add(opts.timeout * 1000, pinger_update_tabs, NULL);
}

static gboolean pinger_active(void) {
  gboolean active = false;
  for (int i = opts.min; i < opts.lim; i++) if (pings[i].active) { active = true; break; }
  if (pinger_state.run != active) {
    pinger_state.run = active;
    tab_updater(active);
    appbar_update();
    if (!active) PP_NOTIFY("%s finished", "Pings");
  }
  return active;
}

static void process_stopped(GObject *process, GAsyncResult *result, t_proc *proc) {
  static gchar *nodata_mesg = "No data";
  if (G_IS_SUBPROCESS(process)) {
    GError *error = NULL;
    gboolean okay = g_subprocess_wait_check_finish(G_SUBPROCESS(process), result, &error);
    int sig = proc ? proc->sig : 0;
    if (!okay && sig) {
      int rc = g_subprocess_get_term_sig(G_SUBPROCESS(process));
      if (rc != sig) ERROR("g_subprocess_get_term_sig()"); // rc != sig: to skip own stop signals
    }
  }
  if (proc) {
    if G_IS_OBJECT(proc->proc) {
      LOG("clear ping[%d] resources", proc->ndx);
      CLEAR_G_OBJECT(&proc->proc);
    }
    proc->proc = NULL;
    proc->active = false;
    proc->sig = 0;
  } else if (G_IS_OBJECT(process)) {
    GObject *p = process;
    CLEAR_G_OBJECT(&p);
  }
  if (!pinger_active()) { // final update
    pingtab_update();
    if (!pinger_state.gotdata && (!info_mesg || (info_mesg == notyet_mesg))) pinger_set_error(nodata_mesg);
  }
}

static void on_stdout(GObject *stream, GAsyncResult *result, t_proc *p) {
  if (!G_IS_INPUT_STREAM(stream)) return;
  if (!p || (p->ndx < 0)) return;
  if (p->ndx < 0) return;
  GError *error = NULL;
  gssize size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) {    // error
    ERROR("g_input_stream_read_finish(stdout)");
    pinger_stop_nth(p->ndx, "stdin error");
  } else if (size) { // data
    if (!p->out) return; // possible at external exit
    gssize left = BUFF_SIZE - size;
    p->out[(left > 0) ? size : size - 1] = 0;
    parser_parse(p->ndx, p->out);
    RESET_ISTREAM(stream, p->out, on_stdout, p);
  } else {           // EOF
    pinger_stop_nth(p->ndx, "EOF");
    stat_last_tx(p->ndx);
  }
}

static void on_stderr(GObject *stream, GAsyncResult *result, t_proc *p) {
  if (!G_IS_INPUT_STREAM(stream)) return;
  if (!p || (p->ndx < 0)) return;
  GError *error = NULL;
  gssize size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) {    // error
    ERROR("g_input_stream_read_finish(stderr)");
  } else if (size) { // data
    if (!p->err) return; // possible at external exit
    gchar *s = p->err;
    gssize left = BUFF_SIZE - size;
    s[(left > 0) ? size : size - 1] = 0;
    { int l = strlen(SKIPNAME); if (!strncmp(s, SKIPNAME, l)) s += l; } // skip program name
    s = g_strstrip(s); LOG("ERROR: %s", s);
    UPD_STR(ping_errors[p->ndx], s); // save error
    pinger_set_error(last_error());  // display the last one
    RESET_ISTREAM(stream, p->err, on_stderr, p);
  } // else EOF
}

#define MAX_ARGC 32

#define SANDBOX_MESG "Snap container related:\n" \
  "if minimal ping's slot (network-observe) is not autoconnected (permission denied),\n" \
  "it can be allowed and connected with the following command:\n" \
  "  snap connect pingpath:network-observe :network-observe"

static gboolean create_ping(int at, t_proc *p) {
  if (!p->out) p->out = g_malloc0(BUFF_SIZE);
  if (!p->err) p->err = g_malloc0(BUFF_SIZE);
  if (!p->out || !p->err) { WARN("%s(%d): g_malloc0() failed", __func__, at); return false; }
  const gchar* argv[MAX_ARGC] = {NULL}; int argc = 0;
  argv[argc++] = PING;
  argv[argc++] = "-OD";
  if (!opts.dns) argv[argc++] = "-n";
  char sttl[16]; g_snprintf(sttl, sizeof(sttl), "-t%d", at + 1);       argv[argc++] = sttl;
  char scls[16]; g_snprintf(scls, sizeof(scls), "-c%d", opts.cycles);  argv[argc++] = scls;
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
  GError *error = NULL;
  p->proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
  if (!p->proc) {
    PP_NOTIFY("%s", error ? error->message : unkn_error);
    if (error->code == 3) PP_NOTIFY("%s", SANDBOX_MESG);
    ERROR("g_subprocess_newv()");
    return false;
  }
  g_subprocess_wait_check_async(p->proc, NULL, (GAsyncReadyCallback)process_stopped, p);
  GInputStream *output = g_subprocess_get_stdout_pipe(p->proc);
  GInputStream *errput = g_subprocess_get_stderr_pipe(p->proc);
  if (output && errput) {
    RESET_ISTREAM(output, p->out, on_stdout, p);
    RESET_ISTREAM(errput, p->err, on_stderr, p);
    p->active = true;
    p->ndx = at;
#ifdef DEBUGGING
    if (verbose & 2) {
      gchar* deb_argv[MAX_ARGC];
      memcpy(deb_argv, argv, sizeof(deb_argv));
      gchar *deb_argv_s = g_strjoinv(", ", deb_argv);
      DEBUG("ping[ttl=%d]: argv[%s]", at + 1, deb_argv_s);
      g_free(deb_argv_s);
    }
#endif
    LOG("ping[ttl=%d] started", at + 1);
  } else {
    pinger_stop_nth(at, "input failed");
    p->active = false;
    p->ndx = -1;
    WARN("%s: get input streams failed", __func__);
  }
  p->sig = 0;
  return p->active;
}


// pub
//

void pinger_init(void) {
  if (!opts.pad[0]) g_strlcpy(opts.pad, DEF_PPAD, sizeof(opts.pad));
  for (int i = 0; i < MAXTTL; i++) { pings[i].ndx = -1; pings[i].active = false; }
}

void pinger_start(void) {
  if (!opts.target) return;
  stat_clear(true);
  graphtab_free();
  for (int i = opts.min; i < opts.lim; i++) if (!create_ping(i, &pings[i])) break;
  if (pinger_active()) pinger_clear_data(true);
}

void pinger_stop_nth(int nth, const gchar* reason) {
  t_proc *p = &(pings[nth]);
  if (p->proc) {
    const gchar *id = g_subprocess_get_identifier(p->proc);
    if (id) {
      LOG("stop[pid=%s] reason: %s", id ? id : "NA", reason);
      p->sig = SIGTERM;
      g_subprocess_send_signal(p->proc, p->sig);
      id = g_subprocess_get_identifier(p->proc);
      if (id) { g_usleep(20 * 1000); id = g_subprocess_get_identifier(p->proc); } // wait a bit
      if (id) {
        g_subprocess_force_exit(p->proc);
        g_usleep(20 * 1000);
        id = g_subprocess_get_identifier(p->proc);
        if (id) { WARN("Cannot stop subprocess[id=%s]", id); return; }
      }
      GError *error = NULL;
      g_subprocess_wait(p->proc, NULL, &error);
      if (error) { WARN("%s(%d): pid=%s: %s", __func__, nth, id, error->message); g_error_free(error); }
      else LOG("ping[ttl=%d] finished (rc=%d)", p->ndx + 1, g_subprocess_get_status(p->proc));
    }
    if (!id) process_stopped(NULL, NULL, p);
  }
}

void pinger_stop(const gchar* reason) {
  for (int i = opts.min; i < opts.lim; i++) pinger_stop_nth(i, reason);
  pinger_active();
}

#define FREE_PROC_BUFF(nth) { UPD_STR(pings[nth].out, NULL); UPD_STR(pings[nth].err, NULL); }

void pinger_free(void) {
  pinger_free_errors();
  stat_free();
  UPD_STR(opts.target, NULL);
  for (int i = 0; i < MAXTTL; i++) FREE_PROC_BUFF(i);
}

void pinger_free_errors(void) { for (int i = 0; i < MAXTTL; i++) pinger_free_nth_error(i); }

inline void pinger_free_nth_error(int nth) { UPD_STR(ping_errors[nth], NULL); }

void pinger_clear_data(gboolean clean) {
  pinger_free_errors();
  stat_clear(clean);
  hops_no = opts.lim;
  pingtab_clear();
}

inline void pinger_set_error(const gchar *error) { pingtab_set_error(error); }

gboolean pinger_within_range(int min, int max, int got) { // 1..MAXTTL
  if (min > max) return false;
  if ((min < 1) || (min > MAXTTL)) return false;
  if ((max < 1) || (max > MAXTTL)) return false;
  return ((min <= got) && (got <= max));
}

void pinger_on_quit(gboolean andstop) {
  g_source_remove(datetime_id); datetime_id = 0; // stop timer unless it's already done
  pinger_free();
  dns_cache_free();
  whois_cache_free();
  if (andstop) pinger_stop("at exit"); // note: if it's sysexit, subprocesses have to be already terminated by system
}

int pinger_update_tabs(gpointer unused) { pingtab_update(); return G_SOURCE_CONTINUE; }

void pinger_vis_rows(int no) {
  pingtab_vis_rows(no);
  if (opts.graph && opts.legend) notifier_vis_rows(NT_GRAPH_NDX, no);
}

void pinger_update_width(int typ, int max) {
  pingtab_update_width(typ, max);
  if (opts.graph && opts.legend) switch (typ) {
    case ELEM_HOST:
    case ELEM_AS:
      notifier_update_width(typ, NT_GRAPH_NDX, max);
  }
}

