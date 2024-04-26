#include "qdma.h"

#include "array_size.h"
#include "cmac_adapter_block.h"
#include "memory-barriers.h"
#include "qdma_function_block.h"
#include <stdbool.h>
#include <stddef.h>

//--------------------------------------------------------------------------------------------------
bool qdma_get_queues(volatile struct qdma_function_block* qdma,
                     unsigned int* base_queue, unsigned int* num_queues) {
    union qdma_function_qconf qconf = {._v = qdma->qconf._v};

    *base_queue = qconf.qbase;
    *num_queues = qconf.numq;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool qdma_set_queues(volatile struct qdma_function_block* qdma,
                     unsigned int base_queue, unsigned int num_queues) {
#define FUNCTION_QUEUES ARRAY_SIZE(qdma->indir_table)
#define MAX_QUEUES (2 * FUNCTION_QUEUES) // TODO: what is the actual total?

    if (num_queues > FUNCTION_QUEUES || base_queue + num_queues > MAX_QUEUES) {
        return false;
    }

    unsigned int spread = num_queues == 0 ? 1 : num_queues;
    for (unsigned int q = 0; q < FUNCTION_QUEUES; ++q) {
        qdma->indir_table[q] = q % spread;
    }
    barrier();

    union qdma_function_qconf qconf = {
        .qbase = base_queue,
        .numq = num_queues,
    };
    qdma->qconf._v = qconf._v;

    return true;

#undef MAX_QUEUES
#undef FUNCTION_QUEUES
}

//--------------------------------------------------------------------------------------------------
#define CMAC_ADAPTER_STATS_COUNTER(_name) \
{ \
    .name = #_name, \
    .offset = offsetof(struct cmac_adapter_block, _name##_lo), \
    .size = 2 * sizeof(uint32_t), \
}

static const struct stats_counter_spec cmac_adapter_stats_counters[] = {
    CMAC_ADAPTER_STATS_COUNTER(tx_packets),
    CMAC_ADAPTER_STATS_COUNTER(tx_bytes),
    CMAC_ADAPTER_STATS_COUNTER(tx_drop_packets),
    CMAC_ADAPTER_STATS_COUNTER(tx_drop_bytes),
    CMAC_ADAPTER_STATS_COUNTER(rx_packets),
    CMAC_ADAPTER_STATS_COUNTER(rx_bytes),
    CMAC_ADAPTER_STATS_COUNTER(rx_drop_packets),
    CMAC_ADAPTER_STATS_COUNTER(rx_drop_bytes),
    CMAC_ADAPTER_STATS_COUNTER(rx_err_packets),
    CMAC_ADAPTER_STATS_COUNTER(rx_err_bytes),
};

//--------------------------------------------------------------------------------------------------
static uint64_t cmac_adapter_stats_read_counter(const struct stats_block_spec* bspec,
                                                const struct stats_counter_spec* cspec) {
    volatile uint32_t* pair = (typeof(pair))(bspec->base + cspec->offset);
    return ((uint64_t)pair[1] << 32) | (uint64_t)pair[0];
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* qdma_stats_zone_alloc(struct stats_domain* domain,
                                         volatile struct cmac_adapter_block* adapter,
                                         const char* name) {
    struct stats_block_spec bspecs[] = {
        {
            // NOTE: Although the register block is named "cmac", it's actually used for host qdma.
            .name = "cmac_adapter",
            .base = adapter,
            .counters = cmac_adapter_stats_counters,
            .ncounters = ARRAY_SIZE(cmac_adapter_stats_counters),
            .read_counter = cmac_adapter_stats_read_counter,
        },
    };
    struct stats_zone_spec zspec = {
        .name = name,
        .blocks = bspecs,
        .nblocks = ARRAY_SIZE(bspecs),
    };
    return stats_zone_alloc(domain, &zspec);
}

//--------------------------------------------------------------------------------------------------
void qdma_stats_zone_free(struct stats_zone* zone) {
    stats_zone_free(zone);
}
