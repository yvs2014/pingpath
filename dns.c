
#include "dns.h"
#include "stat.h"

typedef struct dns_elem { // network stuff
  GSList *refs; // hops requested addr resolv
  t_host host;  // copy of origin addr and resulting resolved hostname
} t_dns_elem;

static GSList *dns_query;
static GSList *dns_cache;

#ifdef DNS_DEBUGGING
#define DNS_DEBUG(fmt, ...) { if (verbose & 4) g_message("DNS: " fmt, __VA_ARGS__); }
#else
#define DNS_DEBUG(...) {}
#endif

#if DNS_DEBUGGING && DNS_DEBUGGING > 1
#define PR_DNS_Q { LOG("DNS: %s: q=%p", __func__, dns_query); _pr_dns_qlist(dns_query); }
#define PR_DNS_C { LOG("DNS: %s: c=%p", __func__, dns_cache); _pr_dns_clist(dns_cache); }
static void _pr_dns_qlist(GSList *qlist) {
  int i = 0;
  for (GSList *p = qlist; p; p = p->next, i++) {
    t_dns_elem *data = p->data; if (!data) continue;
    DNS_DEBUG("q[%d]: addr=%s name=%s refs=%p", i, data->host.addr, data->host.name, data->refs);
    print_refs(data->refs, "DNS:  ");
  }
}
static void _pr_dns_clist(GSList *clist) {
  int i = 0;
  for (GSList *p = clist; p; p = p->next, i++) {
    t_host *c = p->data; if (!c) continue;
    DNS_DEBUG("c[%d]: addr=%s name=%s", i, c->addr, c->name);
  }
}
#else
#define PR_DNS_Q {}
#define PR_DNS_C {}
#endif

static void dns_query_free(t_dns_elem *elem) {
  if (elem) { host_free(&elem->host); g_slist_free_full(elem->refs, g_free); }
}

static int dns_query_cmp(const void *a, const void *b) {
  if (!a || !b) return -1;
  return g_strcmp0(((t_dns_elem*)a)->host.addr, ((t_dns_elem*)b)->host.addr);
}

static t_host* dns_cache_find(gchar* addr) {
  t_host find = { .addr = addr };
  GSList *found = g_slist_find_custom(dns_cache, &find, host_cmp);
  return found ? found->data : NULL;
}

static t_dns_elem* dns_query_find(gchar* addr) {
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
        if (orig->name) stat_check_hostname_max(g_utf8_strlen(orig->name, MAXHOSTNAME));
        DNS_DEBUG("%s(%d,%d) addr=%s name=%s", __func__, hop->at, ndx, orig->addr, orig->name);
      } else LOG("dns(%s) origin is changed: %s", elem->host.addr, orig->addr);
    }
  }
}

static void dns_cache_update(gchar *addr, gchar *name) {
  if (!addr) return;
  t_host *host = dns_cache_find(addr);
  if (host) { UPD_NSTR(host->name, name, MAXHOSTNAME); PR_DNS_C; return; }
  host = g_malloc0(sizeof(t_host));
  if (host) {
    host->addr = g_strndup(addr, MAXHOSTNAME);
    host->name = g_strndup(name ? name : unkn_field, MAXHOSTNAME);
    if (host->addr && host->name) {
      if (list_add_nodup(&dns_cache, host, host_cmp, DNS_CACHE_MAX)) {
        LOG("dns cache updated with addr=%s name=%s", host->addr, host->name);
        PR_DNS_C; return;
      } else FAILX(addr, "add cache");
    } else FAILX(addr, "dup host");
  } else FAILX(addr, "alloc host");
  host_free(host);
  g_free(host);
}

static t_dns_elem* dns_query_save(const gchar *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_dns_elem *elem = g_malloc0(sizeof(t_dns_elem));
  if (!elem) return NULL;
  elem->host.addr = g_strndup(addr, MAXHOSTNAME);
  if (elem->host.addr) {
    if (list_add_ref(&elem->refs, hop, ndx)) {
      GSList *added = list_add_nodup(&dns_query, elem, dns_query_cmp, DNS_QUERY_MAX);
      if (added) {
        DNS_DEBUG("%s(%s) hop[%d] host[%d]=%s", __func__, addr, hop->at, ndx, hop->host[ndx].addr);
        PR_DNS_Q; return added->data;
      } else FAILX(addr, "add query");
    } else FAILX(addr, "add ref");
  } else FAILX(addr, "dup addr");
  dns_query_free(elem);
  g_free(elem);
  return NULL;
}

static t_dns_elem* dns_query_fill(gchar *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_dns_elem *found = dns_query_find(addr);
  if (found) {
    if (!list_add_ref(&found->refs, hop, ndx)) FAILX(addr, "add ref");
    return NULL;
  }
  DNS_DEBUG("%s(%s) hop[%d] host[%d]=%s", __func__, addr, hop->at, ndx, hop->host[ndx].addr);
  return dns_query_save(addr, hop, ndx);
}

static void on_dns_lookup(GObject* src, GAsyncResult *result, t_dns_elem *elem) {
  GError *error = NULL;
  gchar *name = g_resolver_lookup_by_address_finish(G_RESOLVER(src), result, &error);
  if (elem) {
    if (!name) DNS_DEBUG("%s unresolved", elem->host.addr);
    elem->host.name = name;
    DNS_DEBUG("%s(%s) name=%s", __func__, elem->host.addr, name);
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
  gchar *addr = hop->host[ndx].addr;
  if (!addr) return;
  PR_DNS_C;
  t_host *cached = dns_cache_find(addr);
  if (cached) { // update with cached data and return
    UPD_NSTR(hop->host[ndx].name, cached->name, MAXHOSTNAME);
    if (hop->cached) hop->cached = false;
    if (hop->cached_nl) hop->cached_nl = false;
    DNS_DEBUG("cache hit[%d,%d]: addr=%s -> name=%s", hop->at, ndx, addr, cached->name);
    return;
  }
  PR_DNS_Q;
  t_dns_elem *query = dns_query_fill(addr, hop, ndx);
  if (!query) return;
  GResolver *res = g_resolver_get_default();
  if (res) {
    GInetAddress *ia = g_inet_address_new_from_string(addr);
    if (ia) {
      DNS_DEBUG("send query=%s", addr);
      g_resolver_lookup_by_address_async(res, ia, NULL, (GAsyncReadyCallback)on_dns_lookup, query);
      g_object_unref(ia);
    } else FAIL("g_inet_address_new_from_string()");
    g_object_unref(res);
  } else FAIL("g_resolver_get_default()");
}

void dns_cache_free(void) {
  PR_DNS_C;
  g_slist_foreach(dns_cache, (GFunc)host_free, NULL);
  g_slist_free_full(dns_cache, g_free);
  dns_cache = NULL;
  PR_DNS_Q;
  g_slist_foreach(dns_query, (GFunc)dns_query_free, NULL);
  g_slist_free_full(dns_query, g_free);
  dns_query = NULL;
}

