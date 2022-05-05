#include <stdlib.h> /* calloc */
#include <stdint.h> /* uint32_t */
#include <gmp.h>    /* mpz_* */
#include <stdio.h>  /* stderr, NULL */

#include "sdnetapi.h"		/* API */

static enum sn_config_status pack_partial_key_mask(mpz_t *key_part, mpz_t *mask_part, const struct sn_match *match, const struct sdnet_field_spec *field_spec) {
  enum sn_config_status rc;
  
  // Set up an all-zeros mask for this field
  mpz_t zero_mask;
  mpz_init_set_ui(zero_mask, 0);

  // Set up an all-ones mask for this field
  mpz_t ones_mask;
  mpz_init_set_ui(ones_mask, 1);
  mpz_mul_2exp(ones_mask, ones_mask, field_spec->size);
  mpz_sub_ui(ones_mask, ones_mask, 1);

  // Default mask will be set to a reasonable default for this field
  mpz_t mask_default;
  mpz_init(mask_default);
  bool has_mask;

  // Fill in the key and mask based on the information provided by the user
  // Note: This mapping is based on the encoding of the match info given by the user, not the
  //       field type defined in p4.  The expanded/defaulted user input will be validated against
  //       the p4 field definition further below.
  switch (match->t) {
  case SN_MATCH_FORMAT_KEY_MASK:
    mpz_set(*key_part,  match->v.key_mask.key);
    mpz_set(*mask_part, match->v.key_mask.mask);
    has_mask = true;
    break;
  case SN_MATCH_FORMAT_KEY_ONLY:
    mpz_set(*key_part, match->v.key_only.key);
    has_mask = false;
    break;
  case SN_MATCH_FORMAT_PREFIX:
    mpz_set(*key_part, match->v.prefix.key);
    if (match->v.prefix.prefix_len > field_spec->size) {
      // Mask prefix length is out of range
      rc = SN_CONFIG_STATUS_MATCH_MASK_TOO_WIDE;
      goto out_error;
    }
    // mask_field = ((1 << prefix_len) - 1)
    mpz_set_ui(*mask_part, 1);
    mpz_mul_2exp(*mask_part, *mask_part, match->v.prefix.prefix_len);
    mpz_sub_ui(*mask_part, *mask_part, 1);
    // mask_field = mask_field << (field_size_bits - prefix_len)
    mpz_mul_2exp(*mask_part, *mask_part, field_spec->size - match->v.prefix.prefix_len);
    has_mask = true;
    break;
  case SN_MATCH_FORMAT_RANGE:
    mpz_set_ui(*key_part,  match->v.range.lower);
    mpz_set_ui(*mask_part, match->v.range.upper);
    has_mask = true;
    break;
  case SN_MATCH_FORMAT_UNUSED:
    // No key or mask provided, field should be ignored/disabled
    mpz_set_ui(*key_part, 0);
    mpz_set(*mask_part, zero_mask);
    has_mask = true;
    break;
  default:
    // Unknown field format
    rc = SN_CONFIG_STATUS_MATCH_INVALID_FORMAT;
    goto out_error;
  }

  // Provide a field-type-specific default value for the mask field
  // Note: The default mask is computed based on the type of the field *as defined in p4*
  // This will be used IFF the caller has not provided an encoding that includes a mask
  switch (field_spec->type) {
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
    mpz_set(mask_default, *key_part);
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
    rc = SN_CONFIG_STATUS_FIELD_SPEC_UNKNOWN_TYPE;
    goto out_error;
    break;
  }

  // Apply a default mask?
  if (!has_mask) {
    // No mask was provided by (or inferred for) the caller.  Use an appropriate default for this field type
    mpz_set(*mask_part, mask_default);
  }
  
  // Validate the provided or default mask for this field based on the field type
  switch (field_spec->type) {
  case 'b':
    // Bit Field: Allowed masks: 0 / -1
    if (!((mpz_cmp(*mask_part, zero_mask) == 0) ||
	  (mpz_cmp(*mask_part, ones_mask) == 0))) {
      // Invalid mask value
      rc = SN_CONFIG_STATUS_MATCH_INVALID_BITFIELD_MASK;
      goto out_error;
    }
    break;
  case 'c':
    // Constant Field: Allowed masks: -1
    if (!((mpz_cmp(*mask_part, ones_mask) == 0))) {
      // Invalid mask value
      rc = SN_CONFIG_STATUS_MATCH_INVALID_CONSTANT_MASK;
      goto out_error;
    }
    break;
  case 'p':
    // Prefix Field: Allowed masks: mask must be contiguous 1's in msbs
    if (!(mpz_cmp(*mask_part, zero_mask) == 0)) {
      // Check if we have a zero bit above the lowest one bit
      mp_bitcnt_t lowest_one_pos = mpz_scan1(*mask_part, 0);
      if (mpz_scan0(*mask_part, lowest_one_pos) < field_spec->size) {
	// Found a zero above the lowest one bit.  Mask is not in prefix form.
	rc = SN_CONFIG_STATUS_MATCH_INVALID_PREFIX_MASK;
	goto out_error;
      }
    }
    break;
  case 'r':
    // Range Field: Allowed masks: mask must be >= key (ie. includes 1 or more values)
    if (mpz_cmp(*mask_part, *key_part) < 0) {
      // upper limit of the range is < lower limit of range
      rc = SN_CONFIG_STATUS_MATCH_INVALID_RANGE_MASK;
      goto out_error;
    }
    break;
  case 't':
    // Ternary Field: Allowed masks: any
    break;
  case 'u':
    // Unused Field: Allowed masks: 0
    if (!(mpz_cmp(*mask_part, zero_mask) != 0)) {
      // Non-zero mask provided for unused field
      rc = SN_CONFIG_STATUS_MATCH_INVALID_UNUSED_MASK;
      goto out_error;
    }
    break;
  default:
    // Unknown Field Type
    rc = SN_CONFIG_STATUS_FIELD_SPEC_UNKNOWN_TYPE;
    goto out_error;
    break;
  }

  // Ensure that the key is not wider than the field
  if (mpz_sizeinbase(*key_part, 2) > field_spec->size) {
    // Key is too large for field
    rc = SN_CONFIG_STATUS_MATCH_KEY_TOO_BIG;
    goto out_error;
  }

  // Ensure that the mask is not wider than the field
  if (mpz_sizeinbase(*mask_part, 2) > field_spec->size) {
    // Mask is too large for field
    rc = SN_CONFIG_STATUS_MATCH_MASK_TOO_BIG;
    goto out_error;
  }

  rc = SN_CONFIG_STATUS_OK;
  // Fall through to clean up
  
 out_error:
  mpz_clear(zero_mask);
  mpz_clear(ones_mask);
  mpz_clear(mask_default);

  return rc;
}

static enum sn_config_status sn_rule_pack_matches(const struct sdnet_field_spec field_specs[], unsigned int key_size_bits, const struct sn_match matches[], size_t num_matches, struct sn_pack * pack)
{
  enum sn_config_status rc;

  // Build up key and mask into large, multi-precision integers
  mpz_t key;
  mpz_t mask;
  mpz_init(key);
  mpz_init(mask);

  mpz_t key_part;
  mpz_t mask_part;
  mpz_init(key_part);
  mpz_init(mask_part);

  // NOTE: The field_spec describes the fields of the key from lsbs up to msbs.
  //       The matches array describes the fields of the key from msbs down to lsbs.
  //
  //       The field_spec array will be consumed in reverse order to account for this
  //       discrepancy.

  for (unsigned int i = 0; i < num_matches; i++) {
    const struct sdnet_field_spec * field_spec = &field_specs[num_matches - i];

    rc = pack_partial_key_mask(&key_part, &mask_part, &matches[i], field_spec);
    if (rc != SN_CONFIG_STATUS_OK) {
      goto out_key_mask_extend_fail;
    }

#ifdef SDNETCONFIG_DEBUG
    gmp_fprintf(stderr, "key part:  %#0*Zx\n", (field_spec->size + 3) / 4 + 2, key_part);
    gmp_fprintf(stderr, "mask part: %#0*Zx\n", (field_spec->size + 3) / 4 + 2, mask_part);
#endif

    // Extend the current key to include this field
    // by shifting all previous fields to the left and inclusive-or the new key field into the lsbs
    mpz_mul_2exp(key, key, field_spec->size);
    mpz_ior(key, key, key_part);

    // Extend the current mask to include this field
    // by shifting all previous fields to the left and inclusive-or the new mask field into the lsbs
    mpz_mul_2exp(mask, mask, field_spec->size);
    mpz_ior(mask, mask, mask_part);
  }

  /*
   * Do the final packing of the complete key/mask into a byte array
   */
  
  // Allocate space for final key and mask storage
  unsigned int key_size_padded_bytes = (key_size_bits + 7) / 8;
  pack->key = (uint8_t *) calloc(1, key_size_padded_bytes);
  pack->key_len = key_size_padded_bytes;
  // Mask is the same length as the key
  pack->mask = (uint8_t *) calloc(1, key_size_padded_bytes);
  pack->mask_len = key_size_padded_bytes;
  
  // Pack the computed key into the key array
  unsigned int final_key_bits = mpz_sizeinbase(key, 2);
  if (final_key_bits > key_size_bits) {
    // Key is larger than expected for this table
    rc = SN_CONFIG_STATUS_PACK_KEY_TOO_BIG;
    goto out_packing_error;
  }
  unsigned int final_key_pad_bytes = pack->key_len - ((final_key_bits + 7) / 8);
  mpz_export(pack->key + final_key_pad_bytes, NULL, 1, 1, 1, 0, key);

  // Pack the computed mask into the mask array
  unsigned int final_mask_bits = mpz_sizeinbase(mask, 2);
  if (final_mask_bits > key_size_bits) {
    // Mask is larger than expected for this table
    rc = SN_CONFIG_STATUS_PACK_MASK_TOO_BIG;
    goto out_packing_error;
  }
  unsigned int final_mask_pad_bytes = pack->mask_len - ((final_mask_bits + 7) / 8);
  mpz_export(pack->mask + final_mask_pad_bytes, NULL, 1, 1, 1, 0, mask);

  // All OK
  mpz_clear(key_part);
  mpz_clear(mask_part);
  mpz_clear(key);
  mpz_clear(mask);

  return SN_CONFIG_STATUS_OK;

 out_packing_error:
  if (pack->key)    free(pack->key);
  if (pack->mask)   free(pack->mask);
 out_key_mask_extend_fail:
  mpz_clear(key_part);
  mpz_clear(mask_part);
  mpz_clear(key);
  mpz_clear(mask);
  return rc;
}

static enum sn_config_status pack_partial_param(mpz_t *param_part, const struct sn_param *param, const struct sdnet_param_spec *param_spec)
{
  // Fill in the param based on the information provided by the user
  switch (param->t) {
  case SN_PARAM_FORMAT_UI:
    mpz_init_set_ui(*param_part, param->v.ui);
    break;
  case SN_PARAM_FORMAT_MPZ:
    mpz_init_set(*param_part, param->v.mpz);
    break;
  default:
    return SN_CONFIG_STATUS_PARAM_INVALID_FORMAT;
  }

  // Ensure that the param is not wider than it should be
  if (mpz_sizeinbase(*param_part, 2) > param_spec->size) {
    // Param is too large for field
    return SN_CONFIG_STATUS_PARAM_TOO_BIG;
  }

  return SN_CONFIG_STATUS_OK;
}

static enum sn_config_status sn_rule_pack_params(const struct sdnet_param_spec param_specs[], unsigned int table_param_size_bits, unsigned int action_param_size_bits, const struct sn_param params[], size_t num_params, struct sn_pack * pack) {
  enum sn_config_status rc;
  
  // Check if we even require parameters for this action
  if (action_param_size_bits == 0) {
    // No parameters, pack an empty array with 1 byte
    pack->params = (uint8_t *)calloc(1, 1);
    pack->params_len = 0;
    return SN_CONFIG_STATUS_OK;
  }

  // Build up the params into a large, multi-precision integer
  mpz_t params_all;
  mpz_init(params_all);

  mpz_t param_part;
  mpz_init(param_part);
  for (unsigned int i = 0; i < num_params; i++) {
    const struct sdnet_param_spec * param_spec = &param_specs[i];

    rc = pack_partial_param(&param_part, &params[i], param_spec);
    if (rc != SN_CONFIG_STATUS_OK) {
      goto out_param_extend_fail;
    }

#ifdef SDNETCONFIG_DEBUG
    gmp_fprintf(stderr, "param part:  %#0*Zx\n", (param_spec->size + 3) / 4 + 2, param_part);
#endif

    // Extend the current param to include this field
    // by shifting all previous params to the left and inclusive-or the new partial param into the lsbs
    mpz_mul_2exp(params_all, params_all, param_spec->size);
    mpz_ior(params_all, params_all, param_part);
  }

  // Allocate space for the final param storage
  // Note: The params for any given action are packed into the lsbs of a bit vector that is sized such that it
  //       would fit the parameters of the longest action parameters.  This means that actions with shorter parameters
  //       still need to allocate a max-sized byte array.
  unsigned int param_size_padded_bytes = (table_param_size_bits + 7) / 8;
  pack->params = (uint8_t *) calloc(1, param_size_padded_bytes);
  pack->params_len = param_size_padded_bytes;

  // Pack the computed params into the param array
  unsigned int final_param_bits = mpz_sizeinbase(params_all, 2);
  if (final_param_bits > action_param_size_bits) {
    // Params are larger than expected for this action
    rc = SN_CONFIG_STATUS_PACK_PARAMS_TOO_BIG;
    goto out_packing_error;
  }
  unsigned int final_param_pad_bytes = pack->params_len - ((final_param_bits + 7) / 8);
  mpz_export(pack->params + final_param_pad_bytes, NULL, 1, 1, 1, 0, params_all);

  // All OK
  mpz_clear(params_all);
  mpz_clear(param_part);

  return SN_CONFIG_STATUS_OK;

 out_packing_error:
  if (pack->params) free(pack->params);
 out_param_extend_fail:
  mpz_clear(params_all);
  mpz_clear(param_part);
  return rc;
}

void sn_rule_param_clear(struct sn_param *param)
{
    switch (param->t) {
    case SN_PARAM_FORMAT_UI:
      // Nothing to free
      break;
    case SN_PARAM_FORMAT_MPZ:
      mpz_clear(param->v.mpz);
      break;
    default:
      // Unknown encoding
      break;
    }
}

void sn_rule_match_clear(struct sn_match *match)
{
  switch (match->t) {
  case SN_MATCH_FORMAT_KEY_MASK:
    mpz_clear(match->v.key_mask.key);
    mpz_clear(match->v.key_mask.mask);
    break;
  case SN_MATCH_FORMAT_KEY_ONLY:
    mpz_clear(match->v.key_only.key);
    break;
  case SN_MATCH_FORMAT_PREFIX:
    mpz_clear(match->v.prefix.key);
    break;
  case SN_MATCH_FORMAT_RANGE:
    // Nothing to free
    break;
  case SN_MATCH_FORMAT_UNUSED:
    // Nothing to free
    break;
  default:
    // Unknown encoding
    break;
  }
}

void sn_rule_clear(struct sn_rule * rule)
{
  if (rule->table_name)  free(rule->table_name);
  if (rule->action_name) free(rule->action_name);

  for (unsigned int i = 0; i < rule->num_matches; i++) {
    sn_rule_match_clear(&rule->matches[i]);
  }

  for (unsigned int i = 0; i < rule->num_params; i++) {
    sn_rule_param_clear(&rule->params[i]);
  }
}

void sn_pack_clear(struct sn_pack * pack)
{
  if (pack->key)    free(pack->key);
  if (pack->mask)   free(pack->mask);
  if (pack->params) free(pack->params);
}

enum sn_config_status sn_rule_pack(const struct sn_rule * rule, struct sn_pack * pack)
{
  enum sn_config_status rc;

  if (rule == NULL) {
    return SN_CONFIG_STATUS_NULL_RULE;
  }

  if (pack == NULL) {
    return SN_CONFIG_STATUS_NULL_PACK;
  }

  // Load the table and action info required for packing
  struct sdnet_table_info table_info;
  struct sdnet_action_info action_info;
  rc = sdnet_config_load_table_and_action_info(rule->table_name, rule->action_name, &table_info, &action_info);
  if (rc != SN_CONFIG_STATUS_OK) {
    return rc;
  }

  // Ensure that the caller has provided exactly the right number of matches for this table
  if (rule->num_matches != table_info.num_fields) {
    // Wrong number of fields provided for this table
    return SN_CONFIG_STATUS_FIELD_SPEC_SIZE_MISMATCH;
  }

  // Ensure that the caller has provided exactly the right number of parameters for this action
  if (rule->num_params != action_info.num_params) {
    // Wrong number of params provided for this action
    return SN_CONFIG_STATUS_PARAM_SPEC_SIZE_MISMATCH;
  }
  
  // Pack the key and mask
  rc = sn_rule_pack_matches(table_info.field_specs,
			    table_info.key_size_bits,
			    rule->matches,
			    rule->num_matches,
			    pack);
  if (rc != SN_CONFIG_STATUS_OK) {
    goto out_error;
  }

  // Pack the params
  rc = sn_rule_pack_params(action_info.param_specs,
			   table_info.response_size_bits - table_info.action_id_size_bits,
			   action_info.param_size_bits,
			   rule->params,
			   rule->num_params,
			   pack);
  if (rc != SN_CONFIG_STATUS_OK) {
    goto out_error;
  }

  // All OK
  return SN_CONFIG_STATUS_OK;

 out_error:
  sn_pack_clear(pack);
  return rc;
}
