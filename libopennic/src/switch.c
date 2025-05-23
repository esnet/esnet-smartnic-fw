#include "switch.h"

#include "array_size.h"
#include "axi4s_probe_block.h"
#include "memory-barriers.h"
#include "regmap-config.h"
#include "smartnic.h"
#include "stats.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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
#define SWITCH_STATS_COUNTER_NLABELS 2
#define SWITCH_STATS_COUNTER(_name, _units) \
{ \
    __STATS_METRIC_SPEC(#_name, NULL, COUNTER, STATS_METRIC_FLAG_MASK(CLEAR_ON_READ), 0), \
    .io = { \
        .offset = offsetof(struct axi4s_probe_block, _name##_upper), \
        .size = 2 * sizeof(uint32_t), \
    }, \
    .nlabels = SWITCH_STATS_COUNTER_NLABELS, \
    .labels = (const struct stats_label_spec[SWITCH_STATS_COUNTER_NLABELS]){ \
        {.key = "units", .value = #_units, .flags = STATS_LABEL_FLAG_MASK(NO_EXPORT)}, \
        {.key = "views", .value = "<placeholder>", .flags = STATS_LABEL_FLAG_MASK(NO_EXPORT)}, \
    }, \
}

static const struct stats_metric_spec switch_stats_metrics[] = {
    SWITCH_STATS_COUNTER(pkt_count, packets),
    SWITCH_STATS_COUNTER(byte_count, bytes),
};

#define VIEW(_name, _port, _direction) #_name ":" #_port ":" #_direction
#define VCAT(_a, _b) _a "," _b

struct switch_stats_block_info {
    const char* name;
    uintptr_t offset;
    const char* views;
};

#define SWITCH_STATS_BLOCK_INFO(_block, _prefix, _name, _views...) \
{ \
    .name = _prefix #_name, \
    .offset = offsetof(struct _block, _name), \
    .views = _views, \
}

#define SWITCH_STATS_TOP_INFO(_name, _views...) \
    SWITCH_STATS_BLOCK_INFO(esnet_smartnic_bar2, "", _name,## _views)
static const struct switch_stats_block_info switch_stats_top_info[] = {
    SWITCH_STATS_TOP_INFO(probe_core_to_app0, VCAT(VCAT(
                          VIEW(sn.igr.mux, 0, out),
                          VIEW(sn.igr.app, 0, in)),
                          VIEW(sn.igr.app.p4, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_core_to_app1, VCAT(VCAT(
                          VIEW(sn.igr.mux, 1, out),
                          VIEW(sn.igr.app, 1, in)),
                          VIEW(sn.igr.app.p4, 1, in))),
    SWITCH_STATS_TOP_INFO(probe_app0_to_core, VCAT(VCAT(
                          VIEW(sn.egr.demux, 0, in),
                          VIEW(sn.egr.app, 0, out)),
                          VIEW(sn.egr.app.p4, 0, out))),
    SWITCH_STATS_TOP_INFO(probe_app1_to_core, VCAT(VCAT(
                          VIEW(sn.egr.demux, 1, in),
                          VIEW(sn.egr.app, 1, out)),
                          VIEW(sn.egr.app.p4, 1, out))),

    SWITCH_STATS_TOP_INFO(probe_from_cmac_0, VCAT(VCAT(
                          VIEW(sn, 0, in),
                          VIEW(sn.igr, 0, in)),
                          VIEW(sn.igr.mux, 0, in))),
    SWITCH_STATS_TOP_INFO(drops_ovfl_from_cmac_0, VCAT(VCAT(
                          VIEW(sn, 0, in),
                          VIEW(sn.igr, 0, in)),
                          VIEW(sn.igr.mux, 0, in))),
    SWITCH_STATS_TOP_INFO(drops_err_from_cmac_0, VCAT(VCAT(
                          VIEW(sn, 0, in),
                          VIEW(sn.igr, 0, in)),
                          VIEW(sn.igr.mux, 0, in))),

    SWITCH_STATS_TOP_INFO(probe_from_cmac_1, VCAT(VCAT(
                          VIEW(sn, 1, in),
                          VIEW(sn.igr, 1, in)),
                          VIEW(sn.igr.mux, 1, in))),
    SWITCH_STATS_TOP_INFO(drops_ovfl_from_cmac_1, VCAT(VCAT(
                          VIEW(sn, 1, in),
                          VIEW(sn.igr, 1, in)),
                          VIEW(sn.igr.mux, 1, in))),
    SWITCH_STATS_TOP_INFO(drops_err_from_cmac_1, VCAT(VCAT(
                          VIEW(sn, 1, in),
                          VIEW(sn.igr, 1, in)),
                          VIEW(sn.igr.mux, 1, in))),

    SWITCH_STATS_TOP_INFO(probe_to_cmac_0, VCAT(VCAT(
                          VIEW(sn, 0, out),
                          VIEW(sn.egr, 0, out)),
                          VIEW(sn.egr.demux, 0, out))),
    SWITCH_STATS_TOP_INFO(drops_ovfl_to_cmac_0, VCAT(VCAT(
                          VIEW(sn, 0, out),
                          VIEW(sn.egr, 0, out)),
                          VIEW(sn.egr.demux, 0, out))),

    SWITCH_STATS_TOP_INFO(probe_to_cmac_1, VCAT(VCAT(
                          VIEW(sn, 1, out),
                          VIEW(sn.egr, 1, out)),
                          VIEW(sn.egr.demux, 1, out))),
    SWITCH_STATS_TOP_INFO(drops_ovfl_to_cmac_1, VCAT(VCAT(
                          VIEW(sn, 1, out),
                          VIEW(sn.egr, 1, out)),
                          VIEW(sn.egr.demux, 1, out))),

    SWITCH_STATS_TOP_INFO(probe_from_host_0,
                          VIEW(sn, 0, in)),
    SWITCH_STATS_TOP_INFO(probe_from_host_1,
                          VIEW(sn, 1, in)),

    SWITCH_STATS_TOP_INFO(probe_to_host_0,
                          VIEW(sn, 0, out)),
    SWITCH_STATS_TOP_INFO(drops_ovfl_to_host_0,
                          VIEW(sn, 0, out)),

    SWITCH_STATS_TOP_INFO(probe_to_host_1,
                          VIEW(sn, 1, out)),
    SWITCH_STATS_TOP_INFO(drops_ovfl_to_host_1,
                          VIEW(sn, 1, out)),

    SWITCH_STATS_TOP_INFO(probe_to_bypass_0,
                          VIEW(sn.igr.mux, 0, out)),
    SWITCH_STATS_TOP_INFO(drops_to_bypass_0,
                          VIEW(sn.igr.mux, 0, out)),
#if !REGMAP_CONFIG_WITH_V2_SWITCH_COUNTERS
    SWITCH_STATS_TOP_INFO(drops_from_bypass_0,
                          VIEW(sn.egr.demux, 0, in)),
#endif

    SWITCH_STATS_TOP_INFO(probe_to_bypass_1,
                          VIEW(sn.igr.mux, 1, out)),
    SWITCH_STATS_TOP_INFO(drops_to_bypass_1,
                          VIEW(sn.igr.mux, 1, out)),
#if !REGMAP_CONFIG_WITH_V2_SWITCH_COUNTERS
    SWITCH_STATS_TOP_INFO(drops_from_bypass_1,
                          VIEW(sn.egr.demux, 1, in)),
#endif

    SWITCH_STATS_TOP_INFO(probe_from_pf0, VCAT(
                          VIEW(sn.egr.app, 0, in),
                          VIEW(sn.egr.app.mux, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_from_pf0_vf0, VCAT(
                          VIEW(sn.egr.app, 0, in),
                          VIEW(sn.egr.app.user, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_from_pf0_vf1, VCAT(
                          VIEW(sn.egr.app, 0, in),
                          VIEW(sn.egr.app.p4, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_from_pf0_vf2,
                          VIEW(sn.igr.mux, 0, in)),

    SWITCH_STATS_TOP_INFO(probe_to_pf0, VCAT(
                          VIEW(sn.igr.app, 0, out),
                          VIEW(sn.igr.app.demux, 0, out))),
    SWITCH_STATS_TOP_INFO(probe_to_pf0_vf0, VCAT(
                          VIEW(sn.igr.app, 0, out),
                          VIEW(sn.igr.app.user, 0, out))),
    SWITCH_STATS_TOP_INFO(probe_to_pf0_vf1, VCAT(
                          VIEW(sn.egr.app, 0, out),
                          VIEW(sn.egr.app.p4, 0, out))),
    SWITCH_STATS_TOP_INFO(probe_to_pf0_vf2,
                          VIEW(sn.egr.demux, 0, out)),

    SWITCH_STATS_TOP_INFO(probe_from_pf1, VCAT(
                          VIEW(sn.egr.app, 1, in),
                          VIEW(sn.egr.app.mux, 1, in))),
    SWITCH_STATS_TOP_INFO(probe_from_pf1_vf0, VCAT(
                          VIEW(sn.egr.app, 1, in),
                          VIEW(sn.egr.app.user, 1, in))),
    SWITCH_STATS_TOP_INFO(probe_from_pf1_vf1, VCAT(
                          VIEW(sn.egr.app, 1, in),
                          VIEW(sn.egr.app.p4, 1, in))),
    SWITCH_STATS_TOP_INFO(probe_from_pf1_vf2,
                          VIEW(sn.igr.mux, 1, in)),

    SWITCH_STATS_TOP_INFO(probe_to_pf1, VCAT(
                          VIEW(sn.igr.app, 1, out),
                          VIEW(sn.igr.app.demux, 1, out))),
    SWITCH_STATS_TOP_INFO(probe_to_pf1_vf0, VCAT(
                          VIEW(sn.igr.app, 1, out),
                          VIEW(sn.igr.app.user, 1, out))),
    SWITCH_STATS_TOP_INFO(probe_to_pf1_vf1, VCAT(
                          VIEW(sn.egr.app, 1, out),
                          VIEW(sn.egr.app.p4, 1, out))),
    SWITCH_STATS_TOP_INFO(probe_to_pf1_vf2,
                          VIEW(sn.egr.demux, 1, out)),

    SWITCH_STATS_TOP_INFO(drops_q_range_fail_0,
                          VIEW(sn, 0, out)),
    SWITCH_STATS_TOP_INFO(drops_q_range_fail_1,
                          VIEW(sn, 1, out)),

#if REGMAP_CONFIG_WITH_V2_SWITCH_COUNTERS
    SWITCH_STATS_TOP_INFO(probe_to_app_igr_in0, VCAT(
                          VIEW(sn.igr.app.demux, 0, out),
                          VIEW(sn.igr.app.user, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_to_app_igr_in1, VCAT(
                          VIEW(sn.igr.app.demux, 1, out),
                          VIEW(sn.igr.app.user, 1, in))),

    SWITCH_STATS_TOP_INFO(probe_to_app_igr_p4_out0, VCAT(
                          VIEW(sn.igr.app.p4, 0, out),
                          VIEW(sn.igr.app.demux, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_to_app_igr_p4_out1, VCAT(
                          VIEW(sn.igr.app.p4, 1, out),
                          VIEW(sn.igr.app.demux, 1, in))),

    SWITCH_STATS_TOP_INFO(probe_to_app_egr_in0, VCAT(
                          VIEW(sn.igr.app.user, 0, out),
                          VIEW(sn.egr.app.user, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_to_app_egr_in1, VCAT(
                          VIEW(sn.igr.app.user, 1, out),
                          VIEW(sn.egr.app.user, 1, in))),
    SWITCH_STATS_TOP_INFO(probe_to_app_egr_out0, VCAT(
                          VIEW(sn.egr.app.user, 0, out),
                          VIEW(sn.egr.app.mux, 0, in))),
    SWITCH_STATS_TOP_INFO(probe_to_app_egr_out1, VCAT(
                          VIEW(sn.egr.app.user, 1, out),
                          VIEW(sn.egr.app.mux, 1, in))),

    SWITCH_STATS_TOP_INFO(probe_to_app_egr_p4_in0, VCAT(
                          VIEW(sn.egr.app.p4, 0, in),
                          VIEW(sn.egr.app.mux, 0, out))),
    SWITCH_STATS_TOP_INFO(probe_to_app_egr_p4_in1, VCAT(
                          VIEW(sn.egr.app.p4, 1, in),
                          VIEW(sn.egr.app.mux, 1, out))),
#endif
};

#define SWITCH_STATS_P4_PROC_INFO(_prefix, _name, _views...) \
    SWITCH_STATS_BLOCK_INFO(p4_proc_decoder, "p4_proc_" #_prefix "_", _name,## _views)

static const struct switch_stats_block_info switch_stats_p4_proc_igr_info[] = {
#if REGMAP_CONFIG_WITH_V2_SWITCH_COUNTERS
    SWITCH_STATS_P4_PROC_INFO(igr, drops_from_p4, VCAT(
                              VIEW(sn.igr.app.p4, 0, out),
                              VIEW(sn.igr.app.p4, 1, out))),
    SWITCH_STATS_P4_PROC_INFO(igr, drops_unset_err_port_0,
                              VIEW(sn.igr.app.p4, 0, out)),
    SWITCH_STATS_P4_PROC_INFO(igr, drops_unset_err_port_1,
                              VIEW(sn.igr.app.p4, 1, out)),
#else
    SWITCH_STATS_P4_PROC_INFO(igr, drops_from_proc_port_0,
                              VIEW(sn.egr.app.p4, 0, out)),
    SWITCH_STATS_P4_PROC_INFO(igr, drops_from_proc_port_1,
                              VIEW(sn.egr.app.p4, 0, out)),

    SWITCH_STATS_P4_PROC_INFO(igr, probe_to_pyld_fifo,
                              VIEW(sn.igr.app.p4, 0, in)),
    SWITCH_STATS_P4_PROC_INFO(igr, drops_to_pyld_fifo,
                              VIEW(sn.igr.app.p4, 0, in)),
#endif
};

static const struct switch_stats_block_info switch_stats_p4_proc_egr_info[] = {
#if REGMAP_CONFIG_WITH_V2_SWITCH_COUNTERS
    SWITCH_STATS_P4_PROC_INFO(egr, drops_from_p4, VCAT(
                              VIEW(sn.egr.app.p4, 0, out),
                              VIEW(sn.egr.app.p4, 1, out))),
    SWITCH_STATS_P4_PROC_INFO(egr, drops_unset_err_port_0,
                              VIEW(sn.egr.app.p4, 0, out)),
    SWITCH_STATS_P4_PROC_INFO(egr, drops_unset_err_port_1,
                              VIEW(sn.egr.app.p4, 1, out)),
#else
    SWITCH_STATS_P4_PROC_INFO(egr, drops_from_proc_port_0,
                              VIEW(sn.egr.app.p4, 1, out)),
    SWITCH_STATS_P4_PROC_INFO(egr, drops_from_proc_port_1,
                              VIEW(sn.egr.app.p4, 1, out)),

    SWITCH_STATS_P4_PROC_INFO(egr, probe_to_pyld_fifo,
                              VIEW(sn.igr.app.p4, 1, in)),
    SWITCH_STATS_P4_PROC_INFO(egr, drops_to_pyld_fifo,
                              VIEW(sn.igr.app.p4, 1, in)),
#endif
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
    struct switch_stats_block_group {
        const struct switch_stats_block_info* info;
        size_t ninfo;
        volatile void* base;
        bool is_proc;
        bool is_valid;
    };
    struct switch_stats_block_group groups[] = {
        {
            .info = switch_stats_top_info,
            .ninfo = ARRAY_SIZE(switch_stats_top_info),
            .base = bar2,
        },
        {
            .info = switch_stats_p4_proc_igr_info,
            .ninfo = ARRAY_SIZE(switch_stats_p4_proc_igr_info),
            .base = &bar2->p4_proc_igr,
            .is_proc = true,
        },
        {
            .info = switch_stats_p4_proc_egr_info,
            .ninfo = ARRAY_SIZE(switch_stats_p4_proc_egr_info),
            .base = &bar2->p4_proc_egr,
            .is_proc = true,
        },
    };

    size_t nblocks = 0;
    for (struct switch_stats_block_group* grp = groups;
         grp < &groups[ARRAY_SIZE(groups)];
         ++grp) {
        if (grp->is_proc) {
            volatile struct p4_proc_decoder* dec = grp->base;
            if (dec->p4_proc.status == UINT32_MAX) {
                // Don't include probes if the P4 processor is not implemented.
                continue;
            }
        }

        nblocks += grp->ninfo;
        grp->is_valid = true;
    }

#define NMETRICS (nblocks * ARRAY_SIZE(switch_stats_metrics))
    struct stats_block_spec bspecs[nblocks];
    struct stats_metric_spec mspecs[NMETRICS];
    struct stats_label_spec lspecs[NMETRICS * SWITCH_STATS_COUNTER_NLABELS];
#undef NMETRICS

    struct stats_block_spec* bspec = bspecs;
    struct stats_metric_spec* mspec = mspecs;
    struct stats_label_spec* lspec = lspecs;
    for (const struct switch_stats_block_group* grp = groups;
         grp < &groups[ARRAY_SIZE(groups)];
         ++grp) {
        if (!grp->is_valid) {
            continue;
        }

        for (const struct switch_stats_block_info* info = grp->info;
             info < &grp->info[grp->ninfo];
             ++info, ++bspec) {
            memset(bspec, 0, sizeof(*bspec));
            bspec->name = info->name;
            bspec->metrics = mspec;
            bspec->nmetrics = ARRAY_SIZE(switch_stats_metrics);
            bspec->io.base = grp->base + info->offset;
            bspec->attach_metrics = switch_stats_attach_metrics;
            bspec->latch_metrics = switch_stats_latch_metrics;
            bspec->release_metrics = switch_stats_release_metrics;
            bspec->read_metric = switch_stats_read_metric;

            for (const struct stats_metric_spec* ms = switch_stats_metrics;
                 ms < &switch_stats_metrics[ARRAY_SIZE(switch_stats_metrics)];
                 ++ms, ++mspec) {
                *mspec = *ms;
                mspec->labels = lspec;
                mspec->nlabels = 0;

                for (const struct stats_label_spec* ls = ms->labels;
                     ls < &ms->labels[ms->nlabels];
                     ++ls, ++lspec, ++mspec->nlabels) {
                    *lspec = *ls;
                    if (strcmp(lspec->key, "views") == 0) {
                        lspec->value = info->views;
                    }
                }
            }
        }
    }

    struct stats_zone_spec zspec = {
        .name = name,
        .blocks = bspecs,
        .nblocks = nblocks,
    };
    return stats_zone_alloc(domain, &zspec);
}

//--------------------------------------------------------------------------------------------------
void switch_stats_zone_free(struct stats_zone* zone) {
    stats_zone_free(zone);
}
