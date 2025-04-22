#include "qdma.h"

#include "array_size.h"
#include "cmac_adapter_block.h"
#include "memory-barriers.h"
#include <stdbool.h>
#include <stddef.h>
#include "unused.h"

//--------------------------------------------------------------------------------------------------
bool qdma_channel_get_queues(volatile struct esnet_smartnic_bar2* bar2, unsigned int channel,
                             unsigned int* base_queue, unsigned int* num_queues) {
    volatile struct qdma_function_block* qdma;
    switch (channel) {
#define CASE(_chan) case _chan: qdma = &bar2->qdma_func##_chan; break

    CASE(0);
    CASE(1);
    default:
        return false;

#undef CASE
    }

    union qdma_function_qconf qconf = {._v = qdma->qconf._v};
    *base_queue = qconf.qbase;
    *num_queues = qconf.numq;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool qdma_channel_set_queues(volatile struct esnet_smartnic_bar2* bar2, unsigned int channel,
                             unsigned int base_queue, unsigned int num_queues) {
    volatile struct qdma_function_block* qdma;
    switch (channel) {
#define CASE(_chan) case _chan: qdma = &bar2->qdma_func##_chan; break

    CASE(0);
    CASE(1);
    default:
        return false;

#undef CASE
    }

    union qdma_function_qconf qconf = {
        .qbase = base_queue,
        .numq = num_queues,
    };
    qdma->qconf._v = qconf._v;
    barrier();

    return true;
}

//--------------------------------------------------------------------------------------------------
bool qdma_function_get_queues(volatile struct esnet_smartnic_bar2* bar2,
                              unsigned int channel, enum qdma_function func,
                              unsigned int* base_queue, unsigned int* num_queues) {
    if (channel >= QDMA_CHANNEL_MAX || func >= qdma_function_MAX) {
        return false;
    }

    volatile struct smartnic_block* sn = &bar2->smartnic_regs;
    switch (channel) {
#define CASE(_chan) \
    case _chan: { \
        union smartnic_igr_q_config_##_chan qconf = {._v = sn->igr_q_config_##_chan[func]._v}; \
        *base_queue = qconf.base; \
        *num_queues = qconf.num_q; \
        break; \
    }

    CASE(0);
    CASE(1);
    default:
        return false;

#undef CASE
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool qdma_function_set_queues(volatile struct esnet_smartnic_bar2* bar2,
                              unsigned int channel, enum qdma_function func,
                              unsigned int base_queue, unsigned int num_queues) {
    if (channel >= QDMA_CHANNEL_MAX || func >= qdma_function_MAX) {
        return false;
    }

    volatile struct smartnic_block* sn = &bar2->smartnic_regs;
    unsigned int spread = num_queues > 0 ? num_queues : 1;
    switch (channel) {
#define CASE_H2Q(_func, _name) \
    case qdma_function_##_func: { \
        if (num_queues > ARRAY_SIZE(h2q->_name##_table)) { \
            return false; \
        }\
        for (unsigned int q = 0; q < ARRAY_SIZE(h2q->_name##_table); ++q) { \
            union smartnic_hash2qid_##_name##_table table = {.value = q % spread}; \
            h2q->_name##_table[q]._v = table._v; \
        } \
        break; \
    }
#define CASE(_chan) \
    case _chan: { \
        volatile struct smartnic_hash2qid_block* h2q = &bar2->smartnic_hash2qid_##_chan; \
        union smartnic_hash2qid_q_config q_config = {.base = base_queue}; \
        h2q->q_config[func]._v = q_config._v; \
        barrier(); \
        \
        switch (func) { \
        CASE_H2Q(PF, pf); \
        CASE_H2Q(VF0, vf0); \
        CASE_H2Q(VF1, vf1); \
        CASE_H2Q(VF2, vf2); \
        default: return false; \
        } \
        barrier(); \
        \
        union smartnic_igr_q_config_##_chan qconf = { \
            .base = base_queue, \
            .num_q = num_queues, \
        }; \
        sn->igr_q_config_##_chan[func]._v = qconf._v; \
        break; \
    }

    CASE(0);
    CASE(1);
    default:
        return false;

#undef CASE
#undef CASE_H2Q
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
#define CMAC_ADAPTER_STATS_COUNTER_NLABELS 1
#define _CMAC_ADAPTER_STATS_COUNTER(_name, _units) \
{ \
    __STATS_METRIC_SPEC(#_name, NULL, COUNTER, 0, 0), \
    .io = { \
        .offset = offsetof(struct cmac_adapter_block, _name##_lo), \
        .size = 2 * sizeof(uint32_t), \
    }, \
    .nlabels = CMAC_ADAPTER_STATS_COUNTER_NLABELS, \
    .labels = (const struct stats_label_spec[CMAC_ADAPTER_STATS_COUNTER_NLABELS]){ \
        {.key = "units", .value = #_units, .flags = STATS_LABEL_FLAG_MASK(NO_EXPORT)}, \
    }, \
}
#define CMAC_ADAPTER_STATS_COUNTER(_prefix, _units) \
    _CMAC_ADAPTER_STATS_COUNTER(_prefix##_##_units, _units)

static const struct stats_metric_spec cmac_adapter_stats_metrics[] = {
    CMAC_ADAPTER_STATS_COUNTER(tx, packets),
    CMAC_ADAPTER_STATS_COUNTER(tx, bytes),
    CMAC_ADAPTER_STATS_COUNTER(tx_drop, packets),
    CMAC_ADAPTER_STATS_COUNTER(tx_drop, bytes),
    CMAC_ADAPTER_STATS_COUNTER(rx, packets),
    CMAC_ADAPTER_STATS_COUNTER(rx, bytes),
    CMAC_ADAPTER_STATS_COUNTER(rx_drop, packets),
    CMAC_ADAPTER_STATS_COUNTER(rx_drop, bytes),
    CMAC_ADAPTER_STATS_COUNTER(rx_err, packets),
    CMAC_ADAPTER_STATS_COUNTER(rx_err, bytes),
};

//--------------------------------------------------------------------------------------------------
static void cmac_adapter_stats_read_metric(const struct stats_block_spec* bspec,
                                           const struct stats_metric_spec* mspec,
                                           uint64_t* value,
                                           void* UNUSED(data)) {
    volatile uint32_t* pair = (typeof(pair))(bspec->io.base + mspec->io.offset);
    *value = ((uint64_t)pair[1] << 32) | (uint64_t)pair[0];
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* qdma_stats_zone_alloc(struct stats_domain* domain,
                                         volatile struct cmac_adapter_block* adapter,
                                         const char* name) {
    struct stats_block_spec bspecs[] = {
        {
            // NOTE: Although the register block is named "cmac", it's actually used for host qdma.
            .name = "cmac_adapter",
            .metrics = cmac_adapter_stats_metrics,
            .nmetrics = ARRAY_SIZE(cmac_adapter_stats_metrics),
            .io = {
                .base = adapter,
            },
            .read_metric = cmac_adapter_stats_read_metric,
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
