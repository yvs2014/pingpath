
#include "parser.h"
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

#define PATT_TS   "^\\[(?<" REN_TS_S ">[0-9]+)(.(?<" REN_TS_U ">[0-9]+))*\\]"
#define PATT_FROM "rom (((?<" REN_NAME ">.*) \\((?<" REN_ADDR ">.*)\\))|(?<" REN_IP ">.*))"
#define PATT_SEQ  "icmp_seq=(?<" REN_SEQ ">\\d+)"

#define SUCCESS 0
#define DISCARD 1

typedef bool parser_fn(GMatchInfo* match, const char *line);

typedef struct ping_part_mark {
  long ts_sec; int ts_usec;
  int seq;
} t_ping_part_mark;

typedef struct ping_part_host {
  gchar *name, *addr;
} t_ping_part_host;

typedef struct ping_success {
  t_ping_part_mark mark;
  t_ping_part_host host;
  int ttl, time;
} t_ping_success;

typedef struct ping_discard {
  t_ping_part_mark mark;
  t_ping_part_host host;
  gchar* reason;
} t_ping_discard;

typedef struct ping_timeout {
  t_ping_part_mark mark;
} t_ping_timeout;

typedef struct response_regex {
  char *pattern;
  parser_fn *parser;
  GRegex *regex;
} t_response_regex;

static bool parse_success_match(GMatchInfo* match, const char *line);
static bool parse_discard_match(GMatchInfo* match, const char *line);
static bool parse_timeout_match(GMatchInfo* match, const char *line);

t_response_regex regexes[] = {
  { .parser = parse_success_match, .pattern =
    PATT_TS " \\d+ bytes f" PATT_FROM ": " PATT_SEQ " ttl=(?<" REN_TTL ">\\d+) time=(?<" REN_TIME ">\\d+.?\\d+)" },
  { .parser = parse_discard_match, .pattern =
    PATT_TS " F" PATT_FROM " " PATT_SEQ " (?<" REN_REASON ">.*)" },
  { .parser = parse_timeout_match, .pattern =
    PATT_TS " no answer yet for " PATT_SEQ },
};

GRegex *multiline_regex;

GRegex* compile_regex(const char *pattern, GRegexCompileFlags flags) {
  GError *err = NULL;
  GRegex *regex = g_regex_new(pattern, flags, 0, &err);
  if (err) {
    LOG("regex PATTERN: %s", pattern);
    LOG("regex ERROR: %s", err->message);
  }
  g_assert(regex);
  return regex;
}

void parser_init(void) {
//  multiline_regex = compile_regex("[^\\n]+", G_REGEX_MULTILINE);
  multiline_regex = compile_regex("\\n", G_REGEX_MULTILINE);
  for (int i = 0; i < (sizeof(regexes) / sizeof(regexes[0])); i++)
    regexes[i].regex = compile_regex(regexes[i].pattern, 0);
}

static gchar* fetch_named(GMatchInfo* match, char *prop) {
  gchar *val = g_match_info_fetch_named(match, prop);
  if (val && !val[0]) { g_free(val); return NULL; }
  return val;
}

static bool valid_mark(GMatchInfo* match, t_ping_part_mark *mark) {
  gchar *s;
  s = fetch_named(match, REN_SEQ);  mark->seq     = s ? atoi(s) : -1;
  if (mark->seq < 0) return false;
  s = fetch_named(match, REN_TS_S); mark->ts_sec  = s ? atol(s) : -1;
  if (mark->ts_sec < 0) return false;
  s = fetch_named(match, REN_TS_U); mark->ts_usec = s ? atoi(s) : 0;
  return true;
}

static bool valid_markhost(GMatchInfo* match, t_ping_part_mark* mark, t_ping_part_host* host,
    const char *typ, const char *line) {
  if (!valid_mark(match, mark)) {
    memset(mark, 0, sizeof(*mark)); LOG("wrong MARK in %s message: %s", typ, line);
    return false;
  }
  gchar *s;
  s = fetch_named(match, REN_NAME); host->name = s ? strdup(s) : NULL;
  s = fetch_named(match, REN_ADDR); host->addr = s ? strdup(s) : NULL;
  if (!host->addr) {
    s = fetch_named(match, REN_IP); host->addr = s ? strdup(s) : NULL;
    if (!host->addr) {
      if (host->name) free(host->name);
      if (host->addr) free(host->addr);
      memset(host, 0, sizeof(*host)); LOG("wrong HOST in %s message: %s", typ, line);
      return false;
    }
  }
  return true;
}

static int usec_or_neg(const char *s) {
  static const int MAX_MSEC = TIMEOUT * 1000 * 10; // 10 times more than timeout in msec
  double val = g_ascii_strtod(s, NULL);
  if ((ERANGE == errno) || (val < 0) || (val > MAX_MSEC)) return -1;
  return floor(val * 1000); // msec to usec
}

static bool parse_success_match(GMatchInfo* match, const char *line) {
  t_ping_success res;
  memset(&res, 0, sizeof(res));
  if (!valid_markhost(match, &res.mark, &res.host, "SUCCESS", line)) return false;
  gchar *s;
  s = fetch_named(match, REN_TTL); res.ttl = s ? atoi(s) : -1;
  if (res.ttl < 0) { LOG("wrong TTL in SUCCESS message: %s", s); return false; }
  s = fetch_named(match, REN_TIME); res.time = s ? usec_or_neg(s) : -1;
  if (res.time < 0) { LOG("wrong TIME in SUCCESS message: %s", s); return false; }
  LOG("SUCCESS: host=%s,%s ttl=%d time=%dusec", res.host.addr, res.host.name, res.ttl, res.time);
  return true;
}

static bool parse_discard_match(GMatchInfo* match, const char *line) {
  t_ping_discard res;
  memset(&res, 0, sizeof(res));
  if (!valid_markhost(match, &res.mark, &res.host, "DISCARD", line)) return false;
  gchar *s = fetch_named(match, REN_REASON); res.reason = s ? strdup(s) : NULL;
  if (!res.reason) { LOG("no reason in DISCARD message: %s", s); return false; }
  LOG("DISCARD: host=%s,%s reason=\"%s\"", res.host.addr, res.host.name, res.reason);
  return true;
}

static bool parse_timeout_match(GMatchInfo* match, const char *line) {
  t_ping_timeout res;
  memset(&res, 0, sizeof(res));
  if (!valid_mark(match, &res.mark)) {
    memset(&res.mark, 0, sizeof(res.mark)); LOG("wrong MARK in %s message: %s", "TIMEOUT", line);
    return false;
  }
  LOG("TIMEOUT: seq=%d ts=%ld.%06d", res.mark.seq, res.mark.ts_sec, res.mark.ts_usec);
  return true;
}

static bool parse_match_wrap(GRegex *re, const char *line, parser_fn *fn) {
  GMatchInfo *match = NULL;
  bool gowith = g_regex_match(re, line, 0, &match);
  if (match) {
    bool valid = gowith ? fn(match, line) : false;
    g_match_info_free(match);
    return valid;
  }
  return false;
}

static void analyze_line(const char *line) {
//  LOG("> \"%s\"", line);
  for (int i = 0; i < (sizeof(regexes) / sizeof(regexes[0])); i++) {
    if (parse_match_wrap(regexes[i].regex, line, regexes[i].parser)) return;
  }
  LOG("UNKNOWN: %s", line);
}

char* parse_input(char *multiline) {
  gchar **lines = g_regex_split(multiline_regex, multiline, 0);
  for (gchar **s = lines; *s; s++) if ((*s)[0]) analyze_line(*s);
  /* TMP */ gchar* last; for (gchar **s = lines; *s; s++) if ((*s)[0]) last = *s; last = strdup(last);
  if (lines) g_strfreev(lines);
  /* TMP */ return last;
}

