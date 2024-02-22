
#include <stdlib.h>

#include "cli.h"
#include "stat.h"
#include "pinger.h"
#include "parser.h"
#include "ui/action.h"
#include "ui/option.h"
#include "ui/notifier.h"

#define CLI_SET_ERR { if (error) { (value && value[0]) ? \
  g_set_error(error, g_quark_from_static_string(""), -1, "Not valid %s: %s", name, value) : \
  g_set_error(error, g_quark_from_static_string(""), -1, "Empty %s is not valid", name); }}

#ifdef CONFIG_DEBUGGING
#define CONF_DEBUG(fmt, ...) { if (verbose & 16) g_message("CONFIG: " fmt, __VA_ARGS__); }
#else
#define CONF_DEBUG(...) {}
#endif

#define CNF_SECTION_MAIN "ping"
#define CNF_STR_IPV    "ipversion"
#define CNF_STR_NODNS  "numeric"
#define CNF_STR_CYCLES "cycles"
#define CNF_STR_IVAL   "interval"
#define CNF_STR_TTL    "ttl"
#define CNF_STR_QOS    "qos"
#define CNF_STR_SIZE   "size"
#define CNF_STR_PLOAD  "payload"
#define CNF_STR_INFO   "info"
#define CNF_STR_STAT   "stat"
#define CNF_STR_THEME  "theme"
#define CNF_STR_GRAPH  "graph"
#define CNF_STR_EXTRA  "extra"
#define CNF_STR_LGND   "legend"
#define CNF_STR_RECAP  "recap"
#define CNF_SECTION_AUX  "aux"

enum { CNF_OPT_IPV, CNF_OPT_NODNS, CNF_OPT_CYCLES, CNF_OPT_IVAL, CNF_OPT_TTL, CNF_OPT_QOS, CNF_OPT_SIZE,
  CNF_OPT_PLOAD, CNF_OPT_INFO, CNF_OPT_STAT, CNF_OPT_THEME, CNF_OPT_GRAPH, CNF_OPT_EXTRA, CNF_OPT_LGND,
  CNF_OPT_RECAP, CNF_OPT_MAX };

typedef struct t_config_option {
  const char *opt;
  int typ;
  void *data;
} t_config_option;

typedef struct t_config_section {
  const char *name;
  t_config_option options[CNF_OPT_MAX];
} t_config_section;

//

static gboolean cli_int_opt(const char *name, const char *value, GError **error, int typ, const char *hdr,
    int min, int max, int *opt) {
  if (!value || !hdr) return false;
  t_minmax range = { .min = min, .max = max };
  int n = parser_int(value, typ, hdr, range);
  if (n < 0) { CLI_SET_ERR; return false; }
  if (opt) *opt = n;
  g_message("%s: %d", hdr, n);
  return true;
}

static gboolean cli_opt_c(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_CYCLES, OPT_CYCLES_HDR, CYCLES_MIN, CYCLES_MAX, &opts->cycles) : false;
}

static gboolean cli_opt_i(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_IVAL, OPT_IVAL_HDR, IVAL_MIN, IVAL_MAX, &opts->timeout) : false;
}

static gboolean cli_opt_q(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_QOS, OPT_QOS_HDR, 0, QOS_MAX, &opts->qos) : false;
}

static gboolean cli_opt_s(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_PSIZE, OPT_PSIZE_HDR, PSIZE_MIN, PSIZE_MAX, &opts->size) : false;
}

static gboolean cli_opt_g(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_GRAPH, OPT_GRAPH_HDR, 0, GRAPH_TYPE_MAX - 1, &opts->graph) : false;
}

#define MASK_NTH(nth) ((mask & (1 << (nth))) ? true : false)

#define MASK_GR_ELEMS(min, max) { int i0 = min; \
  for (int i = min; i < max; i++) graphelem[i].enable = MASK_NTH(i - i0); }

static gboolean cli_opt_G(const char *name, const char *value, t_opts *opts, GError **error) {
  int mask = 0, max = GREL_MAX - GRLG_MAX - 1;
  if (!opts || !cli_int_opt(name, value, error, ENT_STR_GEXTRA, OPT_GREX_HDR, 0, (1 << max) - 1, &mask)) return false;
  MASK_GR_ELEMS(GRLG_MAX + 1, GREL_MAX);
  return true;
}

static gboolean cli_opt_L(const char *name, const char *value, t_opts *opts, GError **error) {
  int mask = 0, max = GRLG_MAX - 1; // -NO, -DASH, +LGND
  if (!opts || !cli_int_opt(name, value, error, ENT_STR_LEGEND, OPT_GRLG_HDR, 0, (1 << max) - 1, &mask)) return false;
  opts->legend = mask & 1; mask >>= 1;
  MASK_GR_ELEMS(GRLG_MAX + 1 - max, GRLG_MAX);
  return true;
}

static gboolean cli_opt_t(const char *name, const char *value, t_opts *opts, GError **error) {
  if (!value || !opts) return false;
  t_ent_spn *en = &ent_spn[ENT_SPN_TTL];
  if (!en) return false;
  gchar *cp = g_strdup(value);
  gboolean okay = false;
  if (cp) {
    int *pmin = en->aux[SPN_AUX_MIN].pval;
    int *plim = en->aux[SPN_AUX_LIM].pval;
    int min = 0, lim = MAXTTL;
    t_minmax r = parser_range(cp, OPT_TTL_HDR);
    if ((r.min >= 0) && (r.max >= 0)) {
      okay = pinger_within_range(1, r.max, r.min) && pinger_within_range(r.min, MAXTTL, r.max);
      if (okay) {
        min = r.min - 1; if (pmin) *pmin = min;
        lim = r.max; if (plim) *plim = lim;
      }
    } else if (r.min >= 0) {
      okay = pinger_within_range(1, plim ? *plim : lim, r.min);
      if (okay) { min = r.min - 1; if (pmin) *pmin = min; }
    } else if (r.max >= 0) {
      okay = pinger_within_range(pmin ? (*pmin + 1) : 1, MAXTTL, r.max);
      if (okay) { lim = r.max; if (plim) *plim = lim; }
    }
    if (okay) g_message("%s: %d <=> %d", en->c.en.name, min + 1, lim);
    else g_message("%s: no match with [%d,%d]", en->c.en.name, 1, MAXTTL);
    g_free(cp);
  } else FAILX(name, "g_sdtrdup()");
  if (!okay) CLI_SET_ERR;
  return okay;
}

static t_ent_bool* cli_infostat_entry(int c, int typ) {
  int ndx = -1;
  if        (typ == OPT_TYPE_INFO) switch (c) { // info elements
    case 'h': ndx = ENT_BOOL_HOST; break;
    case 'a': ndx = ENT_BOOL_AS;   break;
    case 'c': ndx = ENT_BOOL_CC;   break;
    case 'd': ndx = ENT_BOOL_DESC; break;
    case 'r': ndx = ENT_BOOL_RT;   break;
  } else if (typ == OPT_TYPE_STAT) switch (c) { // stat elements
    case 'l': ndx = ENT_BOOL_LOSS; break;
    case 's': ndx = ENT_BOOL_SENT; break;
    case 'r': ndx = ENT_BOOL_RECV; break;
    case 'm': ndx = ENT_BOOL_LAST; break; // 'm'sec
    case 'b': ndx = ENT_BOOL_BEST; break;
    case 'w': ndx = ENT_BOOL_WRST; break;
    case 'a': ndx = ENT_BOOL_AVRG; break;
    case 'j': ndx = ENT_BOOL_JTTR; break;
  }
  return (ndx < 0) ? NULL : &ent_bool[ndx];
}

static const char* cli_opt_infostat(const char *value, int typ, const gchar *hdr) {
  const char *str = parser_str(value, hdr, typ);
  if (str) for (const char *p = str; *p; p++) {
    t_ent_bool *en = cli_infostat_entry(*p, typ);
    if (!en) continue;
    if (en->pval) *en->pval = true;
    g_message("%s: %s: %s", en->prefix, en->en.name, TOGGLE_ON_HDR);
  }
  return str;
}

#define RECAP_TYPE_EXPAND(type) ((type == 't') ? "text" : ((type == 'c') ? "csv" : \
 ((g_ascii_tolower(type) == 'j') ? "json" : "unknown")))

static gboolean cli_opt_elem(const char *name, const char *value, GError **error, int typ) {
  if (!value) return false;
  const char *str = NULL;
  switch (typ) {
    case OPT_TYPE_INFO: stat_clean_elems(ENT_EXP_INFO); str = cli_opt_infostat(value, typ, OPT_INFO_HEADER); break;
    case OPT_TYPE_STAT: stat_clean_elems(ENT_EXP_STAT); str = cli_opt_infostat(value, typ, OPT_STAT_HDR); break;
    case OPT_TYPE_PAD: {
      t_ent_str *en = &ent_str[ENT_STR_PLOAD];
      str = parser_str(value, en->en.name, typ);
      if (str) {
        if (en->pstr) g_strlcpy(en->pstr, str, en->slen);
        g_message("%s: %s", en->en.name, str);
      }
    } break;
    case OPT_TYPE_RECAP: {
      str = parser_str(value, OPT_RECAP_HDR, OPT_TYPE_RECAP);
      if (str) { opts.recap = str[0]; g_message("Summary: %s", RECAP_TYPE_EXPAND(opts.recap)); }
    } break;
  }
  if (!str) CLI_SET_ERR;
  return (str != NULL);
}

static gboolean cli_opt_p(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_PAD);
}

static gboolean cli_opt_I(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_INFO);
}

static gboolean cli_opt_S(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_STAT);
}

static gboolean cli_opt_T(const char *name, const char *value, t_opts *opts, GError **error) {
  if (!opts) return false;
  int mask = opts->darkgraph | (opts->darkgraph << 1);
  gboolean re = cli_int_opt(name, value, error, ENT_STR_THEME, "Theme dark/light bits", 0, 3, &mask);
  if (re) {
    opts->darktheme = (mask & 0x1) ? true : false;
    opts->darkgraph = (mask & 0x2) ? true : false;
  }
  return re;
}

static gboolean cli_opt_r(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_RECAP);
}

static gboolean config_opt_not_found(GError *opterr, const gchar *section, const gchar *name,
   const gchar *value, GError **error) {
  if (g_error_matches(opterr, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
      g_error_matches(opterr, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) return true;
  g_warning("%s:%s: %s", section, name, opterr ? opterr->message : unkn_error);
  CLI_SET_ERR; return false;
}

static void cli_set_opt_dns(gboolean numeric, t_opts *opts) {
  if (!opts) return;
  gboolean dns = !numeric; if (opts->dns == dns) return;
  opts->dns = dns; g_message(OPT_DNS_HDR " %s", dns ? TOGGLE_ON_HDR : TOGGLE_OFF_HDR);
}

static gboolean cli_opt_f(const char *name, const char *value, t_opts *opts, GError **error) {
  const t_config_section config_sections[] = {
    { .name = CNF_SECTION_MAIN, .options = {
      { .opt = CNF_STR_IPV,    .typ = CNF_OPT_IPV },
      { .opt = CNF_STR_NODNS,  .typ = CNF_OPT_NODNS },
      { .opt = CNF_STR_CYCLES, .typ = CNF_OPT_CYCLES, .data = cli_opt_c },
      { .opt = CNF_STR_IVAL,   .typ = CNF_OPT_IVAL,   .data = cli_opt_i },
      { .opt = CNF_STR_TTL,    .typ = CNF_OPT_TTL,    .data = cli_opt_t },
      { .opt = CNF_STR_QOS,    .typ = CNF_OPT_QOS,    .data = cli_opt_q },
      { .opt = CNF_STR_SIZE,   .typ = CNF_OPT_SIZE,   .data = cli_opt_s },
      { .opt = CNF_STR_PLOAD,  .typ = CNF_OPT_PLOAD,  .data = cli_opt_p },
      { .opt = CNF_STR_INFO,   .typ = CNF_OPT_INFO,   .data = cli_opt_I },
      { .opt = CNF_STR_STAT,   .typ = CNF_OPT_STAT,   .data = cli_opt_S },
      {}
    }},
    { .name = CNF_SECTION_AUX, .options = {
      { .opt = CNF_STR_THEME,  .typ = CNF_OPT_THEME,  .data = cli_opt_T },
      { .opt = CNF_STR_GRAPH,  .typ = CNF_OPT_GRAPH,  .data = cli_opt_g },
      { .opt = CNF_STR_EXTRA,  .typ = CNF_OPT_EXTRA,  .data = cli_opt_G },
      { .opt = CNF_STR_LGND,   .typ = CNF_OPT_LGND,   .data = cli_opt_L },
      { .opt = CNF_STR_RECAP,  .typ = CNF_OPT_RECAP,  .data = cli_opt_r },
      {}
    }},
    {}
  };
  g_autoptr(GKeyFile) file = g_key_file_new();
  g_autoptr(GError) ferr = NULL;
  if (!g_key_file_load_from_file(file, value, G_KEY_FILE_NONE, &ferr)) {
    g_warning("%s: \"%s\"", ferr ? ferr->message : unkn_error, value);
    CLI_SET_ERR; return false;
  }
  for (const t_config_section *cfg_section = config_sections; cfg_section; cfg_section++) {
    const char *section = cfg_section->name; if (!section) break;
    for (const t_config_option *option = cfg_section->options; option; option++) {
      const char *optname = option->opt; if (!optname) break;
      g_autoptr(GError) opterr = NULL;
      g_autofree gchar *optval = NULL;
      { g_autoptr(GError) strerr = NULL;
        optval = g_key_file_get_string(file, section, optname, &strerr);
        if (strerr) {
          if (config_opt_not_found(strerr, section, optname, optval, error)) continue;
          else return false;
        }
        CONF_DEBUG("validate %s:%s=%s", section, optname, optval);
      }
      switch (option->typ) {
        case CNF_OPT_IPV: {
          gint val = g_key_file_get_integer(file, section, optname, &opterr);
          if (!opterr) {
            if ((val == 4) || (val == 6)) {
              if (opts->ipv != val) {
                opts->ipv = val;
                g_message("%s only %s", (val == 4) ? OPT_IPV4_HDR : OPT_IPV6_HDR, TOGGLE_ON_HDR);
              }
            } else { g_warning("%s:%s: %s", section, optname, "must be 4 or 6"); CLI_SET_ERR; return false; }
          }
        } break;
        case CNF_OPT_NODNS: {
          gboolean val = g_key_file_get_boolean(file, section, optname, &opterr);
          if (!opterr) cli_set_opt_dns(val, opts);
        } break;
        case CNF_OPT_CYCLES:
        case CNF_OPT_IVAL:
        case CNF_OPT_TTL:
        case CNF_OPT_QOS:
        case CNF_OPT_SIZE:
        case CNF_OPT_PLOAD:
        case CNF_OPT_INFO:
        case CNF_OPT_STAT:
        case CNF_OPT_THEME:
        case CNF_OPT_GRAPH:
        case CNF_OPT_EXTRA:
        case CNF_OPT_LGND:
        case CNF_OPT_RECAP: if (optval) {
          if (option->data && !(((GOptionArgFunc)option->data)(optname, optval, opts, error))) return false;
        } break;
      }
      if (opterr && !config_opt_not_found(opterr, section, optname, optval, error)) return false;
    }
  }
  return true;
}


// pub
//

#define CLI_FAIL(fmt, arg) { g_message(fmt, arg); g_option_context_free(oc); return false; }

#ifdef WITH_JSON
#define RECAP_TYPE_MESG "text, csv, or json"
#else
#define RECAP_TYPE_MESG "text, csv"
#endif

#define CLI_OPT_NONE(sname, lname, adata, mdesc) { .short_name = sname, .long_name = lname, \
  .description = mdesc, .arg = G_OPTION_ARG_NONE, .arg_data = adata }
#define CLI_OPT_SMTH(sname, lname, adata, mdesc, adesc, typ) { .short_name = sname, .long_name = lname, \
  .description = mdesc, .arg = typ, .arg_data = adata, .arg_description = adesc }
#define CLI_OPT_CALL(sname, lname, adata, mdesc, adesc) \
  CLI_OPT_SMTH(sname, lname, adata, mdesc, adesc, G_OPTION_ARG_CALLBACK)
#define CLI_OPT_INTC(sname, lname, adata, mdesc, adesc) \
  CLI_OPT_SMTH(sname, lname, adata, mdesc, adesc, G_OPTION_ARG_INT)

gboolean autostart;

gboolean cli_init(int *pargc, char ***pargv) {
  if (!pargc || !pargv) return false;
  gchar** target = NULL;
  gboolean num = -1, ipv4 = false, ipv6 = false, version = false;
  const GOptionEntry options[] = {
    CLI_OPT_CALL('f', "file",         cli_opt_f, "Read options from file", "<filename>"),
    CLI_OPT_NONE('4', "ipv4",         &ipv4, OPT_IPV4_HDR " only"),
    CLI_OPT_NONE('6', "ipv6",         &ipv6, OPT_IPV4_HDR " only"),
    CLI_OPT_NONE('n', CNF_STR_NODNS,  &num, "Numeric output (i.e. disable " OPT_DNS_HDR " resolv)"),
    CLI_OPT_CALL('c', CNF_STR_CYCLES, cli_opt_c, OPT_CYCLES_HDR " per target", "<number>"),
    CLI_OPT_CALL('i', CNF_STR_IVAL,   cli_opt_i, OPT_IVAL_HDR " between pings", "<seconds>"),
    CLI_OPT_CALL('t', CNF_STR_TTL,    cli_opt_t, OPT_TTL_HDR " range", "[min][,max]"),
    CLI_OPT_CALL('q', CNF_STR_QOS,    cli_opt_q, OPT_QOS_HDR "/ToS byte", "<bits>"),
    CLI_OPT_CALL('s', CNF_STR_SIZE,   cli_opt_s, OPT_PLOAD_HDR " size", "<in-bytes>"),
    CLI_OPT_CALL('p', CNF_STR_PLOAD,  cli_opt_p, OPT_PLOAD_HDR " in hex notation", "<upto-16-bytes>"),
    CLI_OPT_CALL('I', CNF_STR_INFO,   cli_opt_I, OPT_INFO_HEADER " to display", "[" INFO_PATT "]"),
    CLI_OPT_CALL('S', CNF_STR_STAT,   cli_opt_S, OPT_STAT_HDR " to display", "[" STAT_PATT "]"),
    CLI_OPT_CALL('T', CNF_STR_THEME,  cli_opt_T, "Toggle dark/light themes", "<bits>"),
    CLI_OPT_CALL('g', CNF_STR_GRAPH,  cli_opt_g, OPT_GRAPH_HDR " to draw", "<type>"),
    CLI_OPT_CALL('G', CNF_STR_EXTRA,  cli_opt_G, OPT_GREX_HDR, "<elems>"),
    CLI_OPT_CALL('L', CNF_STR_LGND,   cli_opt_L, OPT_GRLG_HDR, "<fields>"),
    CLI_OPT_NONE('R', "run",          &autostart, "Autostart from CLI (if target is set)"),
    CLI_OPT_CALL('r', CNF_STR_RECAP,  cli_opt_r, OPT_RECAP_HDR " at exit (" RECAP_TYPE_MESG ")", "[" RECAP_PATT "]"),
    CLI_OPT_INTC('v', "verbose",      &verbose, "Messaging to stdout", "<level>"),
    CLI_OPT_NONE('V', "version",      &version, "Runtime versions"),
    { .long_name = G_OPTION_REMAINING, .arg = G_OPTION_ARG_STRING_ARRAY, .arg_data = &target, .arg_description = "TARGET" },
    {}
  };
  // options
  GOptionGroup *og = g_option_group_new(APPNAME, APPNAME, "options", &opts, NULL);
  if (!og) { FAIL("g_option_group_new()"); return false; }
  g_option_group_add_entries(og, options);
  GOptionContext *oc = g_option_context_new(NULL);
  if (!oc) { FAIL("g_option_context_new()"); return false; }
  g_option_context_set_main_group(oc, og);
  { GError *error = NULL;
    cli = true; gboolean okay = g_option_context_parse(oc, pargc, pargv, &error); cli = false;
    if (!okay) {
      g_message("%s", error ? error->message : unkn_error); g_error_free(error);
      g_option_context_free(oc); return false; }
    if (version) { g_print("%s", appver); char *ver;
      if ((ver = rtver(GTK_STRV)))   { g_print(" +gtk-%s", ver);   g_free(ver); }
      if ((ver = rtver(CAIRO_STRV))) { g_print(" +cairo-%s", ver); g_free(ver); }
      if ((ver = rtver(PANGO_STRV))) { g_print(" +pango-%s", ver); g_free(ver); }
      g_print("\n"); g_option_context_free(oc); exit(EXIT_SUCCESS);
    }
  }
  if (ipv4 && ipv6) CLI_FAIL("options %s are mutually exclusive", "-4/-6");
  if (ipv4 && (opts.ipv != 4)) { opts.ipv = 4; g_message(OPT_IPV4_HDR " only %s", TOGGLE_ON_HDR); }
  if (ipv6 && (opts.ipv != 6)) { opts.ipv = 6; g_message(OPT_IPV6_HDR " only %s", TOGGLE_ON_HDR); }
  if (num >= 0) cli_set_opt_dns(num, &opts);
  // arguments
  if (target) {
    for (gchar **p = target; *p; p++) {
      const gchar *arg = *p;
      if (!arg || !arg[0]) continue;
      if (arg[0] == '-') CLI_FAIL("Unknown option: '%s'", arg);
      if (opts.target) g_message(ENT_TARGET_HDR " is already set, skip '%s'", arg);
      else {
        cli = true; gchar *pinghost = parser_valid_target(arg); cli = false;
        if (pinghost) g_message(ENT_TARGET_HDR " %s", pinghost);
        else {
          g_message("Invalid " ENT_TARGET_HDR ": '%s'", arg);
          g_strfreev(target); g_option_context_free(oc); return false;
        }
        g_free(opts.target); opts.target = pinghost;
      }
    }
    g_strfreev(target);
  }
  if (autostart) {
    if (opts.target) g_message("%s", "Autostart");
    else CLI_FAIL("Autostart option %s is only used with TARGET setted", "-R");
  }
  if (opts.recap) {
    autostart = true;
    if (opts.target) g_message("%s", "Non-interactive mode with summary at exit");
    else CLI_FAIL("Recap option %s is only used with TARGET setted", "-r");
  }
  g_option_context_free(oc);
  return true;
}

