
#include "valid.h"
#include "parser.h"
#include "common.h"

static bool target_meet_all_conditions(gchar *s, int len, int max) {
  // rfc1123,rfc952 restrictions
  if (len > max) { LOG("Out of length limit (%d > %d)", len, max); return false; }
  if (s[len - 1] == '-') { LOG("Hostname %s", "cannot end with hyphen"); return false; }
  if (!test_hchar0(s)) { LOG("Hostname %s", "must start with a letter or a digit"); return false; }
  if (!test_hchars(s)) { LOG("Hostname %s", "contains not allowed characters"); return false; }
  return true;
}

gchar* valid_target(const gchar *target) {
  if (!target || !target[0]) return NULL;
  gchar *copy = g_strdup(target);
  gchar *hostname = NULL, *s = g_strstrip(copy);
  int len = g_utf8_strlen(s, MAXHOSTNAME + 1);
  if (len && target_meet_all_conditions(s, len, MAXHOSTNAME)) hostname = g_strdup(s);
  g_free(copy);
  return hostname;
}

