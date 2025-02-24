#ifndef INCLUDE_FLOW_CONTROL_H
#define INCLUDE_FLOW_CONTROL_H

#include "smartnic.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
enum fc_interface_type {
    fc_interface_type_UNKNOWN,
    fc_interface_type_PORT,
    fc_interface_type_HOST,
};

//--------------------------------------------------------------------------------------------------
#define FC_NUM_INTFS_PER_TYPE 2
struct fc_interface {
    enum fc_interface_type type;
    unsigned int index;
};

//--------------------------------------------------------------------------------------------------
#define FC_THRESHOLD_UNLIMITED UINT32_MAX
bool fc_get_egress_threshold(volatile struct smartnic_block* blk,
                             const struct fc_interface* intf,
                             uint32_t* threshold);
bool fc_set_egress_threshold(volatile struct smartnic_block* blk,
                             const struct fc_interface* intf,
                             uint32_t threshold);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_FLOW_CONTROL_H
