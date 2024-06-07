
#include "parser.h"
#include "stat.h"
#include "ui/notifier.h"

#define MAX_LINES 10

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
#define REN_SKIP "skip"

#define PATT_TS   "^\\[(?<" REN_TS_S ">\\d+)(.(?<" REN_TS_U ">\\d+))*\\]"
#define PATT_FROM "rom (((?<" REN_NAME ">.*) \\((?<" REN_ADDR ">.*)\\))|(?<" REN_IP ">.*))"
#define PATT_SEQ  "icmp_seq=(?<" REN_SEQ ">\\d+)"

#define DIGIT_OR_LETTER "\\d\\pLu\\pLl"

#define WHOIS_RT_TAG   "route"
#define WHOIS_AS_TAG   "origin"
#define WHOIS_DESC_TAG "descr"

#define WHOIS_NL       "\n"
#define WHOIS_COMMENT  '%'
#define WHOIS_DELIM    ':'
#define WHOIS_CCDEL    ','

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
    PATT_TS " \\d+ bytes f" PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) time=(?<" REN_TIME ">\\d+.?\\d+)" }},
  { .cb = parse_discard_match, .rx = { .pattern =
    PATT_TS " F" PATT_FROM " " PATT_SEQ " (?<" REN_REASON ">.*)" }},
  { .cb = parse_timeout_match, .rx = { .pattern =
    PATT_TS " no answer yet for " PATT_SEQ }},
  { .cb = parse_info_match,    .rx = { .pattern =
    PATT_TS " \\d+ bytes f" PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) (?<" REN_SKIP ">time=.* ms )(?<" REN_INFO ">.*)" }},
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
    WARN("regex PATTERN: %s", pattern);
    WARN("regex ERROR: %s", err->message);
    g_error_free(err);
  }
  return regex;
}

static char* fetch_named_str(GMatchInfo* match, char *prop) {
  char *s = g_match_info_fetch_named(match, prop);
  char *val = (s && s[0]) ? g_strdup(s) : NULL;
  g_free(s); return val;
}

static int fetch_named_int(GMatchInfo* match, char *prop) {
  char *s = g_match_info_fetch_named(match, prop);
  int val = (s && s[0]) ? atoi(s) : -1;
  g_free(s); return val;
}

static long long fetch_named_ll(GMatchInfo* match, char *prop) {
  char *s = g_match_info_fetch_named(match, prop);
  long long val = (s && s[0]) ? atoll(s) : -1;
  g_free(s); return val;
}

static int fetch_named_rtt(GMatchInfo* match, char *prop) {
  char *s = g_match_info_fetch_named(match, prop);
  double val = -1;
  if (s && s[0]) {
    val = g_ascii_strtod(s, NULL);
    if (ERANGE == errno) val = -1;
    if (val > 0) val *= 1000; // msec to usec
  }
  g_free(s); return (val < 0) ? -1 : floor(val);
}

static gboolean valid_mark(GMatchInfo* match, t_tseq *mark) {
  mark->seq = fetch_named_int(match, REN_SEQ); if (mark->seq < 0) return false;
  mark->sec = fetch_named_ll(match, REN_TS_S); if (mark->sec < 0) return false;
  mark->usec = fetch_named_int(match, REN_TS_U); if (mark->usec < 0) mark->usec = 0;
  return true;
}

static gboolean valid_markhost(GMatchInfo* match, t_tseq* mark, t_host* host, const char *type, const char *line) {
  if (!valid_mark(match, mark)) { DEBUG("wrong MARK in %s message: %s", type, line); return false; }
  host->addr = fetch_named_str(match, REN_ADDR);
  if (!host->addr) {
    host->addr = fetch_named_str(match, REN_IP);
    if (!host->addr) { DEBUG("wrong HOST in %s message: %s", type, line); return false; }
  }
  host->name = fetch_named_str(match, REN_NAME);
  return true;
}

static gboolean parse_success_match(int at, GMatchInfo* match, const char *line) {
  t_ping_success res;
  memset(&res, 0, sizeof(res));
  res.ttl = fetch_named_int(match, REN_TTL);
  if (res.ttl < 0) { DEBUG("wrong TTL in SUCCESS message: %s", line); return false; }
  res.time = fetch_named_rtt(match, REN_TIME);
  if (res.time < 0) { DEBUG("wrong TIME in SUCCESS message: %s", line); return false; }
  // to not free() after other mandatory fields unless it's failed
  if (!valid_markhost(match, &res.mark, &res.host, "SUCCESS", line)) return false;
  DEBUG("SUCCESS[at=%d seq=%d]: host=%s,%s ttl=%d time=%dusec", at, res.mark.seq, res.host.addr, res.host.name, res.ttl, res.time);
  stat_save_success(at, &res);
  return true;
}

static gboolean parse_discard_match(int at, GMatchInfo* match, const char *line) {
  t_ping_discard res;
  memset(&res, 0, sizeof(res));
  if (!valid_markhost(match, &res.mark, &res.host, "DISCARD", line)) return false;
  res.reason = fetch_named_str(match, REN_REASON);
  DEBUG("DISCARD[at=%d seq=%d]: host=%s,%s reason=\"%s\"", at, res.mark.seq, res.host.addr, res.host.name, res.reason);
  stat_save_discard(at, &res);
  return true;
}

static gboolean parse_timeout_match(int at, GMatchInfo* match, const char *line) {
  t_ping_timeout res;
  memset(&res, 0, sizeof(res));
  if (!valid_mark(match, &res.mark)) { DEBUG("wrong MARK in TIMEOUT message: %s", line); return false; }
  DEBUG("TIMEOUT[at=%d seq=%d]: seq=%d ts=%lld.%06d", at, res.mark.seq, res.mark.seq, res.mark.sec, res.mark.usec);
  stat_save_timeout(at, &res);
  return true;
}

static gboolean parse_info_match(int at, GMatchInfo* match, const char *line) {
  t_ping_info res;
  memset(&res, 0, sizeof(res));
  res.ttl = fetch_named_int(match, REN_TTL);
  if (res.ttl < 0) { DEBUG("wrong TTL in INFO message: %s", line); return false; }
  // after other mandatory fields to not free() if their fetch failed
  if (!valid_markhost(match, &res.mark, &res.host, "INFO", line)) return false;
  res.info = fetch_named_str(match, REN_INFO);
  DEBUG("INFO[at=%d seq=%d]: host=%s,%s ttl=%d info=\"%s\"", at, res.mark.seq, res.host.addr, res.host.name, res.ttl, res.info);
  stat_save_info(at, &res);
  return true;
}

static gboolean parse_match_wrap(int at, GRegex *re, const char *line, parser_cb parser) {
  GMatchInfo *match = NULL;
  if (!parser) return false;
  gboolean gowith = g_regex_match(re, line, 0, &match);
  if (match) {
    gboolean valid = gowith ? parser(at, match, line) : false;
    g_match_info_free(match);
    return valid;
  }
  return false;
}

static void analyze_line(int at, const char *line) {
  static int n_regexes = G_N_ELEMENTS(regexes);
  for (int i = 0; i < n_regexes; i++)
    if (parse_match_wrap(at, regexes[i].rx.regex, line, regexes[i].cb)) return;
  DEBUG("UNKNOWN[at=%d]: %s", at, line);
}

#define GREEDY false
#define LAZY   true

static char* split_pair(char **ps, int c, gboolean lazy) {
  if (!ps || !*ps) return NULL;
  char *val = lazy ? strchr(*ps, c) : strrchr(*ps, c);
  if (!val) return NULL;
  *val++ = 0;
  *ps = g_strstrip(*ps);
  return g_strstrip(val);
}

static inline gboolean parser_valid_char0(char *str) { return g_regex_match(hostname_char0_regex, str, 0, NULL); }
static inline gboolean parser_valid_host(char *host) { return g_regex_match(hostname_chars_regex, host, 0, NULL); }

#define HNAME_ERROR(cause) PARSER_MESG("Hostname %s", cause)

static gboolean target_meet_all_conditions(char *s, int len, int max) {
  // rfc1123,rfc952 restrictions
  if (len > max) { PARSER_MESG("Hostname: out of length limit (%d > %d)", len, max); return false; }
  if (s[len - 1] == '-') { HNAME_ERROR("cannot end with hyphen"); return false; }
  if (!parser_valid_char0(s)) { HNAME_ERROR("must start with a letter or a digit"); return false; }
  if (!parser_valid_host(s)) { HNAME_ERROR("contains not allowed characters"); return false; }
  return true;
}

static char* parser_valid_int(const char *option, const char *str) {
  char *cpy = g_strdup(str);
  if (!cpy) return NULL;
  char *val = g_strstrip(cpy);
  gboolean valid = g_regex_match(str_rx[OPT_TYPE_INT].regex, val, 0, NULL);
  if (valid) { val = (val && val[0]) ? g_strdup(val) : NULL; }
  else { PARSER_MESG("%s: no match %s regex", option, str_rx[OPT_TYPE_INT].pattern); val = NULL; }
  g_free(cpy);
  return val;
}


// pub
//
gboolean parser_init(void) {
  multiline_regex = compile_regex("\\n", G_REGEX_MULTILINE);
  hostname_char0_regex = compile_regex("^[" DIGIT_OR_LETTER "]", 0);
  hostname_chars_regex = compile_regex("^[" DIGIT_OR_LETTER ".-]+$", 0);
  gboolean re = multiline_regex && hostname_char0_regex && hostname_chars_regex;
  for (int i = 0; i < G_N_ELEMENTS(regexes); i++) {
    regexes[i].rx.regex = compile_regex(regexes[i].rx.pattern, 0);
    if (!regexes[i].rx.regex) re = false;
  }
  for (int i = 0; i < G_N_ELEMENTS(str_rx); i++) {
    str_rx[i].regex = compile_regex(str_rx[i].pattern, 0);
    if (!str_rx[i].regex) re = false;
  }
  return re;
}

void parser_parse(int at, char *input) {
  if (input) {
    char **lines = g_regex_split(multiline_regex, input, 0);
    if (lines) {
      for (char **s = lines; *s; s++) if ((*s)[0]) analyze_line(at, *s);
      g_strfreev(lines);
    }
  }
}

gboolean parser_mmint(const char *str, const char *option, t_minmax mm, int *re) {
  gboolean okay = false;
  if (str) {
    char *val = parser_valid_int(option, str);
    if (val) {
      errno = 0; int n = strtol(val, NULL, 0); okay = !errno; errno = 0; g_free(val);
      if (okay && MM_OKAY(mm, n)) { okay = true; if (re) *re = n; }
      else PARSER_MESG("%s: out of range[%d,%d]", option, mm.min, mm.max)
  }}
  return okay;
}

#define PARSE_MAX_CHARS 128
char* parser_str(const char *str, const char *option, int cat) {
  if ((cat >= 0) && (cat < G_N_ELEMENTS(str_rx))) {
    char *buff = g_strndup(str, PARSE_MAX_CHARS);
    if (buff) {
      char *val = g_strdup(g_strstrip(buff)); g_free(buff);
      if (val) {
        gboolean valid = g_regex_match(str_rx[cat].regex, val, 0, NULL);
        if (valid) return val;
      } else WARN_("g_strdup()");
      PARSER_MESG("%s: no match %s regex", option, str_rx[cat].pattern);
    } else WARN_("g_strndup()");
  } else WARN("wrong string category: %d", cat);
  return NULL;
}

char* parser_valid_target(const char *target) {
  if (!target || !target[0]) return NULL;
  char *copy = g_strdup(target);
  char *hostname = NULL, *s = g_strstrip(copy);
  int len = s ? g_utf8_strlen(s, MAXHOSTNAME + 1) : 0;
  if (len && target_meet_all_conditions(s, len, MAXHOSTNAME)) hostname = g_strdup(s);
  g_free(copy);
  return hostname;
}

#define DUP_WITH_MARK(dst, val) { \
  if (dst) { CLR_STR(dst); dst = g_strdup_printf("%s*", val); } \
  else dst = g_strndup(val, MAXHOSTNAME); }

void parser_whois(char *buff, int sz, char* elem[]) {
  // if there are multiple sources (despite -m query), take the last tag and mark it with '*'
  static const char skip_as_prfx[] = "AS";
  static int as_prfx_len = sizeof(skip_as_prfx) - 1;
  if (!buff || !elem) return;
  memset(elem, 0, sizeof(char*) * WHOIS_NDX_MAX);
  char *p = buff, *s;
  while ((s = strsep(&p, WHOIS_NL))) {
    s = g_strstrip(s);
    if (s && s[0] && (s[0] != WHOIS_COMMENT)) {
      char *val = split_pair(&s, WHOIS_DELIM, LAZY);
      if (!val) continue;
      int ndx = -1;
      if (STR_EQ(s, WHOIS_RT_TAG)) ndx = WHOIS_RT_NDX;
      else if (STR_EQ(s, WHOIS_AS_TAG)) {
        ndx = WHOIS_AS_NDX;
        int l = strnlen(val, MAXHOSTNAME);
        if ((l > as_prfx_len) && !g_ascii_strncasecmp(val, skip_as_prfx, as_prfx_len))
          val += as_prfx_len;
      } else if (STR_EQ(s, WHOIS_DESC_TAG)) {
        ndx = WHOIS_DESC_NDX;
        char *cc = split_pair(&val, WHOIS_CCDEL, GREEDY);
        if (cc) DUP_WITH_MARK(elem[WHOIS_CC_NDX], cc);
      }
      if (ndx >= 0) DUP_WITH_MARK(elem[ndx], val);
    }
  }
  for (int i = 0; i < WHOIS_NDX_MAX; i++) if (!elem[i]) elem[i] = g_strdup(unkn_whois);
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

gboolean parser_range(char *range, char delim, const char *option, t_minmax *re) {
  if (!range || !re) return false;
  char *min = range;
  char *max = split_pair(&min, delim, LAZY);
  t_minmax val = { INT_MIN, INT_MIN };
  gboolean okay = true;
  okay &= parser_valint(min, &val.min, option);
  okay &= parser_valint(max, &val.max, option);
  if (okay) *re = val;
  return okay;
}

#ifdef WITH_PLOT
gboolean parser_ivec4(char *range, char delim, const char *option, ivec4_t re) {
  if (!range) return false;
  const char delims[] = { delim, 0 };
  char **abcs = g_strsplit(range, delims, 4);
  ivec4_t val = { INT_MIN, INT_MIN, INT_MIN, INT_MIN };
  gboolean okay = true;
  if (abcs) for (int i = 0; abcs[i] && (i < G_N_ELEMENTS(val)); i++)
    okay &= parser_valint(abcs[i], &val[i], option);
  g_strfreev(abcs);
  if (okay) { re[0] = val[0]; re[1] = val[1]; re[2] = val[2]; re[3] = val[3]; }
  return okay;
}
#endif

