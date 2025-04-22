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

extern size_t snp4_sdnet_count(void);
extern bool snp4_sdnet_present(unsigned int sdnet_idx);
extern void * snp4_init(unsigned int sdnet_idx, uintptr_t snp4_base_addr);
extern bool snp4_deinit(void * snp4_handle);
extern void snp4_log_enable(void * snp4_handle, bool enable, const char * prefix);
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
				  uint32_t priority,
				  bool replace);
extern bool snp4_table_delete_k(void * snp4_handle,
				const char * table_name,
				uint8_t * key,
				size_t    key_len,
				uint8_t * mask,
				size_t    mask_len);
extern bool snp4_table_ecc_counters_read(void * snp4_handle,
                                         const char * table_name,
                                         uint32_t * corrected_single_bit_errors,
                                         uint32_t * detected_double_bit_errors);
extern void snp4_print_target_config (unsigned int sdnet_idx);


struct snp4_table_data {
  uint8_t * value;
  uint8_t * mask;
  uint32_t width; // in bits
  size_t len;     // in bytes
};

struct snp4_table_entry {
  const char * table_name;

  uint32_t priority;
  struct snp4_table_data key;

  struct {
    const char * name;
    struct snp4_table_data params;
  } action;
};

extern bool snp4_table_for_each_entry(void * snp4_handle,
                                      const char * table_name,
                                      bool (*callback)(const struct snp4_table_entry * entry,
                                                       void * arg),
                                      void * arg);

// Define some limits for the supported pipeline properties handled by this library
// NOTE: These are not necessarily related to the limits of the underlying hardware
#define SNP4_MAX_PIPELINE_TABLES 64
#define SNP4_MAX_PIPELINE_COUNTER_BLOCKS 64
#define SNP4_MAX_TABLE_MATCHES 64
#define SNP4_MAX_TABLE_ACTIONS 64
#define SNP4_MAX_ACTION_PARAMS 64

struct snp4_info_param {
  const char * name;

  uint16_t bits;
};

struct snp4_info_action {
  const char * name;

  uint16_t param_bits;
  struct snp4_info_param params[SNP4_MAX_ACTION_PARAMS];
  uint16_t num_params;
};

enum snp4_info_match_type {
  SNP4_INFO_MATCH_TYPE_INVALID,
  SNP4_INFO_MATCH_TYPE_BITFIELD,
  SNP4_INFO_MATCH_TYPE_CONSTANT,
  SNP4_INFO_MATCH_TYPE_PREFIX,
  SNP4_INFO_MATCH_TYPE_RANGE,
  SNP4_INFO_MATCH_TYPE_TERNARY,
  SNP4_INFO_MATCH_TYPE_UNUSED,
};

struct snp4_info_match {
  enum snp4_info_match_type type;
  uint16_t bits;
};

enum snp4_info_table_endian {
  SNP4_INFO_TABLE_ENDIAN_LITTLE = 0,
  SNP4_INFO_TABLE_ENDIAN_BIG = 1,
};

enum snp4_info_table_mode {
  SNP4_INFO_TABLE_MODE_BCAM,
  SNP4_INFO_TABLE_MODE_STCAM,
  SNP4_INFO_TABLE_MODE_TCAM,
  SNP4_INFO_TABLE_MODE_DCAM,
  SNP4_INFO_TABLE_MODE_TINY_BCAM,
  SNP4_INFO_TABLE_MODE_TINY_TCAM,
};

struct snp4_info_table {
  const char * name;
  uint32_t num_entries;

  enum snp4_info_table_endian endian;
  enum snp4_info_table_mode mode;
  uint32_t num_masks; // Only valid when mode is SNP4_INFO_TABLE_MODE_STCAM.

  uint16_t key_bits;
  struct snp4_info_match matches[SNP4_MAX_TABLE_MATCHES];
  uint16_t num_matches;

  uint16_t response_bits;
  uint16_t actionid_bits;
  struct snp4_info_action actions[SNP4_MAX_TABLE_ACTIONS];
  uint16_t num_actions;

  bool priority_required;
  uint16_t priority_bits;
};

enum snp4_info_counter_type {
  SNP4_INFO_COUNTER_TYPE_PACKETS,
  SNP4_INFO_COUNTER_TYPE_BYTES,
  SNP4_INFO_COUNTER_TYPE_PACKETS_AND_BYTES,
  SNP4_INFO_COUNTER_TYPE_FLAG,
};

struct snp4_info_counter_block {
  const char * name;
  enum snp4_info_counter_type type;
  uint32_t width;
  uint32_t num_counters;

  const char* const* aliases;
  size_t num_aliases;
};

struct snp4_info_pipeline {
  const char * name;
  struct snp4_info_table tables[SNP4_MAX_PIPELINE_TABLES];
  uint16_t num_tables;
  struct snp4_info_counter_block counter_blocks[SNP4_MAX_PIPELINE_COUNTER_BLOCKS];
  uint16_t num_counter_blocks;
};

enum snp4_status {
  SNP4_STATUS_OK,
  SNP4_STATUS_NULL_PIPELINE,
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

  SNP4_STATUS_INFO_TOO_MANY_TABLES,
  SNP4_STATUS_INFO_TOO_MANY_MATCHES,
  SNP4_STATUS_INFO_TOO_MANY_ACTIONS,
  SNP4_STATUS_INFO_TOO_MANY_PARAMS,
  SNP4_STATUS_INFO_INVALID_ENDIAN,
  SNP4_STATUS_INFO_INVALID_MODE,

  SNP4_STATUS_INFO_TOO_MANY_COUNTER_BLOCKS,
  SNP4_STATUS_INFO_INVALID_COUNTER_TYPE,
};

extern enum snp4_status snp4_info_get_pipeline(unsigned int sdnet_idx, struct snp4_info_pipeline * pipeline);
extern const struct snp4_info_table * snp4_info_get_table_by_name(const struct snp4_info_pipeline * pipeline, const char * table_name);
extern const struct snp4_info_action * snp4_info_get_action_by_name(const struct snp4_info_table * table, const char * action_name);

enum sn_match_format {
  SN_MATCH_FORMAT_UNSET,
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
  SN_PARAM_FORMAT_UNSET,
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
  const char * table_name;

  struct sn_match matches[SNP4_MAX_TABLE_MATCHES];
  size_t num_matches;

  const char * action_name;
  struct sn_param params[SNP4_MAX_ACTION_PARAMS];
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

extern enum snp4_status snp4_rule_pack(const struct snp4_info_pipeline * pipeline, const struct sn_rule * rule, struct sn_pack * pack);
extern enum snp4_status snp4_rule_pack_matches(const struct snp4_info_match match_info_specs[], unsigned int key_size_bits, const struct sn_match matches[], size_t num_matches, struct sn_pack * pack);
extern enum snp4_status snp4_rule_pack_params(const struct snp4_info_param param_info_specs[], unsigned int table_param_size_bits, unsigned int action_param_size_bits, const struct sn_param params[], size_t num_params, struct sn_pack * pack);
extern void snp4_rule_init(struct sn_rule * rule);
extern void snp4_rule_clear(struct sn_rule * rule);
extern void snp4_pack_init(struct sn_pack * pack);
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
  struct sn_cfg *entries[10240];
};

extern struct sn_cfg_set *snp4_cfg_set_load_p4bm(FILE * f);
extern void snp4_cfg_set_free(struct sn_cfg_set *cfg_set);

// NOTE: Counters are cleared on read by the underlying vitisnetp4 driver.
extern bool snp4_counter_simple_read(void * snp4_handle, const char * block_name, uint32_t index, uint64_t * count);
extern bool snp4_counter_simple_write(void * snp4_handle, const char * block_name, uint32_t index, uint64_t value);
extern bool snp4_counter_combo_read(void * snp4_handle, const char * block_name, uint32_t index, uint64_t * packets, uint64_t * bytes);
extern bool snp4_counter_combo_write(void * snp4_handle, const char * block_name, uint32_t index, uint64_t packets, uint64_t bytes);
extern bool snp4_counter_block_simple_read(void * snp4_handle, const char * block_name, uint64_t * counts, size_t ncounts); // counts is array of size ncounts
extern bool snp4_counter_block_combo_read(void * snp4_handle, const char * block_name, uint64_t * packets, uint64_t * bytes, size_t ncounts); // packets/bytes are arrays of size ncounts
extern bool snp4_counter_block_reset(void * snp4_handle, const char * block_name);
extern bool snp4_counter_block_reset_all(void * snp4_handle);

#ifdef __cplusplus
}
#endif

#endif	/* SNP4_H */
