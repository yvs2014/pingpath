
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "cli.h"
#include "common.h"
#include "pinger.h"
#include "parser.h"
#include "ui/option.h"

#define CLI_SET_ERR do { if (error) { (value && value[0])  \
  ? g_set_error(error, g_quark_from_static_string(""), -1, \
    "%s: %s: %s", name, CLI_INVAL_HDR, value)              \
  : g_set_error(error, g_quark_from_static_string(""), -1, \
    "%s: %s",     name, CLI_NOVAL_HDR); }                  \
} while (0)

#ifdef CONFIG_DEBUGGING
#define CONF_DEBUG(fmt, ...) do { if (verbose & 16) \
  g_message("%s: " fmt, CLI_CONF_HDR, __VA_ARGS__); } while (0)
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

#ifdef WITH_PLOT
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
    else g_warning("%s: %s [%d,%d]", enc->name, CLI_NOMATCH_HDR, opt_mm_ttl.min, opt_mm_ttl.max);
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
      if (okay)
        g_message("%s: %d(0x%02x) <=> %d(0x%02x)", elem->title,
           minmax->min, minmax->min, minmax->max, minmax->max);
      else g_warning("%s: %s [%d,%d]", name, CLI_NOMATCH_HDR, opt_mm_col.min, opt_mm_col.max);
    } else g_warning("%s: %s: %s", name, CLI_NOPARSE_HDR, dup);
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
      if (okay) g_message("%s: %s, %s %d/%d/%d, %s %d", title,
        *pval[0] ? OPT_GLOB_HDR : OPT_LOCAL_HDR,
        ORIENT_HDR, *pval[1], *pval[2], *pval[3],
        ROT_STEP,   *pval[4]);
      else g_warning("%s: %s [%d,%d]", name, CLI_NOMATCH_HDR, minmax.min, minmax.max);
    } else g_warning("%s: %s: %s", name, CLI_NOPARSE_HDR, dup);
  }
  return okay;
}

static gboolean cli_opt_X_f(char *dup, int type, int ndx) {
  gboolean okay = false;
  t_ent_spn_aux *aux = ent_spn[type].list[ndx].aux;
  if (dup && aux && aux->mm && aux->pval) {
    okay = parser_mmint(dup, aux->prfx, *aux->mm, aux->pval);
    if (okay) g_message("%s: %d°", aux->prfx, *aux->pval);
    else g_warning("%s: %s [%d,%d]",
      ent_spn[type].c.en.name, CLI_NOMATCH_HDR, aux->mm->min, aux->mm->max);
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
      default: g_warning("%s: %s: '%c'", name, CLI_BADTAG_HDR, tag);
    }
    g_free(dup);
    if (xpairbits & 1U) {
      g_warning("%s: %s: '%c'", name, CLI_DUPTAG_HDR, tag);
      xpairbits &= ~1U;
    }
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
        if (pair[0][1]) g_warning("%s: %s: %s", name, CLI_BADTAG_HDR, pair[0]);
        else okay = cli_opt_X_pair(pair[1], pair[0][0], name, opts);
      } else g_warning("%s: %s: %s", name, CLI_NOPAIR_HDR, *tagval);
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
    if (pbool) {
      *pbool = true;
      g_message("%s: %s: %s", en->prefix, en->en.name, TOGGLE_ON_HDR);
    }
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
  int num = desc->mm.max - desc->mm.min + 1;
  size_t len = (num > 0) ? num : 0;
  type2ndx_fn type2ndx = desc->t2n;
  if (type2ndx && order && (len > 0) && (strnlen(order, len + 1) == len)) {
    t_type_elem *elems = &desc->elems[desc->mm.min];
    t_type_elem newelems[len];
    memmove(newelems, elems, sizeof(newelems)); // BUFFNOLINT
    int n = 0;
    for (const char *p = order; *p; p++, n++) {
      int ndx = char2ndx(desc->cat, true, *p);
      int type = char2ndx(desc->cat, false, *p);
      if ((type < 0) || (ndx < 0))
        WARN("%s: ent=%d elem=%d", CLI_INVAL_HDR, ndx, type);
      else {
        ndx = type2ndx(type);
        if (ndx < 0) WARN("%s: %d", CLI_BADTYPE_HDR, type);
        else newelems[n] = desc->elems[ndx];
      }
    }
    memmove(elems, newelems, sizeof(newelems)); // BUFFNOLINT
  } else WARN("%s", CLI_NDXDIFF_HDR);
  g_free(order);
}

static char* cli_char_opts(int type, const char *value, unsigned cat, t_elem_desc *desc, int max, const char *hdr) {
  if (!desc || !desc->elems) return NULL;
  CLI_REORDER_PRINT_ELEMS(desc->elems, max);
  clean_elems(type);
  char *str = cli_opt_charelem(parser_str(value, hdr, cat), desc->patt, desc->cat);
  if (str) {
    if (str[0]) reorder_elems(str, desc);
    else g_message("%s: %s", hdr, TOGGLE_OFF_HDR);
  }
  CLI_REORDER_PRINT_ELEMS(desc->elems, max);
  return str;
}

static gboolean cli_opt_elem(const char *name, const char *value, GError **error, unsigned cat) {
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
      str = parser_str(value, CLI_SUMM_DESC, OPT_TYPE_RECAP);
      if (str) {
        opts.recap = str[0];
        const char *type = NULL;
        switch (opts.recap) {
          case 't': type = CLI_ROPT_T_HDR;  break;
          case 'c': type = CLI_ROPT_C_HDR;  break;
#ifdef WITH_JSON
          case 'j':
          case 'J': type = CLI_ROPT_J_HDR;  break;
#endif
          default:  type = CLI_BADTYPE_HDR; break;
        }
        g_message("%s: %s", CLI_SUMM_DESC, type);
      }
    } break;
    default: break;
  }
  if (str) { g_free(str); return true; }
  CLI_SET_ERR; return false;
}

static gboolean cli_opt_p(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_PAD);
}

static gboolean cli_opt_I(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_INFO);
}

static gboolean cli_opt_S(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_STAT);
}

static gboolean cli_opt_L(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_GRLG);
}

static gboolean cli_opt_G(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_GREX);
}

#ifdef WITH_PLOT
static gboolean cli_opt_P(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
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

  gboolean okay = cli_int_opt(name, value, error, CLI_TOPT_HDR, 0, max, &mask);
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

static gboolean cli_opt_r(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  return cli_opt_elem(name, value, error, OPT_TYPE_RECAP);
}

static gboolean cli_opt_A(const char *name, const char *value, t_opts *opts G_GNUC_UNUSED, GError **error) {
  int aopt = activetab + 1;
  gboolean valid = cli_int_opt(name, value, error, CLI_AOPT_DESC, ATAB_MIN + 1, ATAB_MAX + 1, &aopt);
  if (valid) activetab = aopt - 1;
  return valid;
}

static gboolean config_opt_not_found(GError *opterr, const char *section, const char *name,
   const char *value, GError **error) {
  if (g_error_matches(opterr, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
      g_error_matches(opterr, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) return true;
  g_warning("%s:%s: %s", section, name, opterr ? opterr->message : UNKN_ERR);
  CLI_SET_ERR; return false;
}

static void cli_set_opt_dns(gboolean dns, t_opts *opts) {
  if (opts && (opts->dns != dns)) {
    opts->dns = dns;
    g_message("%s %s", OPT_DNS_HDR, onoff(dns));
  }
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
    g_warning("%s: %s", ferr ? ferr->message : UNKN_ERR, value);
    CLI_SET_ERR;
    return false;
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
        CONF_DEBUG("%s %s:%s=%s", CLI_TOVAL_HDR, section, optname, optval);
      }
      switch (option->type) {
        case CNF_OPT_TABS: {
          int val = g_key_file_get_integer(file, section, optname, &opterr);
          if (!opterr) {
            if (val == 1) {
              if (opts) { opts->graph = 0; TABON(CLI_1D_DESC); }
#ifdef WITH_PLOT
            } else if (val == 2) {
              if (opts) TABON(CLI_2D_DESC);
#endif
            } else {
              g_warning("%s:%s: %s: %d", section, optname, CLI_INVAL_HDR, val);
              CLI_SET_ERR;
              return false;
            }
          }
        } break;
        case CNF_OPT_IPV: {
          int val = g_key_file_get_integer(file, section, optname, &opterr);
          if (!opterr) {
            if ((val == 4) || (val == 6)) {
              if (opts->ipv != val) {
                opts->ipv = val;
                g_message("%s: %s", (val == 4) ? OPT_IPV4_HDR : OPT_IPV6_HDR, TOGGLE_ON_HDR);
              }
            } else {
              g_warning("%s:%s: %s: %d", section, optname, CLI_INVAL_HDR, val);
              CLI_SET_ERR;
              return false;
            }
          }
        } break;
        case CNF_OPT_NODNS: {
          gboolean numerical = g_key_file_get_boolean(file, section, optname, &opterr);
          if (!opterr) cli_set_opt_dns(!numerical, opts);
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
#define RECAP_TYPE_MESG CLI_SUMM_TCJ
#else
#define RECAP_TYPE_MESG CLI_SUMM_TC
#endif

#define CLI_OPT_NONE(ch, name, data) {                              \
  .short_name = (ch), .long_name = (name), .description = desc[ch], \
  .arg = G_OPTION_ARG_NONE, .arg_data = (data) }
#define CLI_OPT_SMTH(ch, name, type, data, arg_desc) {              \
  .short_name = (ch), .long_name = (name), .description = desc[ch], \
  .arg = (type), .arg_data = (data), .arg_description = (arg_desc) }
#define CLI_OPT_CALL(ch, name, data, arg_desc) \
  CLI_OPT_SMTH(ch, name, G_OPTION_ARG_CALLBACK, data, arg_desc)
#define CLI_OPT_INTC(ch, name, data, arg_desc) \
  CLI_OPT_SMTH(ch, name, G_OPTION_ARG_INT,      data, arg_desc)

gboolean autostart;

static inline gboolean cli_tab_opt(const gboolean off[2], t_opts *opts) {
  gboolean okay = (off[0] + off[1]) < 2;
  if (okay) {
    if (opts && off[0]) { opts->graph = 0; TABON(CLI_1D_DESC); }
#ifdef WITH_PLOT
    if (opts && off[1]) TABON(CLI_2D_DESC);
#endif
  } else g_message("%s: -%c -%c", CLI_MUT_EXC_HDR, '1', '2');
  return okay;
}

static inline void print_libver(const char *pre, char *ver) {
  if (ver) { g_print(" %s-%s", pre, ver); g_free(ver); }}

static int cli_init_proc(int *pargc, char ***pargv,
  char* desc[CHAR_MAX], const char *target_hdr) {
#define CLI_FAIL(fmt, ...) {      \
  g_message(fmt, __VA_ARGS__);    \
  g_option_context_free(context); \
  return EXIT_FAILURE;            \
}
  char** target = NULL;
  int num = -1;
  gboolean onoff[ONOFF_MAX + 1] = {0};
  const GOptionEntry options[] = {
    CLI_OPT_CALL('f', "file",         cli_opt_f, CLI_FOPT_HINT),
    CLI_OPT_NONE('n', CNF_STR_NODNS,  &num),
    CLI_OPT_CALL('c', CNF_STR_CYCLES, cli_opt_c, CLI_COPT_HINT),
    CLI_OPT_CALL('i', CNF_STR_IVAL,   cli_opt_i, CLI_IOPT_HINT),
    CLI_OPT_CALL('t', CNF_STR_TTL,    cli_opt_t, CLI_TTL_HINT),
    CLI_OPT_CALL('q', CNF_STR_QOS,    cli_opt_q, CLI_BITS_HINT),
    CLI_OPT_CALL('s', CNF_STR_SIZE,   cli_opt_s, CLI_SIZE_HINT),
    CLI_OPT_CALL('p', CNF_STR_PLOAD,  cli_opt_p, CLI_POPT_HINT),
    CLI_OPT_CALL('I', CNF_STR_INFO,   cli_opt_I, "[" INFO_PATT "]"),
    CLI_OPT_CALL('S', CNF_STR_STAT,   cli_opt_S, "[" STAT_PATT "]"),
    CLI_OPT_CALL('T', CNF_STR_THEME,  cli_opt_T, CLI_BITS_HINT),
    CLI_OPT_CALL('g', CNF_STR_GRAPH,  cli_opt_g, "[123]"),
    CLI_OPT_CALL('G', CNF_STR_EXTRA,  cli_opt_G, "[" GREX_PATT "]"),
    CLI_OPT_CALL('L', CNF_STR_LGND,   cli_opt_L, "[" GRLG_PATT "]"),
#ifdef WITH_PLOT
    CLI_OPT_CALL('P', CNF_STR_PLEL,   cli_opt_P, "[" PLEL_PATT "]"),
    CLI_OPT_CALL('X', CNF_STR_PLEX,   cli_opt_X, CLI_XOPT_HINT),
#endif
    CLI_OPT_CALL('r', CNF_STR_RECAP,  cli_opt_r, "[" RECAP_PATT "]"),
    CLI_OPT_NONE('R', "run",          &autostart),
    CLI_OPT_CALL('A', CNF_STR_ATAB,   cli_opt_A, "[12"
#ifdef WITH_PLOT
      "3"
#endif
    "]"),
    CLI_OPT_INTC('v', "verbose",      &verbose, CLI_VOPT_HINT),
    CLI_OPT_NONE('V', "version",      &onoff[ONOFF_VERSION]),
    CLI_OPT_NONE('1', "no-graphics",  &onoff[ONOFF_1D]),
#ifdef WITH_PLOT
    CLI_OPT_NONE('2', "no-3d",        &onoff[ONOFF_2D]),
#endif
    CLI_OPT_NONE('4', "ipv4",         &onoff[ONOFF_IPV4]),
    CLI_OPT_NONE('6', "ipv6",         &onoff[ONOFF_IPV6]),
    { .long_name = G_OPTION_REMAINING, .arg = G_OPTION_ARG_STRING_ARRAY,
      .arg_data = (void*)&target, .arg_description = target_hdr },
    {} // trailing 0
  };
  // options
  GOptionGroup *group = g_option_group_new(APPNAME, APPNAME, CLI_OPTS_HDR, &opts, NULL);
  if (!group) return EXIT_FAILURE;
  g_option_group_add_entries(group, options);
  GOptionContext *context = g_option_context_new(NULL);
  if (!context) return EXIT_FAILURE;
  g_option_context_set_main_group(context, group);
  { GError *error = NULL;
    cli = true;
    gboolean okay = g_option_context_parse(context, pargc, pargv, &error);
    cli = false;
    if (!okay) {
      g_warning("%s", error ? error->message : UNKN_ERR);
      g_error_free(error);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }
    if (onoff[ONOFF_VERSION]) {
      g_print("%s\n", APPVER);
      g_print("%s: %cDND %cJSON %cPLOT %cNLS\n", CLI_APPFEAT_HDR,
#ifdef WITH_DND
      '+',
#else
      '-',
#endif
#ifdef WITH_JSON
      '+',
#else
      '-',
#endif
#ifdef WITH_PLOT
      '+',
#else
      '-',
#endif
#ifdef WITH_NLS
      '+'
#else
      '-'
#endif
      );
      g_print("%s:", CLI_LIBVER_HDR);
      print_libver("gtk",   rtver(GTK_STRV));
      print_libver("glib",  rtver(GLIB_STRV));
      print_libver("cairo", rtver(CAIRO_STRV));
      print_libver("pango", rtver(PANGO_STRV));
      g_print("\n");
      g_option_context_free(context);
      return -1;
    }
  }
  if (!cli_tab_opt(&onoff[ONOFF_1D], &opts))
    return EXIT_FAILURE;
  if (onoff[ONOFF_IPV4] && onoff[ONOFF_IPV6])
    CLI_FAIL("%s: -%c -%c", CLI_MUT_EXC_HDR, '4', '6');
  if (onoff[ONOFF_IPV4] && (opts.ipv != 4)) {
    opts.ipv = 4;
    g_message("%s: %s", OPT_IPV4_HDR, TOGGLE_ON_HDR);
  }
  if (onoff[ONOFF_IPV6] && (opts.ipv != 6)) {
    opts.ipv = 6;
    g_message("%s: %s", OPT_IPV6_HDR, TOGGLE_ON_HDR);
  }
  if (num >= 0)
    cli_set_opt_dns(!num, &opts);
  // arguments
  if (target) {
    for (char **ptr = target; *ptr; ptr++) {
      if (!ptr || !ptr[0])
        continue;
      const char *arg = *ptr;
      if (arg[0] == '-')
        CLI_FAIL("%s: %s", CLI_BADOPT_HDR, arg);
      if (opts.target)
        g_message("%s: %s: %s", TARGET_HDR, CLI_SKIPARG_HDR, arg);
      else {
        cli = true; char *pinghost = parser_valid_target(arg); cli = false;
        if (pinghost)
          g_message("%s: %s", TARGET_HDR, pinghost);
        else {
          g_message("%s: %s: %s", TARGET_HDR, CLI_INVAL_HDR, arg);
          g_strfreev(target); g_option_context_free(context); return EXIT_FAILURE;
        }
        g_free(opts.target); opts.target = pinghost;
      }
    }
    g_strfreev(target);
  }
  if (opts.recap) autostart = true;
  if (autostart && !opts.target) CLI_FAIL("%s", CLI_NOGOAL_HDR);
  if (opts.recap) g_message("%s", CLI_RECAP_HDR);
  else if (autostart) g_message("%s", CLI_ROPT_DESC);
  g_option_context_free(context);
  return EXIT_SUCCESS;
#undef CLI_FAIL
}

int cli_init(int *pargc, char ***pargv) {
  if (!pargc || !pargv) return EXIT_FAILURE;
  char* desc[CHAR_MAX] = {
    ['f'] = g_strdup(CLI_FOPT_DESC),
    ['n'] = g_strdup(CLI_NOPT_DESC),
    ['c'] = g_strdup(OPT_CYCLES_HDR),
    ['i'] = g_strdup(CLI_IOPT_DESC),
    ['t'] = g_strdup(CLI_TTL_DESC),
    ['q'] = g_strdup(CLI_QOS_DESC),
    ['s'] = g_strdup(CLI_SIZE_DESC),
    ['p'] = g_strdup(CLI_POPT_DESC),
    ['I'] = g_strdup(OPT_INFO_HDR),
    ['S'] = g_strdup(CLI_STAT_DESC),
    ['T'] = g_strdup(CLI_TOPT_DESC),
    ['g'] = g_strdup(OPT_GRAPH_HDR),
    ['G'] = g_strdup(OPT_GREX_HDR),
    ['L'] = g_strdup(OPT_GRLG_HDR),
#ifdef WITH_PLOT
    ['P'] = g_strdup(OPT_PLOT_HDR),
    ['X'] = g_strdup(OPT_PLEX_HDR),
#endif
    ['r'] = g_strdup_printf("%s (%s)", CLI_SUMM_DESC, RECAP_TYPE_MESG),
    ['R'] = g_strdup(CLI_ROPT_DESC),
    ['A'] = g_strdup(CLI_AOPT_DESC),
    ['v'] = g_strdup(CLI_VERB_DESC),
    ['V'] = g_strdup(CLI_VERS_DESC),
    ['1'] = g_strdup(CLI_1D_DESC),
#ifdef WITH_PLOT
    ['2'] = g_strdup(CLI_2D_DESC),
#endif
    ['4'] = g_strdup(OPT_IPV4_HDR),
    ['6'] = g_strdup(OPT_IPV6_HDR),
  };
  char *target_hdr = g_utf8_strup(TARGET_HDR, -1);
  int rc = cli_init_proc(pargc, pargv, desc, target_hdr);
  for (unsigned i = 0; i < G_N_ELEMENTS(desc); i++)
    g_free(desc[i]);
  g_free(target_hdr);
  return rc;
}

