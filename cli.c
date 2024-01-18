
#include "cli.h"
#include "stat.h"
#include "pinger.h"
#include "parser.h"
#include "ui/action.h"
#include "ui/option.h"
#include "ui/notifier.h"

#define CLI_MESG(fmt, ...) { cli = true; PP_NOTIFY(fmt, __VA_ARGS__); cli = false; }

static gboolean cli_int_opt(const char *name, const char *value, GError **error, int typ, const char *hdr,
    int min, int max, int *opt) {
  if (!value || !hdr) return false;
  cli = true;
  t_minmax range = { .min = min, .max = max };
  int n = parser_int(value, typ, hdr, range);
  cli = false;
  if (n < 0) {
    if (error) g_set_error(error, g_quark_from_static_string(""), -1, "option %s: invalid value: %s", name, value);
    return false;
  }
  if (opt) *opt = n;
  CLI_MESG("%s: %d", hdr, n);
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
    t_minmax r = parser_range(cp);
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
    if (okay) CLI_MESG("%s: %d <=> %d", en->c.en.name, min + 1, lim)
    else CLI_MESG("%s: no match with [%d,%d]", en->c.en.name, 1, MAXTTL);
    g_free(cp);
  } else CLI_MESG("%s: %s failed", name, "g_sdtrdup()");
  return okay;
}

static t_ent_bool* cli_infostat_entry(int c, int typ) {
  int ndx = -1;
  if        (typ == STR_RX_INFO) switch (c) { // info elements
    case 'h': ndx = ENT_BOOL_HOST; break;
    case 'a': ndx = ENT_BOOL_AS;   break;
    case 'c': ndx = ENT_BOOL_CC;   break;
    case 'd': ndx = ENT_BOOL_DESC; break;
    case 'r': ndx = ENT_BOOL_RT;   break;
  } else if (typ == STR_RX_STAT) switch (c) { // stat elements
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

static const char* cli_opt_infostat(const char *value, int sz, int typ, const gchar *hdr) {
  const char *str = parser_str(value, hdr, sz, typ);
  for (const char *p = str; *p; p++) {
    t_ent_bool *en = cli_infostat_entry(*p, typ);
    if (!en) continue;
    if (en->pval) *en->pval = true;
    PP_NOTIFY("%s: %s: %s", en->prefix, en->en.name, TOGGLE_ON_HDR);
  }
  return str;
}

static gboolean cli_opt_elem(const char *name, const char *value, GError **error, int sz, int typ) {
  if (!value) return false;
  const char *str = NULL;
  cli = true;
  switch (typ) {
    case STR_RX_INFO: stat_clean_elems(ENT_EXP_INFO); str = cli_opt_infostat(value, sz, typ, OPT_INFO_HDR); break;
    case STR_RX_STAT: stat_clean_elems(ENT_EXP_STAT); str = cli_opt_infostat(value, sz, typ, OPT_STAT_HDR); break;
    case STR_RX_PAD: {
      t_ent_str *en = &ent_str[ENT_STR_PLOAD];
      str = parser_str(value, en->en.name, sz, typ);
      if (en->pstr) g_strlcpy(en->pstr, str, en->slen);
      PP_NOTIFY("%s: %s", en->en.name, str);
    } break;
  }
  cli = false;
  if (!str)
    g_set_error(error, g_quark_from_static_string(""), -1, "option %s: invalid value: '%s'", name, value);
  return (str != NULL);
}

static gboolean cli_opt_p(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, PAD_SIZE, STR_RX_PAD);
}

static gboolean cli_opt_I(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, sizeof(INFO_PATT) * 2, STR_RX_INFO);
}

static gboolean cli_opt_S(const char *name, const char *value, gpointer unused, GError **error) {
  return cli_opt_elem(name, value, error, sizeof(STAT_PATT) * 2, STR_RX_STAT);
}


// pub
//

gboolean cli_init(int *pargc, char ***pargv) {
  if (!pargc || !pargv) return false;
  gchar** target = NULL;
  gboolean num = false, ipv4 = false, ipv6 = false;
  const GOptionEntry options[] = {
    { .long_name = "numeric",  .short_name = 'n', .arg = G_OPTION_ARG_NONE,     .arg_data = &num,
      .description = "Numeric output (i.e. disable " OPT_DNS_HDR " resolv)" },
    { .long_name = "cycles",   .short_name = 'c', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_c,
      .arg_description = "<number>", .description = OPT_CYCLES_HDR " per target" },
    { .long_name = "interval", .short_name = 'i', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_i,
      .arg_description = "<seconds>", .description = OPT_IVAL_HDR " between pings" },
    { .long_name = "ttl",      .short_name = 't', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_t,
      .arg_description = "[min][,max]", .description = OPT_TTL_HDR " range" },
    { .long_name = "qos",      .short_name = 'q', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_q,
      .arg_description = "<bits>", .description = OPT_QOS_HDR "/ToS byte" },
    { .long_name = "size",     .short_name = 's', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_s,
      .arg_description = "<in-bytes>", .description = OPT_PLOAD_HDR " size" },
    { .long_name = "payload",  .short_name = 'p', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_p,
      .arg_description = "<upto-16-bytes>", .description = OPT_PLOAD_HDR " in hex notation" },
    { .long_name = "info",     .short_name = 'I', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_I,
      .arg_description = "[" INFO_PATT "]", .description = OPT_INFO_HDR " to display" },
    { .long_name = "stat",     .short_name = 'S', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_S,
      .arg_description = "[" STAT_PATT "]", .description = OPT_STAT_HDR " to display" },
    { .long_name = "graph",    .short_name = 'g', .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_g,
      .arg_description = "<type>", .description = OPT_GRAPH_HDR " to draw" },
    { .long_name = "ipv4",     .short_name = '4', .arg = G_OPTION_ARG_NONE,     .arg_data = &ipv4,
      .description = OPT_IPV4_HDR " only" },
    { .long_name = "ipv6",     .short_name = '6', .arg = G_OPTION_ARG_NONE,     .arg_data = &ipv6,
      .description = OPT_IPV4_HDR " only" },
    { .long_name = "verbose",  .short_name = 'v', .arg = G_OPTION_ARG_INT,      .arg_data = &verbose,
      .arg_description = "<level>", .description = "Messaging to stdout" },
    { .long_name = G_OPTION_REMAINING, .arg = G_OPTION_ARG_STRING_ARRAY, .arg_data = &target, .arg_description = "TARGET" },
    {}
  };
  // options
  GOptionGroup *og = g_option_group_new(APPNAME, APPNAME, "options", &opts, NULL);
  if (!og) { g_warning("%s failed", "g_option_group_new()"); return false; }
  g_option_group_add_entries(og, options);
  GOptionContext *oc = g_option_context_new(NULL);
  if (!oc) { g_warning("%s failed", "g_option_context_new()"); return false; }
  g_option_context_set_main_group(oc, og);
  GError *error = NULL;
  if (!g_option_context_parse(oc, pargc, pargv, &error)) { g_warning("%s", error->message); return false; }
  if (ipv4 && ipv6) { g_warning("options %s are mutually exclusive", "-4/-6"); return false; }
  if (ipv4 && (opts.ipv != 4)) { opts.ipv = 4; CLI_MESG(OPT_IPV4_HDR " only %s", TOGGLE_ON_HDR); }
  if (ipv6 && (opts.ipv != 6)) { opts.ipv = 6; CLI_MESG(OPT_IPV6_HDR " only %s", TOGGLE_ON_HDR); }
  if (num != !opts.dns) { opts.dns = !num; CLI_MESG(OPT_DNS_HDR " %s", opts.dns ? TOGGLE_ON_HDR : TOGGLE_OFF_HDR); }
  // arguments
  if (target) for (gchar **p = target; *p; p++) {
    const gchar *arg = *p;
    if (!arg || !arg[0]) continue;
    if (arg[0] == '-') { g_warning("Unknown option: '%s'", arg); return false; }
    if (opts.target) g_warning(ENT_TARGET_HDR " is already set, skip '%s'", arg);
    else {
      cli = true; gchar *target = parser_valid_target(arg); cli = false;
      if (!target) { g_warning("Invalid target: '%s'", arg); return false; }
      else CLI_MESG(ENT_TARGET_HDR " %s", target);
      g_free(opts.target); opts.target = target;
    }
  }
  g_strfreev(target);
  g_option_context_free(oc);
  return true;
}

