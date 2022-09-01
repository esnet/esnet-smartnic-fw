#include <stdio.h>
#include <argp.h> /* argp_parse */
#include <stdbool.h> /* bool */
#include <string.h>  /* strcmp */
#include <stdlib.h>  /* malloc */

#include "smartnic.h"		/* smartnic_* */
#include "snp4.h"		/* snp4_* */
#include "arguments_common.h"	/* arguments_global, cmd_* */

static char doc_p4[] =
  "\n"
  "This subcommand is used to interact with the vitisnetp4 block within the smartnic"
  "\v"
  "--"
  ;

static char args_doc_p4[] = "(info | config-apply | config-check)";

static struct argp_option argp_options_p4[] = {
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

struct arguments_p4 {
  struct arguments_global* global;
  char *config;
  enum config_format config_format;
  char *command;
};

static error_t parse_opt_p4 (int key, char *arg, struct argp_state *state)
{
  struct arguments_p4 *arguments = state->input;

  switch(key) {
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

void cmd_p4(struct argp_state *state)
{
  struct arguments_p4 arguments = {0,};
  int  argc   = state->argc - state->next +1;
  char **argv = &state->argv[state->next - 1];
  char *argv0 = argv[0];

  arguments.global = state->input;

  struct argp argp_p4 = {
    .options  = argp_options_p4,
    .parser   = parse_opt_p4,
    .args_doc = args_doc_p4,
    .doc      = doc_p4,
  };

  // Temporarily override argv[0] to make error messages show the correct prefix
  argv[0] = malloc(strlen(state->name) + strlen(" p4") + 1);
  if (!argv[0]) {
    argp_failure(state, 1, ENOMEM, 0);
  }
  sprintf(argv[0], "%s p4", state->name);

  // Invoke the subcommand parser
  argp_parse (&argp_p4, argc, argv, ARGP_IN_ORDER, &argc, &arguments);

  // Restore the previous argv[0]
  free(argv[0]);
  argv[0] = argv0;

  // Always claim to have consumed all options in the subcommand to prevent the parent from reparsing
  // any options that occur after the subcommand
  state->next += argc - 1;

  // Load any config file provided on the command line
  struct sn_cfg_set *cfg_set = NULL;
  if (arguments.config) {
    FILE * config_file = fopen(arguments.config, "r");
    if (config_file == NULL) {
      fprintf(stderr, "ERROR: cannot open sn config file (%s) for reading\n", arguments.config);
      return;
    }

    switch (arguments.config_format) {
    case CONFIG_FORMAT_P4BM_SIM:
      cfg_set = snp4_cfg_set_load_p4bm(config_file);
      break;
    default:
      cfg_set = NULL;
      break;
    }

    // We're done reading the config file, close it
    fclose(config_file);

    if (cfg_set == NULL) {
      fprintf(stderr, "ERROR: failed to parse provided sn config file (%s)\n", arguments.config);
      return;
    }
    printf("OK: Parsed all %u entries from (%s).\n", cfg_set->num_entries, arguments.config);

    // Pack the entries
    bool cfg_set_valid = true;
    for (unsigned int entry_idx = 0; entry_idx < cfg_set->num_entries; entry_idx++) {
      struct sn_cfg *cfg = cfg_set->entries[entry_idx];

      enum snp4_status rc = snp4_rule_pack(&cfg->rule, &cfg->pack);
      if (rc == SNP4_STATUS_OK) {
	fprintf(stderr, "[%03u] OK\n", cfg->line_no);
      } else {
	fprintf(stderr, "[%03u] ERROR %3d packing: %s\n", cfg->line_no, rc, cfg->raw);
	cfg_set_valid = false;
      }
    }

    if (!cfg_set_valid) {
      fprintf(stderr, "ERROR: failed to pack one or more sn config file entries\n");
      return;
    }

    fprintf(stderr, "OK: Packed all %u entries\n", cfg_set->num_entries);
  }

  // Handle all steps which do not require actually mapping the FPGA memory
  if (!strcmp(arguments.command, "info")) {
    snp4_print_target_config();
    return;
  } else if (!strcmp(arguments.command, "config-check")) {
    // We've already done all validation steps as we processed the config file above, clean up and exit
    snp4_cfg_set_free(cfg_set);
    return;
  }

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

  if (!strcmp(arguments.command, "config-apply")) {
    // Insert the packed cfg_set entries into the sdnet block
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
  }

  return;
}

