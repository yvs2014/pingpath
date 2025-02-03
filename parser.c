
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#include "common.h"
#include "parser.h"
#include "stat.h"
#include "ui/notifier.h"

#define REN_NAME "name"
#define REN_ADDR "addr"
#define REN_IP   "ip"
#define REN_SEQ  "seq"
#define REN_TTL  "ttl"
#define REN_TIME "time"
#define REN_TS_S "tss"
#define REN_TS_U "tsu"
#define REN_REASON "reason"
#define REN_INFO "info"

#define PATT_TS   "^\\[(?<" REN_TS_S ">\\d+)(.(?<" REN_TS_U ">\\d+))*\\]"
#define PATT_FROM "[fF]rom (((?<" REN_NAME ">.*) \\((?<" REN_ADDR ">.*)\\))|(?<" REN_IP ">[^:]*))"
#define PATT_SEQ  "icmp_seq=(?<" REN_SEQ ">\\d+)"

#define DIGIT_OR_LETTER "\\d\\pLu\\pLl"

#define WHOIS_RT_TAG   "route"
#define WHOIS_AS_TAG   "origin"
#define WHOIS_DESC_TAG "descr"

#define WHOIS_NL       "\n"
enum { WHOIS_COMMENT = '%', WHOIS_DELIM = ':', WHOIS_CCDEL = ',' };

#define PARSER_MESG(fmt, ...) { if (cli) g_message(fmt, __VA_ARGS__); else notifier_inform(fmt, __VA_ARGS__); }

typedef gboolean parser_cb(int at, GMatchInfo* match, const char *line);

typedef struct parser_regex {
  char *pattern;
  GRegex *regex;
} t_parser_regex;

typedef struct response_regex {
  t_parser_regex rx;
  parser_cb *cb;
} t_response_regex;

static gboolean parse_success_match(int at, GMatchInfo* match, const char *line);
static gboolean parse_discard_match(int at, GMatchInfo* match, const char *line);
static gboolean parse_timeout_match(int at, GMatchInfo* match, const char *line);
static gboolean parse_info_match(int at, GMatchInfo* match, const char *line); // like 'success' without 'time'

static const GRegex *multiline_regex;
static const GRegex *hostname_char0_regex;
static const GRegex *hostname_chars_regex;

static t_response_regex regexes[] = {
  { .cb = parse_success_match, .rx = { .pattern =
    PATT_TS " \\d+ bytes " PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) time=(?<" REN_TIME ">\\d+.?\\d+)" }},
  { .cb = parse_discard_match, .rx = { .pattern =
    PATT_TS " " PATT_FROM ":? " PATT_SEQ " (?<" REN_REASON ">.*)" }},
  { .cb = parse_timeout_match, .rx = { .pattern =
    PATT_TS " [nN]o answer yet(: | for )+" PATT_SEQ }},
  { .cb = parse_info_match,    .rx = { .pattern =
    PATT_TS " \\d+ bytes " PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) (time=.* ms )(?<" REN_INFO ">.*)" }},
};

#define NODUPCHARS "(?!.*(.).*\\1)"

static t_parser_regex str_rx[] = {
  [OPT_TYPE_INT]   = { .pattern = "^[\\-\\+]*(\\d+|0x[\\da-fA-F]+|0[0-7]+)$" },
  [OPT_TYPE_PAD]   = { .pattern = "^[\\da-fA-F]{1,32}$" },
  [OPT_TYPE_INFO]  = { .pattern = "^" NODUPCHARS "[" INFO_PATT "]*$" },
  [OPT_TYPE_STAT]  = { .pattern = "^" NODUPCHARS "[" STAT_PATT "]*$" },
  [OPT_TYPE_GRLG]  = { .pattern = "^" NODUPCHARS "[" GRLG_PATT "]*$" },
  [OPT_TYPE_GREX]  = { .pattern = "^" NODUPCHARS "[" GREX_PATT "]*$" },
#ifdef WITH_PLOT
  [OPT_TYPE_PLEL]  = { .pattern = "^" NODUPCHARS "[" PLEL_PATT "]*$" },
#endif
  [OPT_TYPE_RECAP] = { .pattern = "^[" RECAP_PATT "]$" },
};

static GRegex* compile_regex(const char *pattern, GRegexCompileFlags flags) {
  GError *err = NULL;
  GRegex *regex = g_regex_new(pattern, flags, 0, &err);
  if (err) {
    WARN("%s: %s: %s", REGEX_HDR, PATT_HDR,  pattern);
    WARN("%s: %s: %s", REGEX_HDR, ERROR_HDR, err->message);
    g_error_free(err);
  }
  return regex;
}

static char* fetch_named_str(GMatchInfo* match, char *prop) {
  char *str = g_match_info_fetch_named(match, prop);
  char *val = (str && str[0]) ? g_strdup(str) : NULL;
  g_free(str); return val;
}

static int fetch_named_int(GMatchInfo* match, char *prop) {
  char *str = g_match_info_fetch_named(match, prop);
  int val = (str && str[0]) ? atoi(str) : -1;
  g_free(str); return val;
}

static long long fetch_named_ll(GMatchInfo* match, char *prop) {
  char *str = g_match_info_fetch_named(match, prop);
  long long val = (str && str[0]) ? atoll(str) : -1;
  g_free(str); return val;
}

static int fetch_named_rtt(GMatchInfo* match, char *prop) {
  char *str = g_match_info_fetch_named(match, prop);
  double val = -1;
  if (str && str[0]) {
    val = g_ascii_strtod(str, NULL);
    if (ERANGE == errno) val = -1;
    if (val > 0) val *= 1000; // msec to usec
  }
  g_free(str); return (val < 0) ? -1 : floor(val);
}

static gboolean valid_mark(GMatchInfo* match, t_tseq *mark) {
  mark->seq = fetch_named_int(match, REN_SEQ); if (mark->seq < 0) return false;
  mark->sec = fetch_named_ll(match, REN_TS_S); if (mark->sec < 0) return false;
  mark->usec = fetch_named_int(match, REN_TS_U); if (mark->usec < 0) mark->usec = 0;
  return true;
}

static gboolean valid_markhost(GMatchInfo* match, t_tseq* mark, t_host* host, const char *line) {
  if (!valid_mark(match, mark)) { DEBUG("%s: %s", BADTAG_HDR, line); return false; }
  host->addr = fetch_named_str(match, REN_ADDR);
  if (!host->addr) {
    host->addr = fetch_named_str(match, REN_IP);
    if (!host->addr) { DEBUG("%s: %s: %s", ADDR_HDR, INVAL_HDR, line); return false; }
  }
  host->name = fetch_named_str(match, REN_NAME);
  return true;
}

static gboolean parse_success_match(int at, GMatchInfo* match, const char *line) {
  t_ping_success res = {0};
  //
  res.ttl = fetch_named_int(match, REN_TTL);
  if (res.ttl < 0) {
    DEBUG("%s: %s: %s", TTL_HDR, INVAL_HDR, line);
    return false;
  }
  //
  res.time = fetch_named_rtt(match, REN_TIME);
  if (res.time < 0) {
    DEBUG("%s: %s: %s", DELAY_HDR, INVAL_HDR, line);
    return false;
  }
  // not to free() after other mandatory fields unless it's failed
  if (!valid_markhost(match, &res.mark, &res.host, line))
    return false;
  //
  DEBUG("%s[%d] %s=%d %s=%s %s=%s %s=%d %s=%d%s", PARSE_SUCCESS, at,
    SEQ_HDR, res.mark.seq, ADDR_HDR, res.host.addr,
    NAME_HDR, mnemo(res.host.name), TTL_HDR, res.ttl,
    DELAY_HDR, res.time, UNIT_USEC);
  stat_save_success(at, &res);
  return true;
}

static gboolean parse_discard_match(int at, GMatchInfo* match, const char *line) {
  t_ping_discard res = {0};
  if (!valid_markhost(match, &res.mark, &res.host, line))
    return false;
  res.reason = fetch_named_str(match, REN_REASON);
  DEBUG("%s[%d] %s=%d %s=%s %s=%s %s=\"%s\"", PARSE_DISCARD, at,
    SEQ_HDR, res.mark.seq, ADDR_HDR, res.host.addr,
    NAME_HDR, mnemo(res.host.name), REASON_HDR, res.reason);
  stat_save_discard(at, &res);
  return true;
}

static gboolean parse_timeout_match(int at, GMatchInfo* match, const char *line) {
  t_ping_timeout res = {0};
  if (!valid_mark(match, &res.mark)) {
    DEBUG("%s: %s", BADTAG_HDR, line);
    return false;
  }
  DEBUG("%s[%d] %s=%d %s=%lld.%06d", PARSE_TIMEOUT, at,
    SEQ_HDR, res.mark.seq, TIMESTAMP_HDR, res.mark.sec, res.mark.usec);
  stat_save_timeout(at, &res);
  return true;
}

static gboolean parse_info_match(int at, GMatchInfo* match, const char *line) {
  t_ping_info res = {0};
  //
  res.ttl = fetch_named_int(match, REN_TTL);
  if (res.ttl < 0) {
    DEBUG("%s: %s: %s", TTL_HDR, INVAL_HDR, line);
    return false;
  }
  // not to free() after other mandatory fields unless it's failed
  if (!valid_markhost(match, &res.mark, &res.host, line))
    return false;
  res.info = fetch_named_str(match, REN_INFO);
  //
  DEBUG("%s[%d] %s=%d %s=%s %s=%s %s=%d %s=\"%s\"", PARSE_INFO, at,
    SEQ_HDR, res.mark.seq, ADDR_HDR, res.host.addr,
    NAME_HDR, mnemo(res.host.name), TTL_HDR, res.ttl, INFOSTAMP_HDR, res.info);
  stat_save_info(at, &res);
  return true;
}

static gboolean parse_match_wrap(int at, GRegex *re, const char *line, parser_cb parser) {
  if (parser) {
    GMatchInfo *match = NULL;
    gboolean gowith = g_regex_match(re, line, 0, &match);
    if (match) {
      gboolean valid = gowith ? parser(at, match, line) : false;
      g_match_info_free(match);
      return valid;
    }
  }
  return false;
}

static void analyze_line(int at, const char *line) {
  int n = G_N_ELEMENTS(regexes);
  for (int i = 0; i < n; i++)
    if (parse_match_wrap(at, regexes[i].rx.regex, line, regexes[i].cb))
      return;
  DEBUG("%s[%d] %s", PARSE_UNKN, at, line);
}

#define GREEDY false
#define LAZY   true

static char* split_pair(char **pstr, int ch, gboolean lazy) {
  if (!pstr || !*pstr) return NULL;
  char *val = lazy ? strchr(*pstr, ch) : strrchr(*pstr, ch);
  if (!val) return NULL;
  *val++ = 0;
  *pstr = g_strstrip(*pstr);
  return g_strstrip(val);
}

static inline gboolean parser_valid_char0(char *str) { return g_regex_match(hostname_char0_regex, str, 0, NULL); }
static inline gboolean parser_valid_host(char *host) { return g_regex_match(hostname_chars_regex, host, 0, NULL); }

static gboolean target_meet_all_conditions(char *str, int len, int max) {
#define HOSTNAME_ERROR(fmt, ...) do {                 \
  PARSER_MESG("%s: " fmt, HOSTNAME_HDR, __VA_ARGS__); \
  return false;                                       \
} while (0)
  // rfc1123,rfc952 restrictions
  if (len > max)                HOSTNAME_ERROR("%s (%d)", OVERLEN_HDR, max);
  if (str[len - 1] == '-')      HOSTNAME_ERROR("%s", ENDHYPEN_ERR);
  if (!parser_valid_char0(str)) HOSTNAME_ERROR("%s", STARTCHAR_ERR);
  if (!parser_valid_host(str))  HOSTNAME_ERROR("%s", INVALCHAR_ERR);
  return true;
#undef HOSTNAME_ERROR
}

static char* parser_valid_int(const char *option, const char *str) {
  char *cpy = g_strdup(str);
  if (!cpy) return NULL;
  char *val = g_strstrip(cpy);
  gboolean valid = g_regex_match(str_rx[OPT_TYPE_INT].regex, val, 0, NULL);
  if (valid) val = (val && val[0]) ? g_strdup(val) : NULL;
  else {
    PARSER_MESG("%s: %s: %s", option, RENOMATCH_ERR, str_rx[OPT_TYPE_INT].pattern);
    val = NULL;
  }
  g_free(cpy);
  return val;
}


// pub
//
gboolean parser_init(void) {
  multiline_regex      = compile_regex("\\n", G_REGEX_MULTILINE);
  hostname_char0_regex = compile_regex("^[" DIGIT_OR_LETTER ":]", 0);
  hostname_chars_regex = compile_regex("^[" DIGIT_OR_LETTER ":.-]+$", 0);
  gboolean okay = multiline_regex && hostname_char0_regex && hostname_chars_regex;
  for (unsigned i = 0; i < G_N_ELEMENTS(regexes); i++) {
    regexes[i].rx.regex = compile_regex(regexes[i].rx.pattern, 0);
    if (!regexes[i].rx.regex) okay = false;
  }
  for (unsigned i = 0; i < G_N_ELEMENTS(str_rx); i++) {
    str_rx[i].regex = compile_regex(str_rx[i].pattern, 0);
    if (!str_rx[i].regex) okay = false;
  }
  return okay;
}

void parser_parse(int at, char *input) {
  if (input) {
    char **lines = g_regex_split(multiline_regex, input, 0);
    if (lines) {
      for (char **pstr = lines; *pstr; pstr++) if ((*pstr)[0]) analyze_line(at, *pstr);
      g_strfreev(lines);
    }
  }
}

gboolean parser_mmint(const char *str, const char *option, t_minmax minmax, int *value) {
  gboolean okay = false;
  if (str) {
    char *val = parser_valid_int(option, str);
    if (val) {
      errno = 0; int len = strtol(val, NULL, 0);
      okay = !errno && MM_OKAY(minmax, len);
      errno = 0; g_free(val);
      if (okay) { if (value) *value = len; }
      else PARSER_MESG("%s: %s: %d <=> %d", option, OUTRANGE_ERR, minmax.min, minmax.max)
  }}
  return okay;
}

char* parser_str(const char *str, const char *option, unsigned cat) {
  const int PARSE_MAX_CHARS = 128;
  if (cat < G_N_ELEMENTS(str_rx)) {
    char *buff = g_strndup(str, PARSE_MAX_CHARS);
    if (buff) {
      char *val = g_strdup(g_strstrip(buff)); g_free(buff);
      if (val) {
        gboolean valid = g_regex_match(str_rx[cat].regex, val, 0, NULL);
        if (valid) return val;
      } else WARN("g_strdup()");
      PARSER_MESG("%s: %s: %s", option, RENOMATCH_ERR, str_rx[cat].pattern);
    } else WARN("g_strndup()");
  } else WARN("%s: %d", WRONGCAT_ERR, cat);
  return NULL;
}

char* parser_valid_target(const char *target) {
  if (!target || !target[0]) return NULL;
  char *dup = g_strdup(target);
  char *hostname = NULL, *str = g_strstrip(dup);
  int len = str ? g_utf8_strlen(str, MAXHOSTNAME + 1) : 0;
  if (len && target_meet_all_conditions(str, len, MAXHOSTNAME)) hostname = g_strdup(str);
  g_free(dup);
  return hostname;
}


void parser_whois(char *buff, t_whois *welem) {
#define DUP_WITH_MARK(dst, val) do {          \
  if (dst) {                                  \
    CLR_STR(dst);                             \
    (dst) = g_strdup_printf("%s*", val);      \
  } else (dst) = g_strndup(val, MAXHOSTNAME); \
} while (0)
#define SKIP_PREFIX "AS" /* included by server */
  // if there are multiple sources (despite -m query), take the last tag and mark it with '*'
  int skip_len = sizeof(SKIP_PREFIX) - 1;
  if (!buff || !welem) return;
  memset(welem, 0, sizeof(*welem)); // BUFFNOLINT
  char *str = NULL;
  while ((str = strsep(&buff, WHOIS_NL))) {
    str = g_strstrip(str);
    if (str && str[0] && (str[0] != WHOIS_COMMENT)) {
      char *val = split_pair(&str, WHOIS_DELIM, LAZY);
      if (!val) continue;
      int ndx = -1;
      if (STR_EQ(str, WHOIS_RT_TAG)) ndx = WHOIS_RT_NDX;
      else if (STR_EQ(str, WHOIS_AS_TAG)) {
        ndx = WHOIS_AS_NDX;
        int len = strnlen(val, MAXHOSTNAME);
        if ((len > skip_len) && !g_ascii_strncasecmp(val, SKIP_PREFIX, skip_len))
          val += skip_len;
      } else if (STR_EQ(str, WHOIS_DESC_TAG)) {
        ndx = WHOIS_DESC_NDX;
        char *cc = split_pair(&val, WHOIS_CCDEL, GREEDY);
        if (cc) DUP_WITH_MARK(welem->elem[WHOIS_CC_NDX], cc);
      }
      if (ndx >= 0) DUP_WITH_MARK(welem->elem[ndx], val);
    }
  }
  for (int i = 0; i < WHOIS_NDX_MAX; i++)
    if (!welem->elem[i])
      welem->elem[i] = g_strdup(UNKN_FIELD);
#undef DUP_WITH_MARK
#undef SKIP_PREFIX
}

static gboolean parser_valint(const char* inp, int* outp, const char *option) {
  gboolean okay = true;
  if (inp && inp[0]) {
    char *valid = parser_valid_int(option, inp);
    if (valid) {
      errno = 0; *outp = strtol(valid, NULL, 0);
      okay &= !errno; errno = 0;
      g_free(valid);
    } else okay = false;
  }
  return okay;
}

gboolean parser_range(char *range, char delim, const char *option, t_minmax *minmax) {
  if (!range || !minmax) return false;
  char *min = range;
  char *max = split_pair(&min, delim, LAZY);
  t_minmax val = {INT_MIN, INT_MIN};
  gboolean okay = true;
  okay &= parser_valint(min, &val.min, option);
  okay &= parser_valint(max, &val.max, option);
  if (okay) *minmax = val;
  return okay;
}

#ifdef WITH_PLOT
gboolean parser_ivec(char *range, char delim, const char *option, int *dest, unsigned max) {
  if (!range || (max <= 0)) return false;
  const char delims[] = { delim, 0 };
  char **abcs = g_strsplit(range, delims, max);
  int val[max]; for (unsigned i = 0; i < max; i++) val[i] = INT_MIN;
  gboolean okay = true;
  if (abcs) for (unsigned i = 0; abcs[i] && (i < G_N_ELEMENTS(val)); i++)
    okay &= parser_valint(abcs[i], &val[i], option);
  g_strfreev(abcs);
  if (okay) for (unsigned i = 0; i < max; i++) dest[i] = val[i];
  return okay;
}
#endif

