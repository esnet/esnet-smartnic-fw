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




