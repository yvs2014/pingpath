
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "config.h"

#ifdef WITH_JSON
#include <json-glib/json-glib.h>
#endif

#include "common.h"

#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "dns.h"
#include "whois.h"
#include "ui/style.h"
#include "ui/appbar.h"
#include "ui/notifier.h"
#include "tabs/aux.h"
#include "tabs/ping.h"
#include "series.h"

#ifndef PINGDIR
#define PINGDIR "/bin"
#endif
#define BINPING PINGDIR "/ping"
#define SKIPNAME BINPING ": "

#define RESET_ISTREAM(ins, buff, cb, data) g_input_stream_read_async(G_INPUT_STREAM(ins), \
  (buff), BUFF_SIZE, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)(cb), (data))

#define CLEAR_G_OBJECT(obj) g_clear_object(obj) // NOLINT(bugprone-sizeof-expression)
#define PINGER_MESG(fmt, ...) { if (opts.recap) g_message(fmt, __VA_ARGS__); else notifier_inform(fmt, __VA_ARGS__); }

enum { CSV_DEL = ';' };
#define HOP_INDENT g_print("%3s", "")
#define PRINT_CSV_DIV { if (csv) g_print("%c\n", CSV_DEL); }
#define PRINT_TEXT_ELEM(str, len) { const char *var = (str); \
  if (csv) print_csv_elem(var); else g_print(" %-*s", len, var); }

static void print_csv_elem(const char *str) {
  char *delim = strchr(str, CSV_DEL);
  g_print(delim ? "%c\"%s\"" : "%c%s", CSV_DEL, str);
}

typedef struct proc {
  GSubprocess *proc;
  gboolean active; // process state
  int sig;         // signal of own stop
  int ndx;         // index in internal list
  char *out, *err; // stdout, stderr buffers
} t_proc;

t_pinger_state pinger_state;
unsigned stat_timer, exp_timer;
gboolean atquit;

//

static t_proc pings[MAXTTL];
static char* ping_errors[MAXTTL];
static const char *nodata_mesg = "No data";
static const char *notyet_mesg = "No data yet";
static const char *info_mesg;

//

static char* last_error(void) {
  static char last_error_buff[BUFF_SIZE];
  for (int i = opts.lim; i > opts.min; i--) {
    char *err = ping_errors[i - 1];
    if (err) {
      g_strlcpy(last_error_buff, err, sizeof(last_error_buff));
      return last_error_buff;
    }
  }
  return NULL;
}

static void pinger_error_free(void) { for (int i = 0; i < MAXTTL; i++) pinger_nth_free_error(i); }

#define PN_PRMAX(name) { const char *str = name; if (str && str[0]) { \
  int len = g_utf8_strlen(str, MAXHOSTNAME); if (len > maxes[j]) maxes[j] = len; }}

static void pinger_print_text(gboolean csv) {
  { char ts[32]; timestamp(ts, sizeof(ts));
    csv ? g_print("%s%c%s\n", ts, CSV_DEL, opts.target) : g_print("[%s] %s\n", ts, opts.target); }
  PRINT_CSV_DIV;
  if (pinger_state.gotdata) {
    if (csv) g_print("%s", OPT_TTL_HDR); else HOP_INDENT;
    int lim = (tgtat > visibles) ? (visibles + 1) : tgtat;
    //
    int maxes[PE_MAX] = {0};
    for (int j = 0; j < PE_MAX; j++) PN_PRMAX(pingelem[j].name);
    for (int i = opts.min; i < lim; i++) for (int j = 0; j < PE_MAX; j++) PN_PRMAX(stat_str_elem(i, pingelem[j].type));
    //
    for (int j = PE_NO + 1; j < PE_MAX; j++) { // header
      int type = pingelem[j].type;
      if (pingelem[j].enable && (type != PE_FILL)) PRINT_TEXT_ELEM(pingelem[j].name, maxes[j]);
    }
    g_print("\n");
    PRINT_CSV_DIV;
    for (int i = opts.min; i < lim; i++) { // data per hop
      t_ping_column column[G_N_ELEMENTS(pingelem)] = {0};
      int lines = 0;
      for (unsigned j = 0; j < G_N_ELEMENTS(pingelem); j++) if (pingelem[j].enable) {
        int type = pingelem[j].type;
        switch (type) {
          case PE_NO:
            csv ? g_print("%d", i + 1) : g_print("%2d.", i + 1); break;
          case PE_HOST:
          case PE_AS:
          case PE_CC:
          case PE_DESC:
          case PE_RT: {
            int max = stat_ping_column(i, type, &column[j]); if (lines < max) lines = max;
            PRINT_TEXT_ELEM(column[j].cell[0] ? column[j].cell[0] : "", maxes[j]);
          } break;
          case PE_LOSS:
          case PE_SENT:
          case PE_RECV:
          case PE_LAST:
          case PE_BEST:
          case PE_WRST:
          case PE_AVRG:
          case PE_JTTR:
            PRINT_TEXT_ELEM(stat_str_elem(i, type), maxes[j]); break;
          default: break;
        }
      }
      g_print("\n");
      for (int k = 1; k < lines; k++) { // multihop
        if (!csv) HOP_INDENT;
        for (int j = PE_HOST; j <= PE_RT; j++)
          if (pingelem[j].enable) PRINT_TEXT_ELEM(column[j].cell[k] ? column[j].cell[k] : "", maxes[j]);
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
  if (!pretty) { char *prop = g_utf8_strdown(name, -1); if (prop) { fn(obj, prop, val); g_free(prop); return; }} \
  fn(obj, name, val); }

static void pinger_json_prop_str(JsonObject *obj, const char *name, const char *val, gboolean pretty) {
  if (val) PINGER_JSON_LOWER_FN(json_object_set_string_member);
}

static void pinger_json_prop_int(JsonObject *obj, const char *name, int val, gboolean pretty) {
  if (val >= 0) PINGER_JSON_LOWER_FN(json_object_set_int_member);
}

static void pinger_json_prop_arr(JsonObject *obj, const char *name, JsonArray *val, gboolean pretty) {
  PINGER_JSON_LOWER_FN(json_object_set_array_member);
}

static void pinger_json_add_int(JsonObject *obj, int nth, int type, const char *name) {
  int val = stat_int_elem(nth, type);
  if (val >= 0) json_object_set_int_member(obj, name, val);
}

static void pinger_json_add_dbl(JsonObject *obj, int nth, int type, const char *name) {
  double val = stat_dbl_elem(nth, type);
  if (val >= 0) json_object_set_double_member(obj, name, val);
}

static void pinger_json_msec(JsonObject *obj, const char *name, int nth, int type) {
  const char *elem = stat_str_elem(nth, type);
  if (name && elem && elem[0]) {
    char *val = g_strdup_printf("%sms", elem);
    if (val) { json_object_set_string_member(obj, name, val); g_free(val); }
  }
}

static gboolean pinger_json_hop_info(JsonObject *obj, int nth, gboolean pretty) {
  JsonArray *arr = json_array_new();
  if (!arr) { FAILX("hop array", "json_array_new()"); return false; }
  pinger_json_prop_arr(obj, OPT_INFO_HDR, arr, pretty);
  t_ping_column column[PX_MAX] = {0};
  int lines = 0;
  for (int ndx = PE_HOST; ndx <= PE_RT; ndx++)
    if (pingelem[ndx].enable) { // collect multihop info
      int max = stat_ping_column(nth, pingelem[ndx].type, &column[ndx]);
      if (max > lines) lines = max;
    }
  for (int j = 0; j < lines; j++) { // add collected info
    JsonObject *info = json_object_new();
    if (!info) { FAILX("hop info", "json_object_new()"); return false; }
    json_array_add_object_element(arr, info);
    for (int ndx = PE_HOST; ndx <= PE_RT; ndx++) if (pingelem[ndx].enable) {
      int type = pingelem[ndx].type;
      switch (type) {
        case PE_HOST: {
          const char *addr = column[PX_ADDR].cell[j], *name = column[PX_HOST].cell[j];
          if (pretty) {
            char *host = (name) ? g_strdup_printf("%s (%s)", name, addr) : g_strdup(addr);
            if (host) { pinger_json_prop_str(info, pingelem[ndx].name, host, pretty); g_free(host); }
            else pinger_json_prop_str(info, pingelem[ndx].name, column[ndx].cell[j], pretty);
          } else {
            pinger_json_prop_str(info, "addr", addr, pretty);
            pinger_json_prop_str(info, "name", name, pretty);
          }
        } break;
        case PE_AS:
        case PE_CC:
        case PE_DESC:
        case PE_RT:
          pinger_json_prop_str(info, pingelem[ndx].name, column[ndx].cell[j], pretty); break;
        default: break;
      }
    }
  }
  return true;
}

static void pinger_json_stat_info(JsonObject *obj, int nth, gboolean pretty) {
  for (int ndx = PE_LOSS; ndx < PE_MAX; ndx++) if (pingelem[ndx].enable) {
    char *name = pretty ? g_strdup(pingelem[ndx].name) : g_utf8_strdown(pingelem[ndx].name, -1);
    int type = pingelem[ndx].type;
    switch (type) {
      case PE_LOSS:
        if (pretty) json_object_set_string_member(obj, name, stat_str_elem(nth, type));
        else pinger_json_add_dbl(obj, nth, type, name);
        break;
      case PE_SENT:
      case PE_RECV:
        if (pretty) json_object_set_string_member(obj, name, stat_str_elem(nth, type));
        else pinger_json_add_int(obj, nth, type, name);
        break;
      case PE_LAST:
      case PE_BEST:
      case PE_WRST:
        if (pretty) pinger_json_msec(obj, name, nth, type);
        else pinger_json_add_int(obj, nth, type, name);
        break;
      case PE_AVRG:
      case PE_JTTR:
        if (pretty) pinger_json_msec(obj, name, nth, type);
        else pinger_json_add_dbl(obj, nth, type, name);
        break;
      default: break;
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
  for (int ndx = PE_HOST; ndx <= PE_RT; ndx++) if (pingelem[ndx].enable) { hop_info = true; break; }
  int lim = (tgtat > visibles) ? (visibles + 1) : tgtat;
  for (int i = opts.min; i < lim; i++) { // data per hop
    JsonObject *line = json_object_new();
    if (!line) { FAIL("json_object_new()"); return false; }
    json_array_add_object_element(arr, line);
    if (pretty) {
      char *ttl = g_strdup_printf("%d", i + 1);
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
      { char ts[32];
        pinger_json_prop_str(obj, ENT_TSTAMP_HDR, timestamp(ts, sizeof(ts)), pretty); }
      char *info = NULL;
      gboolean okay = true;
      if (pinger_state.gotdata) {
        okay = pinger_json_mainbody(obj, pretty);
        if (okay && info_mesg) info = g_strdup(info_mesg);
      } else info = info_mesg ? g_strdup_printf("%s (%s)", nodata_mesg, info_mesg) : g_strdup(nodata_mesg);
      if (okay) {
        if (info) { pinger_json_prop_str(obj, "Info", info, pretty); g_free(info); }
        g_object_set(gen, "pretty", pretty, NULL);
        json_generator_set_root(gen, node);
        size_t len = 0; char *json = json_generator_to_data(gen, &len);
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
    default: break;
  }
}

static void tab_updater(gboolean reset) {
  static int ping_seq;
  if (stat_timer) { g_source_remove(stat_timer); stat_timer = 0; }
  if (reset) { ping_seq = 0; stat_timer = g_timeout_add_seconds(opts.timeout, (GSourceFunc)pinger_update_tabs, &ping_seq); }
}

static void pinger_update(void) {
  static const char *nopong_mesg[] = { "Not reached", "Not reached yet"};
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

static gboolean pinger_is_active(void) {
  for (int i = opts.min; i < opts.lim; i++) if (pings[i].active) return true;
  return false;
}

static gboolean pinger_update_status(void) {
  gboolean active = pinger_is_active();
  if (!atquit && (pinger_state.run != active)) {
    pinger_state.run = active;
    tab_updater(active);
    if (!opts.recap) { appbar_update(); if (!active) notifier_inform("%s finished", "Pings"); }
  }
  return active;
}

#define NORECAP_DRAW_REL (need_drawing() && !opts.recap)

static void process_stopped(GObject *process, GAsyncResult *result, t_proc *proc) {
  if (G_IS_SUBPROCESS(process)) {
    GError *error = NULL;
    gboolean okay = g_subprocess_wait_check_finish(G_SUBPROCESS(process), result, &error);
    int sig = proc ? proc->sig : 0;
    if (!okay && sig) {
      int rc = g_subprocess_get_term_sig(G_SUBPROCESS(process));
      // rc != sig: to skip own stop signals
      if (rc != sig) { ERROR("g_subprocess_get_term_sig()"); error = NULL; }
    }
    if (error) g_error_free(error);
  }
  if (proc) {
    if (G_IS_OBJECT(proc->proc)) {
      LOG("clear ping[%d] resources", proc->ndx);
      CLEAR_G_OBJECT(&proc->proc);
    }
    proc->proc = NULL;
    proc->active = false;
    proc->sig = 0;
  } else if (G_IS_OBJECT(process)) {
    GObject *obj = process;
    CLEAR_G_OBJECT(&obj);
  }
  if (!atquit && !pinger_is_active()) { // final update at process exit
    pinger_update_status();
    pinger_update();
    if (!pinger_state.gotdata && (!info_mesg || (info_mesg == notyet_mesg))) pinger_set_error(nodata_mesg);
    if (NORECAP_DRAW_REL) {
      series_update();
      if (!pinger_state.pause) drawtab_update();
    }
  }
}

static void on_stdout(GObject *stream, GAsyncResult *result, t_proc *proc) {
  if (!G_IS_INPUT_STREAM(stream)) return;
  if (!proc || (proc->ndx < 0)) return;
  if (proc->ndx < 0) return;
  GError *error = NULL;
  ssize_t size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) {    // error
    ERROR("g_input_stream_read_finish(stdout)");
    pinger_nth_stop(proc->ndx, "stdin error");
  } else if (size) { // data
    if (!proc->out || atquit) return; // possible at external exit
    ssize_t left = BUFF_SIZE - size;
    proc->out[(left > 0) ? size : size - 1] = 0;
    parser_parse(proc->ndx, proc->out);
    RESET_ISTREAM(stream, proc->out, on_stdout, proc);
  } else {           // EOF
    pinger_nth_stop(proc->ndx, "EOF");
    stat_last_tx(proc->ndx);
  }
}

static void on_stderr(GObject *stream, GAsyncResult *result, t_proc *proc) {
  if (!G_IS_INPUT_STREAM(stream)) return;
  if (!proc || (proc->ndx < 0)) return;
  GError *error = NULL;
  ssize_t size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) {    // error
    ERROR("g_input_stream_read_finish(stderr)");
  } else if (size) { // data
    if (!proc->err || atquit) return; // possible at external exit
    char *str = proc->err;
    ssize_t left = BUFF_SIZE - size;
    str[(left > 0) ? size : size - 1] = 0;
    { size_t len = strlen(SKIPNAME); if (!strncmp(str, SKIPNAME, len)) str += len; } // skip program name
    str = g_strstrip(str); LOG("ERROR: %s", str);
    UPD_STR(ping_errors[proc->ndx], str); // save error
    pinger_set_error(last_error());       // display the last one
    RESET_ISTREAM(stream, proc->err, on_stderr, proc);
  } // else EOF
}

enum { MAX_ARGC = 32 };

#define SANDBOX_MESG "Snap container related:\n" \
  "if minimal ping's slot (network-observe) is not autoconnected (permission denied),\n" \
  "it can be allowed and connected with the following command:\n" \
  "  snap connect pingpath:network-observe :network-observe"

static gboolean create_ping(int at, t_proc *proc) {
  if (!proc->out) proc->out = g_malloc0(BUFF_SIZE);
  if (!proc->err) proc->err = g_malloc0(BUFF_SIZE);
  if (!proc->out || !proc->err) { WARN("at=%d: g_malloc0() failed", at); return false; }
  const char* argv[MAX_ARGC] = {0}; int argc = 0;
  argv[argc++] = BINPING;
  argv[argc++] = "-OD";
  if (!opts.dns) argv[argc++] = "-n";
  char sttl[16]; g_snprintf(sttl, sizeof(sttl), "-t%d", at + 1);       argv[argc++] = sttl;
  char scls[16]; g_snprintf(scls, sizeof(scls), "-c%d", opts.cycles);  argv[argc++] = scls;
  char sitm[16]; g_snprintf(sitm, sizeof(sitm), "-i%d", opts.timeout); argv[argc++] = sitm;
  char sWtm[16]; g_snprintf(sWtm, sizeof(sitm), "-W%d", opts.timeout); argv[argc++] = sWtm;
  char sqos[16]; if (opts.qos != DEF_QOS) {
    g_snprintf(sqos, sizeof(sqos), "-Q%d",   opts.qos);  argv[argc++] = sqos; }
  char spad[64]; if (strncmp(opts.pad, DEF_PPAD, sizeof(opts.pad))) {
    g_snprintf(spad, sizeof(spad), "-p%s", opts.pad);    argv[argc++] = spad; }
  char spsz[16]; if (opts.size != DEF_PSIZE) {
    g_snprintf(spsz, sizeof(spsz), "-s%d",   opts.size); argv[argc++] = spsz; }
  char sipv[4];  if ((opts.ipv == 4) || (opts.ipv == 6)) {
    g_snprintf(sipv, sizeof(sipv), "-%d",    opts.ipv);  argv[argc++] = sipv; }
  argv[argc++] = "--";
  argv[argc++] = opts.target;
  GError *error = NULL;
  proc->proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error); // ENUMNOLINT
  if (!proc->proc) {
    PINGER_MESG("%s", error ? error->message : unkn_error);
    if (error->code == 3) PINGER_MESG("%s", SANDBOX_MESG);
    ERROR("g_subprocess_newv()");
    return false;
  }
  g_subprocess_wait_check_async(proc->proc, NULL, (GAsyncReadyCallback)process_stopped, proc);
  GInputStream *output = g_subprocess_get_stdout_pipe(proc->proc);
  GInputStream *errput = g_subprocess_get_stderr_pipe(proc->proc);
  if (output && errput) {
    RESET_ISTREAM(output, proc->out, on_stdout, proc);
    RESET_ISTREAM(errput, proc->err, on_stderr, proc);
    proc->active = true;
    proc->ndx = at;
#ifdef DEBUGGING
    if (verbose & 2) {
      char* deb_argv[MAX_ARGC];
      memcpy(deb_argv, argv, sizeof(deb_argv)); // BUFFNOLINT
      char *deb_argv_s = g_strjoinv(", ", deb_argv);
      DEBUG("ping[ttl=%d]: argv[%s]", at + 1, deb_argv_s);
      g_free(deb_argv_s);
    }
#endif
    LOG("ping[ttl=%d] started", at + 1);
  } else {
    pinger_nth_stop(at, "input failed");
    proc->active = false;
    proc->ndx = -1;
    FAIL("get input streams");
  }
  proc->sig = 0;
  return proc->active;
}

static gboolean pinger_on_expired(gpointer user_data G_GNUC_UNUSED) {
  if (!atquit) for (int i = 0; i < MAXTTL; i++) if (pings[i].active) {
    LOG("proc(%d) is still active after expire time, stopping..", i);
    pinger_nth_stop(i, "runtime expired");
  }
  exp_timer = 0; return G_SOURCE_REMOVE;
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
  gboolean active = pinger_update_status();
  if (!active) return;
  pinger_clear_data(true);
  if (!opts.recap) { drawtab_refresh(); pinger_vis_rows(0); }
  notifier_set_visible(NT_LEGEND_NDX, false);
  // schedule expiration check out
  if (exp_timer) { g_source_remove(exp_timer); exp_timer = 0; }
  unsigned exp_in = round(opts.cycles * (opts.timeout * 1.024)) + opts.timeout * 2; // ~24msec of possible ping time resolution
  exp_timer = g_timeout_add_seconds(exp_in, pinger_on_expired, NULL);
  LOG("expiration set to %d seconds for %d cycles", exp_in, opts.cycles);
}

void pinger_nth_stop(int nth, const char* reason) {
  t_proc *proc = &pings[nth];
  if (proc->proc) {
    const char *id = g_subprocess_get_identifier(proc->proc);
    if (id) {
      LOG("stop[pid=%s] reason: %s", id ? id : "NA", reason);
      proc->sig = SIGTERM;
      g_subprocess_send_signal(proc->proc, proc->sig);
      id = g_subprocess_get_identifier(proc->proc);
      if (id) { g_usleep(20 * 1000); id = g_subprocess_get_identifier(proc->proc); } // wait a bit
      if (id) {
        g_subprocess_force_exit(proc->proc);
        g_usleep(20 * 1000);
        id = g_subprocess_get_identifier(proc->proc);
        if (id) { WARN("Cannot stop subprocess[id=%s]", id); return; }
      }
      GError *error = NULL;
      g_subprocess_wait(proc->proc, NULL, &error);
      if (error) { WARN("%s(%d): pid=%s: %s", __func__, nth, id, error->message); g_error_free(error); }
      else LOG("ping[ttl=%d] finished (rc=%d)", proc->ndx + 1, g_subprocess_get_status(proc->proc));
    }
    if (!id) process_stopped(NULL, NULL, proc);
  }
}

void pinger_stop(const char* reason) {
  for (int i = opts.min; i < opts.lim; i++) pinger_nth_stop(i, reason);
  if (!atquit) pinger_update_status();
}

inline void pinger_nth_free_error(int nth) { if (ping_errors[nth]) CLR_STR(ping_errors[nth]); }

void pinger_clear_data(gboolean clean) {
  stat_clear(clean);
  pinger_error_free();
  pinger_set_error(NULL);
  tgtat = opts.lim;
  if (!opts.recap) { pingtab_clear(); series_free(false); }
}

void pinger_set_error(const char *error) {
  info_mesg = error;
  if (!opts.recap) pingtab_set_error(error);
}

gboolean pinger_within_range(int min, int max, int got) { // 1..MAXTTL
  if (min > max) return false;
  if ((min < MINTTL) || (min > MAXTTL)) return false;
  if ((max < MINTTL) || (max > MAXTTL)) return false;
  return ((min <= got) && (got <= max));
}

void pinger_cleanup(void) {
  // stop timers unless it's already done
  if (exp_timer) { g_source_remove(exp_timer); exp_timer = 0; }
  if (stat_timer) { g_source_remove(stat_timer); stat_timer = 0; }
  if (datetime_id) { g_source_remove(datetime_id); datetime_id = 0; }
  // stop pings unless they're finished
  pinger_stop("at exit");
  // free other resources
  CLR_STR(opts.target);
  stat_free();
  dns_cache_free();
  whois_cache_free();
  pinger_error_free();
  drawtab_free();
  series_free(true);
  style_free();
}

int pinger_update_tabs(int *pseq) {
  if (atquit) { stat_timer = 0; return G_SOURCE_REMOVE; }
  if (pseq) (*pseq)++;
  pinger_update();
  if (pinger_state.run && NORECAP_DRAW_REL) {
    if (pseq) series_update();
    drawtab_update();
  }
  return G_SOURCE_CONTINUE;
}

inline void pinger_vis_rows(int upto) { if (!opts.recap) pingtab_vis_rows(upto); notifier_legend_vis_rows(upto); }

int pinger_recap_cb(GApplication *app) {
  static int recap_cnt;
  { recap_cnt++; // indicate 'in progress'
    int dec = recap_cnt / 10, rest = recap_cnt % 10;
    if (rest) fputc('.', stderr);
    else fprintf(stderr, "%d", dec % 10); // BUFFNOLINT
  }
  if (!pinger_is_active()) {
    fputc('\n', stderr);
    pinger_recap();
    if (G_IS_APPLICATION(app)) g_application_release(app);
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

