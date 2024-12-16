#ifndef INCLUDE_SWITCH_H
#define INCLUDE_SWITCH_H

#include "smartnic.h"
#include "stats.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
enum switch_interface {
    switch_interface_UNKNOWN,
    switch_interface_PHYSICAL,
    switch_interface_TEST,
};

//--------------------------------------------------------------------------------------------------
enum switch_destination {
    switch_destination_UNKNOWN,
    switch_destination_BYPASS,
    switch_destination_DROP,
    switch_destination_APP,
};

//--------------------------------------------------------------------------------------------------
#define SWITCH_NUM_PORTS 2
struct switch_ingress_selector {
    unsigned int index;
    enum switch_interface intf;
    enum switch_destination dest;
};

struct switch_egress_selector {
    unsigned int index;
    enum switch_interface intf;
};

//--------------------------------------------------------------------------------------------------
enum switch_bypass_mode {
    switch_bypass_mode_UNKNOWN,
    switch_bypass_mode_STRAIGHT,
    switch_bypass_mode_SWAP,
};

//--------------------------------------------------------------------------------------------------
bool switch_get_ingress_selector(volatile struct smartnic_block* blk,
                                 struct switch_ingress_selector* sel);
bool switch_set_ingress_selector(volatile struct smartnic_block* blk,
                                 const struct switch_ingress_selector* sel);

bool switch_get_egress_selector(volatile struct smartnic_block* blk,
                                struct switch_egress_selector* sel);
bool switch_set_egress_selector(volatile struct smartnic_block* blk,
                                const struct switch_egress_selector* sel);

bool switch_get_bypass_mode(volatile struct smartnic_block* blk,
                            enum switch_bypass_mode* mode);
bool switch_set_bypass_mode(volatile struct smartnic_block* blk,
                            enum switch_bypass_mode mode);

void switch_set_defaults_one_to_one(volatile struct smartnic_block* blk);

struct stats_zone* switch_stats_zone_alloc(struct stats_domain* domain,
                                           volatile struct esnet_smartnic_bar2* bar2,
                                           const char* name);
void switch_stats_zone_free(struct stats_zone* zone);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SWITCH_H
