//
// Copyright (c) 2021, The Regents of the University of California,
// through Lawrence Berkeley National Laboratory (subject to receipt of
// any required approvals from the U.S. Dept. of Energy).  All rights
// reserved.
//

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

extern void * sdnet_init(uintptr_t sdnet_base_addr);
extern bool sdnet_deinit(void * sdnet_handle);
extern bool sdnet_reset_all_tables(void * sdnet_handle);
extern bool sdnet_table_insert_kma(void * sdnet_handle,
				   const char * table_name,
				   uint8_t * key,
				   size_t key_len,
				   uint8_t * mask,
				   size_t mask_len,
				   const char * action_name,
				   uint8_t * params,
				   size_t params_len,
				   uint32_t priority);
extern bool sdnet_table_delete_k(void * sdnet_handle,
				 const char * table_name,
				 uint8_t * key,
				 size_t    key_len,
				 uint8_t * mask,
				 size_t    mask_len);
extern void sdnet_print_target_config (void);

enum sdnet_config_opcode {
			  OP_TABLE_ADD = 1,
			  OP_TABLE_DELETE,
};
struct sdnet_config_entry {
  // Original context for config file entry
  uint32_t line_no;
  const char *raw;

  // Parsed content of config file entry
  enum sdnet_config_opcode op;
  const char *table_name;
  uint8_t * key;
  size_t key_len;
  uint8_t * mask;
  size_t mask_len;
  const char * action_name;
  uint8_t * params;
  size_t params_len;
  uint32_t priority;
};

struct sdnet_config {
  uint32_t num_entries;
  struct sdnet_config_entry *entries[1024];
};

extern struct sdnet_config *sdnet_config_load_p4bm(const char *config_file);
extern void sdnet_config_free(struct sdnet_config *config);
