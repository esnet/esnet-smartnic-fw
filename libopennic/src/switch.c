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
static bool switch_port_to_mux_tid(unsigned int index,
                                   enum switch_interface intf,
                                   unsigned int* tid) {
    if (index >= SWITCH_NUM_PORTS) {
        return false;
    }

    switch(intf) {
    case switch_interface_PHYSICAL:
        *tid = index;
        break;

    case switch_interface_TEST:
        *tid = SWITCH_NUM_PORTS + index;
        break;

    case switch_interface_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_destination_from_mux_sel(enum switch_destination* dest, unsigned int value) {
    switch (value) {
#define CASE(_name) { \
        case SMARTNIC_SMARTNIC_MUX_OUT_SEL_VALUE_##_name: \
            *dest = switch_destination_##_name; \
            break; \
        }

    CASE(BYPASS);
    CASE(DROP);
    CASE(APP);

    default:
        return false;

#undef CASE
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool switch_destination_to_mux_sel(enum switch_destination dest, unsigned int* value) {
    switch (dest) {
#define CASE(_name) { \
        case switch_destination_##_name: \
            *value = SMARTNIC_SMARTNIC_MUX_OUT_SEL_VALUE_##_name; \
            break; \
        }

    CASE(BYPASS);
    CASE(DROP);
    CASE(APP);

    case switch_destination_UNKNOWN:
    default:
        return false;

#undef CASE
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
static bool __switch_get_ingress_selector(volatile struct smartnic_block* blk,
                                          struct switch_ingress_selector* sel) {
    unsigned int tid;
    if (!switch_port_to_mux_tid(sel->index, sel->intf, &tid) ||
        tid >= ARRAY_SIZE(blk->smartnic_mux_out_sel)) {
        return false;
    }

    union smartnic_smartnic_mux_out_sel mux = {._v = blk->smartnic_mux_out_sel[tid]._v};
    return switch_destination_from_mux_sel(&sel->dest, mux.value);
}

bool switch_get_ingress_selector(volatile struct smartnic_block* blk,
                                 struct switch_ingress_selector* sel) {
    sel->intf = switch_interface_TEST;
    if (!__switch_get_ingress_selector(blk, sel)) {
        return false;
    }

    if (sel->dest != switch_destination_DROP) {
        return true;
    }

    sel->intf = switch_interface_PHYSICAL;
    return __switch_get_ingress_selector(blk, sel);
}

//--------------------------------------------------------------------------------------------------
static bool __switch_set_ingress_selector(volatile struct smartnic_block* blk,
                                          const struct switch_ingress_selector* sel) {
    unsigned int tid;
    unsigned int value;
    if (!switch_port_to_mux_tid(sel->index, sel->intf, &tid) ||
        !switch_destination_to_mux_sel(sel->dest, &value) ||
        tid >= ARRAY_SIZE(blk->smartnic_mux_out_sel)) {
        return false;
    }

    union smartnic_smartnic_mux_out_sel mux = {._v = blk->smartnic_mux_out_sel[tid]._v};
    mux.value = value;
    blk->smartnic_mux_out_sel[tid]._v = mux._v;
    barrier();

    return true;
}

bool switch_set_ingress_selector(volatile struct smartnic_block* blk,
                                 const struct switch_ingress_selector* sel) {
    struct switch_ingress_selector ps = *sel;
    ps.intf = switch_interface_PHYSICAL;
    if (ps.intf != sel->intf) {
        ps.dest = switch_destination_DROP;
    }

    struct switch_ingress_selector ts = *sel;
    ts.intf = switch_interface_TEST;
    if (ts.intf != sel->intf) {
        ts.dest = switch_destination_DROP;
    }

    return __switch_set_ingress_selector(blk, &ps) && __switch_set_ingress_selector(blk, &ts);
}

//--------------------------------------------------------------------------------------------------
bool switch_get_egress_selector(volatile struct smartnic_block* blk,
                                struct switch_egress_selector* sel) {
    union smartnic_smartnic_demux_out_sel demux = {._v = blk->smartnic_demux_out_sel._v};
    switch (sel->index) {
#define CASE(_idx) \
    case _idx: { \
        sel->intf = demux.port##_idx ? switch_interface_TEST : switch_interface_PHYSICAL; \
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
bool switch_set_egress_selector(volatile struct smartnic_block* blk,
                                const struct switch_egress_selector* sel) {
    unsigned int value;
    switch (sel->intf) {
    case switch_interface_PHYSICAL: value = 0; break;
    case switch_interface_TEST: value = 1; break;
    default: return false;
    }

    union smartnic_smartnic_demux_out_sel demux = {._v = blk->smartnic_demux_out_sel._v};
    switch (sel->index) {
#define CASE(_idx) case _idx: demux.port##_idx = value; break

    CASE(0);
    CASE(1);
    default:
        return false;

#undef CASE
    }

    blk->smartnic_demux_out_sel._v = demux._v;
    barrier();

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_get_bypass_mode(volatile struct smartnic_block* blk,
                            enum switch_bypass_mode* mode) {
    union smartnic_bypass_config config = {._v = blk->bypass_config._v};
    *mode = config.swap_paths ? switch_bypass_mode_SWAP : switch_bypass_mode_STRAIGHT;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_set_bypass_mode(volatile struct smartnic_block* blk,
                            enum switch_bypass_mode mode) {
    union smartnic_bypass_config config = {._v = blk->bypass_config._v};
    switch (mode) {
    case switch_bypass_mode_STRAIGHT:
        config.swap_paths = 0;
        break;

    case switch_bypass_mode_SWAP:
        config.swap_paths = 1;
        break;

    default:
        return false;
    }

    blk->bypass_config._v = config._v;
    barrier();

    return true;
}

//--------------------------------------------------------------------------------------------------
void switch_set_defaults_one_to_one(volatile struct smartnic_block* blk) {
    // Reset all ingress connections to drop prior to changing the switch configuration.
    for (unsigned int tid = 0; tid < ARRAY_SIZE(blk->smartnic_mux_out_sel); ++tid) {
        blk->smartnic_mux_out_sel[tid]._v = SMARTNIC_SMARTNIC_MUX_OUT_SEL_VALUE_DROP;
    }
    barrier();

    struct switch_ingress_selector is = {
        .intf = switch_interface_PHYSICAL,
        .dest = switch_destination_APP,
    };
    for (is.index = 0; is.index < SWITCH_NUM_PORTS; ++is.index) {
        switch_set_ingress_selector(blk, &is);
    }

    struct switch_egress_selector es = {
        .intf = switch_interface_PHYSICAL,
    };
    for (es.index = 0; es.index < SWITCH_NUM_PORTS; ++es.index) {
        switch_set_egress_selector(blk, &es);
    }

    switch_set_bypass_mode(blk, switch_bypass_mode_STRAIGHT);
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
