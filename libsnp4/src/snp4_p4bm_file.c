#include <stdio.h> /* NULL, FILE */
#include <stdlib.h> /* calloc */
#include <stdint.h> /* uint32_t */
#include <string.h> /* strsep */
#include <stdbool.h> /* bool */
#include <gmp.h>    /* mpz_* */

#include "snp4.h"		/* API */
#include "array_size.h"		/* ARRAY_SIZE */

static void snp4_cfg_free(struct sn_cfg *cfg)
{
  if (cfg == NULL) {
    return;
  }

  // Free the allocated fields contained inside of a config entry
  if (cfg->raw) {
    free((void *)cfg->raw);
    cfg->raw = NULL;
  }
  snp4_rule_clear(&cfg->rule);
  snp4_pack_clear(&cfg->pack);

  // Free the cfg struct itself
  free(cfg);
}

void snp4_cfg_set_free(struct sn_cfg_set *cfg_set)
{
  if (cfg_set == NULL) {
    return;
  }

  for (uint32_t cfg_idx = 0; cfg_idx < cfg_set->num_entries; cfg_idx++) {
    struct sn_cfg *cfg = cfg_set->entries[cfg_idx];
    snp4_cfg_free(cfg);
  }
  free(cfg_set);
}

/*
 * Key and Match fields are packed into arrays of bytes in big endian order.
 *    ie. msbs of the key/mask are in the lowest memory address
 * Each field is internally packed in big endian byte order.
 * The first (left-most) field in the list is packed into the lsbs of the key.
 * The next field in the list is packed immediately above the previous field.
 *
 * e.g. Given a table format string of "8c:16c:32p"
 *      and a match string of "0x11 0x2233 0x44556677/32"
 *      the resulting array would be:
 *        Addr:     0    1    2    3    4    5    7
 *        Value: 0x44 0x55 0x66 0x77 0x22 0x33 0x11
 */

static bool parse_match_fields(struct sn_rule *rule, char *match_str, unsigned int line_no)
{
#ifndef SDNETCONFIG_DEBUG
  (void)line_no;
#endif

  // Parse each of the key/mask fields in the match
  char *match_token;
  char **match_cursor = &match_str;

  unsigned int match_idx;
  for (match_idx = 0; match_idx < ARRAY_SIZE(rule->matches); match_idx++) {
    struct sn_match *match = &rule->matches[match_idx];

    // Read one key/mask field
    match_token = strsep(match_cursor, " ");
    while ((match_token != NULL) && (strlen(match_token) == 0)) {
      // Skip past blocks of delimiters without consuming match slots
      match_token = strsep(match_cursor, " ");
    }
    if (match_token == NULL) {
      // Finished processing the matches
      break;
    }

#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] key/mask field: '%s'\n", line_no, match_token);
#endif

    char *mask_field_sep;
    if ((mask_field_sep = strstr(match_token, "&&&")) != NULL) {
      // Key and Mask format
      *mask_field_sep = '\0';
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: '&&&'  key_field: '%s'  mask_field: '%s'\n", line_no, match_token, mask_field_sep + 3);
#endif
      match->t = SN_MATCH_FORMAT_KEY_MASK;
      if (mpz_init_set_str(match->v.key_mask.key, match_token, 0) != 0) {
	mpz_clear(match->v.key_mask.key);
	goto out_parse_error;
      }
      if (mpz_init_set_str(match->v.key_mask.mask, mask_field_sep + 3, 0) != 0) {
	mpz_clear(match->v.key_mask.key);
	mpz_clear(match->v.key_mask.mask);
	goto out_parse_error;
      }

    } else if ((mask_field_sep = strstr(match_token, "/")) != NULL) {
      // Prefix format
      *mask_field_sep = '\0';
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: '/'  key_field: '%s'  mask_field: '%s'\n", line_no, match_token, mask_field_sep + 1);
#endif
      match->t = SN_MATCH_FORMAT_PREFIX;
      if (mpz_init_set_str(match->v.prefix.key, match_token, 0) != 0) {
	mpz_clear(match->v.key_mask.key);
	goto out_parse_error;
      }
      // Use the gmp library's parsing code for this rather than C stdlib for consistency in how all numbers are parsed.
      // Specifically, this allows for hex (0x), binary (0b), octal (0) and decimal notation.
      mpz_t prefix_len;
      if (mpz_init_set_str(prefix_len, mask_field_sep + 1, 0) != 0) {
	mpz_clear(match->v.prefix.key);
	mpz_clear(prefix_len);
	goto out_parse_error;
      }
      match->v.prefix.prefix_len = (uint16_t) mpz_get_ui(prefix_len);

    } else if ((mask_field_sep = strstr(match_token, "->")) != NULL) {
      // Range format
      *mask_field_sep = '\0';
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: '->'  key_field: '%s'  mask_field: '%s'\n", line_no, match_token, mask_field_sep + 2);
#endif
      match->t = SN_MATCH_FORMAT_RANGE;
      mpz_t lower;
      if (mpz_init_set_str(lower, match_token, 0) != 0) {
	mpz_clear(lower);
	goto out_parse_error;
      }
      match->v.range.lower = (uint16_t) mpz_get_ui(lower);
      mpz_clear(lower);

      mpz_t upper;
      if (mpz_init_set_str(upper, mask_field_sep + 2, 0) != 0) {
	mpz_clear(upper);
	goto out_parse_error;
      }
      match->v.range.upper = (uint16_t) mpz_get_ui(upper);
      mpz_clear(upper);

    } else {
      // Key only format
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: NONE  key_field: '%s'\n", line_no, match_token);
#endif
      match->t = SN_MATCH_FORMAT_KEY_ONLY;
      if (mpz_init_set_str(match->v.key_only.key, match_token, 0) != 0) {
	mpz_clear(match->v.key_only.key);
	goto out_parse_error;
      }
    }
  }

  rule->num_matches = match_idx;

  return true;

 out_parse_error:
  return false;
}

static bool parse_param_fields(struct sn_rule *rule, char *param_str, unsigned int line_no)
{
#ifndef SDNETCONFIG_DEBUG
  (void)line_no;
#endif

  // Parse each of the param fields
  char *param_token;
  char **param_cursor = &param_str;

  unsigned int param_idx;
  for (param_idx = 0; param_idx < ARRAY_SIZE(rule->params); param_idx++) {
    struct sn_param *param = &rule->params[param_idx];

    // Read one param field
    param_token = strsep(param_cursor, " ");
    while ((param_token != NULL) && (strlen(param_token) == 0)) {
      // Skip past blocks of delimiters without consuming parameter slots
      param_token = strsep(param_cursor, " ");
    }
    if (param_token == NULL) {
      // Finished processing params
      break;
    }

#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] param: '%s'\n", line_no, param_token);
#endif

    param->t = SN_PARAM_FORMAT_MPZ;
    if (mpz_init_set_str(param->v.mpz, param_token, 0) < 0) {
      mpz_clear(param->v.mpz);
      goto out_parse_error;
    }
  }

  rule->num_params = param_idx;

  return true;

 out_parse_error:
  return false;
}

static bool parse_table_add_cmd(struct sn_rule *rule, char *line, unsigned int line_no)
{
  char *token;
  char **cursor = &line;

  // discard the command "table_add "
  token = strsep(cursor, " ");

  // table name
  token = strsep(cursor, " ");
  rule->table_name = strdup(token);

  // action name
  token = strsep(cursor, " ");
  rule->action_name = strdup(token);

  // Find split between match and action params
  char *param_sep = strstr(*cursor, " =>");
  if (param_sep == NULL) {
    // Missing separator between match fields and action param fields
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] missing => separator\n", line_no);
#endif
    goto out_error;
  }
  *param_sep = '\0';

  char *match_str = *cursor;
  char *param_str = param_sep + 3;

  if (!parse_match_fields(rule, match_str, line_no)) {
    // Failed to parse the provided match fields for this table
    goto out_error;
  }

  if (!parse_param_fields(rule, param_str, line_no)) {
    // Failed to parse the provided action parameters for this table + action
    goto out_error;
  }

  // Find out if the user has provided the optional priority field by detecting if we have one
  // extra parameter than this table requires and assuming the extra field must be the priority.
  struct sn_table_info table_info;
  struct sn_action_info action_info;
  enum snp4_status rc;
  rc = snp4_config_load_table_and_action_info(rule->table_name, rule->action_name, &table_info, &action_info);
  if (rc != SNP4_STATUS_OK) {
    goto out_error;
  }

  if (rule->num_params == (action_info.num_params + 1)) {
    // Use the last param as the priority and fix up the parsed params
    struct sn_param *param = &rule->params[rule->num_params - 1];
    switch (param->t) {
    case SN_PARAM_FORMAT_UI:
      rule->priority = param->v.ui;
      break;
    case SN_PARAM_FORMAT_MPZ:
      rule->priority = mpz_get_ui(param->v.mpz);
      break;
    default:
      // Unhandled encoding for param
      return false;
    }
    // Drop the last parameter from the list
    snp4_rule_param_clear(param);
    rule->num_params--;
  }

  return true;

 out_error:
  snp4_rule_clear(rule);
  return false;
}

struct sn_cfg_set *snp4_cfg_set_load_p4bm(FILE * f)
{
  struct sn_cfg_set *cfg_set = (struct sn_cfg_set *) calloc(1, sizeof(struct sn_cfg_set));
  if (cfg_set == NULL) {
    return NULL;
  }
  cfg_set->num_entries = 0;

  #define TABLE_ADD_CMD "table_add "
  char *line = NULL;
  size_t len = 0;
  ssize_t bytes_read;
  uint32_t line_no = 0;
  while ((bytes_read = getline(&line, &len, f)) != -1) {
    line_no++;

    // strip any trailing newline
    if (line[bytes_read - 1] == '\n') {
      line[bytes_read - 1] = '\0';
    }

#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] read %3lu bytes: '%s' -- ", line_no, bytes_read, line);
#endif
    if (strncmp(line, TABLE_ADD_CMD, strlen(TABLE_ADD_CMD)) != 0) {
      // Not a table add command, skip to the next line
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "skipped.\n");
#endif
      continue;
    }
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "processing...\n");
#endif

    // We have a table add command
    struct sn_cfg *cfg = (struct sn_cfg *) calloc(1, sizeof(struct sn_cfg));
    if (cfg == NULL) {
      goto out_error;
    }

    cfg->raw = strdup(line);
    cfg->line_no = line_no;

    cfg->op = OP_TABLE_ADD;
    if (!parse_table_add_cmd(&cfg->rule,
			     line,
			     line_no)) {
      // Failed to parse the table add command
      snp4_rule_clear(&cfg->rule);
      goto out_error;
    }

    // Add this entry to the list of table entries
    cfg_set->entries[cfg_set->num_entries] = cfg;
    cfg_set->num_entries++;
  }

  return cfg_set;

 out_error:
  snp4_cfg_set_free(cfg_set);
  return NULL;
}

