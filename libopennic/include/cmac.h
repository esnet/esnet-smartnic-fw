#if !defined(INCLUDE_CMAC_H)
#define INCLUDE_CMAC_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "cmac_block.h"
#include "stats.h"
#include <stdbool.h>

extern bool cmac_reset(volatile struct cmac_block * cmac);
extern bool cmac_loopback_enable(volatile struct cmac_block * cmac);
extern bool cmac_loopback_disable(volatile struct cmac_block * cmac);
extern bool cmac_loopback_is_enabled(volatile struct cmac_block * cmac);
extern bool cmac_enable(volatile struct cmac_block * cmac);
extern bool cmac_disable(volatile struct cmac_block * cmac);
extern bool cmac_is_enabled(volatile struct cmac_block * cmac);
extern bool cmac_rsfec_enable(volatile struct cmac_block * cmac);
extern bool cmac_rsfec_disable(volatile struct cmac_block * cmac);
extern bool cmac_rsfec_is_enabled(volatile struct cmac_block * cmac);
extern bool cmac_rx_status_is_link_up(volatile struct cmac_block * cmac);

extern struct stats_zone* cmac_stats_zone_alloc(struct stats_domain * domain,
                                                volatile struct cmac_block * cmac,
                                                const char * name);
extern void cmac_stats_zone_free(struct stats_zone * zone);

#ifdef __cplusplus
}
#endif

#endif	// INCLUDE_CMAC_H
