
#include <stdlib.h>

#include "cli.h"
#include "stat.h"
#include "pinger.h"
#include "parser.h"
#include "ui/action.h"
#include "ui/option.h"
#include "ui/notifier.h"

#define CLI_SET_ERR { if (error) \
  g_set_error(error, g_quark_from_static_string(""), -1, "Not valid option %s: %s", name, value); }

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

static gboolean cli_opt_r(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_RECAP);
}


// pub
//

#define CLI_FAIL(fmt, arg) { g_message(fmt, arg); g_option_context_free(oc); return false; }

#ifdef WITH_JSON
#define RECAP_TYPE_MESG "text, csv, or json"
#else
#define RECAP_TYPE_MESG "text, csv"
#endif

gboolean autostart;

gboolean cli_init(int *pargc, char ***pargv) {
  if (!pargc || !pargv) return false;
  gchar** target = NULL;
  gboolean num = false, ipv4 = false, ipv6 = false, version = false;
  const GOptionEntry options[] = {
    { .long_name = "ipv4",     .short_name = '4', .arg = G_OPTION_ARG_NONE,     .arg_data = &ipv4,
      .description = OPT_IPV4_HDR " only" },
    { .long_name = "ipv6",     .short_name = '6', .arg = G_OPTION_ARG_NONE,     .arg_data = &ipv6,
      .description = OPT_IPV4_HDR " only" },
    { .long_name = "numeric",  .short_name = 'n', .arg = G_OPTION_ARG_NONE,     .arg_data = &num,
      .description = "Numeric output (i.e. disable " OPT_DNS_HDR " resolv)" },
    { .long_name = "cycles",   .short_name = 'c', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_c,
      .arg_description = "<number>",         .description = OPT_CYCLES_HDR " per target" },
    { .long_name = "interval", .short_name = 'i', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_i,
      .arg_description = "<seconds>",        .description = OPT_IVAL_HDR " between pings" },
    { .long_name = "ttl",      .short_name = 't', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_t,
      .arg_description = "[min][,max]",      .description = OPT_TTL_HDR " range" },
    { .long_name = "qos",      .short_name = 'q', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_q,
      .arg_description = "<bits>",           .description = OPT_QOS_HDR "/ToS byte" },
    { .long_name = "size",     .short_name = 's', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_s,
      .arg_description = "<in-bytes>",       .description = OPT_PLOAD_HDR " size" },
    { .long_name = "payload",  .short_name = 'p', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_p,
      .arg_description = "<upto-16-bytes>",  .description = OPT_PLOAD_HDR " in hex notation" },
    { .long_name = "info",     .short_name = 'I', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_I,
      .arg_description = "[" INFO_PATT "]",  .description = OPT_INFO_HEADER " to display" },
    { .long_name = "stat",     .short_name = 'S', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_S,
      .arg_description = "[" STAT_PATT "]",  .description = OPT_STAT_HDR " to display" },
    { .long_name = "graph",    .short_name = 'g', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_g,
      .arg_description = "<type>",           .description = OPT_GRAPH_HDR " to draw" },
    { .long_name = "gextra",   .short_name = 'G', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_G,
      .arg_description = "<elems>",          .description = OPT_GREX_HDR },
    { .long_name = "legend",   .short_name = 'L', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_L,
      .arg_description = "<fields>",         .description = OPT_GRLG_HDR },
    { .long_name = "run",      .short_name = 'R', .arg = G_OPTION_ARG_NONE,     .arg_data = &autostart,
      .description = "Autostart from CLI (if target is set)" },
    { .long_name = "recap",    .short_name = 'r', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_r,
      .arg_description = "[" RECAP_PATT "]", .description = OPT_RECAP_HDR " at exit (" RECAP_TYPE_MESG ")"},
    { .long_name = "verbose",  .short_name = 'v', .arg = G_OPTION_ARG_INT,      .arg_data = &verbose,
      .arg_description = "<level>",          .description = "Messaging to stdout" },
    { .long_name = "version",  .short_name = 'V', .arg = G_OPTION_ARG_NONE,     .arg_data = &version,
      .description = "Runtime versions" },
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
    if (!okay) CLI_FAIL("%s", error->message);
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
  if (num != !opts.dns) { opts.dns = !num; g_message(OPT_DNS_HDR " %s", opts.dns ? TOGGLE_ON_HDR : TOGGLE_OFF_HDR); }
  // arguments
  if (target) {
    for (gchar **p = target; *p; p++) {
      const gchar *arg = *p;
      if (!arg || !arg[0]) continue;
      if (arg[0] == '-') CLI_FAIL("Unknown option: '%s'", arg);
      if (opts.target) g_message(ENT_TARGET_HDR " is already set, skip '%s'", arg);
      else {
        cli = true; gchar *target = parser_valid_target(arg); cli = false;
        if (target) g_message(ENT_TARGET_HDR " %s", target);
        else CLI_FAIL("Invalid target: '%s'", arg);
        g_free(opts.target); opts.target = target;
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

