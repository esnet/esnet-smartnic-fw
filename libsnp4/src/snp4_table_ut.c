#include <string.h>		/* strcmp */
#include "snp4.h"		/* API */
#include "unused.h"		/* UNUSED */

static const struct sn_table_info t_p128 = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 128,
  .action_id_size_bits = 2,
  .response_size_bits = 64,
  .priority_size_bits = 8,
  .field_specs = {
    { .size = 128, .type = 'p', },
  },
  .num_fields = 1,
};

static const struct sn_table_info t_b17 = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 17,
  .action_id_size_bits = 2,
  .response_size_bits = 64,
  .priority_size_bits = 8,
  .field_specs = {
    { .size = 17, .type = 'b', },
  },
  .num_fields = 1,
};

static const struct sn_table_info t_c31 = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 31,
  .action_id_size_bits = 2,
  .response_size_bits = 64,
  .priority_size_bits = 8,
  .field_specs = {
    { .size = 31, .type = 'c', },
  },
  .num_fields = 1,
};

static const struct sn_table_info t_r16 = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 16,
  .action_id_size_bits = 2,
  .response_size_bits = 64,
  .priority_size_bits = 8,
  .field_specs = {
    { .size = 16, .type = 'r', },
  },
  .num_fields = 1,
};

static const struct sn_table_info t_t9 = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 9,
  .action_id_size_bits = 2,
  .response_size_bits = 64,
  .priority_size_bits = 8,
  .field_specs = {
    { .size = 9, .type = 't', },
  },
  .num_fields = 1,
};

static const struct sn_table_info t_u3 = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 3,
  .action_id_size_bits = 2,
  .response_size_bits = 64,
  .priority_size_bits = 8,
  .field_specs = {
    { .size = 3, .type = 'u', },
  },
  .num_fields = 1,
};

static const struct sn_table_info t_multi = {
  .endian = TABLE_ENDIAN_BIG,
  .key_size_bits = 127,
  .action_id_size_bits = 0,
  .response_size_bits = 61,
  .priority_size_bits = 3,
  .field_specs = {
    { .size = 3, .type = 'b', },
    { .size = 3, .type = 'c', },
    { .size = 6, .type = 'p', },
    { .size = 13, .type = 'r', },
    { .size = 7, .type = 't', },
    { .size = 2, .type = 'u', },
  },
};

static const struct sn_action_info a_nop = {
  .param_size_bits = 0,
  .param_specs = {
    {
      .size = 24,
    },
  },
  .num_params = 0,
};

static const struct sn_action_info a_one = {
  .param_size_bits = 24,
  .param_specs = {
    { .size = 24, },
  },
  .num_params = 1,
};

static const struct sn_action_info a_two = {
  .param_size_bits = 52,
  .param_specs = {
    { .size = 13, },
    { .size = 39, },
  },
  .num_params = 2,
};

static const struct sn_action_info a_three = {
  .param_size_bits = 60,
  .param_specs = {
    { .size = 13, },
    { .size = 39, },
    { .size = 8, },
  },
  .num_params = 3,
};

enum snp4_status snp4_config_load_table_and_action_info(const char *table_name, const char *action_name, struct sn_table_info *table_info, struct sn_action_info *action_info) {
  enum snp4_status rc;

  if (strcmp(table_name, "t_p128") == 0) {
    *table_info = t_p128;
  } else if (strcmp(table_name, "t_b17") == 0) {
    *table_info = t_b17;
  } else if (strcmp(table_name, "t_c31") == 0) {
    *table_info = t_c31;
  } else if (strcmp(table_name, "t_r16") == 0) {
    *table_info = t_r16;
  } else if (strcmp(table_name, "t_t9") == 0) {
    *table_info = t_t9;
  } else if (strcmp(table_name, "t_u3") == 0) {
    *table_info = t_u3;
  } else if (strcmp(table_name, "t_multi") == 0) {
    *table_info = t_multi;
  } else {
    return SNP4_STATUS_INVALID_TABLE_NAME;
  }

  if (strcmp(action_name, "a_nop") == 0) {
    *action_info = a_nop;
  } else if (strcmp(action_name, "a_one") == 0) {
    *action_info = a_one;
  } else if (strcmp(action_name, "a_two") == 0) {
    *action_info = a_two;
  } else if (strcmp(action_name, "a_three") == 0) {
    *action_info = a_three;
  } else {
    return SNP4_STATUS_INVALID_ACTION_FOR_TABLE;
  }
  
  rc = SNP4_STATUS_OK;
  return rc;
}

