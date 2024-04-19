#if !defined(INCLUDE_SMARTNIC_H)
#define INCLUDE_SMARTNIC_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>		/* bool */
#include <stdint.h>		/* uint* */
#include "esnet_smartnic_toplevel.h" /* ESNET_SMARTNIC_* */

#include "smartnic-block-compat.h"

volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_fd(int fd);
volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_path(const char *path, bool create);
volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_pciaddr(const char *addr);
void smartnic_unmap_bar2(volatile struct esnet_smartnic_bar2 * virt_addr);

bool smartnic_probe_read_counters(volatile struct axi4s_probe_block * probe, uint64_t *packet_count, uint64_t *byte_count, bool clear);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SMARTNIC_H
