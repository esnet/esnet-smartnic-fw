#include <stdio.h> /* NULL, FILE */
#include <stdlib.h> /* calloc */
#include <stdint.h> /* uint32_t */
#include <string.h> /* strsep */
#include <gmp.h>    /* mpz_* */

#include "sdnetapi.h"		/* API */
#include "sdnet_0_defs.h"	/* XilVitisNetP4TargetConfig_sdnet_0 */
#include "unused.h"		/* UNUSED */
#include "array_size.h"		/* ARRAY_SIZE */

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

static enum sn_config_status sdnet_config_load_field_specs(const struct XilVitisNetP4TargetTableConfig *t, struct sdnet_field_spec field_specs[], size_t *num_field_specs) {
  enum sn_config_status rc;

  unsigned int entries = 0;

  char *table_fmt = strdup(t->Config.CamConfig.FormatStringPtr);
  char *table_fmt_cursor = table_fmt;
  char *field_fmt;
  while((field_fmt = strsep(&table_fmt_cursor, ":")) != NULL) {
    if (entries >= *num_field_specs) {
      rc = SN_CONFIG_STATUS_FIELD_SPEC_OVERFLOW;
      goto out_spec_table_too_small;
    }
    struct sdnet_field_spec * field_spec = &field_specs[entries];
    entries++;

    int matched = sscanf(field_fmt, "%u%c", &field_spec->size, &field_spec->type);
    if (matched != 2) {
      // Field format is not in the expected form
      rc = SN_CONFIG_STATUS_FIELD_SPEC_FORMAT_INVALID;
      goto out_spec_format_invalid;
    }
  }

  // All OK
  *num_field_specs = entries;
  return SN_CONFIG_STATUS_OK;

 out_spec_format_invalid:
 out_spec_table_too_small:
  free(table_fmt);
  return(rc);
}

static enum sn_config_status sdnet_config_load_param_specs(const XilVitisNetP4Action *a, struct sdnet_param_spec param_specs[], size_t *num_param_specs) {
  if (a->ParamListSize > *num_param_specs) {
    // Param list doesn't fit in the provided array
    return SN_CONFIG_STATUS_PARAM_SPEC_OVERFLOW;
  }

  for (unsigned int pidx = 0; pidx < a->ParamListSize; pidx++) {
    XilVitisNetP4Attribute *attr = &a->ParamListPtr[pidx];
    struct sdnet_param_spec * param_spec = &param_specs[pidx];

    param_spec->size = attr->Value;
  }

  // All OK
  *num_param_specs = a->ParamListSize;
  return SN_CONFIG_STATUS_OK;
}

/*
 * Copy out some key facts about a p4 table to insulate the caller from all of the data types used internally
 * by the vitisnet p4 libraries.  This insulation makes unit testing easier for code that uses these types.
 */
static enum sn_config_status sdnet_config_load_table_info(const struct XilVitisNetP4TargetTableConfig *t, struct sdnet_table_info *table_info) {
  enum sn_config_status rc;

  switch (t->Config.Endian) {
  case XIL_VITIS_NET_P4_LITTLE_ENDIAN:
    table_info->endian = TABLE_ENDIAN_LITTLE;
    break;
  case XIL_VITIS_NET_P4_BIG_ENDIAN:
    table_info->endian = TABLE_ENDIAN_BIG;
    break;
  default:
    return SN_CONFIG_STATUS_INVALID_TABLE_CONFIG;
  }

  table_info->key_size_bits = t->Config.KeySizeBits;
  table_info->action_id_size_bits = t->Config.ActionIdWidthBits;

  table_info->response_size_bits = t->Config.CamConfig.ResponseSizeBits;
  table_info->priority_size_bits = t->Config.CamConfig.PrioritySizeBits;

  // Load this table's field spec into an array
  table_info->num_fields = ARRAY_SIZE(table_info->field_specs);
  rc = sdnet_config_load_field_specs(t, table_info->field_specs, &table_info->num_fields);
  if (rc != SN_CONFIG_STATUS_OK) {
    return rc;
  }

  return SN_CONFIG_STATUS_OK;
}

/*
 * Copy out some key facts about a p4 table to insulate the caller from all of the data types used internally
 * by the vitisnet p4 libraries.  This insulation makes unit testing easier for code that uses these types.
 */
static enum sn_config_status sdnet_config_load_action_info(const struct XilVitisNetP4Action *a, struct sdnet_action_info *action_info) {
  enum sn_config_status rc;

  action_info->param_size_bits = sdnet_config_get_param_size_bits(a);

  action_info->num_params = ARRAY_SIZE(action_info->param_specs);
  rc = sdnet_config_load_param_specs(a, action_info->param_specs, &action_info->num_params);
  if (rc != SN_CONFIG_STATUS_OK) {
    return rc;
  }

  return SN_CONFIG_STATUS_OK;
}

/*
 * Copy out some key facts about a p4 table and action to insulate the caller from all of the data types used internally
 * by the vitisnet p4 libraries.  This insulation makes unit testing easier for code that uses these types.
 */
enum sn_config_status sdnet_config_load_table_and_action_info(const char *table_name, const char *action_name, struct sdnet_table_info *table_info, struct sdnet_action_info *action_info) {
  enum sn_config_status rc;

  // Validate the table name
  const struct XilVitisNetP4TargetTableConfig *t = sdnet_config_find_table_by_name(&XilVitisNetP4TargetConfig_sdnet_0, table_name);
  if (t == NULL) {
    // No matching table name
    return SN_CONFIG_STATUS_INVALID_TABLE_NAME;
  }

  rc = sdnet_config_load_table_info(t, table_info);
  if (rc != SN_CONFIG_STATUS_OK) {
    return rc;
  }

  // Validate the action name
  const struct XilVitisNetP4Action *a = sdnet_config_find_action_by_name(t, action_name);
  if (a == NULL) {
    // No matching action for this table
    return SN_CONFIG_STATUS_INVALID_ACTION_FOR_TABLE;
  }

  rc = sdnet_config_load_action_info(a, action_info);
  if (rc != SN_CONFIG_STATUS_OK) {
    return rc;
  }

  return SN_CONFIG_STATUS_OK;
}

