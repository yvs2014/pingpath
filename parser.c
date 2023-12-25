
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

const int tos_max   = 255;
const int psize_min = 16;
const int psize_max = 9000 - 20 - 8;

typedef bool parser_cb(int at, GMatchInfo* match, const char *line);

typedef struct parser_regex {
  char *pattern;
  GRegex *regex;
} t_parser_regex;

typedef struct response_regex {
  t_parser_regex rx;
  parser_cb *cb;
} t_response_regex;

enum { STR_RX_INT, STR_RX_PAD, STR_RX_MAX };

static bool parse_success_match(int at, GMatchInfo* match, const char *line);
static bool parse_discard_match(int at, GMatchInfo* match, const char *line);
static bool parse_timeout_match(int at, GMatchInfo* match, const char *line);
static bool parse_info_match(int at, GMatchInfo* match, const char *line); // like 'success' without 'time'

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
    PATT_TS " \\d+ bytes f" PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) (?<" REN_INFO ">.*)" }},
};

static t_parser_regex str_rx[STR_RX_MAX] = {
  [STR_RX_INT] = { .pattern = "^\\d+$" },
  [STR_RX_PAD] = { .pattern = "^[\\da-fA-F]{1,32}$" },
};

static GRegex* compile_regex(const char *pattern, GRegexCompileFlags flags) {
  GError *err = NULL;
  GRegex *regex = g_regex_new(pattern, flags, 0, &err);
  if (err) {
    WARN("regex PATTERN: %s", pattern);
    WARN("regex ERROR: %s", err->message);
  }
  g_assert(regex);
  return regex;
}

static gchar* fetch_named_str(GMatchInfo* match, char *prop) {
  gchar *s = g_match_info_fetch_named(match, prop);
  gchar *val = (s && s[0]) ? g_strdup(s) : NULL;
  g_free(s); return val;
}

static int fetch_named_int(GMatchInfo* match, char *prop) {
  gchar *s = g_match_info_fetch_named(match, prop);
  int val = (s && s[0]) ? atoi(s) : -1;
  g_free(s); return val;
}

static long long fetch_named_ll(GMatchInfo* match, char *prop) {
  gchar *s = g_match_info_fetch_named(match, prop);
  long long val = (s && s[0]) ? atoll(s) : -1;
  g_free(s); return val;
}

static int fetch_named_rtt(GMatchInfo* match, char *prop) {
  gchar *s = g_match_info_fetch_named(match, prop);
  double val = -1;
  if (s && s[0]) {
    val = g_ascii_strtod(s, NULL);
    if (ERANGE == errno) val = -1;
    if (val > 0) val *= 1000; // msec to usec
  }
  g_free(s); return (val < 0) ? -1 : floor(val);
}

static bool valid_mark(GMatchInfo* match, t_tseq *mark) {
  mark->seq = fetch_named_int(match, REN_SEQ); if (mark->seq < 0) return false;
  mark->sec = fetch_named_ll(match, REN_TS_S); if (mark->sec < 0) return false;
  mark->usec = fetch_named_int(match, REN_TS_U); if (mark->usec < 0) mark->usec = 0;
  return true;
}

static bool valid_markhost(GMatchInfo* match, t_tseq* mark, t_host* host,
    const char *typ, const char *line) {
  if (!valid_mark(match, mark)) { WARN("wrong MARK in %s message: %s", typ, line); return false; }
  host->addr = fetch_named_str(match, REN_ADDR);
  if (!host->addr) {
    host->addr = fetch_named_str(match, REN_IP);
    if (!host->addr) { WARN("wrong HOST in %s message: %s", typ, line); return false; }
  }
  host->name = fetch_named_str(match, REN_NAME);
  return true;
}

static bool parse_success_match(int at, GMatchInfo* match, const char *line) {
  t_ping_success res;
  memset(&res, 0, sizeof(res));
  res.ttl = fetch_named_int(match, REN_TTL);
  if (res.ttl < 0) { WARN("wrong TTL in SUCCESS message: %s", line); return false; }
  res.time = fetch_named_rtt(match, REN_TIME);
  if (res.time < 0) { WARN("wrong TIME in SUCCESS message: %s", line); return false; }
  // to not free() after other mandatory fields unless it's failed
  if (!valid_markhost(match, &res.mark, &res.host, "SUCCESS", line)) return false;
  DEBUG("SUCCESS[at=%d seq=%d]: host=%s,%s ttl=%d time=%dusec", at, res.mark.seq, res.host.addr, res.host.name, res.ttl, res.time);
  stat_save_success(at, &res);
  return true;
}

static bool parse_discard_match(int at, GMatchInfo* match, const char *line) {
  t_ping_discard res;
  memset(&res, 0, sizeof(res));
  if (!valid_markhost(match, &res.mark, &res.host, "DISCARD", line)) return false;
  res.reason = fetch_named_str(match, REN_REASON);
  DEBUG("DISCARD[at=%d seq=%d]: host=%s,%s reason=\"%s\"", at, res.mark.seq, res.host.addr, res.host.name, res.reason);
  stat_save_discard(at, &res);
  return true;
}

static bool parse_timeout_match(int at, GMatchInfo* match, const char *line) {
  t_ping_timeout res;
  memset(&res, 0, sizeof(res));
  if (!valid_mark(match, &res.mark)) { WARN("wrong MARK in TIMEOUT message: %s", line); return false; }
  DEBUG("TIMEOUT[at=%d seq=%d]: seq=%d ts=%lld.%06d", at, res.mark.seq, res.mark.seq, res.mark.sec, res.mark.usec);
  stat_save_timeout(at, &res);
  return true;
}

static bool parse_info_match(int at, GMatchInfo* match, const char *line) {
  t_ping_info res;
  memset(&res, 0, sizeof(res));
  res.ttl = fetch_named_int(match, REN_TTL);
  if (res.ttl < 0) { WARN("wrong TTL in INFO message: %s", line); return false; }
  // after other mandatory fields to not free() if their fetch failed
  if (!valid_markhost(match, &res.mark, &res.host, "INFO", line)) return false;
  res.info = fetch_named_str(match, REN_INFO);
  DEBUG("INFO[at=%d seq=%d]: host=%s,%s ttl=%d info=\"%s\"", at, res.mark.seq, res.host.addr, res.host.name, res.ttl, res.info);
  stat_save_info(at, &res);
  return true;
}

static bool parse_match_wrap(int at, GRegex *re, const char *line, parser_cb parser) {
  GMatchInfo *match = NULL;
  if (!parser) return false;
  bool gowith = g_regex_match(re, line, 0, &match);
  if (match) {
    bool valid = gowith ? parser(at, match, line) : false;
    g_match_info_free(match);
    return valid;
  }
  return false;
}

static void analyze_line(int at, const char *line) {
  for (int i = 0; i < G_N_ELEMENTS(regexes); i++)
    if (parse_match_wrap(at, regexes[i].rx.regex, line, regexes[i].cb)) return;
  DEBUG("UNKNOWN[at=%d]: %s", at, line);
}

static char* split_with_delim(char **ps, int c) {
  if (!ps || !*ps) return NULL;
  char *val = strchr(*ps, c);
  if (!val) return NULL;
  *val++ = 0;
  *ps = g_strstrip(*ps);
  return g_strstrip(val);
}

static inline bool parser_valid_char0(gchar *str) { return g_regex_match(hostname_char0_regex, str, 0, NULL); }
static inline bool parser_valid_host(gchar *host) { return g_regex_match(hostname_chars_regex, host, 0, NULL); }

#define INV_HN_NOTIFY(cause) notifier_inform("Hostname %s", cause)

static bool target_meet_all_conditions(gchar *s, int len, int max) {
  // rfc1123,rfc952 restrictions
  if (len > max) { notifier_inform("Hostname: out of length limit (%d > %d)", len, max); return false; }
  if (s[len - 1] == '-') { INV_HN_NOTIFY("cannot end with hyphen"); return false; }
  if (!parser_valid_char0(s)) { INV_HN_NOTIFY("must start with a letter or a digit"); return false; }
  if (!parser_valid_host(s)) { INV_HN_NOTIFY("contains not allowed characters"); return false; }
  return true;
}


// pub
//
bool parser_init(void) {
  multiline_regex = compile_regex("\\n", G_REGEX_MULTILINE);
  hostname_char0_regex = compile_regex("^[" DIGIT_OR_LETTER "]", 0);
  hostname_chars_regex = compile_regex("^[" DIGIT_OR_LETTER ".-]+$", 0);
  bool re = multiline_regex && hostname_char0_regex && hostname_chars_regex;
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
  gchar **lines = g_regex_split(multiline_regex, input, 0);
  for (gchar **s = lines; *s; s++) if ((*s)[0]) analyze_line(at, *s);
  if (lines) g_strfreev(lines);
}

#define CI_MIN 1
#define PIERR(fmt, ...) { notifier_inform("%s: " fmt, option, __VA_ARGS__); n = -1; }
#define PIOUT(min, max) PIERR("out of range[%d,%d]", min, max)

int parser_int(const gchar *str, int typ, const gchar *option) {
  GMatchInfo *match = NULL;
  gchar *cpy = g_strdup(str);
  if (!cpy) return -1;
  gchar *val = g_strstrip(cpy);
  bool valid = g_regex_match(str_rx[STR_RX_INT].regex, val, 0, &match);
  int n = atol(val);
  if (valid) {
    g_match_info_free(match);
    if ((n >= 0) && (n <= INT_MAX)) {
      switch (typ) {
        case ENT_STR_CYCLES:
        case ENT_STR_IVAL:
        case ENT_STR_LOGMAX:
          if (n < CI_MIN) PIERR("less than minimal(%d)", CI_MIN);
          break;
        case ENT_STR_QOS:
          if (n > tos_max) PIOUT(0, tos_max);
          break;
        case ENT_STR_PSIZE:
          if ((n < psize_min) || (n > psize_max)) PIOUT(psize_min, psize_max);
          break;
      }
    } else PIOUT(0, INT_MAX)
  } else PIERR("no match %s regex", str_rx[STR_RX_INT].pattern);
  g_free(cpy);
  return n;
}

const char* parser_pad(const gchar *str, const gchar *option) {
  static char pad_buff[PAD_SIZE];
  GMatchInfo *match = NULL;
  g_strlcpy(pad_buff, str, sizeof(pad_buff));
  char *val = g_strstrip(pad_buff);
  bool valid = g_regex_match(str_rx[STR_RX_PAD].regex, val, 0, &match);
  if (valid) {
    g_match_info_free(match);
    return val;
  } else notifier_inform("%s: no match %s regex", option, str_rx[STR_RX_PAD].pattern);
  return NULL;
}

gchar* parser_valid_target(const gchar *target) {
  if (!target || !target[0]) return NULL;
  gchar *copy = g_strdup(target);
  gchar *hostname = NULL, *s = g_strstrip(copy);
  int len = g_utf8_strlen(s, MAXHOSTNAME + 1);
  if (len && target_meet_all_conditions(s, len, MAXHOSTNAME)) hostname = g_strdup(s);
  g_free(copy);
  return hostname;
}

void parser_whois(gchar *buff, int sz, gchar* elem[]) {
  static const char skip_as_prfx[] = "AS";
  static int as_prfx_len = sizeof(skip_as_prfx) - 1;
  if (!buff || !elem) return;
  memset(elem, 0, sizeof(gchar*) * WHOIS_NDX_MAX);
  char *p = buff, *s;
  while ((s = strsep(&p, WHOIS_NL))) {
    s = g_strstrip(s);
    if (s && s[0] && (s[0] != WHOIS_COMMENT)) {
      char *val = split_with_delim(&s, WHOIS_DELIM);
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
        char *cc = split_with_delim(&val, WHOIS_CCDEL);
        if (cc) elem[WHOIS_CC_NDX] = g_strndup(cc, MAXHOSTNAME);
      }
      if (ndx >= 0) elem[ndx] = g_strndup(val, MAXHOSTNAME);
    }
  }
  for (int i = 0; i < WHOIS_NDX_MAX; i++) if (!elem[i]) elem[i] = g_strdup(unkn_whois);
}

