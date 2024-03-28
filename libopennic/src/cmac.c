#include "cmac.h"		/* API */
#include "memory-barriers.h"	/* barrier() */

bool cmac_reset(volatile struct cmac_block * cmac)
{
  union cmac_reset reset_on = {
    .usr_tx_reset = 1,
    .usr_rx_reset = 1,
  };
  union cmac_reset reset_off = {
    .usr_tx_reset = 0,
    .usr_rx_reset = 0,
  };

  cmac->reset._v = reset_on._v;
  barrier();
  cmac->reset._v = reset_off._v;

  return true;
}

bool cmac_loopback_enable(volatile struct cmac_block * cmac)
{
  cmac->gt_loopback = 1;
  return true;
}

bool cmac_loopback_disable(volatile struct cmac_block * cmac)
{
  cmac->gt_loopback = 0;
  return true;
}

bool cmac_loopback_is_enabled(volatile struct cmac_block * cmac) {
  return cmac->gt_loopback != 0;
}

bool cmac_enable(volatile struct cmac_block * cmac) {
  union cmac_conf_rx_1 conf_rx = {
    .ctl_rx_enable = 1,
  };
  cmac->conf_rx_1._v = conf_rx._v;

  union cmac_conf_tx_1 conf_tx = {
    .ctl_tx_enable = 1,
  };
  cmac->conf_tx_1._v = conf_tx._v;

  barrier();

  uint32_t tx_status;
  for (int i = 0; i < 2; i++) {
    tx_status = cmac->stat_tx_status._v;
  }

  uint32_t rx_status;
  for (int i = 0; i < 2; i++) {
    rx_status = cmac->stat_rx_status._v;
  }

  if (tx_status != 0) return false;
  if (rx_status != 3) return false;

  return true;
}

bool cmac_disable(volatile struct cmac_block * cmac) {
  union cmac_conf_rx_1 conf_rx = {
    .ctl_rx_enable = 0,
  };
  cmac->conf_rx_1._v = conf_rx._v;

  union cmac_conf_tx_1 conf_tx = {
    .ctl_tx_enable = 0,
  };
  cmac->conf_tx_1._v = conf_tx._v;

  barrier();

  return true;
}

bool cmac_is_enabled(volatile struct cmac_block * cmac) {
  union cmac_conf_rx_1 rx = {._v = cmac->conf_rx_1._v};
  union cmac_conf_tx_1 tx = {._v = cmac->conf_tx_1._v};
  return rx.ctl_rx_enable && tx.ctl_tx_enable;
}

bool cmac_rsfec_enable(volatile struct cmac_block * cmac) {
  union cmac_rsfec_conf_ind_correction rsfec_conf_ind_correction = {
    .ctl_rx_rsfec_ieee_err_ind_mode = 1,
    .ctl_rx_rsfec_en_ind = 1,
    .ctl_rx_rsfec_en_cor = 1,
  };
  cmac->rsfec_conf_ind_correction._v = rsfec_conf_ind_correction._v;

  union cmac_rsfec_conf_enable rsfec_conf_enable = {
    .ctl_tx_rsfec_enable = 1,
    .ctl_rx_rsfec_enable = 1,
  };
  cmac->rsfec_conf_enable._v = rsfec_conf_enable._v;

  barrier();

  cmac_reset(cmac);

  return true;
}

bool cmac_rsfec_disable(volatile struct cmac_block * cmac) {
  union cmac_rsfec_conf_ind_correction rsfec_conf_ind_correction = {
    .ctl_rx_rsfec_ieee_err_ind_mode = 0,
    .ctl_rx_rsfec_en_ind = 0,
    .ctl_rx_rsfec_en_cor = 0,
  };
  cmac->rsfec_conf_ind_correction._v = rsfec_conf_ind_correction._v;

  union cmac_rsfec_conf_enable rsfec_conf_enable = {
    .ctl_tx_rsfec_enable = 0,
    .ctl_rx_rsfec_enable = 0,
  };
  cmac->rsfec_conf_enable._v = rsfec_conf_enable._v;

  barrier();

  return true;
}

bool cmac_rsfec_is_enabled(volatile struct cmac_block * cmac) {
  union cmac_rsfec_conf_enable rsfec = {._v = cmac->rsfec_conf_enable._v};
  return rsfec.ctl_rx_rsfec_enable && rsfec.ctl_tx_rsfec_enable;
}

bool cmac_rx_status_is_link_up(volatile struct cmac_block * cmac) {
  union cmac_stat_rx_status status;

  // Read twice to clear sticky bits.
  for (unsigned int i = 0; i < 2; ++i) {
    status._v = cmac->stat_rx_status._v;
  }

  return status.stat_rx_status && status.stat_rx_aligned;
}
