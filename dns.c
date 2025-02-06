
#include <stdlib.h>

#include "common.h"
#include "dns.h"


typedef struct dns_elem { // network stuff
  GSList *refs; // hops requested addr resolv
  t_host host;  // copy of origin addr and resulting resolved hostname
} t_dns_elem;

static GSList *dns_query;
static GSList *dns_cache;

#if DNS_EXTRA_DEBUG
#define PR_DNS_Q do { LOG("%s: %s %c", DNS_HDR, __func__, 'q'); _pr_dns_qlist(dns_query); } while (0)
#define PR_DNS_C do { LOG("%s: %s %c", DNS_HDR, __func__, 'c'); _pr_dns_clist(dns_cache); } while (0)
static void _pr_dns_qlist(GSList *qlist) {
  int i = 0;
  for (GSList *p = qlist; p; p = p->next, i++) {
    t_dns_elem *data = p->data; if (!data) continue;
    DNS_DEBUG("%c[%d]: %s=%s %s=%s", 'q', i,
      ADDR_HDR, data->host.addr, NAME_HDR, mnemo(data->host.name));
    print_refs(data->refs, DNS_HDR);
  }
}
static void _pr_dns_clist(GSList *clist) {
  int i = 0;
  for (GSList *p = clist; p; p = p->next, i++) {
    t_host *c = p->data; if (!c) continue;
    DNS_DEBUG("%c[%d]: %s=%s %s=%s", 'c', i,
      ADDR_HDR, c->addr, NAME_HDR, mnemo(c->name));
  }
}
#else
#define PR_DNS_Q NOOP
#define PR_DNS_C NOOP
#endif

static void dns_query_free(t_dns_elem *elem) {
  if (elem) {
    host_free(&elem->host);
    g_slist_free_full(elem->refs, g_free);
  }
}

static int dns_query_cmp(const void *a, const void *b) {
  return (a && b) ?
    g_strcmp0(((t_dns_elem*)a)->host.addr, ((t_dns_elem*)b)->host.addr) :
    -1;
}

static t_host* dns_cache_find(char* addr) {
  t_host find = { .addr = addr };
  GSList *found = g_slist_find_custom(dns_cache, &find, host_cmp);
  return found ? found->data : NULL;
}

static t_dns_elem* dns_query_find(char* addr) {
  t_dns_elem find = { .host = { .addr = addr }};
  GSList *found = g_slist_find_custom(dns_query, &find, dns_query_cmp);
  return found ? found->data : NULL;
}

static void dns_query_complete(t_ref *ref, t_dns_elem *elem) {
  if (!ref || !elem) return;
  t_hop *hop = ref->hop;
  if (hop) {
    int ndx = ref->ndx;
    t_host *orig = &hop->host[ndx];
    if (hop && !orig->name) {
      if (STR_EQ(orig->addr, elem->host.addr)) {
        UPD_NSTR(orig->name, elem->host.name, MAXHOSTNAME);
        if (hop->cached) hop->cached = false;
        if (hop->cached_nl) hop->cached_nl = false;
        DNS_DEBUG("%s [%d,%d]: %s=%s %s=%s", DONE_HDR, hop->at, ndx,
          ADDR_HDR, orig->addr, NAME_HDR, mnemo(orig->name));
      } else LOG("%s: %s: %s -> %s", DNS_HDR, ORIG_CHNG_HDR,
        orig->addr, elem->host.addr);
    }
  }
}

static void dns_cache_update(char *addr, char *name) {
  if (!addr) return;
  t_host *host = dns_cache_find(addr);
  if (host) {
    UPD_NSTR(host->name, name, MAXHOSTNAME);
    PR_DNS_C;
    return;
  }
  host = g_malloc0(sizeof(t_host));
  if (host) {
    host->addr = g_strndup(addr, MAXHOSTNAME);
    host->name = g_strndup(name ? name : UNKN_FIELD, MAXHOSTNAME);
    if (host->addr && host->name) {
      if (list_add_nodup(&dns_cache, host, host_cmp, DNS_CACHE_MAX)) {
        LOG("%s: %s=%s %s=%s", DNS_CUP_HDR,
          ADDR_HDR, host->addr, NAME_HDR, mnemo(host->name));
        PR_DNS_C;
        return;
      }
      FAILX(addr, "add cache");
    } else FAILX(addr, "dup host");
  } else FAILX(addr, "alloc host");
  host_free(host);
  g_free(host);
}

static t_dns_elem* dns_query_save(const char *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_dns_elem *elem = g_malloc0(sizeof(t_dns_elem));
  if (!elem) return NULL;
  elem->host.addr = g_strndup(addr, MAXHOSTNAME);
  if (elem->host.addr) {
    if (list_add_ref(&elem->refs, hop, ndx)) {
      GSList *added = list_add_nodup(&dns_query, elem, dns_query_cmp, DNS_QUERY_MAX);
      if (added) {
        DNS_DEBUG("%s: %s %s=%d %s[%d]=%s", SAVEQ_HDR, addr,
          HOP_HDR, hop->at, ADDR_HDR, ndx, hop->host[ndx].addr);
        PR_DNS_Q;
        return added->data;
      }
      FAILX(addr, "add query");
    } else FAILX(addr, "add ref");
  } else FAILX(addr, "dup addr");
  dns_query_free(elem);
  g_free(elem);
  return NULL;
}

static t_dns_elem* dns_query_fill(char *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_dns_elem *found = dns_query_find(addr);
  if (found) {
    if (!list_add_ref(&found->refs, hop, ndx)) FAILX(addr, "add ref");
    return NULL;
  }
  DNS_DEBUG("%s %s: %s=%d %s[%d]=%s", LOOKUP_HDR, addr, HOP_HDR, hop->at + 1,
   ADDR_HDR, ndx, hop->host[ndx].addr);
  return dns_query_save(addr, hop, ndx);
}

static void on_dns_lookup(GObject* src, GAsyncResult *result, t_dns_elem *elem) {
  GError *error = NULL;
  char *name = g_resolver_lookup_by_address_finish(G_RESOLVER(src), result, &error);
  if (elem) {
    if (!name) DNS_DEBUG("%s: %s", UNRES_HDR, elem->host.addr);
    elem->host.name = name;
    DNS_DEBUG("%s=%s: %s=%s", ADDR_HDR, elem->host.addr, NAME_HDR, mnemo(name));
    PR_DNS_Q;
    dns_cache_update(elem->host.addr, name);
    g_slist_foreach(elem->refs, (GFunc)dns_query_complete, elem);
    dns_query_free(elem);
    dns_query = g_slist_remove(dns_query, elem);
    g_free(elem);
    PR_DNS_Q;
  }
}


// pub
//

void dns_lookup(t_hop *hop, int ndx) {
  if (!hop) return;
  char *addr = hop->host[ndx].addr;
  if (!addr) return;
  PR_DNS_C;
  t_host *cached = dns_cache_find(addr);
  if (cached) { // update with cached data and return
    UPD_NSTR(hop->host[ndx].name, cached->name, MAXHOSTNAME);
    if (hop->cached)    hop->cached    = false;
    if (hop->cached_nl) hop->cached_nl = false;
    DNS_DEBUG("%s [%d,%d]: %s -> %s=%s", CACHE_HIT_HDR, hop->at, ndx, addr,
      NAME_HDR, mnemo(cached->name));
    return;
  }
  PR_DNS_Q;
  t_dns_elem *query = dns_query_fill(addr, hop, ndx);
  if (!query) return;
  GResolver *res = g_resolver_get_default();
  if (res) {
    GInetAddress *inet_addr = g_inet_address_new_from_string(addr);
    if (inet_addr) {
      DNS_DEBUG("%s %s", SENDQ_HDR, addr);
      g_resolver_lookup_by_address_async(res, inet_addr, NULL, (GAsyncReadyCallback)on_dns_lookup, query);
      g_object_unref(inet_addr);
    } else FAIL("g_inet_address_new_from_string()");
    g_object_unref(res);
  } else FAIL("g_resolver_get_default()");
}

static void dns_host_on_free(gpointer data, gpointer user_data G_GNUC_UNUSED) {
  host_free(data);
}
static void dns_query_on_free(gpointer data, gpointer user_data G_GNUC_UNUSED) {
  dns_query_free(data);
}

void dns_cache_free(void) {
  PR_DNS_C;
  g_slist_foreach(dns_cache, dns_host_on_free, NULL);
  g_slist_free_full(dns_cache, g_free);
  dns_cache = NULL;
  PR_DNS_Q;
  g_slist_foreach(dns_query, dns_query_on_free, NULL);
  g_slist_free_full(dns_query, g_free);
  dns_query = NULL;
}

