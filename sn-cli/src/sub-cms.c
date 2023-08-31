#include <stdio.h>
#include <argp.h> /* argp_parse */
#include <stdbool.h> /* bool */
#include <string.h>  /* strcmp */
#include <stdlib.h>  /* malloc */
#include <inttypes.h>		/* strtoumax */
#include <unistd.h>		/* usleep */

#include "smartnic.h"		/* smartnic_* */
#include "unused.h"		/* UNUSED */
#include "array_size.h"		/* ARRAY_SIZE */
#include "memory-barriers.h"	/* barrier */
#include "arguments_common.h"	/* arguments_global, cmd_* */

static char doc_cms[] =
  "\n"
  "This subcommand is used to interact with the smartnic Card Management Subsystem (CMS) block"
  "\v"
  "--"
  ;

static char args_doc_cms[] = "(cardinfo | enable | disable | fan | power | sensors | status | temp | voltage)";

static struct argp_option argp_options_cms[] = {
  { "format", 'f',
    "FORMAT", 0,
    "Format of output (default: table)",
    0,
  },
  { 0, 0,
    0, 0,
    0,
    0,
  },
};

enum output_format {
  OUTPUT_FORMAT_TABLE = 0,
  OUTPUT_FORMAT_MAX, // Add entries above this line
};

struct arguments_cms {
  struct arguments_global* global;
  enum output_format output_format;
  char *command;
};

static error_t parse_opt_cms (int key, char *arg, struct argp_state *state)
{
  struct arguments_cms *arguments = state->input;

  switch(key) {
  case 'f':
    if (!strcmp(arg, "table")) {
      arguments->output_format = OUTPUT_FORMAT_TABLE;
    } else {
      // Unknown config file format
      argp_usage (state);
    }
    break;
  case ARGP_KEY_ARG:
    if (state->arg_num >= 1) {
      // Too many arguments
      argp_usage (state);
    }
    arguments->command = arg;
    break;
  case ARGP_KEY_END:
    if (state->arg_num < 1) {
      // Not enough arguments
      argp_usage (state);
    }
    break;
  case ARGP_KEY_SUCCESS:
  case ARGP_KEY_ERROR:
    // Always claim to have consumed all options and arguments to prevent the parent from reparsing
    // options after the subcommand
    state->next = state->argc;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

enum cms_ops {
	      CMS_OP_CARD_INFO_REQ = 4,
};

static void cms_mb_release_reset(volatile struct cms_block * cms)
{
  if (cms->mb_resetn_reg == 1) {
    // CMC is already out of reset, nothing to do
    return;
  }

  printf("Enabling CMC Microblaze (this takes 5s)\n");
  cms->mb_resetn_reg = 1;
  barrier();
  // Sleep for 5s to allow the embedded microblaze to boot fully
  // Without this, register reads can return old/stale values while
  // the microblaze is booting.
  usleep(5 * 1000 * 1000);
}

static void cms_mb_assert_reset(volatile struct cms_block *cms)
{
  printf("Disabling CMC Microblaze\n");
  cms->mb_resetn_reg = 0;
  barrier();
}

static void cms_wait_reg_map_ready(volatile struct cms_block * cms)
{
  union cms_host_status2_reg host_status2;
  do {
    host_status2._v = cms->host_status2_reg._v;
  } while (!host_status2.reg_map_ready);
}

static void cms_wait_mailbox_ready(volatile struct cms_block * cms)
{
  union cms_control_reg control;
  do {
    control._v = cms->control_reg._v;
  } while (control.mailbox_msg_status);
}

static void cms_set_mailbox_busy(volatile struct cms_block * cms)
{
  // Read current value
  union cms_control_reg control;
  control._v = cms->control_reg._v;
  // Set the busy bit
  control.mailbox_msg_status = 1;
  // Write back
  cms->control_reg._v = control._v;
  barrier();
}

enum cms_status_sc_mode {
			 CMS_STATUS_SC_MODE_UNKNOWN = 0,
			 CMS_STATUS_SC_MODE_NORMAL = 1,
			 CMS_STATUS_SC_MODE_BSL_MODE_UNSYNCED = 2,
			 CMS_STATUS_SC_MODE_BSL_MODE_SYNCED = 3,
			 CMS_STATUS_SC_MODE_BSL_MODE_SYNCED_SC_NOT_UPGRADABLE = 4,
			 CMS_STATUS_SC_MODE_NORMAL_SC_NOT_UPGRADABLE = 5,
};

static void cms_wait_for_sc_ready(volatile struct cms_block * cms)
{
  union cms_status_reg status;
  do {
    status._v = cms->status_reg._v;
  } while ((status.sat_ctrl_mode != CMS_STATUS_SC_MODE_NORMAL) &&
	   (status.sat_ctrl_mode != CMS_STATUS_SC_MODE_NORMAL_SC_NOT_UPGRADABLE));
}

static void cms_block_enable(volatile struct cms_block * cms)
{
  // Ensure that the microblaze is out of reset
  cms_mb_release_reset(cms);

  // Wait for reg map to be ready
  cms_wait_reg_map_ready(cms);

  // Wait for SC to be ready
  cms_wait_for_sc_ready(cms);

  // Wait for mailbox to be ready/available
  cms_wait_mailbox_ready(cms);
}

static void cms_block_disable(volatile struct cms_block *cms)
{
  cms_mb_assert_reset(cms);
}

static const char * sc_error_to_string(uint32_t sat_ctrl_err)
{
  switch (sat_ctrl_err) {
  case 0: return "No Error";
  case 1: return "SAT_COMMS_CHKSUM_ERR";
  case 2: return "SAT_COMMS_EOP_ERR";
  case 3: return "SAT_COMMS_SOP_ERR";
  case 4: return "SAT_COMMS_ESQ_SEQ_ERR";
  case 5: return "SAT_COMMS_BAD_MSG_ID";
  case 6: return "SAT_COMMS_BAD_VERSION";
  case 7: return "SAT_COMMS_RX_BUF_OVERFLOW";
  case 8: return "SAT_COMMS_BAD_SENSOR_ID";
  case 9: return "SAT_COMMS_NS_MSG_ID";
  case 10: return "SAT_COMMS_SC_FUN_ERR";
  case 11: return "SAT_COMMS_FAIL_TO_EN_BSL";
  default: return "Undocumented Error";
  }
}

static const char * host_msg_error_to_string(uint32_t host_msg_err)
{
  switch (host_msg_err) {
  case 0: return "CMS_HOST_MSG_NO_ERR";
  case 1: return "CMS_HOST_MSG_BAD_OPCODE_ERR";
  case 2: return "CMS_HOST_MSG_BRD_INFO_MISSING_ERR";
  case 3: return "CMS_HOST_MSG_LENGTH_ERR";
  case 4: return "CMS_HOST_MSG_SAT_FW_WRITE_FAIL";
  case 5: return "CMS_HOST_MSG_SAT_FW_UPDATE_FAIL";
  case 6: return "CMS_HOST_MSG_SAT_FW_LOAD_FAIL";
  case 7: return "CMS_HOST_MSG_SAT_FW_ERASE_FAIL";
  case 9: return "CMS_HOST_MSG_CSDR_FAILED";
  case 10: return "CMS_HOST_MSG_QSFP_FAIL";
  case 8:			/* Undocumented */
  default: return "Undocumented Error";
  }
}

static void print_status(volatile struct cms_block * cms)
{
  printf("\tMicroBlaze CPU : %s\n", cms->mb_resetn_reg ? "Running" : "Held in reset");

  printf("\tRegmap ID      : 0x%08x\n", cms->reg_map_id_reg);
  printf("\tCMS FW Version : %d.%d.%d (0x%08x)\n",
	 (cms->fw_version_reg & 0x00FF0000) >> 16,
	 (cms->fw_version_reg & 0x0000FF00) >>  8,
	 (cms->fw_version_reg & 0x000000FF) >>  0,
	 cms->fw_version_reg);

  union cms_status_reg status;
  status._v = cms->status_reg._v;
  printf("\tSC Mode        : %u\n", status.sat_ctrl_mode);
  printf("\tSC Comms Ver   : %u\n", status.sat_ctrl_comms_ver);

  union cms_error_reg error;
  error._v = cms->error_reg._v;
  printf("\tSC Error Status: SC %s, Packet %s\n",
	 error.sat_ctrl_err ? "Error" : "No Error",
	 error.pkt_error ? "Error" : "No Error");
  printf("\tSC Error Code  : %s\n", sc_error_to_string(error.sat_ctrl_err));

  uint32_t profile = cms->profile_name_reg;
  printf("\tProfile Name   : %c%c%c%c\n",
	 (profile & 0xFF000000) >> 24,
	 (profile & 0x00FF0000) >> 16,
	 (profile & 0x0000FF00) >>  8,
	 (profile & 0x000000FF) >>  0);

  union cms_control_reg control;
  control._v = cms->control_reg._v;
  printf("\tCMS Control    :\n");
  printf("\t  HBM Temp Mon : %s\n", control.hbm_temp_monitor_enable ? "Enabled" : "Disabled");
  printf("\t  QSFP GPIO    : %s (only needed on U200/U250)\n", control.qsfp_gpio_enable ? "Enabled" : "Disabled");
  printf("\t  Mailbox      : %s\n", control.mailbox_msg_status ? "Busy" : "Ready");

  union cms_host_status2_reg host_status2;
  host_status2._v = cms->host_status2_reg._v;
  printf("\tRegmap Ready   : %s\n", host_status2.reg_map_ready ? "Yes" : "No");

  printf("\tHost Msg Error : %s\n", host_msg_error_to_string(cms->host_msg_error_reg));

  printf("\tCore Build Ver : 0x%08x\n", cms->core_build_version_reg);
  printf("\tOEM ID         : 0x%08x\n", cms->oem_id_reg);
}

static float sensor2v(uint32_t sensor)
{
	return (float)sensor / 1000.0;
}

static void print_voltage(volatile struct cms_block * cms)
{
  // Ensure that the CMS block is fully enabled/ready
  cms_block_enable(cms);

  printf("\t12v PEX         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_12v_pex_ins_reg),
	 sensor2v(cms->_12v_pex_avg_reg),
	 sensor2v(cms->_12v_pex_max_reg));
  printf("\t3v3 PEX         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_3v3_pex_ins_reg),
	 sensor2v(cms->_3v3_pex_avg_reg),
	 sensor2v(cms->_3v3_pex_max_reg));
  printf("\t3v3 AUX         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_3v3_aux_ins_reg),
	 sensor2v(cms->_3v3_aux_avg_reg),
	 sensor2v(cms->_3v3_aux_max_reg));
  printf("\t12v AUX         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_12v_aux_ins_reg),
	 sensor2v(cms->_12v_aux_avg_reg),
	 sensor2v(cms->_12v_aux_max_reg));
  printf("\tDDR4 VPP Bottom : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->ddr4_vpp_btm_ins_reg),
	 sensor2v(cms->ddr4_vpp_btm_avg_reg),
	 sensor2v(cms->ddr4_vpp_btm_max_reg));
  printf("\tSYS 5v5         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->sys_5v5_ins_reg),
	 sensor2v(cms->sys_5v5_avg_reg),
	 sensor2v(cms->sys_5v5_max_reg));
  printf("\tVCC 1v2 Top     : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vcc1v2_top_ins_reg),
	 sensor2v(cms->vcc1v2_top_avg_reg),
	 sensor2v(cms->vcc1v2_top_max_reg));
  printf("\tVCC 1v8         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vcc1v8_ins_reg),
	 sensor2v(cms->vcc1v8_avg_reg),
	 sensor2v(cms->vcc1v8_max_reg));
  printf("\tVCC 0v85        : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vcc0v85_ins_reg),
	 sensor2v(cms->vcc0v85_avg_reg),
	 sensor2v(cms->vcc0v85_max_reg));
  printf("\tDDR4 VPP Top    : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->ddr4_vpp_top_ins_reg),
	 sensor2v(cms->ddr4_vpp_top_avg_reg),
	 sensor2v(cms->ddr4_vpp_top_max_reg));
  printf("\tMGT 0V9 AVCC    : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->mgt0v9avcc_ins_reg),
	 sensor2v(cms->mgt0v9avcc_avg_reg),
	 sensor2v(cms->mgt0v9avcc_max_reg));
  printf("\t12V SW          : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_12v_sw_ins_reg),
	 sensor2v(cms->_12v_sw_avg_reg),
	 sensor2v(cms->_12v_sw_max_reg));
  printf("\tMGT AVTT        : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->mgtavtt_ins_reg),
	 sensor2v(cms->mgtavtt_avg_reg),
	 sensor2v(cms->mgtavtt_max_reg));
  printf("\tVCC 1v2 Bottom  : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vcc1v2_btm_ins_reg),
	 sensor2v(cms->vcc1v2_btm_avg_reg),
	 sensor2v(cms->vcc1v2_btm_max_reg));
  printf("\tVCCINT          : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccint_ins_reg),
	 sensor2v(cms->vccint_avg_reg),
	 sensor2v(cms->vccint_max_reg));
  printf("\tVCC 3v3         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vcc3v3_ins_reg),
	 sensor2v(cms->vcc3v3_avg_reg),
	 sensor2v(cms->vcc3v3_max_reg));
  printf("\tHBM 1v2         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->hbm_1v2_ins_reg),
	 sensor2v(cms->hbm_1v2_avg_reg),
	 sensor2v(cms->hbm_1v2_max_reg));
  printf("\tVPP 2v5         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vpp2v5_ins_reg),
	 sensor2v(cms->vpp2v5_avg_reg),
	 sensor2v(cms->vpp2v5_max_reg));
  printf("\tVCCINT IO       : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccint_io_ins_reg),
	 sensor2v(cms->vccint_io_avg_reg),
	 sensor2v(cms->vccint_io_max_reg));
  printf("\t12v AUX1        : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_12v_aux1_ins_reg),
	 sensor2v(cms->_12v_aux1_avg_reg),
	 sensor2v(cms->_12v_aux1_max_reg));
  printf("\tVCC AUX         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccaux_ins_reg),
	 sensor2v(cms->vccaux_avg_reg),
	 sensor2v(cms->vccaux_max_reg));
  printf("\tVCC AUX PMC     : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccaux_pmc_ins_reg),
	 sensor2v(cms->vccaux_pmc_avg_reg),
	 sensor2v(cms->vccaux_pmc_max_reg));
  printf("\tVCC RAM         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccram_ins_reg),
	 sensor2v(cms->vccram_avg_reg),
	 sensor2v(cms->vccram_max_reg));
  printf("\tVCCINT VCU 0v9  : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccint_vcu_0v9_ins_reg),
	 sensor2v(cms->vccint_vcu_0v9_avg_reg),
	 sensor2v(cms->vccint_vcu_0v9_max_reg));
  printf("\tVCCIO 1v2       : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_1v2_vccio_ins_reg),
	 sensor2v(cms->_1v2_vccio_avg_reg),
	 sensor2v(cms->_1v2_vccio_max_reg));
  printf("\tGTAVCC          : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->gtavcc_ins_reg),
	 sensor2v(cms->gtavcc_avg_reg),
	 sensor2v(cms->gtavcc_max_reg));
  printf("\tVCCSOC          : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vccsoc_ins_reg),
	 sensor2v(cms->vccsoc_avg_reg),
	 sensor2v(cms->vccsoc_max_reg));
  printf("\tVCC 5v0         : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->vcc_5v0_ins_reg),
	 sensor2v(cms->vcc_5v0_avg_reg),
	 sensor2v(cms->vcc_5v0_max_reg));
  printf("\t2v5 VPP23       : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->_2v5_vpp23_ins_reg),
	 sensor2v(cms->_2v5_vpp23_avg_reg),
	 sensor2v(cms->_2v5_vpp23_max_reg));
  printf("\tGTVCC AUX       : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->gtvcc_aux_ins_reg),
	 sensor2v(cms->gtvcc_aux_avg_reg),
	 sensor2v(cms->gtvcc_aux_max_reg));
  printf("\tCMC VCC 1v5     : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->cmc_vcc1v5_ins_reg),
	 sensor2v(cms->cmc_vcc1v5_avg_reg),
	 sensor2v(cms->cmc_vcc1v5_max_reg));
  printf("\tCMC MGTAVCC     : %8.2f V (avg: %8.2f, max: %8.2f)\n",
	 sensor2v(cms->cmc_mgtavcc_ins_reg),
	 sensor2v(cms->cmc_mgtavcc_avg_reg),
	 sensor2v(cms->cmc_mgtavcc_max_reg));
}

static float sensor2i(uint32_t sensor)
{
  return (float)sensor / 1000.0;
}

static void print_power(volatile struct cms_block * cms)
{
  // Ensure that the CMS block is fully enabled/ready
  cms_block_enable(cms);

  printf("\t12V PEX I IN    : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->_12vpex_i_in_ins_reg),
	 sensor2i(cms->_12vpex_i_in_avg_reg),
	 sensor2i(cms->_12vpex_i_in_max_reg));
  printf("\t12V AUX I IN    : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->_12v_aux_i_in_ins_reg),
	 sensor2i(cms->_12v_aux_i_in_avg_reg),
	 sensor2i(cms->_12v_aux_i_in_max_reg));
  printf("\tVCCINT I        : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->vccint_i_ins_reg),
	 sensor2i(cms->vccint_i_avg_reg),
	 sensor2i(cms->vccint_i_max_reg));
  printf("\t3v3 PEX I IN    : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->_3v3pex_i_in_ins_reg),
	 sensor2i(cms->_3v3pex_i_in_avg_reg),
	 sensor2i(cms->_3v3pex_i_in_max_reg));
  printf("\tVCCINT IO I     : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->vccint_io_i_ins_reg),
	 sensor2i(cms->vccint_io_i_avg_reg),
	 sensor2i(cms->vccint_io_i_max_reg));
  printf("\tAUX 3v3 I       : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->aux_3v3_i_ins_reg),
	 sensor2i(cms->aux_3v3_i_avg_reg),
	 sensor2i(cms->aux_3v3_i_max_reg));
  printf("\tVCC 1v2 I       : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->vcc1v2_i_ins_reg),
	 sensor2i(cms->vcc1v2_i_avg_reg),
	 sensor2i(cms->vcc1v2_i_max_reg));
  printf("\tV12 IN I        : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->v12_in_i_ins_reg),
	 sensor2i(cms->v12_in_i_avg_reg),
	 sensor2i(cms->v12_in_i_max_reg));
  printf("\tV12 IN AUX0 I   : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->v12_in_aux0_i_ins_reg),
	 sensor2i(cms->v12_in_aux0_i_avg_reg),
	 sensor2i(cms->v12_in_aux0_i_max_reg));
  printf("\tV12 IN AUX1 I   : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->v12_in_aux1_i_ins_reg),
	 sensor2i(cms->v12_in_aux1_i_avg_reg),
	 sensor2i(cms->v12_in_aux1_i_max_reg));
  printf("\tHBM 1v2 I       : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->hbm_1v2_i_ins_reg),
	 sensor2i(cms->hbm_1v2_i_avg_reg),
	 sensor2i(cms->hbm_1v2_i_max_reg));
  printf("\tCMC MGTAVTT I   : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->cmc_mgtavtt_i_ins_reg),
	 sensor2i(cms->cmc_mgtavtt_i_avg_reg),
	 sensor2i(cms->cmc_mgtavtt_i_max_reg));
  printf("\tCMC MGTAVCC I   : %8.2f A (avg: %8.2f, max: %8.2f)\n",
	 sensor2i(cms->cmc_mgtavcc_i_ins_reg),
	 sensor2i(cms->cmc_mgtavcc_i_avg_reg),
	 sensor2i(cms->cmc_mgtavcc_i_max_reg));

  printf("\n");
  printf("\tTotal Power     : %8.2f W (avg: %8.2f)\n",
	 (sensor2i(cms->_12vpex_i_in_ins_reg)  * sensor2v(cms->_12v_pex_ins_reg)) +
	 (sensor2i(cms->_12v_aux_i_in_ins_reg) * sensor2v(cms->_12v_aux_ins_reg)) +
	 (sensor2i(cms->_3v3pex_i_in_ins_reg)  * sensor2v(cms->_3v3_pex_ins_reg)) +
	 (sensor2i(cms->aux_3v3_i_ins_reg)     * sensor2v(cms->_3v3_aux_ins_reg)),

	 (sensor2i(cms->_12vpex_i_in_avg_reg)  * sensor2v(cms->_12v_pex_avg_reg)) +
	 (sensor2i(cms->_12v_aux_i_in_avg_reg) * sensor2v(cms->_12v_aux_avg_reg)) +
	 (sensor2i(cms->_3v3pex_i_in_avg_reg)  * sensor2v(cms->_3v3_pex_avg_reg)) +
	 (sensor2i(cms->aux_3v3_i_avg_reg)     * sensor2v(cms->_3v3_aux_avg_reg)));
}

static void print_temp(volatile struct cms_block * cms)
{
  // Ensure that the CMS block is fully enabled/ready
  cms_block_enable(cms);

  printf("\tFPGA Temp       : %2u C (avg: %2u, max: %2u)\n",
	 cms->fpga_temp_ins_reg,
	 cms->fpga_temp_avg_reg,
	 cms->fpga_temp_max_reg);
  printf("\tFan Temp        : %2u C (avg: %2u, max: %2u)\n",
	 cms->fan_temp_ins_reg,
	 cms->fan_temp_avg_reg,
	 cms->fan_temp_max_reg);
  printf("\tDIMM 0 Temp     : %2u C (avg: %2u, max: %2u)\n",
	 cms->dimm_temp0_ins_reg,
	 cms->dimm_temp0_avg_reg,
	 cms->dimm_temp0_max_reg);
  printf("\tDIMM 1 Temp     : %2u C (avg: %2u, max: %2u)\n",
	 cms->dimm_temp1_ins_reg,
	 cms->dimm_temp1_avg_reg,
	 cms->dimm_temp1_max_reg);
  printf("\tDIMM 2 Temp     : %2u C (avg: %2u, max: %2u)\n",
	 cms->dimm_temp2_ins_reg,
	 cms->dimm_temp2_avg_reg,
	 cms->dimm_temp2_max_reg);
  printf("\tDIMM 3 Temp     : %2u C (avg: %2u, max: %2u)\n",
	 cms->dimm_temp3_ins_reg,
	 cms->dimm_temp3_avg_reg,
	 cms->dimm_temp3_max_reg);
  printf("\tSE98 Temp 0     : %2u C (avg: %2u, max: %2u)\n",
	 cms->se98_temp0_ins_reg,
	 cms->se98_temp0_avg_reg,
	 cms->se98_temp0_max_reg);
  printf("\tSE98 Temp 1     : %2u C (avg: %2u, max: %2u)\n",
	 cms->se98_temp1_ins_reg,
	 cms->se98_temp1_avg_reg,
	 cms->se98_temp1_max_reg);
  printf("\tSE98 Temp 2     : %2u C (avg: %2u, max: %2u)\n",
	 cms->se98_temp2_ins_reg,
	 cms->se98_temp2_avg_reg,
	 cms->se98_temp2_max_reg);
  printf("\tQSFP Cage 0 Temp: %2u C (avg: %2u, max: %2u)\n",
	 cms->cage_temp0_ins_reg,
	 cms->cage_temp0_avg_reg,
	 cms->cage_temp0_max_reg);
  printf("\tQSFP Cage 1 Temp: %2u C (avg: %2u, max: %2u)\n",
	 cms->cage_temp1_ins_reg,
	 cms->cage_temp1_avg_reg,
	 cms->cage_temp1_max_reg);
  printf("\tQSFP Cage 2 Temp: %2u C (avg: %2u, max: %2u)\n",
	 cms->cage_temp2_ins_reg,
	 cms->cage_temp2_avg_reg,
	 cms->cage_temp2_max_reg);
  printf("\tQSFP Cage 3 Temp: %2u C (avg: %2u, max: %2u)\n",
	 cms->cage_temp3_ins_reg,
	 cms->cage_temp3_avg_reg,
	 cms->cage_temp3_max_reg);
  printf("\tHBM 1 Temp      : %2u C (avg: %2u, max: %2u)\n",
	 cms->hbm_temp1_ins_reg,
	 cms->hbm_temp1_avg_reg,
	 cms->hbm_temp1_max_reg);
  printf("\tHBM 2 Temp      : %2u C (avg: %2u, max: %2u)\n",
	 cms->hbm_temp2_ins_reg,
	 cms->hbm_temp2_avg_reg,
	 cms->hbm_temp2_max_reg);
  printf("\tVCCINT IO Temp  : %2u C (avg: %2u, max: %2u)\n",
	 cms->vccint_io_ins_reg,
	 cms->vccint_io_avg_reg,
	 cms->vccint_io_max_reg);
  printf("\tVCCINT Temp     : %2u C (avg: %2u, max: %2u)\n",
	 cms->vccint_temp_ins_reg,
	 cms->vccint_temp_avg_reg,
	 cms->vccint_temp_max_reg);
}

static void print_fan(volatile struct cms_block * cms)
{
  // Ensure that the CMS block is fully enabled/ready
  cms_block_enable(cms);

  printf("\tFan Speed       : %u RPM (avg: %u, max: %u)\n",
	 cms->fan_speed_ins_reg,
	 cms->fan_speed_avg_reg,
	 cms->fan_speed_max_reg);
}

static const char * total_power_avail_to_string(uint8_t total_power_avail)
{
  switch (total_power_avail) {
  case 0: return "75W";
  case 1: return "150W";
  case 2: return "225W";
  case 3: return "300W";
  default: return "??";
  }
}

static const char * config_mode_to_string(uint8_t config_mode)
{
  switch (config_mode) {
  case 0: return "Slave Serial x1";
  case 1: return "Slave Select Map x8";
  case 2: return "Slave Map x16";
  case 3: return "Slave Select Map x32";
  case 4: return "JTAG Boundary Scan x1";
  case 5: return "Master SPI x1";
  case 6: return "Master SPI x2";
  case 7: return "Master SPI x4";
  case 8: return "Master SPI x8";
  case 9: return "Master BPI x8";
  case 10: return "Master BPI x16";
  case 11: return "Master Serial x1";
  case 12: return "Master Select Map x8";
  case 13: return "Master Select Map x16";
  default: return "??";
  }
}

static const char * cage_type_to_string(uint8_t cage_type)
{
  switch (cage_type) {
  case 0: return "QSFP/QSFP+";
  case 1: return "DSFP";
  case 2: return "SFP/SFP+";
  default: return "??";
  }
}

static void print_cardinfo(volatile struct cms_block * cms)
{
  // Ensure that the CMS block is fully enabled/ready
  cms_block_enable(cms);

  volatile uint32_t * mailbox = (uint32_t *)(((uint8_t *)&cms->reg_map_id_reg) + cms->host_msg_offset_reg);

  // Write the card info request opcode and trigger the operation
  mailbox[0] = CMS_OP_CARD_INFO_REQ << 24;
  barrier();
  cms_set_mailbox_busy(cms);

  // Wait for mailbox to be ready/available again (ie. opcode is processed by SC)
  cms_wait_mailbox_ready(cms);

  // Read out the response into a buffer
  uint32_t rsp_len_bytes = mailbox[0] & 0xFFF;
  uint32_t rsp_len_words = (rsp_len_bytes + 3) / sizeof(uint32_t); /* round up to catch partial last word */
  uint8_t * rsp_buf = calloc(rsp_len_words, sizeof(uint32_t));

  uint8_t * rsp_cursor = rsp_buf;
  for (uint32_t i = 0; i < rsp_len_words; i++) {
    uint32_t mb_word = mailbox[1 + i];
    *rsp_cursor++ = (mb_word & 0x000000FF) >>  0;
    *rsp_cursor++ = (mb_word & 0x0000FF00) >>  8;
    *rsp_cursor++ = (mb_word & 0x00FF0000) >> 16;
    *rsp_cursor++ = (mb_word & 0xFF000000) >> 24;
  }

  // Process the card info response TLVs
  struct tlv {
    uint8_t t;
    uint8_t l;
    uint8_t v[];
  } __attribute((packed));
  struct tlv * tlv = (struct tlv *)rsp_buf;
  do {
    switch (tlv->t) {
    case 0x21:
      printf("\tCard Serial: %s\n", tlv->v);
      break;
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
      printf("\tMAC: [%u] %s\n", (tlv->t - 0x22), tlv->v);
      break;
    case 0x26:
      printf("\tCard Revision: %s\n", tlv->v);
      break;
    case 0x27:
      printf("\tCard Name: %s\n", tlv->v);
      break;
    case 0x28:
      printf("\tSC Version: %s\n", tlv->v);
      break;
    case 0x29:
      printf("\tTotal Power Available: %s\n", total_power_avail_to_string(tlv->v[0]));
      break;
    case 0x2A:
      printf("\tFan Presence: %c\n", tlv->v[0]);
      break;
    case 0x2B:
      printf("\tConfig Mode: %s\n", config_mode_to_string(tlv->v[0]));
      break;
    case 0x4B:
      printf("\tMAC Block: count=%u, base=%02x:%02x:%02x:%02x:%02x:%02x\n",
	     tlv->v[0],
	     tlv->v[2],
	     tlv->v[3],
	     tlv->v[4],
	     tlv->v[5],
	     tlv->v[6],
	     tlv->v[7]);
      break;
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
      printf("\tCage Type: [%u] %s\n", (tlv->t - 0x50), cage_type_to_string(tlv->v[0]));
      break;
    default:
      printf("\tUnknown TLV 0x%02x Length %d Skipped\n", tlv->t, tlv->l);
      break;
    }
    // Move to the next TLV
    if (tlv->l == 0) {
      printf("\tFound TLV length of zero, bailing out\n");
      return;
    }
    // Move to the next entry (skip header + payload)
    tlv = (struct tlv *)((uint8_t *)tlv + tlv->l + 2);
  } while ((uint8_t *)tlv < rsp_buf + rsp_len_bytes);
}

void cmd_cms(struct argp_state *state)
{
  struct arguments_cms arguments = {0,};
  int  argc   = state->argc - state->next +1;
  char **argv = &state->argv[state->next - 1];
  char *argv0 = argv[0];

  arguments.global = state->input;

  struct argp argp_cms = {
    .options  = argp_options_cms,
    .parser   = parse_opt_cms,
    .args_doc = args_doc_cms,
    .doc      = doc_cms,
  };

  // Temporarily override argv[0] to make error messages show the correct prefix
  argv[0] = malloc(strlen(state->name) + strlen(" cms") + 1);
  if (!argv[0]) {
    argp_failure(state, 1, ENOMEM, 0);
  }
  sprintf(argv[0], "%s cms", state->name);

  // Invoke the subcommand parser
  argp_parse (&argp_cms, argc, argv, ARGP_IN_ORDER, &argc, &arguments);

  // Restore the previous argv[0]
  free(argv[0]);
  argv[0] = argv0;

  // Always claim to have consumed all options in the subcommand to prevent the parent from reparsing
  // any options that occur after the subcommand
  state->next += argc - 1;

  // Make sure we've been given a PCIe address to work with
  if (arguments.global->pcieaddr == NULL) {
    fprintf(stderr, "ERROR: PCIe address is required but has not been provided\n");
    return;
  }

  // Map the FPGA register space
  volatile struct esnet_smartnic_bar2 * bar2;
  bar2 = smartnic_map_bar2_by_pciaddr(arguments.global->pcieaddr);
  if (bar2 == NULL) {
    fprintf(stderr, "ERROR: failed to mmap FPGA register space for device %s\n", arguments.global->pcieaddr);
    return;
  }

  if (!strcmp(arguments.command, "cardinfo")) {
    print_cardinfo(&bar2->cms);
  } else if (!strcmp(arguments.command, "disable")) {
    cms_block_disable(&bar2->cms);
  } else if (!strcmp(arguments.command, "enable")) {
    cms_block_enable(&bar2->cms);
  } else if (!strcmp(arguments.command, "fan")) {
    print_fan(&bar2->cms);
  } else if (!strcmp(arguments.command, "power")) {
    print_power(&bar2->cms);
  } else if (!strcmp(arguments.command, "sensors")) {
    printf("Voltage\n");
    print_voltage(&bar2->cms);
    printf("Power\n");
    print_power(&bar2->cms);
    printf("Temperature\n");
    print_temp(&bar2->cms);
    printf("Fan\n");
    print_fan(&bar2->cms);
  } else if (!strcmp(arguments.command, "status")) {
    print_status(&bar2->cms);
  } else if (!strcmp(arguments.command, "temp")) {
    print_temp(&bar2->cms);
  } else if (!strcmp(arguments.command, "voltage")) {
    print_voltage(&bar2->cms);
  } else {
    fprintf(stderr, "ERROR: %s is not a valid command\n", arguments.command);
  }

  return;
}

