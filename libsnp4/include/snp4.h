//
// Copyright (c) 2021, The Regents of the University of California,
// through Lawrence Berkeley National Laboratory (subject to receipt of
// any required approvals from the U.S. Dept. of Energy).  All rights
// reserved.
//

#ifndef SNP4_H
#define SNP4_H

#include <gmp.h>		/* mpz_* */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>		/* bool */
#include <stdint.h>		/* uint16_t, uint8_t */
#include <stddef.h>		/* size_t */
#include <stdio.h>		/* FILE */

extern void * snp4_init(uintptr_t snp4_base_addr);
extern bool snp4_deinit(void * snp4_handle);
extern bool snp4_reset_all_tables(void * snp4_handle);
extern bool snp4_reset_one_table(void * snp4_handle, const char * table_name);
extern bool snp4_table_insert_kma(void * snp4_handle,
				  const char * table_name,
				  uint8_t * key,
				  size_t key_len,
				  uint8_t * mask,
				  size_t mask_len,
				  const char * action_name,
				  uint8_t * params,
				  size_t params_len,
				  uint32_t priority);
extern bool snp4_table_delete_k(void * snp4_handle,
				const char * table_name,
				uint8_t * key,
				size_t    key_len,
				uint8_t * mask,
				size_t    mask_len);
extern void snp4_print_target_config (void);

#define SN_RULE_MAX_MATCHES 64
#define SN_RULE_MAX_PARAMS  64

struct sn_field_spec {
  unsigned int size;
  char         type;
};

struct sn_param_spec {
  unsigned int size;
};

enum table_endian {
  TABLE_ENDIAN_BIG,
  TABLE_ENDIAN_LITTLE,
};

struct sn_table_info {
  enum table_endian endian;
  uint16_t key_size_bits;
  uint16_t action_id_size_bits;
  uint16_t response_size_bits;
  uint16_t priority_size_bits;
  struct sn_field_spec field_specs[SN_RULE_MAX_MATCHES];
  size_t num_fields;
};

struct sn_action_info {
  uint16_t param_size_bits;
  struct sn_param_spec param_specs[SN_RULE_MAX_PARAMS];
  size_t num_params;
};

enum sn_match_format {
  SN_MATCH_FORMAT_KEY_MASK,
  SN_MATCH_FORMAT_KEY_ONLY,
  SN_MATCH_FORMAT_PREFIX,
  SN_MATCH_FORMAT_RANGE,
  SN_MATCH_FORMAT_UNUSED,
};

struct sn_match {
  enum sn_match_format t;
  union sn_match_value {
    /* KEY_MASK */
    struct {
      mpz_t key;
      mpz_t mask;
    } key_mask;
    /* KEY_ONLY */
    struct {
      mpz_t key;
    } key_only;
    /* PREFIX */
    struct {
      mpz_t key;
      uint16_t prefix_len;
    } prefix;
    /* RANGE */
    struct {
      uint16_t lower;
      uint16_t upper;
    } range;
    /* UNUSED */
    struct {
    } unused;
  } v;
};

enum sn_param_format {
  SN_PARAM_FORMAT_UI,	/* unsigned long */
  SN_PARAM_FORMAT_MPZ,	/* mpz_t */
};

struct sn_param {
  enum sn_param_format t;
  union sn_param_value {
    unsigned long ui;
    mpz_t mpz;
  } v;
};

struct sn_rule {
  char * table_name;
  
  struct sn_match matches[SN_RULE_MAX_MATCHES];
  size_t num_matches;

  char * action_name;
  struct sn_param params[SN_RULE_MAX_PARAMS];
  size_t num_params;

  uint32_t priority;
};

struct sn_pack {
  uint8_t * key;
  size_t    key_len;

  uint8_t * mask;
  size_t    mask_len;

  uint8_t * params;
  size_t    params_len;
};

enum snp4_status {
  SNP4_STATUS_OK,
  SNP4_STATUS_NULL_ENTRY,
  SNP4_STATUS_NULL_PACK,
  SNP4_STATUS_MALLOC_FAIL,
  SNP4_STATUS_NULL_RULE,
  SNP4_STATUS_FIELD_SPEC_OVERFLOW,
  SNP4_STATUS_FIELD_SPEC_FORMAT_INVALID,
  SNP4_STATUS_FIELD_SPEC_UNKNOWN_TYPE,
  SNP4_STATUS_FIELD_SPEC_SIZE_MISMATCH,

  SNP4_STATUS_PACK_KEY_TOO_BIG,
  SNP4_STATUS_PACK_MASK_TOO_BIG,
  SNP4_STATUS_PACK_PARAMS_TOO_BIG,

  SNP4_STATUS_MATCH_INVALID_FORMAT,
  SNP4_STATUS_MATCH_MASK_TOO_WIDE,
  SNP4_STATUS_MATCH_INVALID_BITFIELD_MASK,
  SNP4_STATUS_MATCH_INVALID_CONSTANT_MASK,
  SNP4_STATUS_MATCH_INVALID_PREFIX_MASK,
  SNP4_STATUS_MATCH_INVALID_RANGE_MASK,
  SNP4_STATUS_MATCH_INVALID_UNUSED_MASK,
  SNP4_STATUS_MATCH_KEY_TOO_BIG,
  SNP4_STATUS_MATCH_MASK_TOO_BIG,

  SNP4_STATUS_INVALID_TABLE_NAME,
  SNP4_STATUS_INVALID_TABLE_CONFIG,
  SNP4_STATUS_INVALID_ACTION_FOR_TABLE,

  SNP4_STATUS_PARAM_INVALID_FORMAT,
  SNP4_STATUS_PARAM_SPEC_OVERFLOW,
  SNP4_STATUS_PARAM_SPEC_SIZE_MISMATCH,
  SNP4_STATUS_PARAM_TOO_BIG,
};

extern enum snp4_status snp4_config_load_table_and_action_info(const char *table_name, const char *action_name, struct sn_table_info *table_info, struct sn_action_info *action_info);

extern enum snp4_status snp4_rule_pack(const struct sn_rule * rule, struct sn_pack * pack);
extern void snp4_rule_clear(struct sn_rule * rule);
extern void snp4_pack_clear(struct sn_pack * pack);
extern void snp4_rule_param_clear(struct sn_param *param);
extern void snp4_rule_match_clear(struct sn_match *match);

enum sn_cfg_op {
  OP_TABLE_ADD = 1,
  OP_TABLE_DELETE,
};
struct sn_cfg {
  // Original context for config file entry
  uint32_t line_no;
  const char *raw;

  // Parsed content of config file entry
  enum sn_cfg_op op;
  struct sn_rule rule;

  // Packed key, mask, params ready for insertion into a smartnic FPGA table
  struct sn_pack pack;
};

struct sn_cfg_set {
  uint32_t num_entries;
  struct sn_cfg *entries[1024];
};

extern struct sn_cfg_set *snp4_cfg_set_load_p4bm(FILE * f);
extern void snp4_cfg_set_free(struct sn_cfg_set *cfg_set);

#ifdef __cplusplus
}
#endif

#endif	/* SNP4_H */
