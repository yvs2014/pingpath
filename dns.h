#ifndef DNS_H
#define DNS_H

#include "common.h"

void dns_lookup(t_hop *hop, int ndx);
void dns_cache_free(void);

#endif
