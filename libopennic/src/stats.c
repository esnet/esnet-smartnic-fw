#include "array_size.h"
#include "prom.h"
#include "stats.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//--------------------------------------------------------------------------------------------------
#define log_err(_rv, _format, _args...) \
    fprintf(stderr, "ERROR(%s)[%d (%s)]: " _format "\n", __func__, _rv, strerror(_rv),## _args)
#define log_panic(_rv, _format, _args...) \
    {log_err(_rv, _format,## _args); exit(EXIT_FAILURE);}

//--------------------------------------------------------------------------------------------------
struct stats_counter {
    struct stats_counter_spec spec;
    struct stats_block* block;

    uint64_t value;
    uint64_t last;

    struct {
        prom_metric_t* metric;
    } prometheus;
};

//--------------------------------------------------------------------------------------------------
struct stats_block {
    struct stats_block_spec spec;
    struct stats_zone* zone;

    struct stats_counter** counters;

    struct {
        prom_collector_t* collector;
    } prometheus;
};

//--------------------------------------------------------------------------------------------------
struct stats_zone {
    struct stats_zone_spec spec;
    struct stats_domain* domain;
    struct stats_zone* next;

    struct stats_block** blocks;
    size_t ncounters;
};

//--------------------------------------------------------------------------------------------------
struct stats_domain {
    struct stats_domain_spec spec;

    struct stats_zone* zones;
    size_t ncounters;

    struct {
        bool running;
        pthread_t handle;
        pthread_mutex_t _mutex;
        pthread_mutex_t* lock;
    } thread;
};

//--------------------------------------------------------------------------------------------------
static inline void stats_domain_lock(struct stats_domain* domain) {
    int rv = pthread_mutex_lock(domain->thread.lock);
    if (rv != 0) {
        log_panic(rv, "pthread_mutex_lock failed");
    }
}

static inline void stats_domain_unlock(struct stats_domain* domain) {
    int rv = pthread_mutex_unlock(domain->thread.lock);
    if (rv != 0) {
        log_panic(rv, "pthread_mutex_unlock failed");
    }
}

//--------------------------------------------------------------------------------------------------
static uint64_t __stats_counter_read(struct stats_counter* cnt) {
    const struct stats_counter_spec* spec = &cnt->spec;
    volatile void* addr = cnt->block->spec.base + spec->offset;
    uint64_t value = 0;

    switch (spec->size) {
#define COUNTER_READ(_width, _addr) (uint64_t)(*((volatile uint##_width##_t*)addr))
    case 1: value = COUNTER_READ(8, addr); break;
    case 2: value = COUNTER_READ(16, addr); break;
    case 4: value = COUNTER_READ(32, addr); break;
    case 8: value = COUNTER_READ(64, addr); break;
#undef COUNTER_READ

    default:
        log_panic(EINVAL, "Invalid size %zu bytes for counter %s at offset 0x%" PRIxPTR,
                  spec->size, spec->name, spec->offset);
        break;
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
static void __stats_counter_attach(struct stats_counter* cnt, struct stats_block* blk) {
    cnt->block = blk;

    int rv = prom_collector_add_metric(blk->prometheus.collector, cnt->prometheus.metric);
    if (rv != 0) {
        log_panic(rv, "prom_collector_add_metric failed for counter %s", cnt->spec.name);
    }
}

//--------------------------------------------------------------------------------------------------
static void __stats_counter_detach(struct stats_counter* cnt) {
    int rv = prom_collector_remove_metric(cnt->block->prometheus.collector, cnt->prometheus.metric);
    if (rv != 0) {
        log_panic(rv, "prom_collector_remove_metric failed for counter %s", cnt->spec.name);
    }
    cnt->prometheus.metric = NULL; // Automatic free when metric is removed.

    cnt->block = NULL;
}

//--------------------------------------------------------------------------------------------------
static void __stats_counter_free(struct stats_counter* cnt) {
    if (cnt->prometheus.metric != NULL) {
        int rv = prom_gauge_destroy(cnt->prometheus.metric);
        if (rv != 0) {
            log_panic(rv, "prom_gauge_destroy failed for counter %s", cnt->spec.name);
        }
    }

    free(cnt);
}

//--------------------------------------------------------------------------------------------------
static struct stats_counter* __stats_counter_alloc(const struct stats_counter_spec* spec) {
    struct stats_counter* cnt = calloc(1, sizeof(*cnt));
    if (cnt == NULL) {
        return NULL;
    }
    cnt->spec = *spec;

    const char* labels[] = {
        "domain",
        "zone",
        "block",
    };
    cnt->prometheus.metric = prom_gauge_new(spec->name, spec->desc, ARRAY_SIZE(labels), labels);
    if (cnt->prometheus.metric == NULL) {
        log_err(ENOMEM, "prom_gauge_new failed for counter %s", spec->name);
        goto free_counter;
    }

    return cnt;

free_counter:
    __stats_counter_free(cnt);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_attach(struct stats_block* blk, struct stats_zone* zone) {
    blk->zone = zone;

    const struct stats_block_spec* spec = &blk->spec;
    for (struct stats_counter** cnt = blk->counters; cnt < &blk->counters[spec->ncounters]; ++cnt) {
        __stats_counter_attach(*cnt, blk);
    }
    zone->ncounters += spec->ncounters;

    int rv = prom_collector_registry_register_collector(
        zone->domain->spec.prometheus.registry, blk->prometheus.collector);
    if (rv != 0) {
        log_panic(rv, "prom_collector_registry_register_collector failed for block %s", spec->name);
    }

    if (spec->attach_counters != NULL) {
        spec->attach_counters(spec);
    }
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_detach(struct stats_block* blk) {
    const struct stats_block_spec* spec = &blk->spec;
    if (spec->detach_counters != NULL) {
        spec->detach_counters(spec);
    }

    blk->zone->ncounters -= spec->ncounters;
    for (struct stats_counter** cnt = blk->counters; cnt < &blk->counters[spec->ncounters]; ++cnt) {
        __stats_counter_detach(*cnt);
    }

    int rv = prom_collector_registry_unregister_collector(
        blk->zone->domain->spec.prometheus.registry, blk->prometheus.collector);
    if (rv != 0) {
        log_panic(rv, "prom_collector_registry_unregister_collector failed for block %s",
                  spec->name);
    }
    blk->prometheus.collector = NULL; // Automatic free when collector is unregistered

    blk->zone = NULL;
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_free(struct stats_block* blk) {
    const struct stats_block_spec* spec = &blk->spec;
    for (struct stats_counter** cnt = blk->counters;
         cnt < &blk->counters[spec->ncounters]; ++cnt) {
        if (*cnt != NULL) {
            __stats_counter_free(*cnt);
        }
    }

    if (blk->prometheus.collector != NULL) {
        int rv = prom_collector_destroy(blk->prometheus.collector);
        if (rv != 0) {
            log_panic(rv, "prom_collector_destroy failed for block %s", spec->name);
        }
    }

    free(blk);
}

//--------------------------------------------------------------------------------------------------
static struct stats_block* __stats_block_alloc(const struct stats_block_spec* spec) {
    struct stats_block* blk = calloc(1, sizeof(*blk) + spec->ncounters * sizeof(blk->counters[0]));
    if (blk == NULL) {
        return NULL;
    }

#define COLLECTOR_NAME_FMT "block_%s_0x%" PRIxPTR
    int len = snprintf(NULL, 0, COLLECTOR_NAME_FMT, spec->name, (uintptr_t)spec->base);
    char name[len + 1];
    snprintf(name, len + 1, COLLECTOR_NAME_FMT, spec->name, (uintptr_t)spec->base);

    blk->prometheus.collector = prom_collector_new(name);
    if (blk->prometheus.collector == NULL) {
        log_err(ENOMEM, "prom_collector_new failed for %s", name);
        goto free_block;
    }
#undef COLLECTOR_NAME_FMT

    /*
     * Copy the block's specification, excluding the counter specification table (since it may have
     * been allocated temporarily by the caller). Each counter specification is copied over below.
     */
    blk->spec = *spec;
    blk->spec.counters = NULL;

    blk->counters = (typeof(blk->counters))&blk[1];
    for (unsigned int n = 0; n < spec->ncounters; ++n) {
        struct stats_counter* cnt = __stats_counter_alloc(&spec->counters[n]);
        if (cnt == NULL) {
            goto free_block;
        }
        blk->counters[n] = cnt;
    }

    return blk;

free_block:
    __stats_block_free(blk);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
static size_t __stats_block_get_counters(struct stats_block* blk,
                                         uint64_t* values,
                                         size_t nvalues) {
    const struct stats_block_spec* spec = &blk->spec;
    if (nvalues > spec->ncounters) {
        nvalues = spec->ncounters;
    }

    for (struct stats_counter** cnt = blk->counters;
         cnt < &blk->counters[nvalues];
         ++cnt, ++values) {
        *values = (*cnt)->value;
    }

    return nvalues;
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_update_counters(struct stats_block* blk, bool clear) {
    const struct stats_block_spec* spec = &blk->spec;
    if (spec->latch_counters != NULL) {
        spec->latch_counters(spec);
    }

    // Must match the ordering and number from __stats_counter_alloc.
    const char* labels[] = {
        blk->zone->domain->spec.name,
        blk->zone->spec.name,
        spec->name,
    };

    for (struct stats_counter** cnt = blk->counters; cnt < &blk->counters[spec->ncounters]; ++cnt) {
        struct stats_counter* c = *cnt;

        uint64_t current;
        if (spec->read_counter != NULL) {
            current = spec->read_counter(spec, &c->spec);
        } else {
            current = __stats_counter_read(c);
        }

        uint64_t diff = current;
        if (!STATS_COUNTER_FLAG_TEST(c->spec.flags, CLEAR_ON_READ)) {
            diff -= c->last;
            c->last = current;
        }

        if (clear) {
            c->value = 0;
        } else {
            c->value += diff;
        }

        prom_gauge_set(c->prometheus.metric, c->value, labels);
    }

    if (spec->release_counters != NULL) {
        spec->release_counters(spec);
    }
}

//--------------------------------------------------------------------------------------------------
static int __stats_block_for_each_counter(struct stats_block* blk,
                                          int (*callback)(const struct stats_for_each_spec* spec,
                                                          uint64_t value, void* arg),
                                          void* arg) {
    struct stats_for_each_spec spec = {
        .domain = &blk->zone->domain->spec,
        .zone = &blk->zone->spec,
        .block = &blk->spec,
    };

    int rv = 0;
    for (struct stats_counter** cnt = blk->counters;
         rv == 0 && cnt < &blk->counters[blk->spec.ncounters];
         ++cnt) {
        spec.counter = &(*cnt)->spec;
        rv = callback(&spec, (*cnt)->value, arg);
    }

    return rv;
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_attach(struct stats_zone* zone, struct stats_domain* domain) {
    struct stats_zone** link = &domain->zones;
    while (*link != NULL) {
        link = &(*link)->next;
    }

    *link = zone;
    zone->next = NULL;
    zone->domain = domain;

    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        __stats_block_attach(*blk, zone);
    }
    domain->ncounters += zone->ncounters;
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_detach(struct stats_zone* zone) {
    struct stats_domain* domain = zone->domain;
    struct stats_zone** link = &domain->zones;
    while (*link != NULL && *link != zone) {
        link = &(*link)->next;
    }

    if (*link == NULL) {
        log_panic(ENOENT, "zone %s not attached to domain %s", zone->spec.name, domain->spec.name);
    }

    domain->ncounters -= zone->ncounters;
    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        __stats_block_detach(*blk);
    }

    *link = zone->next;
    zone->next = NULL;
    zone->domain = NULL;
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_free(struct stats_zone* zone) {
    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        if (*blk != NULL) {
            __stats_block_free(*blk);
        }
    }

    free(zone);
}

//--------------------------------------------------------------------------------------------------
static struct stats_zone* __stats_zone_alloc(const struct stats_zone_spec* spec) {
    struct stats_zone* zone = calloc(1, sizeof(*zone) + spec->nblocks * sizeof(zone->blocks[0]));
    if (zone == NULL) {
        return NULL;
    }

    /*
     * Copy the zone's specification, excluding the block specification table (since it may have
     * been allocated temporarily by the caller). Each block specification is copied over below.
     */
    zone->spec = *spec;
    zone->spec.blocks = NULL;

    zone->blocks = (typeof(zone->blocks))&zone[1];
    for (unsigned int n = 0; n < spec->nblocks; ++n) {
        struct stats_block* blk = __stats_block_alloc(&spec->blocks[n]);
        if (blk == NULL) {
            goto free_zone;
        }
        zone->blocks[n] = blk;
    }

    return zone;

free_zone:
    __stats_zone_free(zone);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* stats_zone_alloc(struct stats_domain* domain,
                                    const struct stats_zone_spec* spec) {
    struct stats_zone* zone = __stats_zone_alloc(spec);
    if (zone != NULL) {
        stats_domain_lock(domain);
        __stats_zone_attach(zone, domain);
        stats_domain_unlock(domain);
    }

    return zone;
}

//--------------------------------------------------------------------------------------------------
void stats_zone_free(struct stats_zone* zone) {
    struct stats_domain* domain = zone->domain;
    stats_domain_lock(domain);
    __stats_zone_detach(zone);
    stats_domain_unlock(domain);

    __stats_zone_free(zone);
}

//--------------------------------------------------------------------------------------------------
size_t stats_zone_number_of_counters(struct stats_zone* zone) {
    stats_domain_lock(zone->domain);
    size_t ncounters = zone->ncounters;
    stats_domain_unlock(zone->domain);

    return ncounters;
}

//--------------------------------------------------------------------------------------------------
static size_t __stats_zone_get_counters(struct stats_zone* zone,
                                        uint64_t* values,
                                        size_t nvalues) {
    size_t ncounters = 0;
    for (struct stats_block** blk = zone->blocks;
         ncounters < nvalues && blk < &zone->blocks[zone->spec.nblocks];
         ++blk) {
        ncounters += __stats_block_get_counters(*blk, &values[ncounters], nvalues - ncounters);
    }

    return ncounters;
}

//--------------------------------------------------------------------------------------------------
size_t stats_zone_get_counters(struct stats_zone* zone, uint64_t* values, size_t nvalues) {
    stats_domain_lock(zone->domain);
    size_t ncounters = __stats_zone_get_counters(zone, values, nvalues);
    stats_domain_unlock(zone->domain);

    return ncounters;
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_update_counters(struct stats_zone* zone) {
    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        __stats_block_update_counters(*blk, false);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_zone_update_counters(struct stats_zone* zone) {
    stats_domain_lock(zone->domain);
    __stats_zone_update_counters(zone);
    stats_domain_unlock(zone->domain);
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_clear_counters(struct stats_zone* zone) {
    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        __stats_block_update_counters(*blk, true);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_zone_clear_counters(struct stats_zone* zone) {
    stats_domain_lock(zone->domain);
    __stats_zone_clear_counters(zone);
    stats_domain_unlock(zone->domain);
}

//--------------------------------------------------------------------------------------------------
static int __stats_zone_for_each_counter(struct stats_zone* zone,
                                         int (*callback)(const struct stats_for_each_spec* spec,
                                                         uint64_t value, void* arg),
                                         void* arg) {
    int rv = 0;
    for (struct stats_block** blk = zone->blocks;
         rv == 0 && blk < &zone->blocks[zone->spec.nblocks];
         ++blk) {
        rv = __stats_block_for_each_counter(*blk, callback, arg);
    }

    return rv;
}

//--------------------------------------------------------------------------------------------------
int stats_zone_for_each_counter(struct stats_zone* zone,
                                int (*callback)(const struct stats_for_each_spec* spec,
                                                uint64_t value, void* arg),
                                void* arg) {
    stats_domain_lock(zone->domain);
    int rv = __stats_zone_for_each_counter(zone, callback, arg);
    stats_domain_unlock(zone->domain);

    return rv;
}

//--------------------------------------------------------------------------------------------------
static void __stats_domain_free(struct stats_domain* domain) {
    while (domain->zones != NULL) {
        struct stats_zone* zone = domain->zones;
        __stats_zone_detach(zone);
        __stats_zone_free(zone);
    }

    if (domain->thread.lock != NULL) {
        int rv = pthread_mutex_destroy(&domain->thread._mutex);
        if (rv != 0) {
            log_panic(rv, "pthread_mutex_destroy failed");
        }
    }

    free(domain);
}

//--------------------------------------------------------------------------------------------------
void stats_domain_free(struct stats_domain* domain) {
    stats_domain_stop(domain);
    __stats_domain_free(domain);
}

//--------------------------------------------------------------------------------------------------
struct stats_domain* stats_domain_alloc(const struct stats_domain_spec* spec) {
    struct stats_domain* domain = calloc(1, sizeof(*domain));
    if (domain == NULL) {
        return NULL;
    }
    domain->spec = *spec;

    int rv = pthread_mutex_init(&domain->thread._mutex, NULL);
    if (rv != 0) {
        log_err(rv, "pthread_mutex_init failed");
        goto free_domain;
    }
    domain->thread.lock = &domain->thread._mutex;

    return domain;

free_domain:
    __stats_domain_free(domain);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
size_t stats_domain_number_of_counters(struct stats_domain* domain) {
    stats_domain_lock(domain);
    size_t ncounters = domain->ncounters;
    stats_domain_unlock(domain);

    return ncounters;
}

//--------------------------------------------------------------------------------------------------
static size_t __stats_domain_get_counters(struct stats_domain* domain,
                                          uint64_t* values,
                                          size_t nvalues) {
    size_t ncounters = 0;
    for (struct stats_zone* zone = domain->zones;
         ncounters < nvalues && zone != NULL;
         zone = zone->next) {
        ncounters += __stats_zone_get_counters(zone, &values[ncounters], nvalues - ncounters);
    }

    return ncounters;
}

//--------------------------------------------------------------------------------------------------
size_t stats_domain_get_counters(struct stats_domain* domain, uint64_t* values, size_t nvalues) {
    stats_domain_lock(domain);
    size_t ncounters = __stats_domain_get_counters(domain, values, nvalues);
    stats_domain_unlock(domain);

    return ncounters;
}

//--------------------------------------------------------------------------------------------------
static void __stats_domain_update_counters(struct stats_domain* domain) {
    for (struct stats_zone* zone = domain->zones; zone != NULL; zone = zone->next) {
        __stats_zone_update_counters(zone);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_domain_update_counters(struct stats_domain* domain) {
    stats_domain_lock(domain);
    __stats_domain_update_counters(domain);
    stats_domain_unlock(domain);
}

//--------------------------------------------------------------------------------------------------
static void __stats_domain_clear_counters(struct stats_domain* domain) {
    for (struct stats_zone* zone = domain->zones; zone != NULL; zone = zone->next) {
        __stats_zone_clear_counters(zone);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_domain_clear_counters(struct stats_domain* domain) {
    stats_domain_lock(domain);
    __stats_domain_clear_counters(domain);
    stats_domain_unlock(domain);
}

//--------------------------------------------------------------------------------------------------
static void* stats_domain_thread(void* arg) {
    struct stats_domain* domain = arg;

    while (1) {
        stats_domain_lock(domain);
        if (!domain->thread.running) {
            stats_domain_unlock(domain);
            break;
        }

        __stats_domain_update_counters(domain);
        stats_domain_unlock(domain);

        usleep(domain->spec.thread.interval_ms * 1000);
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
void stats_domain_start(struct stats_domain* domain) {
    stats_domain_lock(domain);
    bool running = domain->thread.running;
    domain->thread.running = true;
    stats_domain_unlock(domain);

    if (!running) {
        int rv = pthread_create(&domain->thread.handle, NULL, stats_domain_thread, domain);
        if (rv != 0) {
            log_panic(rv, "pthread_create failed for domain %s", domain->spec.name);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void stats_domain_stop(struct stats_domain* domain) {
    stats_domain_lock(domain);
    bool running = domain->thread.running;
    domain->thread.running = false;
    stats_domain_unlock(domain);

    if (running) {
        /*
         * Since a synchronization semaphore is not being used to wake up the thread, this join
         * could, at worst case, end up waiting update-time + sleep-time before the thread notices
         * and properly terminates itself.
         */
        int rv = pthread_join(domain->thread.handle, NULL);
        if (rv != 0) {
            log_panic(rv, "pthread_join failed for domain %s", domain->spec.name);
        }
    }
}

//--------------------------------------------------------------------------------------------------
static int __stats_domain_for_each_counter(struct stats_domain* domain,
                                           int (*callback)(const struct stats_for_each_spec* spec,
                                                           uint64_t value, void* arg),
                                           void* arg) {
    int rv = 0;
    for (struct stats_zone* zone = domain->zones; rv == 0 && zone != NULL; zone = zone->next) {
        rv = __stats_zone_for_each_counter(zone, callback, arg);
    }

    return rv;
}

//--------------------------------------------------------------------------------------------------
int stats_domain_for_each_counter(struct stats_domain* domain,
                                  int (*callback)(const struct stats_for_each_spec* spec,
                                                  uint64_t value, void* arg),
                                  void* arg) {
    stats_domain_lock(domain);
    int rv = __stats_domain_for_each_counter(domain, callback, arg);
    stats_domain_unlock(domain);

    return rv;
}
