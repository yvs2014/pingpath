
#include <stdio.h>
#include <sysexits.h>
#ifdef WITH_JSON
#include <json-glib/json-glib.h>
#endif

#include "pinger.h"
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
#define PINGER_MESG(fmt, ...) { if (opts.recap) g_message(fmt, __VA_ARGS__); else notifier_inform(fmt, __VA_ARGS__); }

#define CSV_DEL ';'
#define HOP_INDENT g_print("%3s", "")
#define PRINT_TEXT_STR(str, len) { const gchar *s = str; csv ? g_print("%c%s", CSV_DEL, s) : g_print(" %-*s", len, s); }
#define PRINT_CSV_DIV { if (csv) g_print("%c\n", CSV_DEL); }

typedef struct proc {
  GSubprocess *proc;
  gboolean active; // process state
  int sig;         // signal of own stop
  int ndx;         // index in internal list
  char *out, *err; // stdout, stderr buffers
} t_proc;

t_opts opts = { .target = NULL, .dns = true, .whois = true, .cycles = DEF_CYCLES, .qos = DEF_QOS, .size = DEF_PSIZE,
  .min = 0, .lim = MAXTTL, .timeout = DEF_TOUT, .tout_usec = DEF_TOUT * G_USEC_PER_SEC, .logmax = DEF_LOGMAX,
  .graph = GRAPH_TYPE_CURVE, .legend = true };

t_pinger_state pinger_state;
guint stat_timer, exp_timer;

static t_proc pings[MAXTTL];
static gchar* ping_errors[MAXTTL];
static const gchar *nodata_mesg = "No data";
static const gchar *notyet_mesg = "No data yet";
static const gchar *info_mesg;
static GMainLoop *recap_loop;

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

static void pinger_free_errors(void) { for (int i = 0; i < MAXTTL; i++) pinger_nth_free_error(i); }

static void pinger_print_text(gboolean csv) {
  csv ? g_print("%s%c%s\n", timestampit(), CSV_DEL, opts.target) :
    g_print("[%s] %s\n", timestampit(), opts.target);
  PRINT_CSV_DIV;
  if (pinger_state.gotdata) {
    if (csv) g_print("%s", OPT_TTL_HDR); else HOP_INDENT;
    for (int typ = ELEM_NO + 1; typ < ELEM_MAX; typ++) // header
      if (statelem[typ].enable && (typ != ELEM_FILL))
        PRINT_TEXT_STR(statelem[typ].name, stat_elem_max(typ));
    g_print("\n");
    PRINT_CSV_DIV;
    int lim = (hops_no > visibles) ? (visibles + 1) : hops_no;
    for (int i = opts.min; i < lim; i++) { // data per hop
      const gchar* elems[ELEM_MAX][MAXADDR]; memset(elems, 0, sizeof(elems));
      for (int typ = 0; typ < ELEM_MAX; typ++) if (statelem[typ].enable) switch (typ) {
        case ELEM_NO:
          csv ? g_print("%d", i + 1) : g_print("%2d.", i + 1); break;
        case ELEM_HOST:
        case ELEM_AS:
        case ELEM_CC:
        case ELEM_DESC:
        case ELEM_RT:
          stat_str_arr(i, typ, elems[typ]);
          PRINT_TEXT_STR(elems[typ][0] ? elems[typ][0] : "", stat_elem_max(typ));
          break;
        case ELEM_LOSS:
        case ELEM_SENT:
        case ELEM_RECV:
        case ELEM_LAST:
        case ELEM_BEST:
        case ELEM_WRST:
        case ELEM_AVRG:
        case ELEM_JTTR:
          PRINT_TEXT_STR(stat_str_elem(i, typ), stat_elem_max(typ)); break;
      }
      g_print("\n");
      for (int j = 1; j < MAXADDR; j++) { // multihop
        if (!elems[ELEM_HOST][j]) break;
        if (!csv) HOP_INDENT;
        for (int mtyp = ELEM_HOST; mtyp < ELEM_RT + 1; mtyp++)
          if (statelem[mtyp].enable)
            PRINT_TEXT_STR(elems[mtyp][j] ? elems[mtyp][j] : "", stat_elem_max(mtyp));
        g_print("\n");
      }
    }
    PRINT_CSV_DIV;
    if (info_mesg) g_print("%s\n", info_mesg);
  } else {
    PRINT_CSV_DIV;
    g_print("%s", nodata_mesg);
    if (info_mesg) g_print(" (%s)", info_mesg);
    g_print("\n");
  }
}

#ifdef WITH_JSON

#define PINGER_JSON_LOWER_FN(fn) { \
  if (!pretty) { gchar *prop = g_utf8_strdown(name, -1); if (prop) { fn(obj, prop, val); g_free(prop); return; }} \
  fn(obj, name, val); }

static void pinger_json_prop_str(JsonObject *obj, const gchar *name, const gchar *val, gboolean pretty) {
  if (val) PINGER_JSON_LOWER_FN(json_object_set_string_member);
}

static void pinger_json_prop_int(JsonObject *obj, const gchar *name, int val, gboolean pretty) {
  if (val >= 0) PINGER_JSON_LOWER_FN(json_object_set_int_member);
}

static void pinger_json_prop_arr(JsonObject *obj, const gchar *name, JsonArray *val, gboolean pretty) {
  PINGER_JSON_LOWER_FN(json_object_set_array_member);
}

static void pinger_json_add_int(JsonObject *obj, int i, int typ, const gchar *name) {
  int val = stat_int_elem(i, typ);
  if (val >= 0) json_object_set_int_member(obj, name, val);
}

static void pinger_json_add_dbl(JsonObject *obj, int i, int typ, const gchar *name) {
  double val = stat_dbl_elem(i, typ);
  if (val >= 0) json_object_set_double_member(obj, name, val);
}

static void pinger_json_msec(JsonObject *obj, const char *name, int i, int typ) {
  const gchar *elem = stat_str_elem(i, typ);
  if (name && elem && elem[0]) {
    gchar *val = g_strdup_printf("%sms", elem);
    if (val) { json_object_set_string_member(obj, name, val); g_free(val); }
  }
}

static gboolean pinger_json_hop_info(JsonObject *obj, int i, gboolean pretty) {
  JsonArray *arr = json_array_new();
  if (!arr) { FAILX("hop array", "json_array_new()"); return false; }
  pinger_json_prop_arr(obj, OPT_INFO_HDR, arr, pretty);
  const gchar* elems[EX_ELEM_MAX][MAXADDR]; memset(elems, 0, sizeof(elems));
  for (int typ = ELEM_HOST; typ <= ELEM_RT; typ++) // collect multihop info
    if (statelem[typ].enable) stat_str_arr(i, typ, elems[typ]);
  if (statelem[ELEM_HOST].enable) for (int typ = EX_ELEM_ADDR; typ < EX_ELEM_MAX; typ++)
    stat_str_arr(i, typ, elems[typ]);
  for (int j = 0; j < MAXADDR; j++) { // add collected info
    if (!elems[ELEM_HOST][j]) break;
    JsonObject *info = json_object_new();
    if (!info) { FAILX("hop info", "json_object_new()"); return false; }
    json_array_add_object_element(arr, info);
    for (int typ = ELEM_HOST; typ <= ELEM_RT; typ++)
      if (statelem[typ].enable) switch (typ) {
        case ELEM_HOST: {
          const gchar *addr = elems[EX_ELEM_ADDR][j], *name = elems[EX_ELEM_HOST][j];
          if (pretty) {
            gchar *host = (name) ? g_strdup_printf("%s (%s)", name, addr) : g_strdup(addr);
            if (host) { pinger_json_prop_str(info, statelem[typ].name, host, pretty); g_free(host); }
            else pinger_json_prop_str(info, statelem[typ].name, elems[typ][j], pretty);
          } else {
            pinger_json_prop_str(info, "addr", addr, pretty);
            pinger_json_prop_str(info, "name", name, pretty);
          }
        } break;
        case ELEM_AS:
        case ELEM_CC:
        case ELEM_DESC:
        case ELEM_RT:
          pinger_json_prop_str(info, statelem[typ].name, elems[typ][j], pretty); break;
      }
  }
  return true;
}

static void pinger_json_stat_info(JsonObject *obj, int i, gboolean pretty) {
  for (int typ = ELEM_LOSS; typ < ELEM_MAX; typ++) if (statelem[typ].enable) {
    gchar *name = pretty ? g_strdup(statelem[typ].name) : g_utf8_strdown(statelem[typ].name, -1);
    switch (typ) {
      case ELEM_LOSS:
        if (pretty) json_object_set_string_member(obj, name, stat_str_elem(i, typ));
        else pinger_json_add_dbl(obj, i, typ, name);
        break;
      case ELEM_SENT:
      case ELEM_RECV:
        if (pretty) json_object_set_string_member(obj, name, stat_str_elem(i, typ));
        else pinger_json_add_int(obj, i, typ, name);
        break;
      case ELEM_LAST:
      case ELEM_BEST:
      case ELEM_WRST:
        if (pretty) pinger_json_msec(obj, name, i, typ);
        else pinger_json_add_int(obj, i, typ, name);
        break;
      case ELEM_AVRG:
      case ELEM_JTTR:
        if (pretty) pinger_json_msec(obj, name, i, typ);
        else pinger_json_add_dbl(obj, i, typ, name);
        break;
    }
    g_free(name);
  }
}

static gboolean pinger_json_mainbody(JsonObject *obj, gboolean pretty) {
  if (!obj) return false;
  JsonArray *arr = json_array_new();
  if (!arr) { FAIL("json_array_new()"); return false; }
  pinger_json_prop_arr(obj, OPT_STAT_HDR, arr, pretty);
  if (pretty) json_object_set_string_member(obj, "Timing", "Milliseconds");
  else json_object_set_string_member(obj, "timing", "microseconds");
  gboolean hop_info = false; // marker of hop part of info
  for (int typ = ELEM_HOST; typ <= ELEM_RT; typ++) if (statelem[typ].enable) { hop_info = true; break; }
  int lim = (hops_no > visibles) ? (visibles + 1) : hops_no;
  for (int i = opts.min; i < lim; i++) { // data per hop
    JsonObject *line = json_object_new();
    if (!line) { FAIL("json_object_new()"); return false; }
    json_array_add_object_element(arr, line);
    if (pretty) {
      gchar *ttl = g_strdup_printf("%d", i + 1);
      if (ttl) { json_object_set_string_member(line, OPT_TTL_HDR, ttl); g_free(ttl); }
    } else pinger_json_prop_int(line, OPT_TTL_HDR, i + 1, pretty);
    if (hop_info && !pinger_json_hop_info(line, i, pretty)) return false;
    pinger_json_stat_info(line, i, pretty);
  }
  return true;
}

static void pinger_print_json(gboolean pretty) {
  JsonGenerator *gen = json_generator_new();
  if (!JSON_IS_GENERATOR(gen)) { FAIL("json_generator_new()"); return; }
  JsonNode *node = json_node_new(JSON_NODE_OBJECT);
  if (node) {
    JsonObject *obj = json_object_new();
    if (obj) {
      json_node_take_object(node, obj);
      pinger_json_prop_str(obj, ENT_TARGET_HDR, opts.target, pretty);
      pinger_json_prop_str(obj, ENT_TSTAMP_HDR, timestampit(), pretty);
      gchar *info = NULL;
      gboolean okay = true;
      if (pinger_state.gotdata) {
        if ((okay = pinger_json_mainbody(obj, pretty))) if (info_mesg) info = g_strdup(info_mesg);
      } else info = info_mesg ? g_strdup_printf("%s (%s)", nodata_mesg, info_mesg) : g_strdup(nodata_mesg);
      if (okay) {
        if (info) { pinger_json_prop_str(obj, "Info", info, pretty); g_free(info); }
        g_object_set(gen, "pretty", pretty, NULL);
        json_generator_set_root(gen, node);
        gsize len; gchar *json = json_generator_to_data(gen, &len);
        if (json && len) g_print("%s\n", json); else FAIL("json_generator_to_data()");
        g_free(json);
      }
    } else FAIL("json_object_new()");
    json_node_free(node);
  } else FAIL("json_node_new()");
  g_object_unref(gen);
}

#endif /* end of WITH_JSON */

static void pinger_recap(void) {
  switch (opts.recap) {
    case RECAP_TEXT:
    case RECAP_CSV:
      pinger_print_text(opts.recap == RECAP_CSV); break;
#ifdef WITH_JSON
    case RECAP_JSON_NUM:
    case RECAP_JSON_PRETTY:
      pinger_print_json(opts.recap == RECAP_JSON_PRETTY); break;
#endif
  }
}

static void tab_updater(gboolean reset) {
  static int ping_seq;
  if (stat_timer) { g_source_remove(stat_timer); stat_timer = 0; }
  if (reset) { ping_seq = 0; stat_timer = g_timeout_add_seconds(opts.timeout, (GSourceFunc)pinger_update_tabs, &ping_seq); }
}

static void pinger_update(void) {
  static const gchar *nopong_mesg[] = { "Not reached", "Not reached yet"};
  if (!opts.recap) pingtab_update();
  { // no data (yet) messages
    gboolean notyet = (info_mesg == notyet_mesg);
    if (pinger_state.gotdata) { if (notyet) pinger_set_error(NULL); }
    else if (!notyet && !info_mesg && pinger_state.gotdata) pinger_set_error(notyet_mesg);
  }
  if (pinger_state.gotdata && !pinger_state.reachable) {
    gboolean yet = pinger_state.run;
    if (!info_mesg || (!yet && (info_mesg == nopong_mesg[1])))
      pinger_set_error(nopong_mesg[yet]);
  }
}

static gboolean pinger_active(void) {
  gboolean active = false;
  for (int i = opts.min; i < opts.lim; i++) if (pings[i].active) { active = true; break; }
  if (pinger_state.run != active) {
    pinger_state.run = active;
    tab_updater(active);
    if (!opts.recap) {
      appbar_update();
      if (!active) notifier_inform("%s finished", "Pings");
    }
  }
  return active;
}

static void process_stopped(GObject *process, GAsyncResult *result, t_proc *proc) {
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
    pinger_update();
    if (!pinger_state.gotdata && (!info_mesg || (info_mesg == notyet_mesg))) pinger_set_error(nodata_mesg);
    if (opts.graph && !opts.recap) graphtab_final_update();
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
    pinger_nth_stop(p->ndx, "stdin error");
  } else if (size) { // data
    if (!p->out) return; // possible at external exit
    gssize left = BUFF_SIZE - size;
    p->out[(left > 0) ? size : size - 1] = 0;
    parser_parse(p->ndx, p->out);
    RESET_ISTREAM(stream, p->out, on_stdout, p);
  } else {           // EOF
    pinger_nth_stop(p->ndx, "EOF");
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
  if (!p->out || !p->err) { WARN("at=%d: g_malloc0() failed", at); return false; }
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
    PINGER_MESG("%s", error ? error->message : unkn_error);
    if (error->code == 3) PINGER_MESG("%s", SANDBOX_MESG);
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
    pinger_nth_stop(at, "input failed");
    p->active = false;
    p->ndx = -1;
    FAIL("get input streams");
  }
  p->sig = 0;
  return p->active;
}

static int pinger_check_expired(gpointer unused) {
  for (int i = 0; i < MAXTTL; i++) if (pings[i].active) {
    LOG("proc(%d) is still active after expire time, stopping..", i);
    pinger_nth_stop(i, "runtime expired");
  }
  exp_timer = 0; return G_SOURCE_REMOVE;
}

static int recap_cb(gpointer unused) {
  static int recap_cnt;
  { recap_cnt++; // indicate 'in progress'
    int dec = recap_cnt / 10, rest = recap_cnt % 10;
    if (rest) fprintf(stderr, "."); else fprintf(stderr, "%d", dec % 10); }
  if (!pinger_active()) {
    fprintf(stderr, "\n");
    pinger_recap();
    if (recap_loop) g_main_loop_quit(recap_loop);
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}


// pub
//

void pinger_init(void) {
  if (!opts.pad[0]) g_strlcpy(opts.pad, DEF_PPAD, sizeof(opts.pad));
  for (int i = 0; i < MAXTTL; i++) { pings[i].ndx = -1; pings[i].active = false; }
}

void pinger_start(void) {
  if (!opts.target) return;
  for (int i = opts.min; i < opts.lim; i++) if (!create_ping(i, &pings[i])) break;
  if (!pinger_active()) return;
  pinger_clear_data(true);
  if (!opts.recap) {
    graphtab_free();
    pinger_vis_rows(0);
  }
  notifier_set_visible(NT_GRAPH_NDX, false);
  // schedule expiration check out
  if (exp_timer) { g_source_remove(exp_timer); exp_timer = 0; }
  guint exp_in = round(opts.cycles * (opts.timeout * 1.024)) + opts.timeout * 2; // ~24msec of possible ping time resolution
  exp_timer = g_timeout_add_seconds(exp_in, pinger_check_expired, NULL);
  LOG("expiration set to %d seconds for %d cycles", exp_in, opts.cycles);
}

void pinger_nth_stop(int nth, const gchar* reason) {
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
  for (int i = opts.min; i < opts.lim; i++) pinger_nth_stop(i, reason);
  pinger_active();
}

#define FREE_PROC_BUFF(nth) { UPD_STR(pings[nth].out, NULL); UPD_STR(pings[nth].err, NULL); }

void pinger_free(void) {
  pinger_free_errors();
  stat_free();
  UPD_STR(opts.target, NULL);
  for (int i = 0; i < MAXTTL; i++) FREE_PROC_BUFF(i);
}

inline void pinger_nth_free_error(int nth) { if (ping_errors[nth]) UPD_STR(ping_errors[nth], NULL); }

void pinger_clear_data(gboolean clean) {
  stat_clear(clean);
  pinger_free_errors();
  pinger_set_error(NULL);
  hops_no = opts.lim;
  if (!opts.recap) pingtab_clear();
}

void pinger_set_error(const gchar *error) {
  info_mesg = error;
  if (!opts.recap) pingtab_set_error(error);
}

gboolean pinger_within_range(int min, int max, int got) { // 1..MAXTTL
  if (min > max) return false;
  if ((min < 1) || (min > MAXTTL)) return false;
  if ((max < 1) || (max > MAXTTL)) return false;
  return ((min <= got) && (got <= max));
}

void pinger_on_quit(gboolean stop) {
  // stop timers unless it's already done
  g_source_remove(datetime_id); datetime_id = 0;
  if (exp_timer) { g_source_remove(exp_timer); exp_timer = 0; }
  if (stat_timer) { g_source_remove(stat_timer); stat_timer = 0; }
  // free resources
  pinger_free();
  dns_cache_free();
  whois_cache_free();
  if (stop) pinger_stop("at exit"); // note: if it's sysexit, subprocesses have to be already terminated by system
}

int pinger_update_tabs(int *pseq) {
  if (pseq) (*pseq)++;
  pinger_update();
  if (opts.graph && !opts.recap) graphtab_update(pseq != NULL);
  return G_SOURCE_CONTINUE;
}

inline void pinger_vis_rows(int no) { if (!opts.recap) pingtab_vis_rows(no); notifier_legend_vis_rows(no); }
inline void pinger_set_width(int typ, int max) { if (!opts.recap) pingtab_set_width(typ, max); }

int pinger_recap_loop() {
  if (!(recap_loop = g_main_loop_new(NULL, false))) {
    FAIL("g_main_loop_new()"); return EX_UNAVAILABLE; }
  g_timeout_add_seconds(1, recap_cb, NULL);
  g_main_loop_run(recap_loop);
  g_main_loop_unref(recap_loop);
  return 0;
}

