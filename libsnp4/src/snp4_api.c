//
// Copyright (c) 2021, The Regents of the University of California,
// through Lawrence Berkeley National Laboratory (subject to receipt of
// any required approvals from the U.S. Dept. of Energy).  All rights
// reserved.
//

#include <stdio.h>		/* fprintf */
#include <string.h>		/* memset, strcmp */
#include "snp4.h"		/* API */
#include "snp4_io.h"		/* snp4_io_reg_* */
#include "unused.h"		/* UNUSED() */

#include "vitisnetp4drv-intf.h"	/* Vitis driver wrapper */

struct snp4_user_context {
  uintptr_t base_addr;
  XilVitisNetP4EnvIf env;
  XilVitisNetP4TargetCtx target;
  XilVitisNetP4CounterCtx * counter_blocks;
  unsigned int sdnet_idx;
  const struct vitis_net_p4_drv_intf* intf;
};

static XilVitisNetP4ReturnType device_write(XilVitisNetP4EnvIf *EnvIfPtr, XilVitisNetP4AddressType address, uint32_t data) {
    // Validate inputs
    if (NULL == EnvIfPtr) {
        return XIL_VITIS_NET_P4_GENERAL_ERR_NULL_PARAM;
    }
    if (NULL == EnvIfPtr->UserCtx)	{
        return XIL_VITIS_NET_P4_GENERAL_ERR_INTERNAL_ASSERTION;
    }

    // Recover virtual base address from user context
    struct snp4_user_context * user_ctx;
    user_ctx = (struct snp4_user_context *)EnvIfPtr->UserCtx;

    // Do the operation
    snp4_io_reg_write(user_ctx->base_addr, address, data);

    return XIL_VITIS_NET_P4_SUCCESS;
}

static XilVitisNetP4ReturnType device_read(XilVitisNetP4EnvIf *EnvIfPtr, XilVitisNetP4AddressType address, uint32_t *dataPtr) {
    // Validate inputs
    if (EnvIfPtr == NULL) {
        return XIL_VITIS_NET_P4_GENERAL_ERR_NULL_PARAM;
    }
    if (EnvIfPtr->UserCtx == NULL) {
        return XIL_VITIS_NET_P4_GENERAL_ERR_INTERNAL_ASSERTION;
    }
    if (dataPtr == NULL) {
        return XIL_VITIS_NET_P4_GENERAL_ERR_INTERNAL_ASSERTION;
    }

    // Recover virtual base address from user context
    struct snp4_user_context * user_ctx;
    user_ctx = (struct snp4_user_context *)EnvIfPtr->UserCtx;

    // Do the operation
    snp4_io_reg_read(user_ctx->base_addr, address, dataPtr);

    return XIL_VITIS_NET_P4_SUCCESS;
}

#ifdef SDNETCONFIG_DEBUG
static XilVitisNetP4ReturnType log_info(XilVitisNetP4EnvIf *UNUSED(EnvIfPtr), const char *MessagePtr)
{
  fprintf(stdout, "%s\n", MessagePtr);

  return XIL_VITIS_NET_P4_SUCCESS;
}

static XilVitisNetP4ReturnType log_error(XilVitisNetP4EnvIf *UNUSED(EnvIfPtr), const char *MessagePtr)
{
  fprintf(stderr, "%s\n", MessagePtr);

  return XIL_VITIS_NET_P4_SUCCESS;
}
#endif

size_t snp4_sdnet_count(void)
{
  return vitis_net_p4_drv_intf_count();
}

bool snp4_sdnet_present(unsigned int sdnet_idx)
{
  return vitis_net_p4_drv_intf_get(sdnet_idx) != NULL;
}

static bool snp4_init_counter_blocks(struct snp4_user_context * snp4_user)
{
  const struct vitis_net_p4_drv_intf * intf = snp4_user->intf;
  const XilVitisNetP4TargetConfig * tcfg = intf->target.config;

  if (tcfg->CounterListSize == 0) {
    return true;
  }

  XilVitisNetP4CounterCtx * blocks = calloc(tcfg->CounterListSize, sizeof(*blocks));
  if (blocks == NULL) {
    return false;
  }

#ifdef SDNETCONFIG_DEBUG
  printf("DEBUG[%s]: CounterListSize=%u\n", __func__, tcfg->CounterListSize);
#endif

  unsigned int n;
  for (n = 0; n < tcfg->CounterListSize; ++n) {
#ifdef SDNETCONFIG_DEBUG
    const XilVitisNetP4TargetCounterConfig * cnt = tcfg->CounterListPtr[n];
    printf("    DEBUG[%s]: idx=%u, name='%s', base=0x%016lx, counter_type=%u, num_counters=%u, "
           "width=%u\n", __func__, n, cnt->NameStringPtr, cnt->Config.BaseAddr,
           cnt->Config.CounterType, cnt->Config.NumCounters, cnt->Config.Width);
#endif

    if (intf->counter.init(&blocks[n], &snp4_user->env, &tcfg->CounterListPtr[n]->Config) != XIL_VITIS_NET_P4_SUCCESS) {
      goto out_free_blocks;
    }
  }

  snp4_user->counter_blocks = blocks;
  return true;

 out_free_blocks:
  for (unsigned int b = 0; b < n; ++b) {
      intf->counter.exit(&blocks[b]);
  }
  free(blocks);
  return false;
}

static void snp4_deinit_counter_blocks(struct snp4_user_context * snp4_user)
{
  const struct vitis_net_p4_drv_intf * intf = snp4_user->intf;
  const XilVitisNetP4TargetConfig * tcfg = intf->target.config;

  for (unsigned int n = 0; n < tcfg->CounterListSize; ++n) {
    intf->counter.exit(&snp4_user->counter_blocks[n]);
  }

  free(snp4_user->counter_blocks);
  snp4_user->counter_blocks = NULL;
}

void * snp4_init(unsigned int sdnet_idx, uintptr_t snp4_base_addr)
{
  struct snp4_user_context * snp4_user;
  snp4_user = (struct snp4_user_context *) calloc(1, sizeof(struct snp4_user_context));
  if (snp4_user == NULL) {
    goto out_fail;
  }

  snp4_user->intf = vitis_net_p4_drv_intf_get(sdnet_idx);
  if (snp4_user->intf == NULL) {
    goto out_fail_user;
  }
  snp4_user->sdnet_idx = sdnet_idx;
  snp4_user->base_addr = snp4_base_addr + snp4_user->intf->info.offset;

  // Initialize the vitisnetp4 env
  if (snp4_user->intf->common.stub_env_if(&snp4_user->env) != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_user;
  }
  snp4_user->env.WordWrite32 = (XilVitisNetP4WordWrite32Fp) &device_write;
  snp4_user->env.WordRead32  = (XilVitisNetP4WordRead32Fp)  &device_read;
  snp4_user->env.UserCtx     = (XilVitisNetP4UserCtxType)   snp4_user;
#ifdef SDNETCONFIG_DEBUG
  snp4_user->env.LogError    = (XilVitisNetP4LogFp)         &log_error;
  snp4_user->env.LogInfo     = (XilVitisNetP4LogFp)         &log_info;
#endif

  // Initialize the vitisnetp4 target
  if (snp4_user->intf->target.init(&snp4_user->target, &snp4_user->env, snp4_user->intf->target.config) != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_user;
  }

  if (!snp4_init_counter_blocks(snp4_user)) {
    goto out_fail_counters;
  }

  return (void *) snp4_user;

 out_fail_counters:
  snp4_user->intf->target.exit(&snp4_user->target);
 out_fail_user:
  free(snp4_user);
 out_fail:
  return NULL;
}

bool snp4_deinit(void * snp4_handle)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;

  snp4_deinit_counter_blocks(snp4_user);
  if (snp4_user->intf->target.exit(&snp4_user->target) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }
  free(snp4_user);

  return true;
}

bool snp4_reset_all_tables(void * snp4_handle)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;

  // Look up the number of tables in the design
  uint32_t num_tables;
  if (snp4_user->intf->target.get_table_count(&snp4_user->target, &num_tables) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  // Reset all of the tables in the design
  for (uint32_t i = 0; i < num_tables; i++) {
    XilVitisNetP4TableCtx * table;
    if (snp4_user->intf->target.get_table_by_index(&snp4_user->target, i, &table) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
    if (snp4_user->intf->table.reset(table) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }

  return true;
}

bool snp4_reset_one_table(void * snp4_handle, const char * table_name)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;

  XilVitisNetP4TableCtx * table;
  if (snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  if (snp4_user->intf->table.reset(table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  return true;
}

bool snp4_table_insert_kma(void * snp4_handle,
			   const char * table_name,
			   uint8_t * key,
			   size_t UNUSED(key_len),
			   uint8_t * mask,
			   size_t UNUSED(mask_len),
			   const char * action_name,
			   uint8_t * params,
			   size_t UNUSED(params_len),
			   uint32_t priority,
			   bool replace)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;

  // Get a handle for the target table
  XilVitisNetP4TableCtx * table;
  if (snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  // Convert the action name to an id
  uint32_t action_id;
  if (snp4_user->intf->table.get_action_id(table, (char *)action_name, &action_id) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  XilVitisNetP4TableMode table_mode;
  if (snp4_user->intf->table.get_mode(table, &table_mode) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  // Certain table modes insist on a NULL mask parameter
  switch (table_mode) {
  case XIL_VITIS_NET_P4_TABLE_MODE_DCAM:
  case XIL_VITIS_NET_P4_TABLE_MODE_BCAM:
  case XIL_VITIS_NET_P4_TABLE_MODE_TINY_BCAM:
    // Mask parameter must be NULL for these table modes
    mask = NULL;
    break;
  default:
    // All other table modes require the mask
    break;
  }

  if (replace) {
    /* Replace an existing entry */
    if (snp4_user->intf->table.update(table, key, mask, action_id, params) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  } else {
    /* Insert an entirely new entry */
    if (snp4_user->intf->table.insert(table, key, mask, priority, action_id, params) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }

  return true;
}

bool snp4_table_delete_k(void * snp4_handle,
			 const char * table_name,
			 uint8_t * key,
			 size_t    UNUSED(key_len),
			 uint8_t * mask,
			 size_t    UNUSED(mask_len))
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;

  // Get a handle for the target table
  XilVitisNetP4TableCtx * table;
  if (snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  XilVitisNetP4TableMode table_mode;
  if (snp4_user->intf->table.get_mode(table, &table_mode) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  // Certain table modes insist on a NULL mask parameter
  switch (table_mode) {
  case XIL_VITIS_NET_P4_TABLE_MODE_DCAM:
  case XIL_VITIS_NET_P4_TABLE_MODE_BCAM:
  case XIL_VITIS_NET_P4_TABLE_MODE_TINY_BCAM:
    // Mask parameter must be NULL for these table modes
    mask = NULL;
    break;
  default:
    // All other table modes require the mask
    break;
  }

  if (snp4_user->intf->table.delete(table, key, mask) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  return true;
}

static XilVitisNetP4CounterCtx * snp4_counter_block_by_name(struct snp4_user_context * snp4_user, const char * block_name)
{
  const XilVitisNetP4TargetConfig * tcfg = snp4_user->intf->target.config;
  for (unsigned int n = 0; n < tcfg->CounterListSize; ++n) {
    if (strcmp(block_name, tcfg->CounterListPtr[n]->NameStringPtr) == 0) {
      return &snp4_user->counter_blocks[n];
    }
  }
  return NULL;
}

bool snp4_counter_simple_read(void * snp4_handle, const char * block_name, uint32_t index, uint64_t * value)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->Config.NumCounters) {
    return false;
  }

  return snp4_user->intf->counter.simple_read(block, index, value) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_simple_write(void * snp4_handle, const char * block_name, uint32_t index, uint64_t value)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->Config.NumCounters) {
    return false;
  }

  return snp4_user->intf->counter.simple_write(block, index, value) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_combo_read(void * snp4_handle, const char * block_name, uint32_t index, uint64_t * packets, uint64_t * bytes)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->Config.NumCounters) {
    return false;
  }

  return snp4_user->intf->counter.combo_read(block, index, packets, bytes) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_combo_write(void * snp4_handle, const char * block_name, uint32_t index, uint64_t packets, uint64_t bytes)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->Config.NumCounters) {
    return false;
  }

  return snp4_user->intf->counter.combo_write(block, index, packets, bytes) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_simple_read(void * snp4_handle, const char * block_name, uint64_t * counts, size_t ncounts)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  if (ncounts > block->Config.NumCounters) {
    memset(&counts[block->Config.NumCounters], 0, sizeof(counts[0]) * (ncounts - block->Config.NumCounters));
    ncounts = block->Config.NumCounters;
  }

  return snp4_user->intf->counter.collect_simple_read(block, 0, ncounts, counts) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_combo_read(void * snp4_handle, const char * block_name, uint64_t * packets, uint64_t * bytes, size_t ncounts)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  if (ncounts > block->Config.NumCounters) {
    memset(&packets[block->Config.NumCounters], 0, sizeof(packets[0]) * (ncounts - block->Config.NumCounters));
    memset(&bytes[block->Config.NumCounters], 0, sizeof(bytes[0]) * (ncounts - block->Config.NumCounters));
    ncounts = block->Config.NumCounters;
  }

  return snp4_user->intf->counter.collect_combo_read(block, 0, ncounts, packets, bytes) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_reset(void * snp4_handle, const char * block_name)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4CounterCtx * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  return snp4_user->intf->counter.reset(block) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_reset_all(void * snp4_handle)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  const XilVitisNetP4TargetConfig * tcfg = snp4_user->intf->target.config;
  for (unsigned int n = 0; n < tcfg->CounterListSize; ++n) {
    if (snp4_user->intf->counter.reset(&snp4_user->counter_blocks[n]) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }
  return true;
}
