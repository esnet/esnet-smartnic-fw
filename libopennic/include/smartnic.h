#include "esnet_smartnic_toplevel.h" /* ESNET_SMARTNIC_* */

volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_pciaddr(char *addr);
void smartnic_unmap_bar2(volatile struct esnet_smartnic_bar2 * virt_addr);
