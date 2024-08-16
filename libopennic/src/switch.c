#include "switch.h"

#include "array_size.h"
#include "axi4s_probe_block.h"
#include "memory-barriers.h"
#include "smartnic.h"
#include "stats.h"
#include <stdbool.h>
#include <stdint.h>
#include "unused.h"

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_from_igr_sw_tid(struct switch_interface_id* intf,
                                                const unsigned int tid) {
    switch (tid) {
    case SMARTNIC_IGR_SW_TID_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_IGR_SW_TID_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_IGR_SW_TID_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_IGR_SW_TID_VALUE_HOST_1:
        intf->type = switch_interface_type_HOST;
        intf->index = 1;
        break;

    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_to_igr_sw_tid(const struct switch_interface_id* intf,
                                              unsigned int* tid) {
    switch(intf->type) {
    case switch_interface_type_PORT:
        switch (intf->index) {
        case 0: *tid = SMARTNIC_IGR_SW_TID_VALUE_CMAC_0; break;
        case 1: *tid = SMARTNIC_IGR_SW_TID_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tid = SMARTNIC_IGR_SW_TID_VALUE_HOST_0; break;
        case 1: *tid = SMARTNIC_IGR_SW_TID_VALUE_HOST_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_processor_id_from_igr_sw_tdest(struct switch_processor_id* proc,
                                                  const unsigned int tdest) {
    switch (tdest) {
    case SMARTNIC_IGR_SW_TDEST_VALUE_APP_BYPASS:
        proc->type = switch_processor_type_BYPASS;
        proc->index = 0;
        break;

    case SMARTNIC_IGR_SW_TDEST_VALUE_DROP:
        proc->type = switch_processor_type_DROP;
        proc->index = 0;
        break;

    case SMARTNIC_IGR_SW_TDEST_VALUE_APP_0:
        proc->type = switch_processor_type_APP;
        proc->index = 0;
        break;

    case SMARTNIC_IGR_SW_TDEST_VALUE_APP_1:
        proc->type = switch_processor_type_APP;
        proc->index = 1;
        break;

    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_processor_id_to_igr_sw_tdest(const struct switch_processor_id* proc,
                                                unsigned int* tdest) {
    switch (proc->type) {
    case switch_processor_type_BYPASS:
        switch (proc->index) {
        case 0: *tdest = SMARTNIC_IGR_SW_TDEST_VALUE_APP_BYPASS; break;
        default: return false;
        }
        break;

    case switch_processor_type_DROP:
        switch (proc->index) {
        case 0: *tdest = SMARTNIC_IGR_SW_TDEST_VALUE_DROP; break;
        default: return false;
        }
        break;

    case switch_processor_type_APP:
        switch (proc->index) {
        case 0: *tdest = SMARTNIC_IGR_SW_TDEST_VALUE_APP_0; break;
        case 1: *tdest = SMARTNIC_IGR_SW_TDEST_VALUE_APP_1; break;
        default: return false;
        }
        break;

    case switch_processor_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_from_bypass_tdest(struct switch_interface_id* intf,
                                                  const unsigned int tdest) {
    switch (tdest) {
    case SMARTNIC_BYPASS_TDEST_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_BYPASS_TDEST_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_BYPASS_TDEST_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_BYPASS_TDEST_VALUE_HOST_1:
        intf->type = switch_interface_type_HOST;
        intf->index = 1;
        break;

    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_to_bypass_tdest(const struct switch_interface_id* intf,
                                                unsigned int* tdest) {
    switch(intf->type) {
    case switch_interface_type_PORT:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_BYPASS_TDEST_VALUE_CMAC_0; break;
        case 1: *tdest = SMARTNIC_BYPASS_TDEST_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_BYPASS_TDEST_VALUE_HOST_0; break;
        case 1: *tdest = SMARTNIC_BYPASS_TDEST_VALUE_HOST_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_from_app_0_tdest(struct switch_interface_id* intf,
                                                 const unsigned int tdest) {
    switch (tdest) {
    case SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_1:
        intf->type = switch_interface_type_HOST;
        intf->index = 1;
        break;

    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_to_app_0_tdest(const struct switch_interface_id* intf,
                                               unsigned int* tdest) {
    switch(intf->type) {
    case switch_interface_type_PORT:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_0; break;
        case 1: *tdest = SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_0; break;
        case 1: *tdest = SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_from_app_1_tdest(struct switch_interface_id* intf,
                                                 const unsigned int tdest) {
    switch (tdest) {
    case SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_1:
        intf->type = switch_interface_type_HOST;
        intf->index = 1;
        break;

    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_to_app_1_tdest(const struct switch_interface_id* intf,
                                               unsigned int* tdest) {
    switch(intf->type) {
    case switch_interface_type_PORT:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_0; break;
        case 1: *tdest = SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_0; break;
        case 1: *tdest = SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_get_ingress_source(volatile struct smartnic_block* blk,
                               const struct switch_interface_id* from_intf,
                               struct switch_interface_id* to_intf) {
    unsigned int from_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tid)) {
        return false;
    }

    union smartnic_igr_sw_tid tid = {._v = blk->igr_sw_tid[from_tid]._v};
    return switch_interface_id_from_igr_sw_tid(to_intf, tid.value);
}

//--------------------------------------------------------------------------------------------------
bool switch_set_ingress_source(volatile struct smartnic_block* blk,
                               const struct switch_interface_id* from_intf,
                               const struct switch_interface_id* to_intf) {
    unsigned int from_tid;
    unsigned int to_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        !switch_interface_id_to_igr_sw_tid(to_intf, &to_tid) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tid)) {
        return false;
    }

    union smartnic_igr_sw_tid tid = {._v = blk->igr_sw_tid[from_tid]._v};
    tid.value = to_tid;
    blk->igr_sw_tid[from_tid]._v = tid._v;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_get_ingress_connection(volatile struct smartnic_block* blk,
                                   const struct switch_interface_id* from_intf,
                                   struct switch_processor_id* to_proc) {
    unsigned int from_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tdest)) {
        return false;
    }

    union smartnic_igr_sw_tdest tdest = {._v = blk->igr_sw_tdest[from_tid]._v};
    return switch_processor_id_from_igr_sw_tdest(to_proc, tdest.value);
}

//--------------------------------------------------------------------------------------------------
bool switch_set_ingress_connection(volatile struct smartnic_block* blk,
                                   const struct switch_interface_id* from_intf,
                                   const struct switch_processor_id* to_proc) {
    unsigned int from_tid;
    unsigned int to_tdest;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        !switch_processor_id_to_igr_sw_tdest(to_proc, &to_tdest) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tdest)) {
        return false;
    }

    union smartnic_igr_sw_tdest tdest = {._v = blk->igr_sw_tdest[from_tid]._v};
    tdest.value = to_tdest;
    blk->igr_sw_tdest[from_tid]._v = tdest._v;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_get_egress_connection(volatile struct smartnic_block* blk,
                                  const struct switch_processor_id* on_proc,
                                  const struct switch_interface_id* from_intf,
                                  struct switch_interface_id* to_intf) {
    unsigned int from_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid)) {
        return false;
    }

    switch (on_proc->type) {
    case switch_processor_type_BYPASS: {
        if (from_tid >= ARRAY_SIZE(blk->bypass_tdest)) {
            return false;
        }

        union smartnic_bypass_tdest tdest = {._v = blk->bypass_tdest[from_tid]._v};
        return switch_interface_id_from_bypass_tdest(to_intf, tdest.value);
    }

    case switch_processor_type_APP:
        switch (on_proc->index) {
        case 0: {
            if (from_tid >= ARRAY_SIZE(blk->app_0_tdest_remap)) {
                return false;
            }

            union smartnic_app_0_tdest_remap tdest = {
                ._v = blk->app_0_tdest_remap[from_tid]._v};
            return switch_interface_id_from_app_0_tdest(to_intf, tdest.value);
        }

        case 1: {
            if (from_tid >= ARRAY_SIZE(blk->app_1_tdest_remap)) {
                return false;
            }

            union smartnic_app_1_tdest_remap tdest = {
                ._v = blk->app_1_tdest_remap[from_tid]._v};
            return switch_interface_id_from_app_1_tdest(to_intf, tdest.value);
        }

        default:
            return false;
        }
        break;

    case switch_processor_type_DROP:
    case switch_processor_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_set_egress_connection(volatile struct smartnic_block* blk,
                                  const struct switch_processor_id* on_proc,
                                  const struct switch_interface_id* from_intf,
                                  const struct switch_interface_id* to_intf) {
    unsigned int from_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid)) {
        return false;
    }

    unsigned int to_tdest;
    switch (on_proc->type) {
    case switch_processor_type_BYPASS: {
        if (!switch_interface_id_to_bypass_tdest(to_intf, &to_tdest) ||
            from_tid >= ARRAY_SIZE(blk->bypass_tdest)) {
            return false;
        }

        union smartnic_bypass_tdest tdest = {._v = blk->bypass_tdest[from_tid]._v};
        tdest.value = to_tdest;
        blk->bypass_tdest[from_tid]._v = tdest._v;
        break;
    }

    case switch_processor_type_APP:
        switch (on_proc->index) {
        case 0: {
            if (!switch_interface_id_to_app_0_tdest(to_intf, &to_tdest) ||
                from_tid >= ARRAY_SIZE(blk->app_0_tdest_remap)) {
                return false;
            }

            union smartnic_app_0_tdest_remap tdest = {
                ._v = blk->app_0_tdest_remap[from_tid]._v};
            tdest.value = to_tdest;
            blk->app_0_tdest_remap[from_tid]._v = tdest._v;
            break;
        }

        case 1: {
            if (!switch_interface_id_to_app_1_tdest(to_intf, &to_tdest) ||
                from_tid >= ARRAY_SIZE(blk->app_1_tdest_remap)) {
                return false;
            }

            union smartnic_app_1_tdest_remap tdest = {
                ._v = blk->app_1_tdest_remap[from_tid]._v};
            tdest.value = to_tdest;
            blk->app_1_tdest_remap[from_tid]._v = tdest._v;
            break;
        }

        default:
            return false;
        }
        break;

    case switch_processor_type_UNKNOWN:
    case switch_processor_type_DROP:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void switch_set_defaults_one_to_one(volatile struct smartnic_block* blk) {
    unsigned int tid;

    // Reset all ingress connections to drop prior to changing the switch configuration.
    for (tid = 0; tid < ARRAY_SIZE(blk->igr_sw_tdest); ++tid) {
        blk->igr_sw_tdest[tid]._v = SMARTNIC_IGR_SW_TDEST_VALUE_DROP;
    }
    barrier();

    // Apply ingress source defaults.
    const union smartnic_igr_sw_tid igr_sw_tid_defaults[] = {
        {.value = SMARTNIC_IGR_SW_TID_VALUE_CMAC_0}, // CMAC 0
        {.value = SMARTNIC_IGR_SW_TID_VALUE_CMAC_1}, // CMAC 1
        {.value = SMARTNIC_IGR_SW_TID_VALUE_HOST_0}, // HOST 0
        {.value = SMARTNIC_IGR_SW_TID_VALUE_HOST_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->igr_sw_tid); ++tid) {
        blk->igr_sw_tid[tid]._v = igr_sw_tid_defaults[tid]._v;
    }

    // Apply ingress connection defaults.
    const union smartnic_igr_sw_tdest igr_sw_tdest_defaults[] = {
        {.value = SMARTNIC_IGR_SW_TDEST_VALUE_APP_0},      // CMAC 0
        {.value = SMARTNIC_IGR_SW_TDEST_VALUE_APP_0},      // CMAC 1
        {.value = SMARTNIC_IGR_SW_TDEST_VALUE_APP_BYPASS}, // HOST 0
        {.value = SMARTNIC_IGR_SW_TDEST_VALUE_APP_BYPASS}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->igr_sw_tdest); ++tid) {
        blk->igr_sw_tdest[tid]._v = igr_sw_tdest_defaults[tid]._v;
    }

    // Apply egress connection defaults to the bypass processor.
    const union smartnic_bypass_tdest bypass_tdest_defaults[] = {
        {.value = SMARTNIC_BYPASS_TDEST_VALUE_HOST_0}, // CMAC 0
        {.value = SMARTNIC_BYPASS_TDEST_VALUE_HOST_1}, // CMAC 1
        {.value = SMARTNIC_BYPASS_TDEST_VALUE_CMAC_0}, // HOST 0
        {.value = SMARTNIC_BYPASS_TDEST_VALUE_CMAC_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->bypass_tdest); ++tid) {
        blk->bypass_tdest[tid]._v = bypass_tdest_defaults[tid]._v;
    }

    // Apply egress connection defaults to the application 0 processor.
    const union smartnic_app_0_tdest_remap app_0_tdest_remap_defaults[] = {
        {.value = SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_0}, // CMAC 0
        {.value = SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_1}, // CMAC 1
        {.value = SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_0}, // HOST 0
        {.value = SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->app_0_tdest_remap); ++tid) {
        blk->app_0_tdest_remap[tid]._v = app_0_tdest_remap_defaults[tid]._v;
    }

    // Apply egress connection defaults to the application 1 processor.
    const union smartnic_app_1_tdest_remap app_1_tdest_remap_defaults[] = {
        {.value = SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_0}, // CMAC 0
        {.value = SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_1}, // CMAC 1
        {.value = SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_0}, // HOST 0
        {.value = SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->app_1_tdest_remap); ++tid) {
        blk->app_1_tdest_remap[tid]._v = app_1_tdest_remap_defaults[tid]._v;
    }

    // Set up FIFO flow control thresholds for egress switch output FIFOs
#define EGR_FC_THRESH_UNLIMITED UINT32_MAX
#define EGR_FC_THRESH_FIFO_MAX  1020
    uint32_t egr_fc_thresh_defaults[] = {
        EGR_FC_THRESH_UNLIMITED, // CMAC 0
        EGR_FC_THRESH_UNLIMITED, // CMAC 1
        EGR_FC_THRESH_FIFO_MAX,  // HOST 0
        EGR_FC_THRESH_UNLIMITED, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->egr_fc_thresh); ++tid) {
        blk->egr_fc_thresh[tid] = egr_fc_thresh_defaults[tid];
    }
}

//--------------------------------------------------------------------------------------------------
#define SWITCH_STATS_COUNTER(_name) \
{ \
    __STATS_METRIC_SPEC(#_name, NULL, COUNTER, STATS_METRIC_FLAG_MASK(CLEAR_ON_READ), 0), \
    .io = { \
        .offset = offsetof(struct axi4s_probe_block, _name##_upper), \
        .size = 2 * sizeof(uint32_t), \
    }, \
}

static const struct stats_metric_spec switch_stats_metrics[] = {
    SWITCH_STATS_COUNTER(pkt_count),
    SWITCH_STATS_COUNTER(byte_count),
};

struct switch_stats_block_info {
    const char* name;
    uintptr_t offset;
};

#define SWITCH_STATS_BLOCK_INFO(_name) \
{ \
    .name = #_name, \
    .offset = offsetof(struct esnet_smartnic_bar2, _name), \
}

static const struct switch_stats_block_info switch_stats_block_info[] = {
    SWITCH_STATS_BLOCK_INFO(probe_from_cmac_0),
    SWITCH_STATS_BLOCK_INFO(drops_ovfl_from_cmac_0),
    SWITCH_STATS_BLOCK_INFO(drops_err_from_cmac_0),
    SWITCH_STATS_BLOCK_INFO(probe_from_cmac_1),
    SWITCH_STATS_BLOCK_INFO(drops_ovfl_from_cmac_1),
    SWITCH_STATS_BLOCK_INFO(drops_err_from_cmac_1),
    SWITCH_STATS_BLOCK_INFO(probe_from_host_0),
    SWITCH_STATS_BLOCK_INFO(probe_from_host_1),
    SWITCH_STATS_BLOCK_INFO(probe_core_to_app0),
    SWITCH_STATS_BLOCK_INFO(probe_core_to_app1),
    SWITCH_STATS_BLOCK_INFO(probe_app0_to_core),
    SWITCH_STATS_BLOCK_INFO(probe_app1_to_core),
    SWITCH_STATS_BLOCK_INFO(probe_to_cmac_0),
    SWITCH_STATS_BLOCK_INFO(drops_ovfl_to_cmac_0),
    SWITCH_STATS_BLOCK_INFO(probe_to_cmac_1),
    SWITCH_STATS_BLOCK_INFO(drops_ovfl_to_cmac_1),
    SWITCH_STATS_BLOCK_INFO(probe_to_host_0),
    SWITCH_STATS_BLOCK_INFO(drops_ovfl_to_host_0),
    SWITCH_STATS_BLOCK_INFO(probe_to_host_1),
    SWITCH_STATS_BLOCK_INFO(drops_ovfl_to_host_1),
    SWITCH_STATS_BLOCK_INFO(probe_to_bypass),
    SWITCH_STATS_BLOCK_INFO(drops_from_igr_sw),
    SWITCH_STATS_BLOCK_INFO(drops_from_bypass),
};

//--------------------------------------------------------------------------------------------------
static void switch_stats_latch_metrics(const struct stats_block_spec* bspec, void* UNUSED(data)) {
    volatile struct axi4s_probe_block* probe = bspec->io.base;

    union axi4s_probe_probe_control control = {
        .latch = AXI4S_PROBE_PROBE_CONTROL_LATCH_LATCH_ON_WR_EVT,
        .clear = AXI4S_PROBE_PROBE_CONTROL_CLEAR_CLEAR_ON_WR_EVT,
    };
    probe->probe_control._v = control._v;
    barrier();
}

//--------------------------------------------------------------------------------------------------
static void switch_stats_release_metrics(const struct stats_block_spec* bspec, void* UNUSED(data)) {
    volatile struct axi4s_probe_block* probe = bspec->io.base;

    union axi4s_probe_probe_control control = {
        .latch = AXI4S_PROBE_PROBE_CONTROL_LATCH_LATCH_ON_CLK,
        .clear = AXI4S_PROBE_PROBE_CONTROL_CLEAR_NO_CLEAR,
    };
    probe->probe_control._v = control._v;
    barrier();
}

//--------------------------------------------------------------------------------------------------
static void switch_stats_attach_metrics(const struct stats_block_spec* bspec) {
    switch_stats_latch_metrics(bspec, NULL);
    switch_stats_release_metrics(bspec, NULL);
}

//--------------------------------------------------------------------------------------------------
static void switch_stats_read_metric(const struct stats_block_spec* bspec,
                                     const struct stats_metric_spec* mspec,
                                     uint64_t* value,
                                     void* UNUSED(data)) {
    volatile uint32_t* pair = (typeof(pair))(bspec->io.base + mspec->io.offset);
    *value = ((uint64_t)pair[0] << 32) | (uint64_t)pair[1];
}

//--------------------------------------------------------------------------------------------------
struct stats_zone* switch_stats_zone_alloc(struct stats_domain* domain,
                                           volatile struct esnet_smartnic_bar2* bar2,
                                           const char* name) {
    struct stats_block_spec bspecs[ARRAY_SIZE(switch_stats_block_info)] = {0};
    for (unsigned int n = 0; n < ARRAY_SIZE(bspecs); ++n) {
        const struct switch_stats_block_info* binfo = &switch_stats_block_info[n];
        struct stats_block_spec* bspec = &bspecs[n];

        bspec->name = binfo->name;
        bspec->metrics = switch_stats_metrics;
        bspec->nmetrics = ARRAY_SIZE(switch_stats_metrics);
        bspec->io.base = (volatile void*)bar2 + binfo->offset;
        bspec->attach_metrics = switch_stats_attach_metrics;
        bspec->latch_metrics = switch_stats_latch_metrics;
        bspec->release_metrics = switch_stats_release_metrics;
        bspec->read_metric = switch_stats_read_metric;
    }

    struct stats_zone_spec zspec = {
        .name = name,
        .blocks = bspecs,
        .nblocks = ARRAY_SIZE(bspecs),
    };
    return stats_zone_alloc(domain, &zspec);
}

//--------------------------------------------------------------------------------------------------
void switch_stats_zone_free(struct stats_zone* zone) {
    stats_zone_free(zone);
}
