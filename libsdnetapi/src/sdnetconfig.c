#include <stdio.h> /* NULL, FILE */
#include <stdlib.h> /* calloc */
#include <stdint.h> /* uint32_t */
#include <string.h> /* strsep */
#include <gmp.h>    /* mpz_* */

#include "sdnetapi.h"		/* API */
#include "sdnet_0_defs.h"	/* XilVitisNetP4TargetConfig_sdnet_0 */
#include "unused.h"		/* UNUSED */

static const struct XilVitisNetP4TargetTableConfig *sdnet_config_find_table_by_name(struct XilVitisNetP4TargetConfig *tcfg, const char *table_name)
{
  for (unsigned int tidx = 0; tidx < tcfg->TableListSize; tidx++) {
    XilVitisNetP4TargetTableConfig *t = tcfg->TableListPtr[tidx];
    if (!strcmp(t->NameStringPtr, table_name)) {
      return t;
    }
  }

  return NULL;
}

static const struct XilVitisNetP4Action *sdnet_config_find_action_by_name(const struct XilVitisNetP4TargetTableConfig *ttc, const char *action_name)
{
  const struct XilVitisNetP4TableConfig *tc = &ttc->Config;
  for (unsigned int aidx = 0; aidx < tc->ActionListSize; aidx++) {
    XilVitisNetP4Action *a = tc->ActionListPtr[aidx];
    if (!strcmp(a->NameStringPtr, action_name)) {
      return a;
    }
  }
  return NULL;
}

static unsigned int sdnet_config_get_param_size_bits(const struct XilVitisNetP4Action *a)
{
  unsigned int param_size_bits = 0;

  for (unsigned int pidx = 0; pidx < a->ParamListSize; pidx++) {
    XilVitisNetP4Attribute *attr = &a->ParamListPtr[pidx];
    param_size_bits += attr->Value;
  }

  return param_size_bits;
}

static void sdnet_config_entry_free(struct sdnet_config_entry *entry)
{
  if (entry == NULL) {
    return;
  }

  // Free the allocated fields contained inside of a config entry
  free((void *)entry->raw);
  free((void *)entry->table_name);
  free((void *)entry->key);
  free((void *)entry->mask);
  free((void *)entry->action_name);
  free((void *)entry->params);

  // Free the entry struct itself
  free(entry);
}

void sdnet_config_free(struct sdnet_config *config)
{
  if (config == NULL) {
    return;
  }

  for (uint32_t entry_idx = 0; entry_idx < config->num_entries; entry_idx++) {
    struct sdnet_config_entry *entry = config->entries[entry_idx];
    sdnet_config_entry_free(entry);
  }
  free(config);
}

enum mask_field_encoding {
			  MASK_FIELD_ENC_BITMASK,
			  MASK_FIELD_ENC_PREFIX,
			  MASK_FIELD_ENC_RANGE,
};

static bool extend_match(mpz_t *key, mpz_t *mask, char field_type, unsigned int field_size_bits, unsigned int UNUSED(field_bit_pos), const char *key_field_str, char *mask_field_str, enum mask_field_encoding mask_field_encoding)
{
  // Ensure that we have pointers for the results
  if (!key) {
    // Missing key storage
    return false;
  }
  if (!mask) {
    // Missing mask storage
    return false;
  }

  // Ensure that a key_field_str has been provided
  if (!key_field_str) {
    // No key provided
    return false;
  }
  if (strlen(key_field_str) == 0) {
    // Empty key provided
    return false;
  }

  // Set up storage for the key field
  mpz_t key_field;
  mpz_init(key_field);

  // Set up storage for the mask field
  mpz_t mask_field;
  mpz_init(mask_field);

  // Set up an all-zeros mask for this field
  mpz_t zero_mask;
  mpz_init_set_ui(zero_mask, 0);

  // Set up an all-ones mask for this field
  mpz_t ones_mask;
  mpz_init_set_ui(ones_mask, 1);
  mpz_mul_2exp(ones_mask, ones_mask, field_size_bits);
  mpz_sub_ui(ones_mask, ones_mask, 1);

  // Parse the (required) key field
  if (mpz_set_str(key_field, key_field_str, 0) < 0) {
    // Failed to parse the key into an integer
    goto out_error;
  }

  // Parse the (optional) mask field
  bool have_mask;
  if (mask_field_str &&
      (strlen(mask_field_str) > 0)) {
    if (mpz_set_str(mask_field, mask_field_str, 0) < 0) {
      // Failed to parse the mask into an integer
      goto out_error;
    }
    have_mask = true;
  } else {
    have_mask = false;
  }

  // Normalize masks encode as prefix lengths into bitmasks
  if (have_mask &&
      (mask_field_encoding == MASK_FIELD_ENC_PREFIX)) {
    unsigned int prefix_len = mpz_get_ui(mask_field);
    if (prefix_len > field_size_bits) {
      // Mask prefix length out of range
      goto out_error;
    }
    // Convert the prefix length into an actual mask field

    // mask_field = ((1 << prefix_len) - 1)
    mpz_set_ui(mask_field, 1);
    mpz_mul_2exp(mask_field, mask_field, prefix_len);
    mpz_sub_ui(mask_field, mask_field, 1);
    // mask_field = mask_field << (field_size_bits - prefix_len)
    mpz_mul_2exp(mask_field, mask_field, field_size_bits - prefix_len);
  }

  // If required, provide a default value for the mask field
  mpz_t mask_default;
  mpz_init(mask_default);

  switch (field_type) {
  case 'b':
    // Bit Field
    mpz_set(mask_default, ones_mask);
    break;
  case 'c':
    // Constant Field
    mpz_set(mask_default, ones_mask);
    break;
  case 'p':
    // Prefix Field
    mpz_set(mask_default, ones_mask);
    break;
  case 'r':
    // Range Field
    // Set the upper limit of the range == lower limit of the range (ie. exactly match the lower value)
    mpz_set(mask_default, key_field);
    break;
  case 't':
    // Ternary Field
    mpz_set(mask_default, ones_mask);
    break;
  case 'u':
    // Unused Field
    mpz_set(mask_default, zero_mask);
    break;
  default:
    // Unknown Field Type
    goto out_error;
    break;
  }
  if (have_mask == false) {
    mpz_set(mask_field, mask_default);
#ifdef SDNETCONFIG_DEBUG
    gmp_fprintf(stderr, "defaulted mask_field:  %#0*Zx\n", (field_size_bits + 3) / 4 + 2, mask_field);
#endif
  }
  mpz_clear(mask_default);

  // Validate the provided or default mask for this field based on the field type
#ifdef SDNETCONFIG_DEBUG
  gmp_fprintf(stderr, "validating mask_field: %#0*Zx\n", (field_size_bits + 3) / 4 + 2, mask_field);
#endif
  switch (field_type) {
  case 'b':
    // Bit Field: Allowed masks: 0 / -1
    if (!((mpz_cmp(mask_field, zero_mask) == 0) ||
	  (mpz_cmp(mask_field, ones_mask) == 0))) {
      // Invalid mask value
      goto out_error;
    }
    break;
  case 'c':
    // Constant Field: Allowed masks: -1
    if (!((mpz_cmp(mask_field, ones_mask) == 0))) {
      // Invalid mask value
      goto out_error;
    }
    break;
  case 'p':
    // Prefix Field: Allowed masks: mask must be contiguous 1's in msbs
    if (!(mpz_cmp(mask_field, zero_mask) == 0)) {
      // Check if we have a zero bit above the lowest one bit
      mp_bitcnt_t lowest_one_pos = mpz_scan1(mask_field, 0);
      if (mpz_scan0(mask_field, lowest_one_pos) < field_size_bits) {
	// Found a zero above the lowest one bit.  Mask is not in prefix form.
	goto out_error;
      }
    }
    break;
  case 'r':
    // Range Field: Allowed masks: mask must be >= key (ie. includes 1 or more values)
    if (mpz_cmp(mask_field, key_field) < 0) {
      // upper limit of the range is < lower limit of range
      goto out_error;
    }
    break;
  case 't':
    // Ternary Field: Allowed masks: any
    break;
  case 'u':
    // Unused Field: Allowed masks: 0
    if (!(mpz_cmp(mask_field, zero_mask) != 0)) {
      // Non-zero mask provided for unused field
      goto out_error;
    }
    break;
  default:
    // Unknown Field Type
    goto out_error;
    break;
  }

  // Ensure that the key is not wider than the field
  if (mpz_sizeinbase(key_field, 2) > field_size_bits) {
    // Key is too large for field
    goto out_error;
  }

  // Ensure that the mask is not wider than the field
  if (mpz_sizeinbase(mask_field, 2) > field_size_bits) {
    // Mask is too large for field
    goto out_error;
  }

#ifdef SDNETCONFIG_DEBUG
  gmp_fprintf(stderr, "key field:  %#0*Zx\n", (field_size_bits + 3) / 4 + 2, key_field);
  gmp_fprintf(stderr, "mask field: %#0*Zx\n", (field_size_bits + 3) / 4 + 2, mask_field);
#endif

  // Extend the current key to include this field
  // by shifting all previous fields to the left and inclusive-or the new key field into the lsbs
  mpz_mul_2exp(*key, *key, field_size_bits);
  mpz_ior(*key, *key, key_field);

  // Extend the current mask to include this field
  // by shifting all previous fields to the left and inclusive-or the new mask field into the lsbs
  mpz_mul_2exp(*mask, *mask, field_size_bits);
  mpz_ior(*mask, *mask, mask_field);

  // Release memory held by intermediate gmp values
  mpz_clear(key_field);
  mpz_clear(mask_field);
  mpz_clear(zero_mask);
  mpz_clear(ones_mask);

  return true;

 out_error:
  // Release memory held by intermediate gmp values
  mpz_clear(key_field);
  mpz_clear(mask_field);
  mpz_clear(zero_mask);
  mpz_clear(ones_mask);

  return false;
}

static bool extend_params(mpz_t *params, unsigned int field_size_bits, unsigned int UNUSED(field_bit_pos), const char *param_field_str)
{
  // Ensure that we have pointers for the results
  if (!params) {
    // Missing param storage
    return false;
  }
  // Ensure that a param_field_str has been provided
  if (!param_field_str) {
    // No param provided
    return false;
  }
  if (strlen(param_field_str) == 0) {
    // Empty param provided
    return false;
  }

  // Set up storage for the param field
  mpz_t param_field;
  mpz_init(param_field);

  // Parse the (required) param field
  if (mpz_set_str(param_field, param_field_str, 0) < 0) {
    // Failed to parse the param into an integer
    goto out_error;
  }

  // Ensure that the param is not wider than the field
  if (mpz_sizeinbase(param_field, 2) > field_size_bits) {
    // Param is too large for field
    goto out_error;
  }

#ifdef SDNETCONFIG_DEBUG
  gmp_fprintf(stderr, "param field:  %#0*Zx\n", (field_size_bits + 3) / 4 + 2, param_field);
#endif

  // Extend the current param to include this field
  // by shifting all previous fields to the left and inclusive-or the new param field into the lsbs
  mpz_mul_2exp(*params, *params, field_size_bits);
  mpz_ior(*params, *params, param_field);

  // Release memory held by intermediate gmp values
  mpz_clear(param_field);

  return true;

 out_error:
  // Release memory held by intermediate gmp values
  mpz_clear(param_field);

  return false;
}

static bool parse_param_fields(struct sdnet_config_entry *entry, const XilVitisNetP4TargetTableConfig *t, const XilVitisNetP4Action *a, char *param_str, unsigned int line_no)
{
#ifndef SDNETCONFIG_DEBUG
  (void)line_no;
#endif

  // Check if we even have parameters
  unsigned int param_size_bits = sdnet_config_get_param_size_bits(a);
  if (param_size_bits == 0) {
    // No parameters, ignore the rest of the line
    entry->params = (uint8_t *)calloc(1, 1);
    entry->params_len = 0;
    entry->priority = 0;
    return true;
  }

  // Allocate space for the final param storage
  // Note: The params for any given action are packed into the lsbs of a bit vector that is sized such that it
  //       would fit the parameters of the longest action parameters.  This means that actions with shorter parameters
  //       still need to allocate a max-sized byte array.
  unsigned int param_size_padded_bytes = (t->Config.CamConfig.ResponseSizeBits - t->Config.ActionIdWidthBits + 7) / 8;
  entry->params = (uint8_t *) calloc(1, param_size_padded_bytes);
  entry->params_len = param_size_padded_bytes;

  // Build up the params into a large, multi-precision integer
  mpz_t params;
  mpz_init(params);
  uint16_t field_bit_pos = 0;

  // Parse each of the param fields into the params storage
  char *param_token;
  char **param_cursor = &param_str;
  for (unsigned int pidx = 0; pidx < a->ParamListSize; pidx++) {
    XilVitisNetP4Attribute *attr = &a->ParamListPtr[pidx];
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] param: '%s' (%u bits)\n", line_no, attr->NameStringPtr, attr->Value);
#endif

    // Read one param field
    param_token = strsep(param_cursor, " ");
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] param: '%s'\n", line_no, param_token);
#endif
    if (param_token == NULL) {
      // Hit the end of the param fields in the input too early
      goto out_error;
    }

    if (!extend_params(&params,
		       attr->Value,
		       field_bit_pos,
		       param_token)) {
      // Failed to parse param field
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] failed to parse param field\n", line_no);
#endif
      goto out_error;
    }
    field_bit_pos += attr->Value;

#ifdef SDNETCONFIG_DEBUG
    gmp_fprintf(stderr, "extended params:  %#0*Zx\n", param_size_padded_bytes * 2 + 2, params);
#endif
  }

#ifdef SDNETCONFIG_DEBUG
  gmp_fprintf(stderr, "final params:     %#0*Zx\n", param_size_padded_bytes * 2 + 2, params);
#endif

  // Parse an (optional) priority value at the end of the line
  param_token = strsep(param_cursor, " ");
  if (param_token &&
      (strlen(param_token) > 0)) {
    entry->priority = strtol(param_token, NULL, 0);
  } else {
    entry->priority = 0;
  }
#ifdef SDNETCONFIG_DEBUG
  fprintf(stderr, "[%3u] priority: '%s' (%u)\n", line_no, param_token, entry->priority);
#endif
#ifdef SDNETCONFIG_DEBUG
  if (*param_cursor &&
      (strlen(*param_cursor) > 0)) {
    fprintf(stderr, "[%3u] trailing garbage ignored: '%s'\n", line_no, *param_cursor);
  }
#endif

  // Pack the computed params into the param array
  unsigned int final_param_bits = mpz_sizeinbase(params, 2);
  if (final_param_bits > param_size_bits) {
    // Params are larger than expected for this table
    goto out_error;
  }
  unsigned int final_param_pad_bytes = entry->params_len - ((final_param_bits + 7) / 8);
  mpz_export(entry->params + final_param_pad_bytes, NULL, 1, 1, 1, 0, params);

  return true;

 out_error:
  return false;
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

static bool parse_match_fields(struct sdnet_config_entry *entry, const XilVitisNetP4TargetTableConfig *t, char *match_str, unsigned int line_no)
{
#ifndef SDNETCONFIG_DEBUG
  (void)line_no;
#endif

  // Allocate space for final key and mask storage
  unsigned int key_size_padded_bytes = (t->Config.KeySizeBits + 7) / 8;
  entry->key = (uint8_t *) calloc(1, key_size_padded_bytes);
  entry->key_len = key_size_padded_bytes;
  // Mask is the same length as the key
  entry->mask = (uint8_t *) calloc(1, key_size_padded_bytes);
  entry->mask_len = key_size_padded_bytes;

  // Build up key and mask into large, multi-precision integers
  mpz_t key;
  mpz_t mask;
  mpz_init(key);
  mpz_init(mask);
  uint16_t field_bit_pos = 0;

  // Split format string into tokens

  struct field_spec {
    unsigned int size;
    char         type;
  };
  #define FIELD_SPEC_MAX_ENTRIES 32
  struct field_spec field_spec_table[FIELD_SPEC_MAX_ENTRIES];
  unsigned int field_spec_entries = 0;

  char *table_fmt = strdup(t->Config.CamConfig.FormatStringPtr);
  char *table_fmt_cursor = table_fmt;
#ifdef SDNETCONFIG_DEBUG
  fprintf(stderr, "[%3u] table format: '%s'\n", line_no, table_fmt);
#endif
  char *field_fmt;
  while((field_fmt = strsep(&table_fmt_cursor, ":")) != NULL) {
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] field format: '%s' -- ", line_no, field_fmt);
#endif
    if (field_spec_entries >= FIELD_SPEC_MAX_ENTRIES) {
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "Table format '%s' requires more than max %u spec entries\n", t->Config.CamConfig.FormatStringPtr, FIELD_SPEC_MAX_ENTRIES);
#endif
      goto out_error;
    }
    struct field_spec * field_spec = &field_spec_table[field_spec_entries];
    field_spec_entries++;

    int matched = sscanf(field_fmt, "%u%c", &field_spec->size, &field_spec->type);
    if (matched != 2) {
      // Field format is not in the expected form
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "Invalid format\n");
#endif
      goto out_error;
    }
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "size=%u, type='%c'\n", field_spec->size, field_spec->type);
#endif
  }

  // Parse each of the key/mask fields in the match
  char *match_token;
  char **match_cursor = &match_str;

  // NOTE: The format string tokens describe the fields of the key from lsbs up to msbs.
  //       The key fields in the p4bm file describe the fields from msbs down to lsbs.
  //
  //       The format string tokens will be consumed in reverse order to account for this
  //       discrepancy.

  for (int field_spec_index = field_spec_entries - 1; field_spec_index >= 0; field_spec_index--) {
    struct field_spec * field_spec = &field_spec_table[field_spec_index];
    unsigned int field_size_bits = field_spec->size;
    char         field_type      = field_spec->type;

    // Read one key/mask field
    match_token = strsep(match_cursor, " ");
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] key/mask field: '%s'\n", line_no, match_token);
#endif
    if (match_token == NULL) {
      // Hit the end of the match fields in the input too early
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] hit end-of-match too early\n", line_no);
#endif
      free(table_fmt);
      goto out_error;
    }

    char *key_field_ptr;

    char *mask_field_ptr;
    char *mask_field_sep;
    enum mask_field_encoding mask_field_encoding;
    if ((mask_field_sep = strstr(match_token, "&&&")) != NULL) {
      key_field_ptr = match_token;
      *mask_field_sep = '\0';
      mask_field_ptr = mask_field_sep + 3;
      mask_field_encoding = MASK_FIELD_ENC_BITMASK;
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: '&&&'  key_field: '%s'  mask_field: '%s'\n", line_no, key_field_ptr, mask_field_ptr);
#endif
    } else if ((mask_field_sep = strstr(match_token, "/")) != NULL) {
      key_field_ptr = match_token;
      *mask_field_sep = '\0';
      mask_field_ptr = mask_field_sep + 1;
      mask_field_encoding = MASK_FIELD_ENC_PREFIX;
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: '/'  key_field: '%s'  mask_field: '%s'\n", line_no, key_field_ptr, mask_field_ptr);
#endif
    } else if ((mask_field_sep = strstr(match_token, "->")) != NULL) {
      key_field_ptr = match_token;
      *mask_field_sep = '\0';
      mask_field_ptr = mask_field_sep + 2;
      mask_field_encoding = MASK_FIELD_ENC_RANGE;
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: '->'  key_field: '%s'  mask_field: '%s'\n", line_no, key_field_ptr, mask_field_ptr);
#endif
    } else {
      key_field_ptr = match_token;
      mask_field_ptr = NULL;
      mask_field_encoding = MASK_FIELD_ENC_BITMASK;
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] sep: NONE  key_field: '%s'\n", line_no, key_field_ptr);
#endif
    }

    if (!extend_match(&key,
		      &mask,
		      field_type,
		      field_size_bits,
		      field_bit_pos,
		      key_field_ptr,
		      mask_field_ptr,
		      mask_field_encoding)) {
      // Failed to parse match field
#ifdef SDNETCONFIG_DEBUG
      fprintf(stderr, "[%3u] failed to parse match field\n", line_no);
#endif
      goto out_error;
    }
    field_bit_pos += field_size_bits;

#ifdef SDNETCONFIG_DEBUG
    gmp_fprintf(stderr, "extended key:  %#0*Zx\n", key_size_padded_bytes * 2 + 2, key);
    gmp_fprintf(stderr, "extended mask: %#0*Zx\n", key_size_padded_bytes * 2 + 2, mask);
#endif
  }
#ifdef SDNETCONFIG_DEBUG
  gmp_fprintf(stderr, "final key   :  %#0*Zx\n", key_size_padded_bytes * 2 + 2, key);
  gmp_fprintf(stderr, "final mask  :  %#0*Zx\n", key_size_padded_bytes * 2 + 2, mask);
#endif

  // Pack the computed key into the key array
  unsigned int final_key_bits = mpz_sizeinbase(key, 2);
  if (final_key_bits > t->Config.KeySizeBits) {
    // Key is larger than expected for this table
    goto out_error;
  }
  unsigned int final_key_pad_bytes = entry->key_len - ((final_key_bits + 7) / 8);
  mpz_export(entry->key + final_key_pad_bytes, NULL, 1, 1, 1, 0, key);

  // Pack the computed mask into the mask array
  unsigned int final_mask_bits = mpz_sizeinbase(mask, 2);
  if (final_mask_bits > t->Config.KeySizeBits) {
    // Mask is larger than expected for this table
    goto out_error;
  }
  unsigned int final_mask_pad_bytes = entry->mask_len - ((final_mask_bits + 7) / 8);
  mpz_export(entry->mask + final_mask_pad_bytes, NULL, 1, 1, 1, 0, mask);

  mpz_clear(key);
  mpz_clear(mask);
  free(table_fmt);

  return true;

 out_error:
  mpz_clear(key);
  mpz_clear(mask);
  free(table_fmt);

  return false;
}

static bool parse_table_add_cmd(XilVitisNetP4TargetConfig *sdnet_config, struct sdnet_config_entry *entry, char *line, unsigned int line_no)
{
  char *token;
  char **cursor = &line;

  // discard the command "table_add "
  token = strsep(cursor, " ");

  // table name
  token = strsep(cursor, " ");
#ifdef SDNETCONFIG_DEBUG
  fprintf(stderr, "[%3u] lookup table name: '%s' -- ", line_no, token);
#endif
  const struct XilVitisNetP4TargetTableConfig *t = sdnet_config_find_table_by_name(sdnet_config, token);
  if (t == NULL) {
    // No matching table name
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "No match.\n");
#endif
    goto out_error;
  }
  entry->table_name = strdup(token);
#ifdef SDNETCONFIG_DEBUG
  fprintf(stderr, "Matched.\n");
#endif

  // action name
  token = strsep(cursor, " ");
#ifdef SDNETCONFIG_DEBUG
  fprintf(stderr, "[%3u] lookup table action name: '%s' -- ", line_no, token);
#endif
  const struct XilVitisNetP4Action *a = sdnet_config_find_action_by_name(t, token);
  if (a == NULL) {
    // No matching action for this table
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "No match.\n");
#endif
    goto out_error;
  }
  entry->action_name = strdup(token);
#ifdef SDNETCONFIG_DEBUG
  fprintf(stderr, "Matched.\n");
#endif

  // Find split between match and action params
  char *param_sep = strstr(*cursor, " => ");
  if (param_sep == NULL) {
    // Missing separator between match fields and action param fields
#ifdef SDNETCONFIG_DEBUG
    fprintf(stderr, "[%3u] missing => separator\n", line_no);
#endif
    goto out_error;
  }
  *param_sep = '\0';

  char *match_str = *cursor;
  char *param_str = param_sep + 4;

  if (!parse_match_fields(entry, t, match_str, line_no)) {
    // Failed to parse the provided match fields for this table
    goto out_error;
  }

  if (!parse_param_fields(entry, t, a, param_str, line_no)) {
    // Failed to parse the provided action parameters for this table + action
    goto out_error;
  }

  return true;

 out_error:
  return false;
}

struct sdnet_config *sdnet_config_load_p4bm(const char *config_file)
{
  struct sdnet_config *config = (struct sdnet_config *) calloc(1, sizeof(struct sdnet_config));
  if (config == NULL) {
    return NULL;
  }

  config->num_entries = 0;

  FILE *f = fopen(config_file, "r");
  if (f == NULL) {
    return NULL;
  }

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
    struct sdnet_config_entry *entry = (struct sdnet_config_entry *) calloc(1, sizeof(struct sdnet_config_entry));
    if (entry == NULL) {
      goto out_error;
    }
    entry->raw = strdup(line);
    entry->line_no = line_no;

    if (!parse_table_add_cmd(&XilVitisNetP4TargetConfig_sdnet_0,
			     entry,
			     line,
			     line_no)) {
      // Failed to parse the table add command
      sdnet_config_entry_free(entry);
      goto out_error;
    }

    // Add this entry to the list of table entries
    config->entries[config->num_entries] = entry;
    config->num_entries++;
  }

  fclose(f);

  return config;

 out_error:
  fclose(f);
  sdnet_config_free(config);
  return NULL;
}


