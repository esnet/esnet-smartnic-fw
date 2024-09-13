#ifndef INCLUDE_STATS_H
#define INCLUDE_STATS_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// The prometheus C client headers are not C++ friendly.
#include "prom.h"

struct stats_domain;
struct stats_domain_spec;
struct stats_zone;
struct stats_zone_spec;
struct stats_block;
struct stats_block_spec;
struct stats_metric;
struct stats_metric_spec;
struct stats_label_spec;

//--------------------------------------------------------------------------------------------------
struct stats_label_format_spec {
    const struct stats_domain_spec* domain;
    const struct stats_zone_spec* zone;
    const struct stats_block_spec* block;
    const struct stats_metric_spec* metric;
    const struct stats_label_spec* label;
    unsigned int idx;
};

struct stats_label_spec {
    const char* key;
    const char* value; // Ignored when value_alloc is provided.

    const char* (*value_alloc)(const struct stats_label_format_spec* spec);
    void (*value_free)(const char* value);
};

static inline void stats_label_value_free(const char* value) {
    free((void*)value);
}

//--------------------------------------------------------------------------------------------------
struct stats_metric_value {
    uint64_t u64;
    double f64;
};

enum stats_metric_type {
    stats_metric_type_COUNTER,
    stats_metric_type_GAUGE,
    stats_metric_type_FLAG,
};

enum stats_metric_flag {
    stats_metric_flag_CLEAR_ON_READ,
    stats_metric_flag_ARRAY,
};

#define STATS_METRIC_FLAG_MASK(_name) (1 << stats_metric_flag_##_name)
#define STATS_METRIC_FLAG_TEST(_flags, _name) (((_flags) & STATS_METRIC_FLAG_MASK(_name)) != 0)

union stats_io_data {
    void* ptr;
    uint64_t u64;
};
#define STATS_IO_DATA_NULL {.ptr = NULL}

struct stats_metric_spec {
    const char* name;
    const char* desc;

    // Defines the number of elements when the stats_metric_flag_ARRAY flag is set.
    size_t nelements;

    const struct stats_label_spec* labels;
    size_t nlabels;

    enum stats_metric_type type;
    unsigned int flags;
    uint64_t init_value;

    struct {
        uintptr_t offset;
        size_t size;
        size_t width;
        size_t shift;
        bool invert;
        union stats_io_data data;
    } io;
};

#define __STATS_METRIC_SPEC(_name, _desc, _type, _flags, _init_value) \
    .name = _name, \
    .desc = _desc, \
    .type = stats_metric_type_##_type, \
    .flags = _flags, \
    .init_value = _init_value

#define STATS_METRIC_SPEC(_name, _desc, _type, _flags, _init_value) \
{ \
    __STATS_METRIC_SPEC(_name, _desc, _type, _flags, _init_value) \
}

#define STATS_METRIC_SPEC_IO(_name, _desc, _type, _flags, _init_value, \
                             _io_type, _io_field, _io_width, _io_shift, _io_invert, _io_data) \
{ \
    __STATS_METRIC_SPEC(_name, _desc, _type, _flags, _init_value), \
    .io = { \
        .offset = offsetof(_io_type, _io_field), \
        .size = sizeof(((_io_type*)NULL)->_io_field), \
        .width = _io_width, \
        .shift = _io_shift, \
        .invert = _io_invert, \
        .data = _io_data, \
    }, \
}

//--------------------------------------------------------------------------------------------------
struct stats_block_spec {
    const char* name;

    size_t nmetrics;
    const struct stats_metric_spec* metrics; /* Used to pass metric specifications during block
                                              * allocation. Not referenced afterwards. */
    struct {
        volatile void* base;
        union stats_io_data data;
    } io;

    struct {
        size_t data_size;
    } latch;

    /*
     * attach_metrics: Called for the driver to take ownership of all metrics in the block.
     * detach_metrics: Called for the driver to release ownership of all metrics in the block.
     * latch_metrics: Called prior to reading the current value of all metrics in the block. Is
     *                passed a stack buffer of latch.data_size for internal use during an update
     *                operation. The buffer is zeroed prior to use, is only valid until the
     *                release_metrics method is invoked and is also passed to the read_metric and
     *                convert_metric methods.
     * release_metrics: Called after reading all metrics in the block.
     * read_metric: Called to read the current value of a single metric in the block. When the
     *              metric is an array, the "values" parameter is an array sized to the "nelements"
     *              member of the struct stats_metric_spec. Otherwise, the value is singular.
     * convert_metric: Called to convert a raw register value to a floating point representation.
     */
    void (*attach_metrics)(const struct stats_block_spec* bspec);
    void (*detach_metrics)(const struct stats_block_spec* bspec);
    void (*latch_metrics)(const struct stats_block_spec* bspec, void* data);
    void (*release_metrics)(const struct stats_block_spec* bspec, void* data);
    void (*read_metric)(const struct stats_block_spec* bspec,
                        const struct stats_metric_spec* mspec,
                        uint64_t* values, void* data);
    double (*convert_metric)(const struct stats_block_spec* bspec,
                             const struct stats_metric_spec* mspec,
                             uint64_t value, void* data);
};

//--------------------------------------------------------------------------------------------------
struct stats_zone_spec {
    const char* name;
    const struct stats_block_spec* blocks;
    size_t nblocks;
};

//--------------------------------------------------------------------------------------------------
struct stats_domain_spec {
    const char* name;
    struct {
        unsigned int interval_ms;
    } thread;

    struct {
        prom_collector_registry_t* registry;
    } prometheus;
};

//--------------------------------------------------------------------------------------------------
struct stats_for_each_spec {
    const struct stats_domain_spec* domain;
    const struct stats_zone_spec* zone;
    const struct stats_block_spec* block;
    const struct stats_metric_spec* metric;
    const struct stats_metric_value* values;
    size_t nvalues;
    struct timespec last_update;
    void* arg;
};

//--------------------------------------------------------------------------------------------------
struct stats_zone* stats_zone_alloc(struct stats_domain* domain,
                                    const struct stats_zone_spec* spec);
void stats_zone_free(struct stats_zone* zone);
void stats_zone_enable(struct stats_zone* zone);
void stats_zone_disable(struct stats_zone* zone);
size_t stats_zone_number_of_values(struct stats_zone* zone);
size_t stats_zone_get_values(struct stats_zone* zone,
                             struct stats_metric_value* values, size_t nvalues);
void stats_zone_update_metrics(struct stats_zone* zone);
void stats_zone_clear_metrics(struct stats_zone* zone);
int stats_zone_for_each_metric(struct stats_zone* zone,
                               int (*callback)(const struct stats_for_each_spec* spec),
                               void* arg);

//--------------------------------------------------------------------------------------------------
struct stats_domain* stats_domain_alloc(const struct stats_domain_spec* spec);
void stats_domain_free(struct stats_domain* domain);
void stats_domain_start(struct stats_domain* domain);
void stats_domain_stop(struct stats_domain* domain);
size_t stats_domain_number_of_values(struct stats_domain* domain);
size_t stats_domain_get_values(struct stats_domain* domain,
                               struct stats_metric_value* values, size_t nvalues);
void stats_domain_update_metrics(struct stats_domain* domain);
void stats_domain_clear_metrics(struct stats_domain* domain);
int stats_domain_for_each_metric(struct stats_domain* domain,
                                 int (*callback)(const struct stats_for_each_spec* spec),
                                 void* arg);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_STATS_H
