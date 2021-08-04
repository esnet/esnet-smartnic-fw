#include "cmac.h"		/* API */
#include "memory-barriers.h"	/* barrier() */

bool cmac_enable(volatile struct cmac_block * cmac) {
  cmac->conf_rx_1.ctl_rx_enable = 0x1;
  cmac->conf_tx_1.ctl_tx_enable = 0x1;
  barrier();

  uint32_t tx_status;
  for (int i = 0; i < 2; i++) {
    tx_status = cmac->stat_tx_status._v;
  }
  if (tx_status != 0) return false;

  uint32_t rx_status;
  for (int i = 0; i < 2; i++) {
    rx_status = cmac->stat_rx_status._v;
  }
  if (rx_status != 3) return false;

  return true;
}

