
#include "parser.h"
#include "stat.h"
#include "common.h"

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

typedef bool parser_fn(int at, GMatchInfo* match, const char *line);

typedef struct response_regex {
  char *pattern;
  parser_fn *parser;
  GRegex *regex;
} t_response_regex;

static bool parse_success_match(int at, GMatchInfo* match, const char *line);
static bool parse_discard_match(int at, GMatchInfo* match, const char *line);
static bool parse_timeout_match(int at, GMatchInfo* match, const char *line);
static bool parse_info_match(int at, GMatchInfo* match, const char *line); // like 'success' without 'time'

t_response_regex regexes[] = {
  { .parser = parse_success_match, .pattern =
    PATT_TS " \\d+ bytes f" PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) time=(?<" REN_TIME ">\\d+.?\\d+)" },
  { .parser = parse_discard_match, .pattern =
    PATT_TS " F" PATT_FROM " " PATT_SEQ " (?<" REN_REASON ">.*)" },
  { .parser = parse_timeout_match, .pattern =
    PATT_TS " no answer yet for " PATT_SEQ },
  { .parser = parse_info_match, .pattern =
    PATT_TS " \\d+ bytes f" PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) (?<" REN_INFO ">.*)" },
};

GRegex *multiline_regex;
GRegex *hostname_char0_regex;
GRegex *hostname_chars_regex;

// aux
//
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

static int fetch_named_rtt(GMatchInfo* match, char *prop, double max) {
  gchar *s = g_match_info_fetch_named(match, prop);
  double val = -1;
  if (s && s[0]) {
    val = g_ascii_strtod(s, NULL);
    if ((ERANGE == errno) || (val > max)) val = -1;
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
  res.time = fetch_named_rtt(match, REN_TIME, TIMEOUT * 1000 * 10); // max: 10 times more than timeout in msec
  if (res.time < 0) { WARN("wrong TIME in SUCCESS message: %s", line); return false; }
  // after other mandatory fields to not free() if their fetch failed
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
  if (!valid_mark(match, &res.mark)) { WARN("wrong MARK in %s message: %s", "TIMEOUT", line); return false; }
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

static bool parse_match_wrap(int at, GRegex *re, const char *line, parser_fn *fn) {
  GMatchInfo *match = NULL;
  bool gowith = g_regex_match(re, line, 0, &match);
  if (match) {
    bool valid = gowith ? fn(at, match, line) : false;
    g_match_info_free(match);
    return valid;
  }
  return false;
}

static void analyze_line(int at, const char *line) {
  for (int i = 0; i < G_N_ELEMENTS(regexes); i++)
    if (parse_match_wrap(at, regexes[i].regex, line, regexes[i].parser)) return;
  DEBUG("UNKNOWN[at=%d]: %s", at, line);
}

// pub
//
bool parser_init(void) {
  multiline_regex = compile_regex("\\n", G_REGEX_MULTILINE);
  bool re = true;
  for (int i = 0; i < G_N_ELEMENTS(regexes); i++) {
    regexes[i].regex = compile_regex(regexes[i].pattern, 0);
    if (!regexes[i].regex) re = false;
  }
  hostname_char0_regex = compile_regex("^[" DIGIT_OR_LETTER "]", 0);
  hostname_chars_regex = compile_regex("^[" DIGIT_OR_LETTER ".-]+$", 0);
  return (re && multiline_regex && hostname_char0_regex && hostname_chars_regex);
}

void parser_parse(int at, char *input) {
  gchar **lines = g_regex_split(multiline_regex, input, 0);
  for (gchar **s = lines; *s; s++) if ((*s)[0]) analyze_line(at, *s);
  if (lines) g_strfreev(lines);
}

inline bool parser_valid_char0(gchar *str) { return g_regex_match(hostname_char0_regex, str, 0, NULL); }
inline bool parser_valid_host(gchar *host) { return g_regex_match(hostname_chars_regex, host, 0, NULL); }

