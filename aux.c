
#include <time.h>
#include "aux.h"

const char *timestampit(void) {
  static char now_ts[32];
  time_t now = time(NULL);
  strftime(now_ts, sizeof(now_ts), "%F %T", localtime(&now));
  return now_ts;
}

