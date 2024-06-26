#ifndef SMARTNIC_BLOCK_COMPAT_H
#define SMARTNIC_BLOCK_COMPAT_H

#ifdef __cpluplus
extern "C" {
#endif

#include "esnet_smartnic_toplevel.h"

/* Emit old names in terms of the new ones. */
#ifdef INCLUDE_SMARTNIC_BLOCK_H
#define smartnic_322mhz_block smartnic_block
#define smartnic_322mhz_regs  smartnic_regs

#define smartnic_322mhz_igr_sw_tid              smartnic_igr_sw_tid
#define SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_0 SMARTNIC_IGR_SW_TID_VALUE_CMAC_0
#define SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_1 SMARTNIC_IGR_SW_TID_VALUE_CMAC_1
#define SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_0 SMARTNIC_IGR_SW_TID_VALUE_HOST_0
#define SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_1 SMARTNIC_IGR_SW_TID_VALUE_HOST_1

#define smartnic_322mhz_igr_sw_tdest                  smartnic_igr_sw_tdest
#define SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_BYPASS SMARTNIC_IGR_SW_TDEST_VALUE_APP_BYPASS
#define SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_DROP       SMARTNIC_IGR_SW_TDEST_VALUE_DROP
#define SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_0      SMARTNIC_IGR_SW_TDEST_VALUE_APP_0
#define SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_1      SMARTNIC_IGR_SW_TDEST_VALUE_APP_1

#define smartnic_322mhz_bypass_tdest              smartnic_bypass_tdest
#define SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_0 SMARTNIC_BYPASS_TDEST_VALUE_CMAC_0
#define SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_1 SMARTNIC_BYPASS_TDEST_VALUE_CMAC_1
#define SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_0 SMARTNIC_BYPASS_TDEST_VALUE_HOST_0
#define SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_1 SMARTNIC_BYPASS_TDEST_VALUE_HOST_1

#define smartnic_322mhz_app_0_tdest_remap              smartnic_app_0_tdest_remap
#define SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_0 SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_0
#define SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_1 SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_1
#define SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_0 SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_0
#define SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_1 SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_1

#define smartnic_322mhz_app_1_tdest_remap              smartnic_app_1_tdest_remap
#define SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_0 SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_0
#define SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_1 SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_1
#define SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_0 SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_0
#define SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_1 SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_1
#endif /* INCLUDE_SMARTNIC_BLOCK_H */

/* Emit new names in terms of the old ones. */
#ifdef INCLUDE_SMARTNIC_322MHZ_BLOCK_H
#define smartnic_block smartnic_322mhz_block
#define smartnic_regs  smartnic_322mhz_regs

#define smartnic_igr_sw_tid              smartnic_322mhz_igr_sw_tid
#define SMARTNIC_IGR_SW_TID_VALUE_CMAC_0 SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_0
#define SMARTNIC_IGR_SW_TID_VALUE_CMAC_1 SMARTNIC_322MHZ_IGR_SW_TID_VALUE_CMAC_1
#define SMARTNIC_IGR_SW_TID_VALUE_HOST_0 SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_0
#define SMARTNIC_IGR_SW_TID_VALUE_HOST_1 SMARTNIC_322MHZ_IGR_SW_TID_VALUE_HOST_1

#define smartnic_igr_sw_tdest                  smartnic_322mhz_igr_sw_tdest
#define SMARTNIC_IGR_SW_TDEST_VALUE_APP_BYPASS SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_BYPASS
#define SMARTNIC_IGR_SW_TDEST_VALUE_DROP       SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_DROP
#define SMARTNIC_IGR_SW_TDEST_VALUE_APP_0      SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_0
#define SMARTNIC_IGR_SW_TDEST_VALUE_APP_1      SMARTNIC_322MHZ_IGR_SW_TDEST_VALUE_APP_1

#define smartnic_bypass_tdest              smartnic_322mhz_bypass_tdest
#define SMARTNIC_BYPASS_TDEST_VALUE_CMAC_0 SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_0
#define SMARTNIC_BYPASS_TDEST_VALUE_CMAC_1 SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_CMAC_1
#define SMARTNIC_BYPASS_TDEST_VALUE_HOST_0 SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_0
#define SMARTNIC_BYPASS_TDEST_VALUE_HOST_1 SMARTNIC_322MHZ_BYPASS_TDEST_VALUE_HOST_1

#define smartnic_app_0_tdest_remap              smartnic_322mhz_app_0_tdest_remap
#define SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_0 SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_0
#define SMARTNIC_APP_0_TDEST_REMAP_VALUE_CMAC_1 SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_CMAC_1
#define SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_0 SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_0
#define SMARTNIC_APP_0_TDEST_REMAP_VALUE_HOST_1 SMARTNIC_322MHZ_APP_0_TDEST_REMAP_VALUE_HOST_1

#define smartnic_app_1_tdest_remap              smartnic_322mhz_app_1_tdest_remap
#define SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_0 SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_0
#define SMARTNIC_APP_1_TDEST_REMAP_VALUE_CMAC_1 SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_CMAC_1
#define SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_0 SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_0
#define SMARTNIC_APP_1_TDEST_REMAP_VALUE_HOST_1 SMARTNIC_322MHZ_APP_1_TDEST_REMAP_VALUE_HOST_1
#endif /* INCLUDE_SMARTNIC_322MHZ_BLOCK_H */

#ifdef __cpluplus
}
#endif
#endif /* SMARTNIC_BLOCK_COMPAT_H */
