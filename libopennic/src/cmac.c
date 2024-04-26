#include "cmac.h"		/* API */
#include "memory-barriers.h"	/* barrier() */

#include "array_size.h"
#include "stats.h"
#include <string.h>

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

#define CMAC_STATS_COUNTER(_name) \
    STATS_COUNTER_SPEC(struct cmac_block, stat_##_name, #_name, NULL, \
                       STATS_COUNTER_FLAG_MASK(CLEAR_ON_READ), NULL)

static const struct stats_counter_spec cmac_stats_counters[] = {
  CMAC_STATS_COUNTER(rx_bip_err_0),
  CMAC_STATS_COUNTER(rx_bip_err_1),
  CMAC_STATS_COUNTER(rx_bip_err_2),
  CMAC_STATS_COUNTER(rx_bip_err_3),
  CMAC_STATS_COUNTER(rx_bip_err_4),
  CMAC_STATS_COUNTER(rx_bip_err_5),
  CMAC_STATS_COUNTER(rx_bip_err_6),
  CMAC_STATS_COUNTER(rx_bip_err_7),
  CMAC_STATS_COUNTER(rx_bip_err_8),
  CMAC_STATS_COUNTER(rx_bip_err_9),
  CMAC_STATS_COUNTER(rx_bip_err_10),
  CMAC_STATS_COUNTER(rx_bip_err_11),
  CMAC_STATS_COUNTER(rx_bip_err_12),
  CMAC_STATS_COUNTER(rx_bip_err_13),
  CMAC_STATS_COUNTER(rx_bip_err_14),
  CMAC_STATS_COUNTER(rx_bip_err_15),
  CMAC_STATS_COUNTER(rx_bip_err_16),
  CMAC_STATS_COUNTER(rx_bip_err_17),
  CMAC_STATS_COUNTER(rx_bip_err_18),
  CMAC_STATS_COUNTER(rx_bip_err_19),
  CMAC_STATS_COUNTER(rx_framing_err_0),
  CMAC_STATS_COUNTER(rx_framing_err_1),
  CMAC_STATS_COUNTER(rx_framing_err_2),
  CMAC_STATS_COUNTER(rx_framing_err_3),
  CMAC_STATS_COUNTER(rx_framing_err_4),
  CMAC_STATS_COUNTER(rx_framing_err_5),
  CMAC_STATS_COUNTER(rx_framing_err_6),
  CMAC_STATS_COUNTER(rx_framing_err_7),
  CMAC_STATS_COUNTER(rx_framing_err_8),
  CMAC_STATS_COUNTER(rx_framing_err_9),
  CMAC_STATS_COUNTER(rx_framing_err_10),
  CMAC_STATS_COUNTER(rx_framing_err_11),
  CMAC_STATS_COUNTER(rx_framing_err_12),
  CMAC_STATS_COUNTER(rx_framing_err_13),
  CMAC_STATS_COUNTER(rx_framing_err_14),
  CMAC_STATS_COUNTER(rx_framing_err_15),
  CMAC_STATS_COUNTER(rx_framing_err_16),
  CMAC_STATS_COUNTER(rx_framing_err_17),
  CMAC_STATS_COUNTER(rx_framing_err_18),
  CMAC_STATS_COUNTER(rx_framing_err_19),
  CMAC_STATS_COUNTER(rx_bad_code),
  CMAC_STATS_COUNTER(tx_frame_error),
  CMAC_STATS_COUNTER(tx_total_pkts),
  CMAC_STATS_COUNTER(tx_total_good_pkts),
  CMAC_STATS_COUNTER(tx_total_bytes),
  CMAC_STATS_COUNTER(tx_total_good_bytes),
  CMAC_STATS_COUNTER(tx_pkt_64_bytes),
  CMAC_STATS_COUNTER(tx_pkt_65_127_bytes),
  CMAC_STATS_COUNTER(tx_pkt_128_255_bytes),
  CMAC_STATS_COUNTER(tx_pkt_256_511_bytes),
  CMAC_STATS_COUNTER(tx_pkt_512_1023_bytes),
  CMAC_STATS_COUNTER(tx_pkt_1024_1518_bytes),
  CMAC_STATS_COUNTER(tx_pkt_1519_1522_bytes),
  CMAC_STATS_COUNTER(tx_pkt_1523_1548_bytes),
  CMAC_STATS_COUNTER(tx_pkt_1549_2047_bytes),
  CMAC_STATS_COUNTER(tx_pkt_2048_4095_bytes),
  CMAC_STATS_COUNTER(tx_pkt_4096_8191_bytes),
  CMAC_STATS_COUNTER(tx_pkt_8192_9215_bytes),
  CMAC_STATS_COUNTER(tx_pkt_large),
  CMAC_STATS_COUNTER(tx_pkt_small),
  CMAC_STATS_COUNTER(tx_bad_fcs),
  CMAC_STATS_COUNTER(tx_unicast),
  CMAC_STATS_COUNTER(tx_multicast),
  CMAC_STATS_COUNTER(tx_broadcast),
  CMAC_STATS_COUNTER(tx_vlan),
  CMAC_STATS_COUNTER(tx_pause),
  CMAC_STATS_COUNTER(tx_user_pause),
  CMAC_STATS_COUNTER(rx_total_pkts),
  CMAC_STATS_COUNTER(rx_total_good_pkts),
  CMAC_STATS_COUNTER(rx_total_bytes),
  CMAC_STATS_COUNTER(rx_total_good_bytes),
  CMAC_STATS_COUNTER(rx_pkt_64_bytes),
  CMAC_STATS_COUNTER(rx_pkt_65_127_bytes),
  CMAC_STATS_COUNTER(rx_pkt_128_255_bytes),
  CMAC_STATS_COUNTER(rx_pkt_256_511_bytes),
  CMAC_STATS_COUNTER(rx_pkt_512_1023_bytes),
  CMAC_STATS_COUNTER(rx_pkt_1024_1518_bytes),
  CMAC_STATS_COUNTER(rx_pkt_1519_1522_bytes),
  CMAC_STATS_COUNTER(rx_pkt_1523_1548_bytes),
  CMAC_STATS_COUNTER(rx_pkt_1549_2047_bytes),
  CMAC_STATS_COUNTER(rx_pkt_2048_4095_bytes),
  CMAC_STATS_COUNTER(rx_pkt_4096_8191_bytes),
  CMAC_STATS_COUNTER(rx_pkt_8192_9215_bytes),
  CMAC_STATS_COUNTER(rx_pkt_large),
  CMAC_STATS_COUNTER(rx_pkt_small),
  CMAC_STATS_COUNTER(rx_undersize),
  CMAC_STATS_COUNTER(rx_fragment),
  CMAC_STATS_COUNTER(rx_oversize),
  CMAC_STATS_COUNTER(rx_toolong),
  CMAC_STATS_COUNTER(rx_jabber),
  CMAC_STATS_COUNTER(rx_bad_fcs),
  CMAC_STATS_COUNTER(rx_pkt_bad_fcs),
  CMAC_STATS_COUNTER(rx_stomped_fcs),
  CMAC_STATS_COUNTER(rx_unicast),
  CMAC_STATS_COUNTER(rx_multicast),
  CMAC_STATS_COUNTER(rx_broadcast),
  CMAC_STATS_COUNTER(rx_vlan),
  CMAC_STATS_COUNTER(rx_pause),
  CMAC_STATS_COUNTER(rx_user_pause),
  CMAC_STATS_COUNTER(rx_inrangeerr),
  CMAC_STATS_COUNTER(rx_truncated),
#if 0
  // Present in the regmap, but not implemented.
  CMAC_STATS_COUNTER(otn_tx_jabber),
  CMAC_STATS_COUNTER(otn_tx_oversize),
  CMAC_STATS_COUNTER(otn_tx_undersize),
  CMAC_STATS_COUNTER(otn_tx_toolong),
  CMAC_STATS_COUNTER(otn_tx_fragment),
  CMAC_STATS_COUNTER(otn_tx_pkt_bad_fcs),
  CMAC_STATS_COUNTER(otn_tx_stomped_fcs),
  CMAC_STATS_COUNTER(otn_tx_bad_code),
#endif
};

static void cmac_stats_latch_counters(const struct stats_block_spec * bspec) {
    volatile struct cmac_block * cmac = bspec->base;

    cmac->tick = 1;
    barrier();
}

struct stats_zone* cmac_stats_zone_alloc(struct stats_domain * domain,
                                         volatile struct cmac_block * cmac,
                                         const char * name) {
  struct stats_block_spec bspecs[] = {
      {
          .name = "cmac",
          .base = cmac,
          .counters = cmac_stats_counters,
          .ncounters = ARRAY_SIZE(cmac_stats_counters),
          .latch_counters = cmac_stats_latch_counters,
      },
  };
  struct stats_zone_spec zspec = {
      .name = name,
      .blocks = bspecs,
      .nblocks = ARRAY_SIZE(bspecs),
  };
  return stats_zone_alloc(domain, &zspec);
}

void cmac_stats_zone_free(struct stats_zone * zone) {
    stats_zone_free(zone);
}
