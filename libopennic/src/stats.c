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
struct stats_metric {
    struct stats_metric_spec spec;
    struct stats_block* block;

    struct stats_metric_value value;
    uint64_t last;
    uint64_t mask;

    struct {
        prom_metric_t* metric;
    } prometheus;
};

//--------------------------------------------------------------------------------------------------
struct stats_block {
    struct stats_block_spec spec;
    struct stats_zone* zone;

    struct stats_metric** metrics;

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
    size_t nmetrics;
};

//--------------------------------------------------------------------------------------------------
struct stats_domain {
    struct stats_domain_spec spec;

    struct stats_zone* zones;
    size_t nmetrics;

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
static uint64_t __stats_metric_read(struct stats_metric* metric) {
    const struct stats_metric_spec* spec = &metric->spec;
    volatile void* addr = metric->block->spec.io.base + spec->io.offset;
    uint64_t value = 0;

    switch (spec->io.size) {
#define METRIC_READ(_width, _addr) (uint64_t)(*((volatile uint##_width##_t*)addr))
    case 1: value = METRIC_READ(8, addr); break;
    case 2: value = METRIC_READ(16, addr); break;
    case 4: value = METRIC_READ(32, addr); break;
    case 8: value = METRIC_READ(64, addr); break;
#undef METRIC_READ

    default:
        log_panic(EINVAL, "Invalid size %zu bytes for metric %s at offset 0x%" PRIxPTR,
                  spec->io.size, spec->name, spec->io.offset);
        break;
    }

    if (spec->io.invert) {
        value = ~value;
    }

    if (spec->io.width > 0) {
        value >>= spec->io.shift;
        value &= metric->mask;
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
static void __stats_metric_attach(struct stats_metric* metric, struct stats_block* blk) {
    metric->block = blk;

    int rv = prom_collector_add_metric(blk->prometheus.collector, metric->prometheus.metric);
    if (rv != 0) {
        log_panic(rv, "prom_collector_add_metric failed for metric %s", metric->spec.name);
    }
}

//--------------------------------------------------------------------------------------------------
static void __stats_metric_detach(struct stats_metric* metric) {
    int rv = prom_collector_remove_metric(metric->block->prometheus.collector,
                                          metric->prometheus.metric);
    if (rv != 0) {
        log_panic(rv, "prom_collector_remove_metric failed for metric %s", metric->spec.name);
    }
    metric->prometheus.metric = NULL; // Automatic free when metric is removed.

    metric->block = NULL;
}

//--------------------------------------------------------------------------------------------------
static void __stats_metric_free(struct stats_metric* metric) {
    if (metric->prometheus.metric != NULL) {
        int rv = prom_gauge_destroy(metric->prometheus.metric);
        if (rv != 0) {
            log_panic(rv, "prom_gauge_destroy failed for metric %s", metric->spec.name);
        }
    }

    free(metric);
}

//--------------------------------------------------------------------------------------------------
static struct stats_metric* __stats_metric_alloc(const struct stats_metric_spec* spec) {
    struct stats_metric* metric = calloc(1, sizeof(*metric));
    if (metric == NULL) {
        return NULL;
    }
    metric->spec = *spec;
    metric->value.u64 = spec->init_value;
    metric->last = spec->init_value;

    if (spec->io.width > 0) {
        metric->mask = (1 << spec->io.width) - 1;
    }

    const char* labels[] = {
        "domain",
        "zone",
        "block",
    };
    metric->prometheus.metric = prom_gauge_new(spec->name, spec->desc, ARRAY_SIZE(labels), labels);
    if (metric->prometheus.metric == NULL) {
        log_err(ENOMEM, "prom_gauge_new failed for metric %s", spec->name);
        goto free_metric;
    }

    return metric;

free_metric:
    __stats_metric_free(metric);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_attach(struct stats_block* blk, struct stats_zone* zone) {
    blk->zone = zone;

    const struct stats_block_spec* spec = &blk->spec;
    for (struct stats_metric** m = blk->metrics; m < &blk->metrics[spec->nmetrics]; ++m) {
        __stats_metric_attach(*m, blk);
    }
    zone->nmetrics += spec->nmetrics;

    int rv = prom_collector_registry_register_collector(
        zone->domain->spec.prometheus.registry, blk->prometheus.collector);
    if (rv != 0) {
        log_panic(rv, "prom_collector_registry_register_collector failed for block %s", spec->name);
    }

    if (spec->attach_metrics != NULL) {
        spec->attach_metrics(spec);
    }
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_detach(struct stats_block* blk) {
    const struct stats_block_spec* spec = &blk->spec;
    if (spec->detach_metrics != NULL) {
        spec->detach_metrics(spec);
    }

    blk->zone->nmetrics -= spec->nmetrics;
    for (struct stats_metric** m = blk->metrics; m < &blk->metrics[spec->nmetrics]; ++m) {
        __stats_metric_detach(*m);
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
    for (struct stats_metric** m = blk->metrics; m < &blk->metrics[spec->nmetrics]; ++m) {
        if (*m != NULL) {
            __stats_metric_free(*m);
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
    struct stats_block* blk = calloc(1, sizeof(*blk) + spec->nmetrics * sizeof(blk->metrics[0]));
    if (blk == NULL) {
        return NULL;
    }

#define COLLECTOR_NAME_FMT "block_%s_0x%" PRIxPTR "_0x%" PRIu64
#define COLLECTOR_NAME_ARGS spec->name, (uintptr_t)spec->io.base, spec->io.data.u64
    int len = snprintf(NULL, 0, COLLECTOR_NAME_FMT, COLLECTOR_NAME_ARGS);
    char name[len + 1];
    snprintf(name, len + 1, COLLECTOR_NAME_FMT, COLLECTOR_NAME_ARGS);

    blk->prometheus.collector = prom_collector_new(name);
    if (blk->prometheus.collector == NULL) {
        log_err(ENOMEM, "prom_collector_new failed for %s", name);
        goto free_block;
    }
#undef COLLECTOR_NAME_FMT
#undef COLLECTOR_NAME_ARGS

    /*
     * Copy the block's specification, excluding the metric specification table (since it may have
     * been allocated temporarily by the caller). Each metric specification is copied over below.
     */
    blk->spec = *spec;
    blk->spec.metrics = NULL;

    blk->metrics = (typeof(blk->metrics))&blk[1];
    for (unsigned int n = 0; n < spec->nmetrics; ++n) {
        struct stats_metric* metric = __stats_metric_alloc(&spec->metrics[n]);
        if (metric == NULL) {
            goto free_block;
        }
        blk->metrics[n] = metric;
    }

    return blk;

free_block:
    __stats_block_free(blk);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
static size_t __stats_block_get_metrics(struct stats_block* blk,
                                        struct stats_metric_value* values,
                                        size_t nvalues) {
    const struct stats_block_spec* spec = &blk->spec;
    if (nvalues > spec->nmetrics) {
        nvalues = spec->nmetrics;
    }

    for (struct stats_metric** m = blk->metrics; m < &blk->metrics[nvalues]; ++m, ++values) {
        *values = (*m)->value;
    }

    return nvalues;
}

//--------------------------------------------------------------------------------------------------
static void __stats_block_update_metrics(struct stats_block* blk, bool clear) {
    const struct stats_block_spec* spec = &blk->spec;

    uint64_t* data = NULL;
    size_t nwords = (spec->latch.data_size + sizeof(*data) - 1) / sizeof(*data);
    typeof(*data) words[nwords];
    if (nwords > 0) {
        memset(words, 0, nwords * sizeof(*data));
        data = words;
    }

    if (spec->latch_metrics != NULL) {
        spec->latch_metrics(spec, data);
    }

    // Must match the ordering and number from __stats_metric_alloc.
    const char* labels[] = {
        blk->zone->domain->spec.name,
        blk->zone->spec.name,
        spec->name,
    };

    for (struct stats_metric** m = blk->metrics; m < &blk->metrics[spec->nmetrics]; ++m) {
        struct stats_metric* metric = *m;

        uint64_t current;
        if (spec->read_metric != NULL) {
            current = spec->read_metric(spec, &metric->spec, data);
        } else {
            current = __stats_metric_read(metric);
        }

        if (clear) {
            metric->value.u64 = metric->spec.init_value;
            metric->last = metric->spec.init_value;
        } else {
            switch (metric->spec.type) {
            case stats_metric_type_COUNTER: {
                uint64_t diff = current;
                if (!STATS_METRIC_FLAG_TEST(metric->spec.flags, CLEAR_ON_READ)) {
                    diff -= metric->last;
                    metric->last = current;
                }

                metric->value.u64 += diff;
                break;
            }

            case stats_metric_type_FLAG:
                metric->value.u64 = current ? 1 : 0;
                break;

            case stats_metric_type_GAUGE:
            default:
                metric->value.u64 = current;
                break;
            }
        }

        if (spec->convert_metric != NULL) {
            metric->value.f64 = spec->convert_metric(spec, &metric->spec, metric->value.u64, data);
        } else {
            metric->value.f64 = (double)metric->value.u64;
        }
        prom_gauge_set(metric->prometheus.metric, metric->value.f64, labels);
    }

    if (spec->release_metrics != NULL) {
        spec->release_metrics(spec, data);
    }
}

//--------------------------------------------------------------------------------------------------
static int __stats_block_for_each_metric(struct stats_block* blk,
                                         int (*callback)(const struct stats_for_each_spec* spec),
                                         void* arg) {
    struct stats_for_each_spec spec = {
        .domain = &blk->zone->domain->spec,
        .zone = &blk->zone->spec,
        .block = &blk->spec,
        .arg = arg,
    };

    int rv = 0;
    for (struct stats_metric** m = blk->metrics;
         rv == 0 && m < &blk->metrics[blk->spec.nmetrics];
         ++m) {
        spec.metric = &(*m)->spec;
        spec.value = (*m)->value;
        rv = callback(&spec);
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
    domain->nmetrics += zone->nmetrics;
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

    domain->nmetrics -= zone->nmetrics;
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
size_t stats_zone_number_of_metrics(struct stats_zone* zone) {
    stats_domain_lock(zone->domain);
    size_t nmetrics = zone->nmetrics;
    stats_domain_unlock(zone->domain);

    return nmetrics;
}

//--------------------------------------------------------------------------------------------------
static size_t __stats_zone_get_metrics(struct stats_zone* zone,
                                       struct stats_metric_value* values,
                                       size_t nvalues) {
    size_t nmetrics = 0;
    for (struct stats_block** blk = zone->blocks;
         nmetrics < nvalues && blk < &zone->blocks[zone->spec.nblocks];
         ++blk) {
        nmetrics += __stats_block_get_metrics(*blk, &values[nmetrics], nvalues - nmetrics);
    }

    return nmetrics;
}

//--------------------------------------------------------------------------------------------------
size_t stats_zone_get_metrics(struct stats_zone* zone,
                              struct stats_metric_value* values,
                              size_t nvalues) {
    stats_domain_lock(zone->domain);
    size_t nmetrics = __stats_zone_get_metrics(zone, values, nvalues);
    stats_domain_unlock(zone->domain);

    return nmetrics;
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_update_metrics(struct stats_zone* zone) {
    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        __stats_block_update_metrics(*blk, false);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_zone_update_metrics(struct stats_zone* zone) {
    stats_domain_lock(zone->domain);
    __stats_zone_update_metrics(zone);
    stats_domain_unlock(zone->domain);
}

//--------------------------------------------------------------------------------------------------
static void __stats_zone_clear_metrics(struct stats_zone* zone) {
    for (struct stats_block** blk = zone->blocks; blk < &zone->blocks[zone->spec.nblocks]; ++blk) {
        __stats_block_update_metrics(*blk, true);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_zone_clear_metrics(struct stats_zone* zone) {
    stats_domain_lock(zone->domain);
    __stats_zone_clear_metrics(zone);
    stats_domain_unlock(zone->domain);
}

//--------------------------------------------------------------------------------------------------
static int __stats_zone_for_each_metric(struct stats_zone* zone,
                                        int (*callback)(const struct stats_for_each_spec* spec),
                                        void* arg) {
    int rv = 0;
    for (struct stats_block** blk = zone->blocks;
         rv == 0 && blk < &zone->blocks[zone->spec.nblocks];
         ++blk) {
        rv = __stats_block_for_each_metric(*blk, callback, arg);
    }

    return rv;
}

//--------------------------------------------------------------------------------------------------
int stats_zone_for_each_metric(struct stats_zone* zone,
                               int (*callback)(const struct stats_for_each_spec* spec),
                               void* arg) {
    stats_domain_lock(zone->domain);
    int rv = __stats_zone_for_each_metric(zone, callback, arg);
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

    if (spec->thread.interval_ms == 0) {
        domain->spec.thread.interval_ms = 1000;
    }

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
size_t stats_domain_number_of_metrics(struct stats_domain* domain) {
    stats_domain_lock(domain);
    size_t nmetrics = domain->nmetrics;
    stats_domain_unlock(domain);

    return nmetrics;
}

//--------------------------------------------------------------------------------------------------
static size_t __stats_domain_get_metrics(struct stats_domain* domain,
                                         struct stats_metric_value* values,
                                         size_t nvalues) {
    size_t nmetrics = 0;
    for (struct stats_zone* zone = domain->zones;
         nmetrics < nvalues && zone != NULL;
         zone = zone->next) {
        nmetrics += __stats_zone_get_metrics(zone, &values[nmetrics], nvalues - nmetrics);
    }

    return nmetrics;
}

//--------------------------------------------------------------------------------------------------
size_t stats_domain_get_metrics(struct stats_domain* domain,
                                struct stats_metric_value* values,
                                size_t nvalues) {
    stats_domain_lock(domain);
    size_t nmetrics = __stats_domain_get_metrics(domain, values, nvalues);
    stats_domain_unlock(domain);

    return nmetrics;
}

//--------------------------------------------------------------------------------------------------
static void __stats_domain_update_metrics(struct stats_domain* domain) {
    for (struct stats_zone* zone = domain->zones; zone != NULL; zone = zone->next) {
        __stats_zone_update_metrics(zone);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_domain_update_metrics(struct stats_domain* domain) {
    stats_domain_lock(domain);
    __stats_domain_update_metrics(domain);
    stats_domain_unlock(domain);
}

//--------------------------------------------------------------------------------------------------
static void __stats_domain_clear_metrics(struct stats_domain* domain) {
    for (struct stats_zone* zone = domain->zones; zone != NULL; zone = zone->next) {
        __stats_zone_clear_metrics(zone);
    }
}

//--------------------------------------------------------------------------------------------------
void stats_domain_clear_metrics(struct stats_domain* domain) {
    stats_domain_lock(domain);
    __stats_domain_clear_metrics(domain);
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

        __stats_domain_update_metrics(domain);
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
static int __stats_domain_for_each_metric(struct stats_domain* domain,
                                          int (*callback)(const struct stats_for_each_spec* spec),
                                          void* arg) {
    int rv = 0;
    for (struct stats_zone* zone = domain->zones; rv == 0 && zone != NULL; zone = zone->next) {
        rv = __stats_zone_for_each_metric(zone, callback, arg);
    }

    return rv;
}

//--------------------------------------------------------------------------------------------------
int stats_domain_for_each_metric(struct stats_domain* domain,
                                 int (*callback)(const struct stats_for_each_spec* spec),
                                 void* arg) {
    stats_domain_lock(domain);
    int rv = __stats_domain_for_each_metric(domain, callback, arg);
    stats_domain_unlock(domain);

    return rv;
}
