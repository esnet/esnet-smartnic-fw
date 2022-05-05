#include <stdio.h>
#include <argp.h> /* argp_parse */
#include <stdbool.h> /* bool */
#include <string.h>  /* strcmp */

#include "smartnic.h"		/* smartnic_* */
#include "snp4.h"		/* snp4_* */

const char *argp_program_version = "sn-cli 1.0";
const char *argp_program_bug_address = "ESnet SmartNIC Developers <smartnic@es.net>";

/* Program documentation. */
static char doc[] = "Tool for interacting with an esnet-smartnic based on a Xilinx U280 card.";

/* A description of the arguments we accept. */
static char args_doc[] = "(p4-info | p4-config-apply | p4-config-check | smartnic-device-info)";

static struct argp_option argp_options[] = {
					    { "config", 'c',
					      "FILE", 0,
					      "File containing match-action entries to load into p4 tables",
					      0,
					    },
					    { "format", 'f',
					      "FORMAT", 0,
					      "Format of config file (default: p4bm)",
					      0,
					    },
					    { "pcieaddr", 'p',
					      "ID", 0,
					      "PCIe BDF ID of the FPGA card.",
					      0,
					    },
					    { "verbose", 'v',
					      0, 0,
					      "Produce verbose output",
					      0,
					    },
					    { 0, 0,
					      0, 0,
					      0,
					      0,
					    },
};

enum config_format {
		    CONFIG_FORMAT_P4BM_SIM = 0,
		    CONFIG_FORMAT_MAX, // Add entries above this line
};

struct arguments {
  bool verbose;
  char *command;
  char *config;
  enum config_format config_format;
  char *pcieaddr;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key) {
  case 'c':
    arguments->config = arg;
    break;
  case 'f':
    if (!strcmp(arg, "p4bm")) {
      arguments->config_format = CONFIG_FORMAT_P4BM_SIM;
    } else {
      // Unknown config file format
      argp_usage (state);
    }
    break;
  case 'p':
    arguments->pcieaddr = arg;
    break;
  case 'v':
    arguments->verbose = true;
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
  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  struct arguments arguments = {
				.verbose = false,
				.command = NULL,
				.config  = NULL,
  };

  struct argp argp = {
		      .options = argp_options,
		      .parser = parse_opt,
		      .args_doc = args_doc,
		      .doc = doc
  };

  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  // Handle all options which do not require actually mapping the FPGA memory
  if (!strcmp(arguments.command, "p4-info")) {
    snp4_print_target_config();
    return 0;
  } else if (!strcmp(arguments.command, "p4-config-check")) {
    if (arguments.config == NULL) {
      fprintf(stderr, "ERROR: config file is required but not provided\n");
      return 1;
    }

    struct sn_cfg_set *cfg_set;

    switch (arguments.config_format) {
    case CONFIG_FORMAT_P4BM_SIM:
      cfg_set = snp4_cfg_set_load_p4bm(arguments.config);
      break;
    default:
      cfg_set = NULL;
      break;
    }

    if (cfg_set == NULL) {
      fprintf(stderr, "ERROR: failed to parse sn config file\n");
      return 1;
    }
    printf("Loaded all %u entries.\n", cfg_set->num_entries);
    snp4_cfg_set_free(cfg_set);
    return 0;
  }

  // All other options after this point require mapping the FPGA

  if (arguments.pcieaddr == NULL) {
    fprintf(stderr, "ERROR: pcieaddr is required but not provided\n");
    return 1;
  }

  volatile struct esnet_smartnic_bar2 * bar2;
  bar2 = smartnic_map_bar2_by_pciaddr(arguments.pcieaddr);
  if (bar2 == NULL) {
    fprintf(stderr, "ERROR: failed to mmap FPGA register space.  Are you root?\n");
    return 1;
  }

  if (!strcmp(arguments.command, "smartnic-device-info")) {
    printf("[syscfg]\n");
    printf("\tbuild_status:  0x%08x\n", bar2->syscfg.build_status);
    printf("\tsystem_status: 0x%08x\n", bar2->syscfg.system_status._v);
    printf("\tshell_status:  0x%08x\n", bar2->syscfg.shell_status._v);
    printf("\tuser_status:   0x%08x\n", bar2->syscfg.user_status);
  } else if (!strcmp(arguments.command, "p4-config-apply")) {
    if (arguments.config == NULL) {
      fprintf(stderr, "ERROR: config file is required but not provided\n");
      return 1;
    }

    struct sn_cfg_set *cfg_set = snp4_cfg_set_load_p4bm(arguments.config);
    if (cfg_set == NULL) {
      fprintf(stderr, "ERROR: failed to parse snp4 config file\n");
      return 1;
    }

    void * snp4_handle = snp4_init((uintptr_t) &bar2->sdnet);
    for (uint32_t entry_idx = 0; entry_idx < cfg_set->num_entries; entry_idx++) {
      struct sn_cfg *cfg = cfg_set->entries[entry_idx];

      fprintf(stderr, "Inserting line %u (%s)\n", cfg->line_no, cfg->raw);
      fprintf(stderr, "\ttable_name: '%s'\n", cfg->rule.table_name);
      fprintf(stderr, "\tkey:  [%02lu bytes]: ", cfg->pack.key_len);
      for (uint16_t i = 0; i < cfg->pack.key_len; i++) {
	fprintf(stderr, "%02x", cfg->pack.key[i]);
      }
      fprintf(stderr, "\n");
      fprintf(stderr, "\tmask: [%02lu bytes]: ", cfg->pack.mask_len);
      for (uint16_t i = 0; i < cfg->pack.mask_len; i++) {
	fprintf(stderr, "%02x", cfg->pack.mask[i]);
      }
      fprintf(stderr, "\n");
      fprintf(stderr, "\taction_name: '%s'\n", cfg->rule.action_name);
      fprintf(stderr, "\nparams: [%02lu bytes]: ", cfg->pack.params_len);
      for (uint16_t i = 0; i < cfg->pack.params_len; i++) {
	fprintf(stderr, "%02x", cfg->pack.params[i]);
      }
      fprintf(stderr, "\n");
      fprintf(stderr, "\tpriority: %u\n", cfg->rule.priority);
      if (!snp4_table_insert_kma(snp4_handle,
				 cfg->rule.table_name,
				 cfg->pack.key,
				 cfg->pack.key_len,
				 cfg->pack.mask,
				 cfg->pack.mask_len,
				 cfg->rule.action_name,
				 cfg->pack.params,
				 cfg->pack.params_len,
				 cfg->rule.priority)) {
	fprintf(stderr, "ERROR\n");
      } else {
	fprintf(stderr, "OK\n");
      }
    }
    snp4_deinit(snp4_handle);
    snp4_cfg_set_free(cfg_set);
  } else {
    // Unknown command provided
    fprintf(stderr, "ERROR: unknown command provided: '%s'\n", arguments.command);
    return 1;
  }
  smartnic_unmap_bar2(bar2);

  return 0;
}
