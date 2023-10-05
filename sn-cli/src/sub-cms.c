#include <stdio.h>
#include <argp.h> /* argp_parse */
#include <stdbool.h> /* bool */
#include <string.h>  /* strcmp */
#include <stdlib.h>  /* malloc */
#include <inttypes.h>		/* strtoumax */
#include <unistd.h>		/* usleep */
#include <math.h>		/* log10 */

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

static char args_doc_cms[] = "(cardinfo | enable | disable | fan | power | qsfpdump | sensors | status | temp | voltage)";

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
	      CMS_OP_CARD_INFO_REQ             = 0x04,
	      CMS_OP_BLOCK_READ_MODULE_I2C     = 0x0B,
	      CMS_OP_READ_MODULE_LOW_SPEED_IO  = 0x0D,
	      CMS_OP_WRITE_MODULE_LOW_SPEED_IO = 0x0E,
	      CMS_OP_BYTE_READ_MODULE_I2C      = 0x0F,
	      CMS_OP_BYTE_WRITE_MODULE_I2C     = 0x10,
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

static union cms_error_reg cms_get_error_reg(volatile struct cms_block *cms)
{
  return cms->error_reg;
}

static void cms_clear_error_reg(volatile struct cms_block *cms)
{
  union cms_control_reg control;
  control._v = cms->control_reg._v;
  control.reset_error_reg = 1;
  cms->control_reg._v = control._v;

  barrier();
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

static bool read_qsfp_page(volatile struct cms_block * cms, uint8_t cage_sel, uint8_t page_sel, bool upper_sel, uint8_t bank_sel, uint8_t rsp_buf[128])
{
  if (!rsp_buf) return false;
  if (cage_sel > 1) return false;

  // Ensure that the CMS block is fully enabled/ready
  cms_block_enable(cms);

  volatile uint32_t * mailbox = (uint32_t *)(((uint8_t *)&cms->reg_map_id_reg) + cms->host_msg_offset_reg);

  // Write the card info request opcode and trigger the operation
  mailbox[0] = CMS_OP_BLOCK_READ_MODULE_I2C << 24;
  mailbox[1] = cage_sel;
  mailbox[2] = page_sel;
  bool bank_valid;
  if (page_sel >= 0x10 && page_sel <= 0x1F) {
    // CMIS pages are banked
    // NOTE: only 32 (out of 256) banks are supported via the CMS block but CMIS only defines 4 used so probably OK
    bank_valid = true;
  } else {
    bank_valid = false;
    bank_sel = 0;
  }
  mailbox[3] = (0 |
		((bank_sel & 0x1F) << 18) |
		((bank_valid ? 0x1 : 0x0) << 17) |
		((upper_sel ? 0x1 : 0x0) << 0));
  barrier();
  cms_set_mailbox_busy(cms);

  // Wait for mailbox to be ready/available again (ie. opcode is processed by SC)
  cms_wait_mailbox_ready(cms);

  // Check if the read succeeded or failed
  union cms_error_reg cms_error = cms_get_error_reg(cms);
  if (cms_error.sat_ctrl_err != 0) {
    printf("SC Error: %u (%s)\n", cms_error.sat_ctrl_err, sc_error_to_string(cms_error.sat_ctrl_err));
    cms_clear_error_reg(cms);
    return false;
  }
  if (cms_error.pkt_error != 0) {
    printf("SC Packet Error: %u\n", cms_error.pkt_error);
    cms_clear_error_reg(cms);
    return false;
  }

  // Read out the response into a buffer
  uint32_t rsp_len_bytes = mailbox[4];
  uint32_t rsp_len_words = (rsp_len_bytes + 3) / sizeof(uint32_t); /* round up to catch partial last word */
  if (rsp_len_words > 128/4) {
    return false;
  }

  uint8_t * rsp_cursor = rsp_buf;
  for (uint32_t i = 0; i < rsp_len_words; i++) {
    uint32_t mb_word = mailbox[5 + i];
    *rsp_cursor++ = (mb_word & 0x000000FF) >>  0;
    *rsp_cursor++ = (mb_word & 0x0000FF00) >>  8;
    *rsp_cursor++ = (mb_word & 0x00FF0000) >> 16;
    *rsp_cursor++ = (mb_word & 0xFF000000) >> 24;
  }

  return true;
}

// See SFF-8024 Rev 4.10 Table 4-1
static const char * sff_identifier_to_string(uint8_t identifier)
{
  switch (identifier) {
  case 0x00: return "Unknown or unspecified";
  case 0x01: return "GBIC";
  case 0x02: return "Module/connector soldered to motherboard (using SFF-8472)";
  case 0x03: return "SFP/SFP+/SFP28 and later with SFF-8472 management interface";
  case 0x04: return "300 pin XBI";
  case 0x05: return "XENPAK";
  case 0x06: return "XFP";
  case 0x07: return "XFF";
  case 0x08: return "XFP-E";
  case 0x09: return "XPAK";
  case 0x0a: return "X2";
  case 0x0b: return "DWDM-SFP/SFP+ (not using SFF-8472)";
  case 0x0c: return "QSFP (INF-8438)";
  case 0x0d: return "QSFP+ or later with SFF-8636 or SFF-8436 management interface (SFF-8436, SFF-8635, SFF-8665, SFF-8685 et al.)";
  case 0x0e: return "CXP or later";
  case 0x0f: return "Shielded Mini Multilane HD 4X";
  case 0x10: return "Shielded Mini Multilane HD 8X";
  case 0x11: return "QSFP28 or later with SFF-8636 management interface (SFF-8665 et al.)";
  case 0x12: return "CXP2 (aka CXP28) or later";
  case 0x13: return "CDFP (Style 1/Style2)";
  case 0x14: return "Shielded Mini Multilane HD 4X Fanout Cable";
  case 0x15: return "Shielded Mini Multilane HD 8X Fanout Cable";
  case 0x16: return "CDFP (Style 3)";
  case 0x17: return "microQSFP";
  case 0x18: return "QSFP-DD Double Density 8X Pluggable Transceiver (INF-8628)";
  case 0x19: return "OSFP 8X Pluggable Transceiver";
  case 0x1a: return "SFP-DD Double Density 2X Pluggable Transceiver with SFP-DD Management Interface Specification";
  case 0x1b: return "DSFP Dual Small Form Factor Pluggable Transceiver";
  case 0x1c: return "x4 MiniLink/OcuLink";
  case 0x1d: return "x8 MiniLink";
  case 0x1e: return "QSFP+ or later with Common Management Interface Specification (CMIS)";
  case 0x1f: return "SFP-DD Double Density 2X Pluggable Transceiver with Common Management Interface Specification (CMIS)";
  case 0x20: return "SFP+ and later with Common Management Interface Specification (CMIS)";
  default:
    if (identifier < 0x80) {
      return "Reserved";
    } else {
      return "Vendor Specific";
    }
    break;
  }

  return "????";
}

static const char * sff_revision_compliance_to_string(uint8_t revision)
{
  switch (revision) {
  case 0x00: return "Revision not specified";
  case 0x01: return "SFF-8436 Rev 4.8 or earlier";
  case 0x02: return "Includes functionality described in revision 4.8 or earlier of SFF-8436";
  case 0x03: return "SFF-8636 Rev 1.3 or earlier";
  case 0x04: return "SFF-8636 Rev 1.4";
  case 0x05: return "SFF-8636 Rev 1.5";
  case 0x06: return "SFF-8636 Rev 2.0";
  case 0x07: return "SFF-8636 Rev 2.5, 2.6 and 2.7";
  case 0x08: return "SFF-8636 Rev 2.8, 2.9 and 2.10";
  default:   return "Reserved";
  }
}

static const char * sff_power_class_1to4_to_string(uint8_t power_class)
{
  switch (power_class) {
  case 0x00: return "Power Class 1 (1.5 W max.)";
  case 0x01: return "Power Class 2 (2.0 W max.)";
  case 0x02: return "Power Class 3 (2.5 W max.)";
  case 0x03: return "Power Class 4 (3.5 W max.) and Power Classes 5, 6 or 7";
  default:   return "????";
  }
}

static const char * sff_power_class_4to7_to_string(uint8_t power_class)
{
  switch (power_class) {
  case 0x00: return "Power Classes 1 to 4";
  case 0x01: return "Power Class 5 (4.0 W max.)";
  case 0x02: return "Power Class 6 (4.5 W max.)";
  case 0x03: return "Power Class 7 (5.0 W max.)";
  default:   return "????";
  }
}

static const char * sff_connector_type_to_string(uint8_t connector_type)
{
  switch (connector_type) {
  case 0x00: return "Unknown or unspecified";
  case 0x01: return "SC (Subscriber Connector)";
  case 0x02: return "Fibre Channel Style 1 copper connector";
  case 0x03: return "Fibre Channel Style 2 copper connector";
  case 0x04: return "BNC/TNC (Bayonet/Threaded Neill-Concelman)";
  case 0x05: return "Fibre Channel coax headers";
  case 0x06: return "Fiber Jack";
  case 0x07: return "LC (Lucent Connector)";
  case 0x08: return "MT-RJ (Mechanical Transfer – Registered Jack)";
  case 0x09: return "MU (Multiple Optical)";
  case 0x0a: return "SG";
  case 0x0b: return "Optical Pigtail";
  case 0x0c: return "MPO 1x12 (Multifiber Parallel Optic)";
  case 0x0d: return "MPO 2x16";
    // 0x0E - 0x1F Reserved
  case 0x20: return "HSSDC II (High Speed Serial Data Connector)";
  case 0x21: return "Copper pigtail";
  case 0x22: return "RJ45 (Registered Jack)";
  case 0x23: return "No separable connector";
  case 0x24: return "MXC 2x16";
  case 0x25: return "CS optical connector";
  case 0x26: return "SN (previously Mini CS) optical connector";
  case 0x27: return "MPO 2x12";
  case 0x28: return "MPO 1x16";
    // 0x29 - 0x7F Reserved
    // 0x80 - 0xFF Vendor Specific
  default:
    if (connector_type <= 0x80) {
      return "Reserved";
    } else {
      return "Vendor Specific";
    }
  }
}

static const char * sff_transmitter_technology_to_string(uint8_t transmitter_technology)
{
  switch (transmitter_technology) {
  case 0x0: return "850 nm VCSEL";
  case 0x1: return "1310 nm VCSEL";
  case 0x2: return "1550 nm VCSEL";
  case 0x3: return "1310 nm FP";
  case 0x4: return "1310 nm DFB";
  case 0x5: return "1550 nm DFB";
  case 0x6: return "1310 nm EML";
  case 0x7: return "1550 nm EML";
  case 0x8: return "Other / Undefined";
  case 0x9: return "1490 nm DFB";
  case 0xa: return "Copper cable unequalized";
  case 0xb: return "Copper cable passive equalized";
  case 0xc: return "Copper cable, near and far end limiting active equalizers";
  case 0xd: return "Copper cable, far end limiting active equalizers";
  case 0xe: return "Copper cable, near end limiting active equalizers";
  case 0xf: return "Copper cable, linear active equalizers";
  default:  return "????";
  }
}

static const char * sff_encoding_to_string(uint8_t encoding)
{
  switch (encoding) {
  case 0x00: return ("Unspecified");
  case 0x01: return ("8B/10B");
  case 0x02: return ("4B/5B");
  case 0x03: return ("NRZ");
  case 0x04: return ("SONET Scrambled");
  case 0x05: return ("64B/66B");
  case 0x06: return ("Manchester");
  case 0x07: return ("256B/257B (transcoded FEC-enabled data)");
  case 0x08: return ("PAM4");
  default:   return ("Reserved");
  }
}

static const char * sff_extended_spec_compliance_to_string(uint8_t ext_spec_compliance)
{
  switch (ext_spec_compliance) {
  case 0x00: return "Unspecified";
  case 0x01: return "100G AOC (Active Optical Cable), retimed or 25GAUI C2M AOC. Providing a worst BER of 5 × 10^-5";
  case 0x02: return "100GBASE-SR4 or 25GBASE-SR";
  case 0x03: return "100GBASE-LR4 or 25GBASE-LR";
  case 0x04: return "100GBASE-ER4 or 25GBASE-ER";
  case 0x05: return "100GBASE-SR10";
  case 0x06: return "100G CWDM4";
  case 0x07: return "100G PSM4 Parallel SMF";
  case 0x08: return "100G ACC (Active Copper Cable), retimed or 25GAUI C2M ACC. Providing a worst BER of 5 × 10^-5";
  case 0x09: return "Obsolete (assigned before 100G CWDM4 MSA required FEC)";
  case 0x0a: return "Reserved";
  case 0x0b: return "100GBASE-CR4, 25GBASE-CR CA-25G-L or 50GBASE-CR2 with RS (Clause91) FEC";
  case 0x0c: return "25GBASE-CR CA-25G-S or 50GBASE-CR2 with BASE-R (Clause 74 Fire code) FEC";
  case 0x0d: return "25GBASE-CR CA-25G-N or 50GBASE-CR2 with no FEC";
  case 0x0e: return "10 Mb/s Single Pair Ethernet (802.3cg, Clause 146/147, 1000 m copper)";
  case 0x0f: return "Reserved";
  case 0x10: return "40GBASE-ER4";
  case 0x11: return "4 x 10GBASE-SR";
  case 0x12: return "40G PSM4 Parallel SMF";
  case 0x13: return "G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310 nm SM)";
  case 0x14: return "G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550 nm SM)";
  case 0x15: return "G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550 nm SM)";
  case 0x16: return "10GBASE-T with SFI electrical interface";
  case 0x17: return "100G CLR4";
  case 0x18: return "100G AOC, retimed or 25GAUI C2M AOC. Providing a worst BER of 10^-12 or below";
  case 0x19: return "100G ACC, retimed or 25GAUI C2M ACC. Providing a worst BER of 10^-12 or below";
  case 0x1a: return "100GE-DWDM2 (DWDM transceiver using 2 wavelengths on a 1550 nm DWDM grid with a reach up to 80 km)";
  case 0x1b: return "100G 1550nm WDM (4 wavelengths)";
  case 0x1c: return "10GBASE-T Short Reach (30 meters)";
  case 0x1d: return "5GBASE-T";
  case 0x1e: return "2.5GBASE-T";
  case 0x1f: return "40G SWDM4";
  case 0x20: return "100G SWDM4";
  case 0x21: return "100G PAM4 BiDi";
  case 0x22: return "4WDM-10 MSA (10km version of 100G CWDM4 with same RS(528,514) FEC in host system)";
  case 0x23: return "4WDM-20 MSA (20km version of 100GBASE-LR4 with RS(528,514) FEC in host system)";
  case 0x24: return "4WDM-40 MSA (40km reach with APD receiver and RS(528,514) FEC in host system)";
  case 0x25: return "100GBASE-DR (Clause 140), CAUI-4 (no FEC)";
  case 0x26: return "100G-FR or 100GBASE-FR1 (Clause 140), CAUI-4 (no FEC on host interface)";
  case 0x27: return "100G-LR or 100GBASE-LR1 (Clause 140), CAUI-4 (no FEC on host interface)";
  case 0x28: return "100GBASE-SR1 (P802.3db, Clause 167), CAUI-4 (no FEC on host interface)";
  case 0x29: return "100GBASE-SR1, 200GBASE-SR2 or 400GBASE-SR4 (P802.3db, Clause 167)";
  case 0x2a: return "100GBASE-FR1 (P802.3cu, Clause 140)";
  case 0x2b: return "100GBASE-LR1 (P802.3cu, Clause 140)";
  case 0x2c: return "100G-LR1-20 MSA, CAUI-4 (no FEC on host interface)";
  case 0x2d: return "100G-ER1-30 MSA, CAUI-4 (no FEC on host interface)";
  case 0x2e: return "100G-ER1-40 MSA, CAUI-4 (no FEC on host interface)";
  case 0x2f: return "100G-LR1-20 MSA";
  case 0x30: return "Active Copper Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. Providing a worst BER of 10^-6 or below";
  case 0x31: return "Active Optical Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. Providing a worst BER of 10^-6 or below";
  case 0x32: return "Active Copper Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. Providing a worst BER of 2.6x10^-4 for ACC, 10^-5 for AUI, or below";
  case 0x33: return "Active Optical Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. Providing a worst BER of 2.6x10^-4 for AOC, 10^-5 for AUI, or below";
  case 0x34: return "100G-ER1-30 MSA";
  case 0x35: return "100G-ER1-40 MSA";
  case 0x36: return "100GBASE-VR1, 200GBASE-VR2 or 400GBASE-VR4 (P802.3db, Clause 167)";
  case 0x37: return "10GBASE-BR (Clause 158)";
  case 0x38: return "25GBASE-BR (Clause 159)";
  case 0x39: return "50GBASE-BR (Clause 160)";
  case 0x3a: return "100GBASE-VR1 (P802.3db, Clause 167), CAUI-4 (no FEC on host interface)";
    // 0x3b - 0x3e are reserved
  case 0x3f: return "100GBASE-CR1, 200GBASE-CR2 or 400GBASE-CR4 (P802.3ck, Clause 162)";
  case 0x40: return "50GBASE-CR, 100GBASE-CR2, or 200GBASE-CR4";
  case 0x41: return "50GBASE-SR, 100GBASE-SR2, or 200GBASE-SR4";
  case 0x42: return "50GBASE-FR or 200GBASE-DR4";
  case 0x43: return "200GBASE-FR4";
  case 0x44: return "200G 1550 nm PSM4";
  case 0x45: return "50GBASE-LR";
  case 0x46: return "200GBASE-LR4";
  case 0x47: return "400GBASE-DR4 (802.3, Clause 124), 100GAUI-1 C2M (Annex 120G)";
  case 0x48: return "400GBASE-FR4 (802.3, Clause 151)";
  case 0x49: return "400GBASE-LR4-6 (802.3, Clause 151)";
  case 0x4a: return "50GBASE-ER (IEEE 802.3, Clause 139)";
  case 0x4b: return "400G-LR4-10";
  case 0x4c: return "400GBASE-ZR (P802.3cw, Clause 156)";
    // 0x4d - 0x7e are reserved
  case 0x7f: return "256GFC-SW4 (FC-PI-7P)";
  case 0x80: return "64GFC (FC-PI-7)";
  case 0x81: return "128GFC (FC-PI-8)";
    // 0x82 - 0xff are reserved
  default:   return "Reserved";
  }
}

static void print_qsfpdump(volatile struct cms_block * cms, uint8_t cage_sel)
{
  uint8_t pages[4][256];
  bool read_ok;

  // Read lower page 0
  printf("Cage %u Page 0 Lower\n", cage_sel);
  read_ok = read_qsfp_page(cms, cage_sel, 0, false, 0, &pages[0][0]);
  if (!read_ok) {
    printf("\tRead Failed\n");
  } else {
    printf("\tIdentifier:     %s\n", sff_identifier_to_string(pages[0][0]));
    printf("\tSpec Revision:  %s\n", sff_revision_compliance_to_string(pages[0][1]));
    printf("\tStatus\n");
    printf("\t\tMemory Mapping:    %s\n", (pages[0][2] & 0x4) ? "Flat" : "Paging");
    printf("\t\tInterrupt Active?: %s\n", (pages[0][2] & 0x2) ? "Not Asserted" : "Asserted");
    printf("\t\tData Ready?:       %s\n", (pages[0][2] & 0x1) ? "Not Ready" : "Ready");

    printf("\tActive Interrupts\n");
    if (pages[0][3] == 0 &&
	pages[0][4] == 0 &&
	pages[0][5] == 0 &&
	pages[0][6] == 0 &&
	pages[0][7] == 0 &&
	pages[0][8] == 0 &&
	pages[0][9] == 0 &&
	pages[0][10] == 0 &&
	pages[0][11] == 0 &&
	pages[0][12] == 0 &&
	pages[0][13] == 0 &&
	pages[0][14] == 0) {
      printf("\t\tNone\n");
    } else {
      if (pages[0][3] & 0x80) printf("\t\tL-Tx4 LOS\n");
      if (pages[0][3] & 0x40) printf("\t\tL-Tx3 LOS\n");
      if (pages[0][3] & 0x20) printf("\t\tL-Tx2 LOS\n");
      if (pages[0][3] & 0x10) printf("\t\tL-Tx1 LOS\n");
      if (pages[0][3] & 0x08) printf("\t\tL-Rx4 LOS\n");
      if (pages[0][3] & 0x04) printf("\t\tL-Rx3 LOS\n");
      if (pages[0][3] & 0x02) printf("\t\tL-Rx2 LOS\n");
      if (pages[0][3] & 0x01) printf("\t\tL-Rx1 LOS\n");

      if (pages[0][4] & 0x80) printf("\t\tL-Tx4 Adapt EQ Fault\n");
      if (pages[0][4] & 0x40) printf("\t\tL-Tx3 Adapt EQ Fault\n");
      if (pages[0][4] & 0x20) printf("\t\tL-Tx2 Adapt EQ Fault\n");
      if (pages[0][4] & 0x10) printf("\t\tL-Tx1 Adapt EQ Fault\n");
      if (pages[0][4] & 0x08) printf("\t\tL-Tx4 Fault\n");
      if (pages[0][4] & 0x04) printf("\t\tL-Tx3 Fault\n");
      if (pages[0][4] & 0x02) printf("\t\tL-Tx2 Fault\n");
      if (pages[0][4] & 0x01) printf("\t\tL-Tx1 Fault\n");

      if (pages[0][5] & 0x80) printf("\t\tL-Tx4 LOL\n");
      if (pages[0][5] & 0x40) printf("\t\tL-Tx3 LOL\n");
      if (pages[0][5] & 0x20) printf("\t\tL-Tx2 LOL\n");
      if (pages[0][5] & 0x10) printf("\t\tL-Tx1 LOL\n");
      if (pages[0][5] & 0x08) printf("\t\tL-Rx4 LOL\n");
      if (pages[0][5] & 0x04) printf("\t\tL-Rx3 LOL\n");
      if (pages[0][5] & 0x02) printf("\t\tL-Rx2 LOL\n");
      if (pages[0][5] & 0x01) printf("\t\tL-Rx1 LOL\n");

      if (pages[0][6] & 0x80) printf("\t\tL-Temp High Alarm\n");
      if (pages[0][6] & 0x40) printf("\t\tL-Temp Low Alarm\n");
      if (pages[0][6] & 0x20) printf("\t\tL-Temp High Warning\n");
      if (pages[0][6] & 0x10) printf("\t\tL-Temp Low Warning\n");
      if (pages[0][6] & 0x08) printf("\t\tReserved\n");
      if (pages[0][6] & 0x04) printf("\t\tReserved\n");
      if (pages[0][6] & 0x02) printf("\t\tTC readiness flag\n");
      if (pages[0][6] & 0x01) printf("\t\tInitialization complete flag\n");

      if (pages[0][7] & 0x80) printf("\t\tL-Vcc High Alarm\n");
      if (pages[0][7] & 0x40) printf("\t\tL-Vcc Low Alarm\n");
      if (pages[0][7] & 0x20) printf("\t\tL-Vcc High Warning\n");
      if (pages[0][7] & 0x10) printf("\t\tL-Vcc Low Warning\n");
      if (pages[0][7] & 0x08) printf("\t\tReserved\n");
      if (pages[0][7] & 0x04) printf("\t\tReserved\n");
      if (pages[0][7] & 0x02) printf("\t\tReserved\n");
      if (pages[0][7] & 0x01) printf("\t\tReserved\n");

      if (pages[0][8] & 0x80) printf("\t\tVendor Specific 0x80\n");
      if (pages[0][8] & 0x40) printf("\t\tVendor Specific 0x40\n");
      if (pages[0][8] & 0x20) printf("\t\tVendor Specific 0x20\n");
      if (pages[0][8] & 0x10) printf("\t\tVendor Specific 0x10\n");
      if (pages[0][8] & 0x08) printf("\t\tVendor Specific 0x08\n");
      if (pages[0][8] & 0x04) printf("\t\tVendor Specific 0x04\n");
      if (pages[0][8] & 0x02) printf("\t\tVendor Specific 0x02\n");
      if (pages[0][8] & 0x01) printf("\t\tVendor Specific 0x01\n");

      if (pages[0][9] & 0x80) printf("\t\tL-Rx1 Power High Alarm\n");
      if (pages[0][9] & 0x40) printf("\t\tL-Rx1 Power Low Alarm\n");
      if (pages[0][9] & 0x20) printf("\t\tL-Rx1 Power High Warning\n");
      if (pages[0][9] & 0x10) printf("\t\tL-Rx1 Power Low Warning\n");
      if (pages[0][9] & 0x08) printf("\t\tL-Rx2 Power High Alarm\n");
      if (pages[0][9] & 0x04) printf("\t\tL-Rx2 Power Low Alarm\n");
      if (pages[0][9] & 0x02) printf("\t\tL-Rx2 Power High Warning\n");
      if (pages[0][9] & 0x01) printf("\t\tL-Rx2 Power Low Warning\n");

      if (pages[0][10] & 0x80) printf("\t\tL-Rx3 Power High Alarm\n");
      if (pages[0][10] & 0x40) printf("\t\tL-Rx3 Power Low Alarm\n");
      if (pages[0][10] & 0x20) printf("\t\tL-Rx3 Power High Warning\n");
      if (pages[0][10] & 0x10) printf("\t\tL-Rx3 Power Low Warning\n");
      if (pages[0][10] & 0x08) printf("\t\tL-Rx4 Power High Alarm\n");
      if (pages[0][10] & 0x04) printf("\t\tL-Rx4 Power Low Alarm\n");
      if (pages[0][10] & 0x02) printf("\t\tL-Rx4 Power High Warning\n");
      if (pages[0][10] & 0x01) printf("\t\tL-Rx4 Power Low Warning\n");

      if (pages[0][11] & 0x80) printf("\t\tL-Tx1 Bias High Alarm\n");
      if (pages[0][11] & 0x40) printf("\t\tL-Tx1 Bias Low Alarm\n");
      if (pages[0][11] & 0x20) printf("\t\tL-Tx1 Bias High Warning\n");
      if (pages[0][11] & 0x10) printf("\t\tL-Tx1 Bias Low Warning\n");
      if (pages[0][11] & 0x08) printf("\t\tL-Tx2 Bias High Alarm\n");
      if (pages[0][11] & 0x04) printf("\t\tL-Tx2 Bias Low Alarm\n");
      if (pages[0][11] & 0x02) printf("\t\tL-Tx2 Bias High Warning\n");
      if (pages[0][11] & 0x01) printf("\t\tL-Tx2 Bias Low Warning\n");

      if (pages[0][12] & 0x80) printf("\t\tL-Tx3 Bias High Alarm\n");
      if (pages[0][12] & 0x40) printf("\t\tL-Tx3 Bias Low Alarm\n");
      if (pages[0][12] & 0x20) printf("\t\tL-Tx3 Bias High Warning\n");
      if (pages[0][12] & 0x10) printf("\t\tL-Tx3 Bias Low Warning\n");
      if (pages[0][12] & 0x08) printf("\t\tL-Tx4 Bias High Alarm\n");
      if (pages[0][12] & 0x04) printf("\t\tL-Tx4 Bias Low Alarm\n");
      if (pages[0][12] & 0x02) printf("\t\tL-Tx4 Bias High Warning\n");
      if (pages[0][12] & 0x01) printf("\t\tL-Tx4 Bias Low Warning\n");

      if (pages[0][13] & 0x80) printf("\t\tL-Tx1 Power High Alarm\n");
      if (pages[0][13] & 0x40) printf("\t\tL-Tx1 Power Low Alarm\n");
      if (pages[0][13] & 0x20) printf("\t\tL-Tx1 Power High Warning\n");
      if (pages[0][13] & 0x10) printf("\t\tL-Tx1 Power Low Warning\n");
      if (pages[0][13] & 0x08) printf("\t\tL-Tx2 Power High Alarm\n");
      if (pages[0][13] & 0x04) printf("\t\tL-Tx2 Power Low Alarm\n");
      if (pages[0][13] & 0x02) printf("\t\tL-Tx2 Power High Warning\n");
      if (pages[0][13] & 0x01) printf("\t\tL-Tx2 Power Low Warning\n");

      if (pages[0][14] & 0x80) printf("\t\tL-Tx3 Power High Alarm\n");
      if (pages[0][14] & 0x40) printf("\t\tL-Tx3 Power Low Alarm\n");
      if (pages[0][14] & 0x20) printf("\t\tL-Tx3 Power High Warning\n");
      if (pages[0][14] & 0x10) printf("\t\tL-Tx3 Power Low Warning\n");
      if (pages[0][14] & 0x08) printf("\t\tL-Tx4 Power High Alarm\n");
      if (pages[0][14] & 0x04) printf("\t\tL-Tx4 Power Low Alarm\n");
      if (pages[0][14] & 0x02) printf("\t\tL-Tx4 Power High Warning\n");
      if (pages[0][14] & 0x01) printf("\t\tL-Tx4 Power Low Warning\n");
    }

    printf("\tTemperature: %5.2f C\n", (float)((int16_t)((pages[0][22] << 8) | pages[0][23])) / 256.0f);
    printf("\tSupply Voltage: %4.2f V\n", (float)((uint16_t)((pages[0][26] << 8) | pages[0][27])) * 100.0f / 1000000.0f);

    printf("\tChannel Monitors:\n");
    float rx1_p = (float)((uint16_t)((pages[0][34] << 8) | pages[0][35])) / 10.0f / 1000.0f;
    float rx2_p = (float)((uint16_t)((pages[0][36] << 8) | pages[0][37])) / 10.0f / 1000.0f;
    float rx3_p = (float)((uint16_t)((pages[0][38] << 8) | pages[0][39])) / 10.0f / 1000.0f;
    float rx4_p = (float)((uint16_t)((pages[0][40] << 8) | pages[0][41])) / 10.0f / 1000.0f;
    float rx1_dbm = 10.0f * log10f(rx1_p);
    float rx2_dbm = 10.0f * log10f(rx2_p);
    float rx3_dbm = 10.0f * log10f(rx3_p);
    float rx4_dbm = 10.0f * log10f(rx4_p);
    printf("\t\tRx1 input power : %5.2f mW (%5.2f dBm)\n", rx1_p, rx1_dbm);
    printf("\t\tRx2 input power : %5.2f mW (%5.2f dBm)\n", rx2_p, rx2_dbm);
    printf("\t\tRx3 input power : %5.2f mW (%5.2f dBm)\n", rx3_p, rx3_dbm);
    printf("\t\tRx4 input power : %5.2f mW (%5.2f dBm)\n", rx4_p, rx4_dbm);

    float tx1_bias = (float)((uint16_t)((pages[0][42] << 8) | pages[0][43])) * 2.0f / 1000.0f;
    float tx2_bias = (float)((uint16_t)((pages[0][44] << 8) | pages[0][45])) * 2.0f / 1000.0f;
    float tx3_bias = (float)((uint16_t)((pages[0][46] << 8) | pages[0][47])) * 2.0f / 1000.0f;
    float tx4_bias = (float)((uint16_t)((pages[0][48] << 8) | pages[0][49])) * 2.0f / 1000.0f;
    printf("\t\tTx1 bias        : %5.2f mA\n", tx1_bias);
    printf("\t\tTx2 bias        : %5.2f mA\n", tx2_bias);
    printf("\t\tTx3 bias        : %5.2f mA\n", tx3_bias);
    printf("\t\tTx4 bias        : %5.2f mA\n", tx4_bias);

    float tx1_p = (float)((uint16_t)((pages[0][50] << 8) | pages[0][51])) / 10.0f / 1000.0f;
    float tx2_p = (float)((uint16_t)((pages[0][52] << 8) | pages[0][53])) / 10.0f / 1000.0f;
    float tx3_p = (float)((uint16_t)((pages[0][54] << 8) | pages[0][55])) / 10.0f / 1000.0f;
    float tx4_p = (float)((uint16_t)((pages[0][56] << 8) | pages[0][57])) / 10.0f / 1000.0f;
    float tx1_dbm = 10.0f * log10f(tx1_p);
    float tx2_dbm = 10.0f * log10f(tx2_p);
    float tx3_dbm = 10.0f * log10f(tx3_p);
    float tx4_dbm = 10.0f * log10f(tx4_p);
    printf("\t\tTx1 output power: %5.2f mW (%5.2f dBm)\n", tx1_p, tx1_dbm);
    printf("\t\tTx2 output power: %5.2f mW (%5.2f dBm)\n", tx2_p, tx2_dbm);
    printf("\t\tTx3 output power: %5.2f mW (%5.2f dBm)\n", tx3_p, tx3_dbm);
    printf("\t\tTx4 output power: %5.2f mW (%5.2f dBm)\n", tx4_p, tx4_dbm);
  }

  // Read upper page 0
  printf("Cage %u Page 0 Upper\n", cage_sel);
  read_ok = read_qsfp_page(cms, cage_sel, 0, true, 0, &pages[0][128]);
  if (!read_ok) {
    printf("\tRead Failed\n");
  } else {
    printf("\tIdentifer:           %s\n", sff_identifier_to_string(pages[0][128]));

    printf("\tExtended Identifier:\n");
    printf("\t\tCLEI Code Present: %s\n", (pages[0][129] & 0x10) ? "Yes" : "No");
    printf("\t\tCDR in Tx        : %s\n", (pages[0][129] & 0x08) ? "Present" : "Not Present");
    printf("\t\tCDR in Rx        : %s\n", (pages[0][129] & 0x04) ? "Present" : "Not Present");
    printf("\t\tPower Class 1-4  : %s\n", sff_power_class_1to4_to_string((pages[0][129] & 0xC0) >> 6));
    printf("\t\tPower Class 4-7  : %s\n", sff_power_class_4to7_to_string((pages[0][129] & 0x03) >> 0));
    printf("\t\tPower Class 8    : %s\n", (pages[0][129] & 0x20) ? "Power Class 8 implemented" : "No");

    printf("\tConnector Type: %s\n", sff_connector_type_to_string(pages[0][130]));
    printf("\tSpec Compliance:\n");
    if (pages[0][131] & 0x80) printf("\t\tExtended\n");
    if (pages[0][131] & 0x40) printf("\t\t10GBASE-LRM\n");
    if (pages[0][131] & 0x20) printf("\t\t10GBASE-LR\n");
    if (pages[0][131] & 0x10) printf("\t\t10GBASE-SR\n");
    if (pages[0][131] & 0x08) printf("\t\t40GBASE-CR4\n");
    if (pages[0][131] & 0x04) printf("\t\t40GBASE-SR4\n");
    if (pages[0][131] & 0x02) printf("\t\t40GBASE-LR4\n");
    if (pages[0][131] & 0x01) printf("\t\t40G Active Cable (XLPPI)\n");

    // Skipped Spec Compliance for SONET, SAS/SATA, GigE, FC

    printf("\tEncoding: %s\n", sff_encoding_to_string(pages[0][138]));
    printf("\tNominal Signalling Rate: ");
    if (pages[0][139] == 0) {
      printf("Not Specified (see Module Technology)\n");
    } else if (pages[0][139] == 0xFF) {
      printf("Exceeds 25.4 GBd (see 222)\n");
    } else {
      printf("%u MBd\n", pages[0][139] * 100);
    }

    printf("\tExtended Spec Compliance: %s\n", sff_extended_spec_compliance_to_string(pages[0][192]));

    printf("\tLength: %3u km  (SMF)\n", pages[0][142]);
    printf("\tLength: %3u m   (OM3 50um)\n", pages[0][143] * 2);
    printf("\tLength: %3u m   (OM2 50um)\n", pages[0][144]);
    printf("\tLength: %3u m   (OM1 62.5um)\n", pages[0][145]);
    printf("\tLoss:   %3u dB  (Copper Cable Attenuation at 25.78 GHz)\n", pages[0][145]);
    printf("\tLength: %3u m   (passive copper or active cable)\n", pages[0][146]);
    printf("\tLength: %3u m   (OM4 50um)\n", pages[0][146] * 2);

    printf("\tDevice Technology: %s\n", sff_transmitter_technology_to_string((pages[0][147] & 0xF0) >> 4));
    printf("\t\t%s Wavelength Control\n", (pages[0][147] & 0x8) ? "Active" : "No");
    printf("\t\t%s Transmitter Device\n", (pages[0][147] & 0x4) ? "Cooled" : "Uncooled");
    printf("\t\t%s Detector\n", (pages[0][147] & 0x2) ? "APD" : "Pin");
    printf("\t\tTransmitter %s Tunable\n", (pages[0][147] & 0x1) ? "" : "Not");

    printf("\tVendor Name: ");
    for (int i = 0; i < 16; i++) {
      printf("%c", pages[0][148+i]);
    }
    printf("\n");

    printf("\tVendor OUI:  %02x:%02x:%02x\n", pages[0][165], pages[0][166], pages[0][167]);

    printf("\tVendor PN  : ");
    for (int i = 0; i < 16; i++) {
      printf("%c", pages[0][168+i]);
    }
    printf("\n");

    printf("\tVendor Rev : ");
    for (int i = 0; i < 2; i++) {
      printf("%c", pages[0][184+i]);
    }
    printf("\n");

    printf("\tVendor SN  : ");
    for (int i = 0; i < 16; i++) {
      printf("%c", pages[0][196+i]);
    }
    printf("\n");

    printf("\tDate Code  : 20%c%c-%c%c-%c%c Lot %c%c\n",
	   pages[0][212], pages[0][213],
	   pages[0][214], pages[0][215],
	   pages[0][216], pages[0][217],
	   pages[0][218], pages[0][219]);

    printf("\tMaximum Case Temperature: ");
    if (pages[0][190] == 0) {
      printf("70 C\n");
    } else {
      printf("%u C\n", pages[0][190]);
    }

    printf("\tCC_BASE: 0x%02x", pages[0][191]);
    uint16_t cksum = 0;
    for (int i = 128; i < 191; i++) {
      cksum += pages[0][i];
    }
    if ((cksum & 0xFF) == pages[0][191]) {
      printf (" (OK)\n");
    } else {
      printf (" (Corrupt)\n");
    }

    printf("\tOptions:\n");
    if (pages[0][193] & 0x80) printf ("\t\t??? Reserved bit set\n");
    if (pages[0][193] & 0x40) printf ("\t\tLPMode/TxDis input signal is configurable using byte 99, bit 1\n");
    if (pages[0][193] & 0x20) printf ("\t\tIntL/RxLOSL output signal is configurable using byte 99, bit 0\n");
    if (pages[0][193] & 0x10) printf ("\t\tTx input adaptive equalizers freeze capable\n");
    if (pages[0][193] & 0x08) printf ("\t\tTx input equalizers auto-adaptive capable\n");
    if (pages[0][193] & 0x04) printf ("\t\tTx input equalizers fixed-programmable settings\n");
    if (pages[0][193] & 0x02) printf ("\t\tRx output emphasis fixed-programmable settings\n");
    if (pages[0][193] & 0x01) printf ("\t\tRx output amplitude fixed-programmable settings\n");

    if (pages[0][194] & 0x80) printf ("\t\tTx CDR On/Off Control implemented\n");
    if (pages[0][194] & 0x40) printf ("\t\tRx CDR On/Off Control implemented\n");
    if (pages[0][194] & 0x20) printf ("\t\tTx CDR Loss of Lock (LOL) flag implemented\n");
    if (pages[0][194] & 0x10) printf ("\t\tRx CDR Loss of Lock (LOL) flag implemented\n");
    if (pages[0][194] & 0x08) printf ("\t\tRx Squelch Disable implemented\n");
    if (pages[0][194] & 0x04) printf ("\t\tRx Output Disable implemented\n");
    if (pages[0][194] & 0x02) printf ("\t\tTx Squelch Disable implemented\n");
    if (pages[0][194] & 0x01) {
      printf ("\t\tTx Squelch implemented\n");
      printf ("\t\tTx Squelch implemented to reduce %s\n", (pages[0][195] & 0x04) ? "Pave" : "OMA");
    }

    if (pages[0][195] & 0x80) printf ("\t\tMemory Page 02h provided\n");
    if (pages[0][195] & 0x40) printf ("\t\tMemory Page 01h provided\n");
    if (pages[0][195] & 0x20) printf ("\t\tRate select is implemented as defined in 6.2.7\n");
    if (pages[0][195] & 0x10) printf ("\t\tTx_Disable is implemented and disables the serial output\n");
    if (pages[0][195] & 0x08) printf ("\t\tTx_Fault signal implemented\n");
    // 0x04 handled as sub-element of Tx Squelch above
    if (pages[0][195] & 0x02) printf ("\t\tTx Loss of Signal implemented\n");
    if (pages[0][195] & 0x01) printf ("\t\tPages 20-21h implemented\n");

    printf("\tDiagnostic Monitoring Type:\n");
    if (pages[0][220] & 0x20) printf ("\t\tTemperature monitoring implemented\n");
    if (pages[0][220] & 0x10) printf ("\t\tSupply voltage monitoring implemented\n");
    printf("\t\tReceived power measurements %s\n", (pages[0][220] & 0x08) ? "Average Power" : "OMA");
    if (pages[0][220] & 0x04) printf ("\t\tTransmitter power measurement\n");

    printf("\tEnhanced Options:\n");
    if (pages[0][221] & 0x10) printf ("\t\tInitialization Complete Flag implemented\n");
    if (pages[0][221] & 0x08) printf ("\t\tExtended Rate Selection\n");
    // 0x04 is reserved
    if (pages[0][221] & 0x02) printf ("\t\tTC readiness flag implemented\n");
    if (pages[0][221] & 0x01) printf ("\t\tSoftware reset is implemented\n");

    printf("\tExtended Baud Rate: %u MBd\n", pages[0][222] * 250);

    printf("\tCC_EXT: 0x%02x", pages[0][223]);
    uint16_t cksum_ext = 0;
    for (int i = 192; i < 223; i++) {
      cksum_ext += pages[0][i];
    }
    if ((cksum_ext & 0xFF) == pages[0][223]) {
      printf (" (OK)\n");
    } else {
      printf (" (Corrupt)\n");
    }
  }

  // Check if Page 01h is provided by checking the relevant Option bit
  if (pages[0][195] & 0x40) {
    printf ("Cage %u Page 01h is provided by this device but not read or decoded by this tool\n", cage_sel);
  } else {
    printf ("Cage %u Page 01h is not provided by this device\n", cage_sel);
  }

  if (pages[0][195] & 0x80) {
    printf ("Cage %u Page 02h is provided by this device but not read or decoded by this tool\n", cage_sel);
  } else {
    printf ("Cage %u Page 02h is not provided by this device\n", cage_sel);
  }

  // Check if Page 3 is supported by checking the "Paging" bit
  if (!(pages[0][2] & 0x4)) {
    printf("Cage %u Page 3 Upper\n", cage_sel);
    read_ok = read_qsfp_page(cms, cage_sel, 3, true, 0, &pages[3][128]);
    if (!read_ok) {
      printf("\tRead Failed\n");
    } else {
      // TODO Decode page 3
    }
  } else {
    printf ("Cage %u Page 03h is not provided by this device\n", cage_sel);
  }
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
  } else if (!strcmp(arguments.command, "qsfpdump")) {
    print_qsfpdump(&bar2->cms, 0);
    printf("\n");
    print_qsfpdump(&bar2->cms, 1);
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

