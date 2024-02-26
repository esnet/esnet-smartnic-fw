#ifndef INCLUDE_SWITCH_H
#define INCLUDE_SWITCH_H

#include "smartnic_322mhz_block.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
enum switch_interface_type {
    switch_interface_type_UNKNOWN,
    switch_interface_type_PORT,
    switch_interface_type_HOST,
};

struct switch_interface_id {
    enum switch_interface_type type;
    unsigned int index;
};

//--------------------------------------------------------------------------------------------------
enum switch_processor_type {
    switch_processor_type_UNKNOWN,
    switch_processor_type_BYPASS,
    switch_processor_type_DROP,
    switch_processor_type_APP,
};

struct switch_processor_id {
    enum switch_processor_type type;
    unsigned int index;
};

//--------------------------------------------------------------------------------------------------
bool switch_get_ingress_source(volatile struct smartnic_322mhz_block* blk,
                               const struct switch_interface_id* from_intf,
                               struct switch_interface_id* to_intf);
bool switch_set_ingress_source(volatile struct smartnic_322mhz_block* blk,
                               const struct switch_interface_id* from_intf,
                               const struct switch_interface_id* to_intf);

bool switch_get_ingress_connection(volatile struct smartnic_322mhz_block* blk,
                                   const struct switch_interface_id* from_intf,
                                   struct switch_processor_id* to_proc);
bool switch_set_ingress_connection(volatile struct smartnic_322mhz_block* blk,
                                   const struct switch_interface_id* from_intf,
                                   const struct switch_processor_id* to_proc);

bool switch_get_egress_connection(volatile struct smartnic_322mhz_block* blk,
                                  const struct switch_processor_id* on_proc,
                                  const struct switch_interface_id* from_intf,
                                  struct switch_interface_id* to_intf);
bool switch_set_egress_connection(volatile struct smartnic_322mhz_block* blk,
                                  const struct switch_processor_id* on_proc,
                                  const struct switch_interface_id* from_intf,
                                  const struct switch_interface_id* to_intf);

void switch_set_defaults_one_to_one(volatile struct smartnic_322mhz_block* blk);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SWITCH_H
