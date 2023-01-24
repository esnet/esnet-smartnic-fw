#include <stdio.h>
#include <argp.h> /* argp_parse */
#include <stdbool.h> /* bool */
#include <string.h>  /* strcmp */
#include <stdlib.h>  /* malloc */
#include <inttypes.h>		/* strtoumax */

#include "smartnic.h"		/* smartnic_* */
#include "cmac.h"		/* cmac_* */
#include "arguments_common.h"	/* arguments_global, cmd_* */

static char doc_cmac[] =
  "\n"
  "This subcommand is used to interact with the 100G CMAC blocks within the smartnic"
  "\v"
  "--"
  ;

static char args_doc_cmac[] = "(enable | disable | status | stats)";

static struct argp_option argp_options_cmac[] = {
  { "format", 'f',
    "FORMAT", 0,
    "Format of output (default: table)",
    0,
  },
  { "port", 'p',
    "PORT", 0,
    "Port number to act on (default: all), may be specified more than once",
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

#define PORT_SELECT_CMAC0 (1 << 0)
#define PORT_SELECT_CMAC1 (1 << 1)
#define PORT_SELECT_ALL (PORT_SELECT_CMAC0 | PORT_SELECT_CMAC1)
#define PORT_SELECT_NUM_PORTS 2

struct arguments_cmac {
  struct arguments_global* global;
  enum output_format output_format;
  char *command;
  uint32_t ports;
};

static error_t parse_opt_cmac (int key, char *arg, struct argp_state *state)
{
  struct arguments_cmac *arguments = state->input;

  switch(key) {
  case 'f':
    if (!strcmp(arg, "table")) {
      arguments->output_format = OUTPUT_FORMAT_TABLE;
    } else {
      // Unknown config file format
      argp_usage (state);
    }
    break;
  case 'p':
    if (!strcmp(arg, "all")) {
      arguments->ports |= PORT_SELECT_ALL;
    } else {
      uintmax_t v;
      char *end;

      // Convert the arg to an integer, taking care to pre-clear errno
      errno = 0;
      v = strtoumax(arg, &end, 10);

      // Check for all the ways the conversion can fail
      if (*end != '\0') {
	// trailing garbage means it didn't convert properly
	argp_error(state, "%s can't be converted to a port number", arg);
      } else if (v == UINTMAX_MAX && errno == ERANGE) {
	// value is out of range
	argp_error(state, "%s is out of range for port conversion", arg);
      } else if (v >= PORT_SELECT_NUM_PORTS) {
	// value is not a valid port number
	argp_error(state, "%s is not a valid port number", arg);
      } else {
	// found a valid port number, set it in the port select bitmask
	arguments->ports |= (1 << v);
      }
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

static void print_cmac_status(volatile struct cmac_block * cmac, bool verbose)
{
  union cmac_conf_rx_1 conf_rx;
  union cmac_conf_tx_1 conf_tx;
  union cmac_stat_rx_status rx_status[2];
  union cmac_stat_tx_status tx_status[2];

  // Read current configuration
  conf_rx._v = cmac->conf_rx_1._v;
  conf_tx._v = cmac->conf_tx_1._v;

  // Read twice to collect historical (since last read) and current status
  for (int i = 0; i < 2; i++) {
    tx_status[i]._v = cmac->stat_tx_status._v;
    rx_status[i]._v = cmac->stat_rx_status._v;
  }

  printf("  Tx (MAC %s/PHY %s -> %s)\n",
         conf_tx.ctl_tx_enable ? "ENABLED" : "DISABLED",
         tx_status[0]._v == 0 ? "UP" : "DOWN",
         tx_status[1]._v == 0 ? "UP" : "DOWN");
  if (verbose) {
    printf("\t%25s %u -> %u\n", "tx_local_fault",
           tx_status[0].stat_tx_local_fault,
           tx_status[1].stat_tx_local_fault);
  }

  printf("  Rx (MAC %s/PHY %s -> %s)\n",
	 conf_rx.ctl_rx_enable ? "ENABLED" : "DISABLED",
         rx_status[0]._v == (CMAC_STAT_RX_STATUS_STAT_RX_STATUS_MASK | CMAC_STAT_RX_STATUS_STAT_RX_ALIGNED_MASK) ? "UP" : "DOWN",
         rx_status[1]._v == (CMAC_STAT_RX_STATUS_STAT_RX_STATUS_MASK | CMAC_STAT_RX_STATUS_STAT_RX_ALIGNED_MASK) ? "UP" : "DOWN");
  if (verbose) {
    printf("\t%25s %u -> %u\n", "rx_got_signal_os",
	    rx_status[0].stat_rx_got_signal_os,
	    rx_status[1].stat_rx_got_signal_os);
    printf("\t%25s %u -> %u\n", "rx_bad_sfd",
	    rx_status[0].stat_rx_bad_sfd,
	    rx_status[1].stat_rx_bad_sfd);
    printf("\t%25s %u -> %u\n", "rx_bad_preamble",
	    rx_status[0].stat_rx_bad_preamble,
	    rx_status[1].stat_rx_bad_preamble);
    printf("\t%25s %u -> %u\n", "rx_test_pattern_mismatch",
	    rx_status[0].stat_rx_test_pattern_mismatch,
	    rx_status[1].stat_rx_test_pattern_mismatch);
    printf("\t%25s %u -> %u\n", "rx_received_local_fault",
	    rx_status[0].stat_rx_received_local_fault,
	    rx_status[1].stat_rx_received_local_fault);
    printf("\t%25s %u -> %u\n", "rx_internal_local_fault",
	    rx_status[0].stat_rx_internal_local_fault,
	    rx_status[1].stat_rx_internal_local_fault);
    printf("\t%25s %u -> %u\n", "rx_local_fault",
	    rx_status[0].stat_rx_local_fault,
	    rx_status[1].stat_rx_local_fault);
    printf("\t%25s %u -> %u\n", "rx_remote_fault",
	    rx_status[0].stat_rx_remote_fault,
	    rx_status[1].stat_rx_remote_fault);
    printf("\t%25s %u -> %u\n", "rx_hi_ber",
	    rx_status[0].stat_rx_hi_ber,
	    rx_status[1].stat_rx_hi_ber);
    printf("\t%25s %u -> %u\n", "rx_aligned_err",
	    rx_status[0].stat_rx_aligned_err,
	    rx_status[1].stat_rx_aligned_err);
    printf("\t%25s %u -> %u\n", "rx_misaligned",
	    rx_status[0].stat_rx_misaligned,
	    rx_status[1].stat_rx_misaligned);
    printf("\t%25s %u -> %u\n", "rx_aligned",
	    rx_status[0].stat_rx_aligned,
	    rx_status[1].stat_rx_aligned);
    printf("\t%25s %u -> %u\n", "rx_status",
	    rx_status[0].stat_rx_status,
	    rx_status[1].stat_rx_status);
  }

  return;
}

static void print_cmac_stats(volatile struct cmac_block * cmac)
{
  // Latch the current stats
  cmac->tick = 1;

  printf("TX %6lu RX %6lu RX-DISC %6lu RX-ERR %6lu\n",
	   cmac->stat_tx_total_good_pkts,
	   cmac->stat_rx_total_good_pkts,
	   cmac->stat_rx_total_pkts - cmac->stat_rx_total_good_pkts,
	   cmac->stat_rx_undersize +
	   cmac->stat_rx_fragment +
	   cmac->stat_rx_oversize +
	   cmac->stat_rx_toolong +
	   cmac->stat_rx_jabber +
	   cmac->stat_rx_bad_fcs +
	   cmac->stat_rx_pkt_bad_fcs +
	   cmac->stat_rx_stomped_fcs);
  return;
}

void cmd_cmac(struct argp_state *state)
{
  struct arguments_cmac arguments = {0,};
  int  argc   = state->argc - state->next +1;
  char **argv = &state->argv[state->next - 1];
  char *argv0 = argv[0];

  arguments.global = state->input;

  struct argp argp_cmac = {
    .options  = argp_options_cmac,
    .parser   = parse_opt_cmac,
    .args_doc = args_doc_cmac,
    .doc      = doc_cmac,
  };

  // Temporarily override argv[0] to make error messages show the correct prefix
  argv[0] = malloc(strlen(state->name) + strlen(" cmac") + 1);
  if (!argv[0]) {
    argp_failure(state, 1, ENOMEM, 0);
  }
  sprintf(argv[0], "%s cmac", state->name);

  // Invoke the subcommand parser
  argp_parse (&argp_cmac, argc, argv, ARGP_IN_ORDER, &argc, &arguments);

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

  if (arguments.ports == 0) {
    // When no ports are specified, the default is to act on all ports
    arguments.ports = PORT_SELECT_ALL;
  }

  if (!strcmp(arguments.command, "enable")) {
    if (arguments.ports & PORT_SELECT_CMAC0) {
      cmac_enable(&bar2->cmac0);
      printf("Enabled CMAC0\n");
    }
    if (arguments.ports & PORT_SELECT_CMAC1) {
      cmac_enable(&bar2->cmac1);
      printf("Enabled CMAC1\n");
    }

  } else if (!strcmp(arguments.command, "disable")) {
    if (arguments.ports & PORT_SELECT_CMAC0) {
      cmac_disable(&bar2->cmac0);
      printf("Disabled CMAC0\n");
    }
    if (arguments.ports & PORT_SELECT_CMAC1) {
      cmac_disable(&bar2->cmac1);
      printf("Disabled CMAC1\n");
    }

  } else if (!strcmp(arguments.command, "status")) {
    if (arguments.ports & PORT_SELECT_CMAC0) {
      printf("CMAC0\n");
      print_cmac_status(&bar2->cmac0, arguments.global->verbose);
      printf("\n");
    }

    if (arguments.ports & PORT_SELECT_CMAC1) {
      printf("CMAC1\n");
      print_cmac_status(&bar2->cmac1, arguments.global->verbose);
      printf("\n");
    }

  } else if (!strcmp(arguments.command, "stats")) {
    if (arguments.ports & PORT_SELECT_CMAC0) {
      printf("CMAC0: ");
      print_cmac_stats(&bar2->cmac0);
    }

    if (arguments.ports & PORT_SELECT_CMAC1) {
      printf("CMAC1: ");
      print_cmac_stats(&bar2->cmac1);
    }

  } else {
    fprintf(stderr, "ERROR: %s is not a valid command\n", arguments.command);
  }

  return;
}

