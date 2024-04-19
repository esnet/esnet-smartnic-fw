#ifndef INCLUDE_STATS_H
#define INCLUDE_STATS_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct stats_domain;
struct stats_zone;
struct stats_block;
struct stats_counter;

//--------------------------------------------------------------------------------------------------
enum stats_counter_flag {
    stats_counter_flag_CLEAR_ON_READ,
};

#define STATS_COUNTER_FLAG_MASK(_name) (1 << stats_counter_flag_##_name)
#define STATS_COUNTER_FLAG_TEST(_flags, _name) (((_flags) & STATS_COUNTER_FLAG_MASK(_name)) != 0)

struct stats_counter_spec {
    const char* name;
    const char* desc;
    uintptr_t offset;
    size_t size;
    unsigned int flags;
    void* data;
};

#define STATS_COUNTER_SPEC(_type, _field, _name, _desc, _flags, _data) \
{ \
    .name = _name, \
    .desc = _desc, \
    .offset = offsetof(_type, _field), \
    .size = sizeof(((_type*)NULL)->_field), \
    .flags = _flags, \
    .data = _data, \
}

//--------------------------------------------------------------------------------------------------
struct stats_block_spec {
    const char* name;
    volatile void* base;

    const struct stats_counter_spec* counters; /* Used to pass counter specifications during block
                                                * allocation. Not referenced afterwards. */
    size_t ncounters;

    /*
     * attach_counters: Called for the driver to take ownership of all counters in the block.
     * detach_counters: Called for the driver to release ownership of all counters in the block.
     * latch_counters: Called prior to reading the current value of all counters in the block.
     * release_counters: Called after reading all counters in the block.
     * read_counter: Called to read the current value of a single counter in the block.
     */
    void* data;
    void (*attach_counters)(const struct stats_block_spec* bspec);
    void (*detach_counters)(const struct stats_block_spec* bspec);
    void (*latch_counters)(const struct stats_block_spec* bspec);
    void (*release_counters)(const struct stats_block_spec* bspec);
    uint64_t (*read_counter)(const struct stats_block_spec* bspec,
                             const struct stats_counter_spec* cspec);
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
};

//--------------------------------------------------------------------------------------------------
struct stats_for_each_spec {
    const struct stats_domain_spec* domain;
    const struct stats_zone_spec* zone;
    const struct stats_block_spec* block;
    const struct stats_counter_spec* counter;
};

//--------------------------------------------------------------------------------------------------
struct stats_zone* stats_zone_alloc(struct stats_domain* domain,
                                    const struct stats_zone_spec* spec);
void stats_zone_free(struct stats_zone* zone);
size_t stats_zone_number_of_counters(struct stats_zone* zone);
size_t stats_zone_get_counters(struct stats_zone* zone, uint64_t* values, size_t nvalues);
void stats_zone_update_counters(struct stats_zone* zone);
void stats_zone_clear_counters(struct stats_zone* zone);
int stats_zone_for_each_counter(struct stats_zone* zone,
                                int (*callback)(const struct stats_for_each_spec* spec,
                                                uint64_t value, void* arg), void* arg);

//--------------------------------------------------------------------------------------------------
struct stats_domain* stats_domain_alloc(const struct stats_domain_spec* spec);
void stats_domain_free(struct stats_domain* domain);
void stats_domain_start(struct stats_domain* domain);
void stats_domain_stop(struct stats_domain* domain);
size_t stats_domain_number_of_counters(struct stats_domain* domain);
size_t stats_domain_get_counters(struct stats_domain* domain, uint64_t* values, size_t nvalues);
void stats_domain_update_counters(struct stats_domain* domain);
void stats_domain_clear_counters(struct stats_domain* domain);
int stats_domain_for_each_counter(struct stats_domain* domain,
                                  int (*callback)(const struct stats_for_each_spec* spec,
                                                  uint64_t value, void* arg), void* arg);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_STATS_H
