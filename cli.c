
#include "cli.h"
#include "pinger.h"
#include "parser.h"
#include "ui/action.h"
#include "ui/notifier.h"

/*
    case ENT_STR_PLOAD: {
      const char *pad = parser_pad(got, en->en.name);
      if (pad) {
        if (en->pstr) g_strlcpy(en->pstr, pad, en->slen);
        notifier_inform("%s: %s", en->en.name, pad);
      } else set_ed_texthint(en);
    } break;
  }
*/

static bool cli_int_opt(const char *name, const char *value, GError **error, int typ, const char *hdr,
    int min, int max, int *opt) {
  if (!value || !hdr) return false;
  cli = true;
  t_minmax range = { .min = min, .max = max };
  int n = parser_int(value, typ, hdr, range);
  cli = false;
  if (n < 0) {
    if (error) g_set_error(error,  g_quark_from_static_string(""), -1, "option %s: invalid value: %s", name, value);
    return false;
  }
  if (opt) *opt = n;
  cli = true; notifier_inform("%s: %d", hdr, n); cli = false;
  return true;
}


static bool cli_opt_c(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_CYCLES, OPT_CYCLES_HDR, CYCLES_MIN, CYCLES_MAX, &opts->cycles) : false;
}

static bool cli_opt_i(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_IVAL, OPT_IVAL_HDR, IVAL_MIN, IVAL_MAX, &opts->timeout) : false;
}

static bool cli_opt_q(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_QOS, OPT_QOS_HDR, 0, QOS_MAX, &opts->qos) : false;
}

static bool cli_opt_s(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, ENT_STR_PSIZE, OPT_PSIZE_HDR, PSIZE_MIN, PSIZE_MAX, &opts->size) : false;
}


// pub
//

bool cli_init(int *pargc, char ***pargv) {
  if (!pargc || !pargv) return false;
  const GOptionEntry options[] = {
    { .long_name = "cycles", .short_name = 'c',
      .flags = G_OPTION_FLAG_IN_MAIN, .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_c,
      .arg_description = "<number>", .description = "<number> of cycles per target" },
    { .long_name = "interval", .short_name = 'i',
      .flags = G_OPTION_FLAG_IN_MAIN, .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_i,
      .arg_description = "<seconds>", .description = "Interval in <seconds> between pings" },
    { .long_name = "qos", .short_name = 'q',
      .flags = G_OPTION_FLAG_IN_MAIN, .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_q,
      .arg_description = "<byte>", .description = "QoS/ToS" },
    { .long_name = "size", .short_name = 's',
      .flags = G_OPTION_FLAG_IN_MAIN, .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_s,
      .arg_description = "<number>", .description = "Payload size" },
//    { .long_name = "logmax", .short_name = 'l',
//      .flags = G_OPTION_FLAG_IN_MAIN, .arg = G_OPTION_ARG_CALLBACK, .arg_data = cli_opt_l,
//      .arg_description = "<number>", .description = "Max lines in log-tab" },
    { .long_name = "numeric", .short_name = 'n',
      .flags = G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_REVERSE, .arg = G_OPTION_ARG_NONE, .arg_data = &opts.dns,
      .description = "Numeric output (i.e. disable DNS resolv)" },
    { .long_name = "verbose", .short_name = 'v',
      .flags = G_OPTION_FLAG_IN_MAIN, .arg = G_OPTION_ARG_INT, .arg_data = &verbose,
      .description = "Verbose level for messages to stdout" },
    {}
  };
  // parse options
  GOptionContext *oc = g_option_context_new(NULL);
  GOptionGroup *og = g_option_group_new(APPNAME, APPNAME, "options", &opts, NULL);
  g_option_group_add_entries(og, options);
  g_option_context_set_main_group(oc, og);
  bool pre_dns = opts.dns;
  GError *error = NULL;
  if (!g_option_context_parse(oc, pargc, pargv, &error)) {
    g_warning("%s", error->message);
    return false;
  }
  if (pre_dns != opts.dns) {
    cli = true; notifier_inform(OPT_DNS_HDR " %s", opts.dns ? TOGGLE_ON_HDR : TOGGLE_OFF_HDR); cli = false; }
  // parse arguments
  for (int i = 1; i < *pargc; i++) {
    const gchar *arg = (*pargv)[i];
    if (!arg || !arg[0]) continue;
    if (arg[0] == '-') { g_warning("Unknown option: '%s'", arg); return false; }
    if (opts.target) g_warning("Target is already set, skip '%s'", arg);
    else {
      cli = true; gchar *target = parser_valid_target(arg); cli = false;
      if (!target) { g_warning("Invalid target: '%s'", arg); return false; }
      else { cli = true; notifier_inform(ENT_TARGET_HDR " %s", target); cli = false; }
      g_free(opts.target); opts.target = target;
    }
  }
  *pargc = 1;
  return true;
}

