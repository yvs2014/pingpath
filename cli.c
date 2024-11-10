
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "config.h"
#include "common.h"

#include "cli.h"
#include "pinger.h"
#include "parser.h"
#include "ui/option.h"

#define CLI_SET_ERR { if (error) { (value && value[0]) ? \
  g_set_error(error, g_quark_from_static_string(""), -1, "Not valid %s: %s", name, value) : \
  g_set_error(error, g_quark_from_static_string(""), -1, "Empty %s is not valid", name); }}

#ifdef CONFIG_DEBUGGING
#define CONF_DEBUG(fmt, ...) do { if (verbose & 16) g_message("CONFIG: " fmt, __VA_ARGS__); } while (0)
#else
#define CONF_DEBUG(...) NOOP
#endif

#define CNF_SECTION_MAIN "ping"
#define CNF_STR_IPV    "ip-version"
#define CNF_STR_NODNS  "numeric"
#define CNF_STR_CYCLES "cycles"
#define CNF_STR_IVAL   "interval"
#define CNF_STR_TTL    "ttl"
#define CNF_STR_QOS    "qos"
#define CNF_STR_SIZE   "size"
#define CNF_STR_PLOAD  "payload"
#define CNF_STR_INFO   "info"
#define CNF_STR_STAT   "stat"
#define CNF_STR_TABS   "tabs"
#define CNF_STR_ATAB   "active-tab"
#define CNF_STR_THEME  "theme"
#define CNF_STR_GRAPH  "graph"
#define CNF_STR_EXTRA  "extra"
#define CNF_STR_LGND   "legend"
#ifdef WITH_PLOT
#define CNF_STR_PLEL   "plot"
#define CNF_STR_PLEX   "plot-params"
#endif
#define CNF_STR_RECAP  "recap"
#define CNF_SECTION_AUX  "aux"

enum { CNF_OPT_IPV, CNF_OPT_NODNS, CNF_OPT_CYCLES, CNF_OPT_IVAL, CNF_OPT_TTL, CNF_OPT_QOS, CNF_OPT_SIZE,
  CNF_OPT_PLOAD, CNF_OPT_INFO, CNF_OPT_STAT, CNF_OPT_TABS, CNF_OPT_ATAB, CNF_OPT_THEME, CNF_OPT_GRAPH, CNF_OPT_EXTRA, CNF_OPT_LGND,
#ifdef WITH_PLOT
  CNF_OPT_PLEL, CNF_OPT_PLEX,
#endif
  CNF_OPT_RECAP, CNF_OPT_MAX };

enum { ONOFF_IPV4, ONOFF_IPV6, ONOFF_VERSION, ONOFF_1D,
#ifdef WITH_PLOT
  ONOFF_2D,
#endif
  ONOFF_MAX };

typedef struct t_config_option {
  const char *opt;
  int type;
  void *data;
} t_config_option;

typedef struct t_config_section {
  const char *name;
  t_config_option options[CNF_OPT_MAX];
} t_config_section;

#ifdef DNDORD_DEBUGGING
static void cli_pr_elems(t_type_elem *elems, int max) {
  GString *string = g_string_new(NULL);
  for (int i = 0; i < max; i++)
    g_string_append_printf(string, " %c%d", elems[i].enable ? '+' : '-', elems[i].type);
  g_message("REORDER: elem:%s", string->str); g_string_free(string, true);
}
#define CLI_REORDER_PRINT_ELEMS(elems, max) do { if (verbose & 32) cli_pr_elems(elems, max); } while (0)
#else
#define CLI_REORDER_PRINT_ELEMS(...) NOOP
#endif

#define TAB1D "With ping tab only"
#ifdef WITH_PLOT
#define TAB2D "Without plot tab"
#define TABON(mesg) { opts->plot = false; g_message(mesg); }
#else
#define TABON(mesg) { g_message(mesg); }
#endif

//

static gboolean cli_int_opt(const char *name, const char *value, GError **error, const char *hdr,
    int min, int max, int *opt) {
  gboolean okay = false;
  if (value && hdr) {
    t_minmax minmax = { .min = min, .max = max };
    okay = parser_mmint(value, hdr, minmax, opt);
    if (okay) { if (opt) g_message("%s: %d", hdr, *opt); }
    else CLI_SET_ERR;
  }
  return okay;
}

static gboolean cli_opt_c(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, OPT_CYCLES_HDR, CYCLES_MIN, CYCLES_MAX, &opts->cycles) : false;
}

static gboolean cli_opt_i(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, OPT_IVAL_HDR, IVAL_MIN, IVAL_MAX, &opts->timeout) : false;
}

static gboolean cli_opt_q(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, OPT_QOS_HDR, 0, QOS_MAX, &opts->qos) : false;
}

static gboolean cli_opt_s(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, OPT_PSIZE_HDR, PSIZE_MIN, PSIZE_MAX, &opts->size) : false;
}

static gboolean cli_opt_g(const char *name, const char *value, t_opts *opts, GError **error) {
  return opts ? cli_int_opt(name, value, error, OPT_GRAPH_HDR, 1, GRAPH_TYPE_MAX - 1, &opts->graph) : false;
}

static gboolean cli_opt_t(const char *name, const char *value, t_opts *opts, GError **error) {
  if (!value || !opts) return false;
  char *dup = g_strdup(value);
  gboolean okay = false;
  if (dup) {
    t_ent_ndx *enc = &ent_spn[ENT_SPN_TTL].c.en;
    t_ent_spn_elem *elem = &ent_spn[ENT_SPN_TTL].list[0];
    int *pmin = elem->aux[0].pval;
    int *plim = elem->aux[1].pval;
    int min = opt_mm_ttl.min - 1, lim = opt_mm_ttl.max;
    t_minmax range; okay = parser_range(dup, ',', OPT_TTL_HDR, &range);
    if (okay) {
      if (range.min != INT_MIN) {
        okay = pinger_within_range(opt_mm_ttl.min, plim ? *plim : lim, range.min);
        if (okay) { min = range.min - 1; if (pmin) *pmin = min; }
      }
      if (range.max != INT_MIN) {
        okay = pinger_within_range(pmin ? (*pmin + 1) : opt_mm_ttl.min, opt_mm_ttl.max, range.max);
        if (okay) { lim = range.max; if (plim) *plim = lim; }
      }
    }
    if (okay) g_message("%s: %d <=> %d", enc->name, min + 1, lim);
    else g_warning("%s: no match with [%d,%d]", enc->name, opt_mm_ttl.min, opt_mm_ttl.max);
    g_free(dup);
  }
  if (!okay) CLI_SET_ERR;
  return okay;
}

#ifdef WITH_PLOT

enum { X_TAGS = 5, R_TAG = 'r', G_TAG = 'g', B_TAG = 'b', O_TAG = 'o', F_TAG = 'f' };
enum { XO_SPACE, XO_AXISX, XO_AXISY, XO_AXISZ, XO_STEP, XO_ITEMS };

static gboolean set_xparam_tag(int in, int *out, const t_minmax minmax) {
  gboolean okay = true;
  if (in != INT_MIN) {
    okay = MM_OKAY(minmax, in);
    if (okay && out) *out = in;
  }
  return okay;
}

static gboolean cli_opt_X_rgb(char *dup, int type, int ndx, t_minmax *minmax) {
  gboolean okay = false;
  if (dup && minmax) {
    const char *name = ent_spn[type].c.en.name;
    t_minmax input; t_ent_spn_elem *elem = &ent_spn[type].list[ndx];
    okay = parser_range(dup, ':', elem->title, &input);
    if (okay) {
      { okay = set_xparam_tag(input.min, elem->aux[0].pval, opt_mm_col); }
      if (okay)
        okay = set_xparam_tag(input.max, elem->aux[1].pval, opt_mm_col);
      if (okay) g_message("%s: from %d (0x%02x) to %d (0x%02x)", elem->title,
        minmax->min, minmax->min, minmax->max, minmax->max);
      else g_warning("%s: no match with [%d,%d]", name, opt_mm_col.min, opt_mm_col.max);
    } else g_warning("%s: cannot parse: %s", name, dup);
  }
  return okay;
}

static gboolean cli_opt_X_o(char *dup, int type, int ndx, int *pval[]) {
  gboolean okay = false;
  if (dup && pval) {
    const char *name = ent_spn[type].c.en.name;
    const char *title = ent_spn[type].list[ndx].title;
    int arr[XO_ITEMS]; okay = parser_ivec(dup, ':', title, arr, G_N_ELEMENTS(arr));
    if (okay) {
      t_minmax minmax = {.min = 0, .max = 1};
      okay = set_xparam_tag(arr[XO_SPACE], pval[XO_SPACE], minmax);
      if (okay) {
        minmax = opt_mm_rot;
        { okay = set_xparam_tag(arr[XO_AXISX], pval[XO_AXISX], minmax); }
        if (okay)
          okay = set_xparam_tag(arr[XO_AXISY], pval[XO_AXISY], minmax);
        if (okay)
          okay = set_xparam_tag(arr[XO_AXISZ], pval[XO_AXISZ], minmax);
      }
      if (okay) {
        minmax = opt_mm_ang;
        { okay = set_xparam_tag(arr[XO_STEP], pval[XO_STEP], minmax); }
      }
      if (okay) g_message("%s: %s space, orientation %d %d %d, step %d", title,
        *pval[0] ? "global" : "local", *pval[1], *pval[2], *pval[3], *pval[4]);
      else g_warning("%s: no match with [%d,%d]", name, minmax.min, minmax.max);
    } else g_warning("%s: cannot parse: %s", name, dup);
  }
  return okay;
}

static gboolean cli_opt_X_f(char *dup, int type, int ndx) {
  gboolean okay = false;
  t_ent_spn_aux *aux = ent_spn[type].list[ndx].aux;
  if (dup && aux && aux->mm && aux->pval) {
    okay = parser_mmint(dup, aux->sfx, *aux->mm, aux->pval);
    if (okay) g_message("%s (%s): %d°", aux->prfx, aux->sfx, *aux->pval);
    else g_warning("%s: no match with [%d,%d]", ent_spn[type].c.en.name, aux->mm->min, aux->mm->max);
  }
  return okay;
}

static gboolean xnth_nodup(unsigned *mask, unsigned nth) {
  gboolean okay = true;
  if (mask && nth) {
    unsigned flag = 1U << nth;
    if (*mask & flag) { okay = false; *mask |= 1U; } else *mask |= flag;
  }
  return okay;
}

static gboolean cli_opt_X_pair(const char *value, char tag, const char *name, t_opts *opts) {
  static unsigned xpairbits;
  gboolean okay = false;
  if (value && opts) {
    char *dup = g_strdup(value);
    if (dup) switch (tag) {
      case R_TAG: if (xnth_nodup(&xpairbits, 1)) okay = cli_opt_X_rgb(dup, ENT_SPN_COLOR, 0, &opts->rcol); break;
      case G_TAG: if (xnth_nodup(&xpairbits, 2)) okay = cli_opt_X_rgb(dup, ENT_SPN_COLOR, 1, &opts->gcol); break;
      case B_TAG: if (xnth_nodup(&xpairbits, 3)) okay = cli_opt_X_rgb(dup, ENT_SPN_COLOR, 2, &opts->bcol); break;
      case O_TAG: if (xnth_nodup(&xpairbits, 4)) {
        int* pval[XO_ITEMS] = { &opts->rglob, &opts->orient[0], &opts->orient[1], &opts->orient[2], &opts->angstep };
        okay = cli_opt_X_o(dup, ENT_SPN_GLOBAL, 0, pval);
      } break;
      case F_TAG: if (xnth_nodup(&xpairbits, 5)) okay = cli_opt_X_f(dup, ENT_SPN_FOV, 0); break;
      default: g_warning("%s: wrong tag: '%c'", name, tag);
    }
    g_free(dup);
    if (xpairbits & 1U) { g_warning("%s: tag duplicate: '%c'", name, tag); xpairbits &= ~1U; }
  }
  return okay;
}

static gboolean cli_opt_X(const char *name, const char *value, t_opts *opts, GError **error) {
  gboolean okay = false;
  if (value && opts) { // tag=subvalues
    char **pairs = g_strsplit(value, ",", X_TAGS + 1);
    if (pairs) for (char **tagval = pairs; *tagval; tagval++) {
      okay = false;
      char **pair = g_strsplit(*tagval, "=", 2); // tag=pair
      if ((pair[0] && pair[0][0]) && (pair[1] && pair[1][0])) {
        if (pair[0][1]) g_warning("%s: no char tag: %s", name, pair[0]);
        else okay = cli_opt_X_pair(pair[1], pair[0][0], name, opts);
      } else g_warning("%s: wrong pair: %s", name, *tagval);
      g_strfreev(pair);
      if (!okay) break;
    }
    g_strfreev(pairs);
    if (!okay) CLI_SET_ERR;
  }
  return okay;
}
#endif

static char* cli_opt_charelem(char *str, const char *patt, int ch) {
  if (str && patt && (ch >= 0)) for (const char *pchar = str; *pchar; pchar++) {
    int ndx = char2ndx(ch, true, *pchar);
    if (ndx < 0) continue;
    t_ent_bool *en = strchr(patt, *pchar) ? &ent_bool[ndx] : NULL;
    if (!en) continue;
    gboolean *pbool = EN_BOOLPTR(en);
    if (pbool) { *pbool = true; g_message("%s: %s: " TOGGLE_ON_HDR, en->prefix, en->en.name); }
  }
  return str;
}

static char* reorder_patt(const char *str, const char *patt) {
  char *order = g_strdup(patt);
  if (order) {
    char *o = order;
    for (const char *p = patt, *pstr = str; *p; p++, o++)
      if (strchr(str, *p)) { *o = *pstr; pstr++; };
    DNDORD("REORDER: %s + %s => %s", patt, str, order);
  }
  return order;
}

static void reorder_elems(const char *str, t_elem_desc *desc) {
  if (!str || !desc || !desc->elems || !desc->patt) return;
  char *order = reorder_patt(str, desc->patt);
  int len = desc->mm.max - desc->mm.min + 1;
  type2ndx_fn type2ndx = desc->t2n;
  if (type2ndx && order && (len > 0) && (strnlen(order, len + 1) == len)) {
    t_type_elem *elems = &desc->elems[desc->mm.min];
    t_type_elem newelems[len];
    memmove(newelems, elems, sizeof(newelems)); // BUFFNOLINT
    int n = 0;
    for (const char *p = order; *p; p++, n++) {
      int ndx = char2ndx(desc->cat, true, *p);
      int type = char2ndx(desc->cat, false, *p);
      if ((type < 0) || (ndx < 0)) WARN("Not valid indexes: ent=%d elem=%d", ndx, type); else {
        ndx = type2ndx(type);
        if (ndx < 0) WARN("Unknown %d type", type);
        else newelems[n] = desc->elems[ndx];
      }
    }
    memmove(elems, newelems, sizeof(newelems)); // BUFFNOLINT
  } else WARN("Number of indexes are different");
  g_free(order);
}

static char* cli_char_opts(int type, const char *value, int cat, t_elem_desc *desc, int max, const char *hdr) {
  if (!desc || !desc->elems) return NULL;
  CLI_REORDER_PRINT_ELEMS(desc->elems, max);
  clean_elems(type);
  char *str = cli_opt_charelem(parser_str(value, hdr, cat), desc->patt, desc->cat);
  if (str) { if (str[0]) reorder_elems(str, desc); else g_message("%s: off", hdr); }
  CLI_REORDER_PRINT_ELEMS(desc->elems, max);
  return str;
}

#define RECAP_TYPE_EXPAND(type) (((type) == 't') ? "text" : (((type) == 'c') ? "csv" : \
 ((g_ascii_tolower(type) == 'j') ? "json" : "unknown")))

static gboolean cli_opt_elem(const char *name, const char *value, GError **error, int cat) {
  if (!value) return false;
  char *str = NULL;
  switch (cat) {
    case OPT_TYPE_PAD: {
      t_ent_str *en = &ent_str[ENT_STR_PLOAD];
      str = parser_str(value, en->en.name, cat);
      if (str) {
        if (en->pstr) g_strlcpy(en->pstr, str, en->slen);
        g_message("%s: %s", en->en.name, str);
      }
    } break;
    case OPT_TYPE_INFO:
      str = cli_char_opts(ENT_EXP_INFO, value, cat, &info_desc, PE_MAX, OPT_INFO_HDR); break;
    case OPT_TYPE_STAT:
      str = cli_char_opts(ENT_EXP_STAT, value, cat, &stat_desc, PE_MAX, OPT_STAT_HDR); break;
    case OPT_TYPE_GRLG:
      str = cli_char_opts(ENT_EXP_LGFL, value, cat, &grlg_desc, GX_MAX, OPT_GRLG_HDR); break;
    case OPT_TYPE_GREX:
      str = cli_char_opts(ENT_EXP_GREX, value, cat, &grex_desc, GX_MAX, OPT_GREX_HDR); break;
#ifdef WITH_PLOT
    case OPT_TYPE_PLEL:
      str = cli_char_opts(ENT_EXP_PLEL, value, cat, &plot_desc, D3_MAX, OPT_PLOT_HDR); break;
#endif
    case OPT_TYPE_RECAP: {
      str = parser_str(value, OPT_RECAP_HDR, OPT_TYPE_RECAP);
      if (str) { opts.recap = str[0]; g_message("Summary: %s", RECAP_TYPE_EXPAND(opts.recap)); }
    } break;
    default: break;
  }
  if (str) { g_free(str); return true; }
  CLI_SET_ERR; return false;
}

static gboolean cli_opt_p(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_PAD);
}

static gboolean cli_opt_I(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_INFO);
}

static gboolean cli_opt_S(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_STAT);
}

static gboolean cli_opt_L(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_GRLG);
}

static gboolean cli_opt_G(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_GREX);
}

#ifdef WITH_PLOT
static gboolean cli_opt_P(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_PLEL);
}
#endif

static gboolean cli_opt_T(const char *name, const char *value, t_opts *opts, GError **error) {
  if (!opts) return false;
  int mask = opts->darktheme | (opts->darkgraph << 1) | (opts->legend << 2)
#ifdef WITH_PLOT
    | (opts->darkplot << 3), max = 0xf;
#else
    , max = 0x7;
#endif

  gboolean okay = cli_int_opt(name, value, error, "Theme bits", 0, max, &mask);
  if (okay) {
    opts->darktheme = (mask & 0x1) ? true : false;
    opts->darkgraph = (mask & 0x2) ? true : false;
    opts->legend    = (mask & 0x4) ? true : false;
#ifdef WITH_PLOT
    opts->darkplot  = (mask & 0x8) ? true : false;
#endif
  }
  return okay;
}

static gboolean cli_opt_r(const char *name, const char *value, void *unused, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_RECAP);
}

static gboolean cli_opt_A(const char *name, const char *value, void *unused, GError **error) {
  int aopt = activetab + 1;
  gboolean valid = cli_int_opt(name, value, error, OPT_ATAB_HDR, ATAB_MIN + 1, ATAB_MAX + 1, &aopt);
  if (valid) activetab = aopt - 1;
  return valid;
}

static gboolean config_opt_not_found(GError *opterr, const char *section, const char *name,
   const char *value, GError **error) {
  if (g_error_matches(opterr, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
      g_error_matches(opterr, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) return true;
  g_warning("%s:%s: %s", section, name, opterr ? opterr->message : unkn_error);
  CLI_SET_ERR; return false;
}

static void cli_set_opt_dns(gboolean numeric, t_opts *opts) {
  if (!opts) return;
  gboolean dns = !numeric; if (opts->dns == dns) return;
  opts->dns = dns; g_message(OPT_DNS_HDR " %s", onoff(dns));
}

static gboolean cli_opt_f(const char *name, const char *value, t_opts *opts, GError **error) {
  const t_config_section config_sections[] = {
    { .name = CNF_SECTION_MAIN, .options = {
      { .opt = CNF_STR_IPV,    .type = CNF_OPT_IPV },
      { .opt = CNF_STR_NODNS,  .type = CNF_OPT_NODNS },
      { .opt = CNF_STR_CYCLES, .type = CNF_OPT_CYCLES, .data = cli_opt_c },
      { .opt = CNF_STR_IVAL,   .type = CNF_OPT_IVAL,   .data = cli_opt_i },
      { .opt = CNF_STR_TTL,    .type = CNF_OPT_TTL,    .data = cli_opt_t },
      { .opt = CNF_STR_QOS,    .type = CNF_OPT_QOS,    .data = cli_opt_q },
      { .opt = CNF_STR_SIZE,   .type = CNF_OPT_SIZE,   .data = cli_opt_s },
      { .opt = CNF_STR_PLOAD,  .type = CNF_OPT_PLOAD,  .data = cli_opt_p },
      { .opt = CNF_STR_INFO,   .type = CNF_OPT_INFO,   .data = cli_opt_I },
      { .opt = CNF_STR_STAT,   .type = CNF_OPT_STAT,   .data = cli_opt_S },
      {} // trailing 0
    }},
    { .name = CNF_SECTION_AUX, .options = {
      { .opt = CNF_STR_TABS,   .type = CNF_OPT_TABS },
      { .opt = CNF_STR_ATAB,   .type = CNF_OPT_ATAB,   .data = cli_opt_A },
      { .opt = CNF_STR_THEME,  .type = CNF_OPT_THEME,  .data = cli_opt_T },
      { .opt = CNF_STR_GRAPH,  .type = CNF_OPT_GRAPH,  .data = cli_opt_g },
      { .opt = CNF_STR_EXTRA,  .type = CNF_OPT_EXTRA,  .data = cli_opt_G },
      { .opt = CNF_STR_LGND,   .type = CNF_OPT_LGND,   .data = cli_opt_L },
#ifdef WITH_PLOT
      { .opt = CNF_STR_PLEL,   .type = CNF_OPT_PLEL,   .data = cli_opt_P },
      { .opt = CNF_STR_PLEX,   .type = CNF_OPT_PLEX,   .data = cli_opt_X },
#endif
      { .opt = CNF_STR_RECAP,  .type = CNF_OPT_RECAP,  .data = cli_opt_r },
      {} // trailing 0
    }},
    {} // trailing 0
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
      g_autofree char *optval = NULL;
      { g_autoptr(GError) strerr = NULL;
        optval = g_key_file_get_string(file, section, optname, &strerr);
        if (strerr) {
          if (config_opt_not_found(strerr, section, optname, optval, error)) continue;
          return false;
        }
        CONF_DEBUG("validate %s:%s=%s", section, optname, optval);
      }
      switch (option->type) {
        case CNF_OPT_TABS: {
          int val = g_key_file_get_integer(file, section, optname, &opterr);
          if (!opterr) {
            if (val == 1) {
              if (opts) { opts->graph = 0; TABON(TAB1D); }
#ifdef WITH_PLOT
            } else if (val == 2) {
              if (opts) TABON(TAB2D);
#endif
            } else { g_warning("%s:%s: %d is not admissible", section, optname, val); CLI_SET_ERR; return false; }
          }
        } break;
        case CNF_OPT_IPV: {
          int val = g_key_file_get_integer(file, section, optname, &opterr);
          if (!opterr) {
            if ((val == 4) || (val == 6)) {
              if (opts->ipv != val) {
                opts->ipv = val;
                g_message("%s only " TOGGLE_ON_HDR, (val == 4) ? OPT_IPV4_HDR : OPT_IPV6_HDR);
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
        case CNF_OPT_ATAB:
        case CNF_OPT_THEME:
        case CNF_OPT_GRAPH:
        case CNF_OPT_EXTRA:
        case CNF_OPT_LGND:
#ifdef WITH_PLOT
        case CNF_OPT_PLEL:
        case CNF_OPT_PLEX:
#endif
        case CNF_OPT_RECAP: if (optval) {
          if (option->data && !(((GOptionArgFunc)option->data)(optname, optval, opts, error))) return false;
        } break;
        default: break;
      }
      if (opterr && !config_opt_not_found(opterr, section, optname, optval, error)) return false;
    }
  }
  return true;
}


// pub
//

#ifdef WITH_JSON
#define RECAP_TYPE_MESG "text, csv, or json"
#else
#define RECAP_TYPE_MESG "text, csv"
#endif

#define CLI_OPT_NONE(sname, lname, adata, mdesc) { .short_name = (sname), .long_name = (lname), \
  .description = (mdesc), .arg = G_OPTION_ARG_NONE, .arg_data = (adata) }
#define CLI_OPT_SMTH(sname, lname, adata, mdesc, adesc, type) { .short_name = (sname), .long_name = (lname), \
  .description = (mdesc), .arg = (type), .arg_data = (adata), .arg_description = (adesc) }
#define CLI_OPT_CALL(sname, lname, adata, mdesc, adesc) \
  CLI_OPT_SMTH(sname, lname, adata, mdesc, adesc, G_OPTION_ARG_CALLBACK)
#define CLI_OPT_INTC(sname, lname, adata, mdesc, adesc) \
  CLI_OPT_SMTH(sname, lname, adata, mdesc, adesc, G_OPTION_ARG_INT)

gboolean autostart;

static inline gboolean cli_tab_opt(const gboolean off[2], t_opts *opts) {
  gboolean okay = (off[0] + off[1]) < 2;
  if (okay) {
    if (opts && off[0]) { opts->graph = 0; TABON(TAB1D); }
#ifdef WITH_PLOT
    if (opts && off[1]) TABON(TAB2D);
#endif
  } else g_message("options %s are mutually exclusive", "-1/-2");
  return okay;
}

static inline void print_libver(const char *pre, char *ver) {
  if (ver) { g_print(" +%s-%s", pre, ver); g_free(ver); }}

int cli_init(int *pargc, char ***pargv) {
#define CLI_FAIL(fmt, arg) { g_message(fmt, arg); g_option_context_free(context); return EXIT_FAILURE; }
  if (!pargc || !pargv) return EXIT_FAILURE;
  char** target = NULL;
  int num = -1; gboolean onoff[ONOFF_MAX + 1] = {false};
  const GOptionEntry options[] = {
    CLI_OPT_CALL('f', "file",         cli_opt_f, "Read options from file", "<filename>"),
    CLI_OPT_NONE('n', CNF_STR_NODNS,  &num, "Numeric output (i.e. disable " OPT_DNS_HDR " resolv)"),
    CLI_OPT_CALL('c', CNF_STR_CYCLES, cli_opt_c, OPT_CYCLES_HDR " per target", "<number>"),
    CLI_OPT_CALL('i', CNF_STR_IVAL,   cli_opt_i, OPT_IVAL_HDR " between pings", "<seconds>"),
    CLI_OPT_CALL('t', CNF_STR_TTL,    cli_opt_t, OPT_TTL_HDR " range", "[min][,max]"),
    CLI_OPT_CALL('q', CNF_STR_QOS,    cli_opt_q, OPT_QOS_HDR "/ToS byte", "<bits>"),
    CLI_OPT_CALL('s', CNF_STR_SIZE,   cli_opt_s, OPT_PLOAD_HDR " size", "<in-bytes>"),
    CLI_OPT_CALL('p', CNF_STR_PLOAD,  cli_opt_p, OPT_PLOAD_HDR " in hex notation", "<upto-16-bytes>"),
    CLI_OPT_CALL('I', CNF_STR_INFO,   cli_opt_I, OPT_INFO_HDR " to display", "[" INFO_PATT "]"),
    CLI_OPT_CALL('S', CNF_STR_STAT,   cli_opt_S, OPT_STAT_HDR " to display", "[" STAT_PATT "]"),
    CLI_OPT_CALL('T', CNF_STR_THEME,  cli_opt_T, "Toggle dark/light themes", "<bits>"),
    CLI_OPT_CALL('g', CNF_STR_GRAPH,  cli_opt_g, OPT_GRAPH_HDR " to draw", "[123]"),
    CLI_OPT_CALL('G', CNF_STR_EXTRA,  cli_opt_G, OPT_GREX_HDR, "[" GREX_PATT "]"),
    CLI_OPT_CALL('L', CNF_STR_LGND,   cli_opt_L, OPT_GRLG_HDR, "[" GRLG_PATT "]"),
#ifdef WITH_PLOT
    CLI_OPT_CALL('P', CNF_STR_PLEL,   cli_opt_P, OPT_PLOT_HDR, "[" PLEL_PATT "]"),
    CLI_OPT_CALL('X', CNF_STR_PLEX,   cli_opt_X, OPT_PLEX_HDR, "<tag:values>"),
#endif
    CLI_OPT_CALL('r', CNF_STR_RECAP,  cli_opt_r, OPT_RECAP_HDR " at exit (" RECAP_TYPE_MESG ")", "[" RECAP_PATT "]"),
    CLI_OPT_NONE('R', "run",          &autostart, "Autostart from CLI (if target is set)"),
    CLI_OPT_CALL('A', CNF_STR_ATAB,   cli_opt_A, OPT_ATAB_HDR, "[12"
#ifdef WITH_PLOT
      "3"
#endif
    "]"),
    CLI_OPT_INTC('v', "verbose",      &verbose, "Debug messages to stdout", "<6bit-level>"),
    CLI_OPT_NONE('V', "version",      &onoff[ONOFF_VERSION], "Runtime versions"),
    CLI_OPT_NONE('1', "pingtab-only", &onoff[ONOFF_1D], TAB1D),
#ifdef WITH_PLOT
    CLI_OPT_NONE('2', "without-plot", &onoff[ONOFF_2D], TAB2D),
#endif
    CLI_OPT_NONE('4', "ipv4",         &onoff[ONOFF_IPV4], OPT_IPV4_HDR " only"),
    CLI_OPT_NONE('6', "ipv6",         &onoff[ONOFF_IPV6], OPT_IPV6_HDR " only"),
    { .long_name = G_OPTION_REMAINING,
      .arg = G_OPTION_ARG_STRING_ARRAY, .arg_data = (void*)&target, .arg_description = "TARGET" },
    {} // trailing 0
  };
  // options
  GOptionGroup *group = g_option_group_new(APPNAME, APPNAME, "options", &opts, NULL);
  if (!group) return EXIT_FAILURE;
  g_option_group_add_entries(group, options);
  GOptionContext *context = g_option_context_new(NULL);
  if (!context) return EXIT_FAILURE;
  g_option_context_set_main_group(context, group);
  { GError *error = NULL;
    cli = true; gboolean okay = g_option_context_parse(context, pargc, pargv, &error); cli = false;
    if (!okay) {
      g_warning("%s", error ? error->message : unkn_error); g_error_free(error);
      g_option_context_free(context); return EXIT_FAILURE; }
    if (onoff[ONOFF_VERSION]) {
      g_print("%s", appver);
      print_libver("gtk",   rtver(GTK_STRV));
      print_libver("glib",  rtver(GLIB_STRV));
      print_libver("cairo", rtver(CAIRO_STRV));
      print_libver("pango", rtver(PANGO_STRV));
      g_print("\n");
      g_option_context_free(context);
      return -1;
    }
  }
  if (!cli_tab_opt(&onoff[ONOFF_1D], &opts)) return EXIT_FAILURE;
  if (onoff[ONOFF_IPV4] && onoff[ONOFF_IPV6]) CLI_FAIL("options %s are mutually exclusive", "-4/-6");
  if (onoff[ONOFF_IPV4] && (opts.ipv != 4)) { opts.ipv = 4; g_message(OPT_IPV4_HDR " only " TOGGLE_ON_HDR); }
  if (onoff[ONOFF_IPV6] && (opts.ipv != 6)) { opts.ipv = 6; g_message(OPT_IPV6_HDR " only " TOGGLE_ON_HDR); }
  if (num >= 0) cli_set_opt_dns(num, &opts);
  // arguments
  if (target) {
    for (char **ptr = target; *ptr; ptr++) {
      const char *arg = *ptr;
      if (!arg || !arg[0]) continue;
      if (arg[0] == '-') CLI_FAIL("Unknown option: '%s'", arg);
      if (opts.target) g_message(ENT_TARGET_HDR " is already set, skip '%s'", arg);
      else {
        cli = true; char *pinghost = parser_valid_target(arg); cli = false;
        if (pinghost) g_message(ENT_TARGET_HDR " %s", pinghost);
        else {
          g_message("Invalid " ENT_TARGET_HDR ": '%s'", arg);
          g_strfreev(target); g_option_context_free(context); return EXIT_FAILURE;
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
  g_option_context_free(context);
  return EXIT_SUCCESS;
#undef CLI_FAIL
}

