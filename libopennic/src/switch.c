#include "switch.h"

#include "array_size.h"
#include "memory-barriers.h"
#include "smartnic_322mhz_block.h"
#include <stdbool.h>
#include <stdint.h>

//--------------------------------------------------------------------------------------------------
static bool switch_interface_id_from_igr_sw_tid(struct switch_interface_id* intf,
                                                const unsigned int tid) {
    switch (tid) {
    case SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_1:
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
        case 0: *tid = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_0; break;
        case 1: *tid = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tid = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_0; break;
        case 1: *tid = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_1; break;
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
    case SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_BYPASS:
        proc->type = switch_processor_type_BYPASS;
        proc->index = 0;
        break;

    case SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_DROP:
        proc->type = switch_processor_type_DROP;
        proc->index = 0;
        break;

    case SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_0:
        proc->type = switch_processor_type_APP;
        proc->index = 0;
        break;

    case SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_1:
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
        case 0: *tdest = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_BYPASS; break;
        default: return false;
        }
        break;

    case switch_processor_type_DROP:
        switch (proc->index) {
        case 0: *tdest = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_DROP; break;
        default: return false;
        }
        break;

    case switch_processor_type_APP:
        switch (proc->index) {
        case 0: *tdest = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_1; break;
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
    case SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_1:
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
        case 0: *tdest = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_1; break;
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
    case SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_1:
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
        case 0: *tdest = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_1; break;
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
    case SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_0:
        intf->type = switch_interface_type_PORT;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_1:
        intf->type = switch_interface_type_PORT;
        intf->index = 1;
        break;

    case SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_0:
        intf->type = switch_interface_type_HOST;
        intf->index = 0;
        break;

    case SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_1:
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
        case 0: *tdest = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_1; break;
        default: return false;
        }
        break;

    case switch_interface_type_HOST:
        switch (intf->index) {
        case 0: *tdest = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_0; break;
        case 1: *tdest = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_1; break;
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
bool switch_get_ingress_source(volatile struct smartnic_322mhz_block* blk,
                               const struct switch_interface_id* from_intf,
                               struct switch_interface_id* to_intf) {
    unsigned int from_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tid)) {
        return false;
    }

    union smartnic_322mhz_igr_sw_tid tid = {._v = blk->igr_sw_tid[from_tid]._v};
    return switch_interface_id_from_igr_sw_tid(to_intf, tid.value);
}

//--------------------------------------------------------------------------------------------------
bool switch_set_ingress_source(volatile struct smartnic_322mhz_block* blk,
                               const struct switch_interface_id* from_intf,
                               const struct switch_interface_id* to_intf) {
    unsigned int from_tid;
    unsigned int to_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        !switch_interface_id_to_igr_sw_tid(to_intf, &to_tid) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tid)) {
        return false;
    }

    union smartnic_322mhz_igr_sw_tid tid = {._v = blk->igr_sw_tid[from_tid]._v};
    tid.value = to_tid;
    blk->igr_sw_tid[from_tid]._v = tid._v;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_get_ingress_connection(volatile struct smartnic_322mhz_block* blk,
                                   const struct switch_interface_id* from_intf,
                                   struct switch_processor_id* to_proc) {
    unsigned int from_tid;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tdest)) {
        return false;
    }

    union smartnic_322mhz_igr_sw_tdest tdest = {._v = blk->igr_sw_tdest[from_tid]._v};
    return switch_processor_id_from_igr_sw_tdest(to_proc, tdest.value);
}

//--------------------------------------------------------------------------------------------------
bool switch_set_ingress_connection(volatile struct smartnic_322mhz_block* blk,
                                   const struct switch_interface_id* from_intf,
                                   const struct switch_processor_id* to_proc) {
    unsigned int from_tid;
    unsigned int to_tdest;
    if (!switch_interface_id_to_igr_sw_tid(from_intf, &from_tid) ||
        !switch_processor_id_to_igr_sw_tdest(to_proc, &to_tdest) ||
        from_tid >= ARRAY_SIZE(blk->igr_sw_tdest)) {
        return false;
    }

    union smartnic_322mhz_igr_sw_tdest tdest = {._v = blk->igr_sw_tdest[from_tid]._v};
    tdest.value = to_tdest;
    blk->igr_sw_tdest[from_tid]._v = tdest._v;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool switch_get_egress_connection(volatile struct smartnic_322mhz_block* blk,
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

        union smartnic_322mhz_bypass_tdest tdest = {._v = blk->bypass_tdest[from_tid]._v};
        return switch_interface_id_from_bypass_tdest(to_intf, tdest.value);
    }

    case switch_processor_type_APP:
        switch (on_proc->index) {
        case 0: {
            if (from_tid >= ARRAY_SIZE(blk->app_0_tdest_remap)) {
                return false;
            }

            union smartnic_322mhz_app_0_tdest_remap tdest = {
                ._v = blk->app_0_tdest_remap[from_tid]._v};
            return switch_interface_id_from_app_0_tdest(to_intf, tdest.value);
        }

        case 1: {
            if (from_tid >= ARRAY_SIZE(blk->app_1_tdest_remap)) {
                return false;
            }

            union smartnic_322mhz_app_1_tdest_remap tdest = {
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
bool switch_set_egress_connection(volatile struct smartnic_322mhz_block* blk,
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

        union smartnic_322mhz_bypass_tdest tdest = {._v = blk->bypass_tdest[from_tid]._v};
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

            union smartnic_322mhz_app_0_tdest_remap tdest = {
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

            union smartnic_322mhz_app_1_tdest_remap tdest = {
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
void switch_set_defaults_one_to_one(volatile struct smartnic_322mhz_block* blk) {
    unsigned int tid;

    // Reset all ingress connections to drop prior to changing the switch configuration.
    for (tid = 0; tid < ARRAY_SIZE(blk->igr_sw_tdest); ++tid) {
        blk->igr_sw_tdest[tid]._v = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_DROP;
    }
    barrier();

    // Apply ingress source defaults.
    const union smartnic_322mhz_igr_sw_tid igr_sw_tid_defaults[] = {
        {.value = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_0}, // CMAC 0
        {.value = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_1}, // CMAC 1
        {.value = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_0}, // HOST 0
        {.value = SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->igr_sw_tid); ++tid) {
        blk->igr_sw_tid[tid]._v = igr_sw_tid_defaults[tid]._v;
    }

    // Apply ingress connection defaults.
    const union smartnic_322mhz_igr_sw_tdest igr_sw_tdest_defaults[] = {
        {.value = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_0},      // CMAC 0
        {.value = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_0},      // CMAC 1
        {.value = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_BYPASS}, // HOST 0
        {.value = SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_BYPASS}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->igr_sw_tdest); ++tid) {
        blk->igr_sw_tdest[tid]._v = igr_sw_tdest_defaults[tid]._v;
    }

    // Apply egress connection defaults to the bypass processor.
    const union smartnic_322mhz_bypass_tdest bypass_tdest_defaults[] = {
        {.value = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_0}, // CMAC 0
        {.value = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_1}, // CMAC 1
        {.value = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_0}, // HOST 0
        {.value = SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->bypass_tdest); ++tid) {
        blk->bypass_tdest[tid]._v = bypass_tdest_defaults[tid]._v;
    }

    // Apply egress connection defaults to the application 0 processor.
    const union smartnic_322mhz_app_0_tdest_remap app_0_tdest_remap_defaults[] = {
        {.value = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_0}, // CMAC 0
        {.value = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_1}, // CMAC 1
        {.value = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_0}, // HOST 0
        {.value = SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_1}, // HOST 1
    };
    for (tid = 0; tid < ARRAY_SIZE(blk->app_0_tdest_remap); ++tid) {
        blk->app_0_tdest_remap[tid]._v = app_0_tdest_remap_defaults[tid]._v;
    }

    // Apply egress connection defaults to the application 1 processor.
    const union smartnic_322mhz_app_1_tdest_remap app_1_tdest_remap_defaults[] = {
        {.value = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_0}, // CMAC 0
        {.value = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_1}, // CMAC 1
        {.value = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_0}, // HOST 0
        {.value = SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_1}, // HOST 1
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

    // Disable split-join support.
    blk->hdr_length = 0;
}
