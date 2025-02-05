#include "flow_control.h"

#include "array_size.h"
#include "memory-barriers.h"
#include "smartnic.h"
#include <stdbool.h>
#include <stdint.h>

//--------------------------------------------------------------------------------------------------
static bool fc_interface_to_egr_threshold_tid(const struct fc_interface* intf, unsigned int* tid) {
    if (intf->index >= FC_NUM_INTFS_PER_TYPE) {
        return false;
    }

    switch(intf->type) {
    case fc_interface_type_PORT:
        *tid = intf->index;
        break;

    case fc_interface_type_HOST:
        *tid = FC_NUM_INTFS_PER_TYPE + intf->index;
        break;

    case fc_interface_type_UNKNOWN:
    default:
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool fc_get_egress_threshold(volatile struct smartnic_block* blk,
                             const struct fc_interface* intf,
                             uint32_t* threshold) {
    unsigned int tid;
    if (!fc_interface_to_egr_threshold_tid(intf, &tid) ||
        tid >= ARRAY_SIZE(blk->egr_fc_thresh)) {
        return false;
    }

    *threshold = blk->egr_fc_thresh[tid];
    return true;
}

//--------------------------------------------------------------------------------------------------
bool fc_set_egress_threshold(volatile struct smartnic_block* blk,
                             const struct fc_interface* intf,
                             uint32_t threshold) {
    unsigned int tid;
    if (!fc_interface_to_egr_threshold_tid(intf, &tid) ||
        tid >= ARRAY_SIZE(blk->egr_fc_thresh)) {
        return false;
    }

    blk->egr_fc_thresh[tid] = threshold;
    barrier();
    return true;
}
