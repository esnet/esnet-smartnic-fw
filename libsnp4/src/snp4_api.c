//
// Copyright (c) 2021, The Regents of the University of California,
// through Lawrence Berkeley National Laboratory (subject to receipt of
// any required approvals from the U.S. Dept. of Energy).  All rights
// reserved.
//

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc, free */
#include <string.h>		/* memset, strcmp */
#include <gmp.h>		/* mpz_* */
#include "snp4.h"		/* API */
#include "snp4_io.h"		/* snp4_io_reg_* */

#include "vitisnetp4drv-intf.h"	/* Vitis driver wrapper */

struct snp4_counter_block {
  XilVitisNetP4CounterCtx ctx;
  size_t num_counters;
};

struct snp4_register_block {
  XilVitisNetP4RegisterTopCtx ctx;
  size_t num_registers;
  size_t words_per_reg;
  const uint32_t * initial_data;
};

struct snp4_user_context {
  struct {
    bool enabled;
    const char * prefix;
  } log;

  uintptr_t base_addr;
  XilVitisNetP4EnvIf env;
  XilVitisNetP4TargetCtx target;
  struct snp4_counter_block * counter_blocks;
  struct snp4_register_block * register_blocks;
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

static XilVitisNetP4ReturnType log_info(XilVitisNetP4EnvIf *EnvIfPtr, const char *MessagePtr)
{
  struct snp4_user_context * user_ctx = (struct snp4_user_context *)EnvIfPtr->UserCtx;
  if (user_ctx->log.enabled) {
    fprintf(stdout, "%s%s\n", user_ctx->log.prefix, MessagePtr);
    fflush(stdout);
  }

  return XIL_VITIS_NET_P4_SUCCESS;
}

static XilVitisNetP4ReturnType log_error(XilVitisNetP4EnvIf *EnvIfPtr, const char *MessagePtr)
{
  struct snp4_user_context * user_ctx = (struct snp4_user_context *)EnvIfPtr->UserCtx;
  if (user_ctx->log.enabled) {
    fprintf(stderr, "%s%s\n", user_ctx->log.prefix, MessagePtr);
    fflush(stderr);
  }

  return XIL_VITIS_NET_P4_SUCCESS;
}

size_t snp4_sdnet_count(void)
{
  return vitis_net_p4_drv_intf_count();
}

bool snp4_sdnet_present(unsigned int sdnet_idx)
{
  return vitis_net_p4_drv_intf_get(sdnet_idx) != NULL;
}

static XilVitisNetP4ReturnType snp4_init_counter_blocks(struct snp4_user_context * snp4_user)
{
  const struct vitis_net_p4_drv_intf * intf = snp4_user->intf;
  const XilVitisNetP4TargetConfig * tcfg = intf->target.config;

  if (tcfg->CounterListSize == 0) {
    return XIL_VITIS_NET_P4_SUCCESS;
  }

  struct snp4_counter_block * blocks = calloc(tcfg->CounterListSize, sizeof(*blocks));
  if (blocks == NULL) {
    return XIL_VITIS_NET_P4_TARGET_ERR_MALLOC_FAILED;
  }

#ifdef SDNETCONFIG_DEBUG
  printf("DEBUG[%s]: CounterListSize=%u\n", __func__, tcfg->CounterListSize);
#endif

  XilVitisNetP4ReturnType rt = XIL_VITIS_NET_P4_SUCCESS;
  unsigned int n;
  for (n = 0; n < tcfg->CounterListSize; ++n) {
    XilVitisNetP4TargetCounterConfig * cnt = tcfg->CounterListPtr[n];
    const XilVitisNetP4CounterConfig * cfg = &cnt->Config;
    struct snp4_counter_block * block = &blocks[n];

#ifdef SDNETCONFIG_DEBUG
    printf("    DEBUG[%s]: idx=%u, name='%s', base=0x%016lx, counter_type=%u, num_counters=%u, "
           "width=%u\n", __func__, n, cnt->NameStringPtr, cfg->BaseAddr, cfg->CounterType,
           cfg->NumCounters, cfg->Width);
#endif

    block->num_counters = cfg->NumCounters;
    rt = intf->counter.init(&block->ctx, &snp4_user->env, &cnt->Config);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      goto out_free_blocks;
    }
  }

  snp4_user->counter_blocks = blocks;
  return rt;

 out_free_blocks:
  for (unsigned int b = 0; b < n; ++b) {
      intf->counter.exit(&blocks[b].ctx);
  }
  free(blocks);
  return rt;
}

static void snp4_deinit_counter_blocks(struct snp4_user_context * snp4_user)
{
  const struct vitis_net_p4_drv_intf * intf = snp4_user->intf;
  const XilVitisNetP4TargetConfig * tcfg = intf->target.config;

  for (unsigned int n = 0; n < tcfg->CounterListSize; ++n) {
    intf->counter.exit(&snp4_user->counter_blocks[n].ctx);
  }

  free(snp4_user->counter_blocks);
  snp4_user->counter_blocks = NULL;
}

static XilVitisNetP4ReturnType snp4_init_register_blocks(struct snp4_user_context * snp4_user)
{
  const struct vitis_net_p4_drv_intf * intf = snp4_user->intf;
  const XilVitisNetP4TargetConfig * tcfg = intf->target.config;

  if (tcfg->RegisterListSize == 0) {
    return XIL_VITIS_NET_P4_SUCCESS;
  }

  struct snp4_register_block * blocks = calloc(tcfg->RegisterListSize, sizeof(*blocks));
  if (blocks == NULL) {
    return XIL_VITIS_NET_P4_TARGET_ERR_MALLOC_FAILED;
  }

#ifdef SDNETCONFIG_DEBUG
  printf("DEBUG[%s]: RegisterListSize=%u\n", __func__, tcfg->RegisterListSize);
#endif

  XilVitisNetP4ReturnType rt = XIL_VITIS_NET_P4_SUCCESS;
  unsigned int n;
  for (n = 0; n < tcfg->RegisterListSize; ++n) {
    XilVitisNetP4TargetRegisterConfig * reg = tcfg->RegisterListPtr[n];
    const XilVitisNetP4RegisterTopConfig * cfg = &reg->Config;
    struct snp4_register_block * block = &blocks[n];

#ifdef SDNETCONFIG_DEBUG
    printf("    DEBUG[%s]: idx=%u, name='%s', base=0x%016lx, version=%u, table_id=%u, "
           "largest_index=%u, data_size=%u, is_dram=%s\n", __func__, n, reg->NameStringPtr,
           cfg->BaseAddr, cfg->version, cfg->table_id, cfg->largest_index, cfg->data_size,
           cfg->dram ? "yes" : "no");
#endif

    block->num_registers = cfg->largest_index + 1;
    block->words_per_reg = (cfg->data_size + 32 - 1) / 32;
    block->initial_data = cfg->InitialData;
    rt = intf->registers.init(&block->ctx, &snp4_user->env, &reg->Config);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      goto out_free_blocks;
    }
  }

  snp4_user->register_blocks = blocks;
  return rt;

 out_free_blocks:
  for (unsigned int b = 0; b < n; ++b) {
      intf->registers.exit(&blocks[b].ctx);
  }
  free(blocks);
  return rt;
}

static void snp4_deinit_register_blocks(struct snp4_user_context * snp4_user)
{
  const struct vitis_net_p4_drv_intf * intf = snp4_user->intf;
  const XilVitisNetP4TargetConfig * tcfg = intf->target.config;

  for (unsigned int n = 0; n < tcfg->RegisterListSize; ++n) {
    intf->registers.exit(&snp4_user->register_blocks[n].ctx);
  }

  free(snp4_user->register_blocks);
  snp4_user->register_blocks = NULL;
}

void * snp4_init(unsigned int sdnet_idx, uintptr_t snp4_base_addr, const char ** error_str)
{
  struct snp4_user_context * snp4_user;
  XilVitisNetP4ReturnType rt;

  snp4_user = (struct snp4_user_context *) calloc(1, sizeof(struct snp4_user_context));
  if (snp4_user == NULL) {
    rt = XIL_VITIS_NET_P4_TARGET_ERR_MALLOC_FAILED;
    goto out_fail;
  }

  snp4_user->intf = vitis_net_p4_drv_intf_get(sdnet_idx);
  if (snp4_user->intf == NULL) {
    rt = XIL_VITIS_NET_P4_GENERAL_ERR_INVALID_CONTEXT;
    goto out_fail_user;
  }
  snp4_user->sdnet_idx = sdnet_idx;
  snp4_user->base_addr = snp4_base_addr + snp4_user->intf->info.offset;

  // Initialize the vitisnetp4 env
  rt = snp4_user->intf->common.stub_env_if(&snp4_user->env);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_user;
  }
  snp4_user->env.WordWrite32 = (XilVitisNetP4WordWrite32Fp) &device_write;
  snp4_user->env.WordRead32  = (XilVitisNetP4WordRead32Fp)  &device_read;
  snp4_user->env.UserCtx     = (XilVitisNetP4UserCtxType)   snp4_user;
  snp4_user->env.LogError    = (XilVitisNetP4LogFp)         &log_error;
  snp4_user->env.LogInfo     = (XilVitisNetP4LogFp)         &log_info;

  snp4_user->intf->cam.set_debug_flags(
    CAM_DEBUG_CONFIG |
    CAM_DEBUG_CONFIG_ARGS |
    CAM_DEBUG_VERBOSE_VERIFY
  );

  // Initialize the vitisnetp4 target
  snp4_log_enable(snp4_user, true, NULL);
  rt = snp4_user->intf->target.init(&snp4_user->target, &snp4_user->env, snp4_user->intf->target.config);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_user;
  }

  rt = snp4_init_counter_blocks(snp4_user);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_counters;
  }

  rt = snp4_init_register_blocks(snp4_user);
  if (rt == XIL_VITIS_NET_P4_SUCCESS) {
    snp4_log_enable(snp4_user, false, NULL);
    goto done;
  }

  snp4_deinit_counter_blocks(snp4_user);
 out_fail_counters:
  snp4_user->intf->target.exit(&snp4_user->target);
 out_fail_user:
  free(snp4_user);
 out_fail:
  snp4_user = NULL;
 done:
  if (error_str != NULL) {
    *error_str = snp4_user->intf->common.return_type_to_string(rt);
  }
  return (void *) snp4_user;
}

bool snp4_deinit(void * snp4_handle, const char ** error_str)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4ReturnType rt;

  snp4_deinit_register_blocks(snp4_user);
  snp4_deinit_counter_blocks(snp4_user);
  rt = snp4_user->intf->target.exit(&snp4_user->target);
  if (rt == XIL_VITIS_NET_P4_SUCCESS) {
    free(snp4_user);
  }

  if (error_str != NULL) {
    *error_str = snp4_user->intf->common.return_type_to_string(rt);
  }
  return rt == XIL_VITIS_NET_P4_SUCCESS;
}

void snp4_log_enable(void * snp4_handle, bool enable, const char * prefix)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  snp4_user->log.prefix = (enable && prefix != NULL) ? prefix : "";
  snp4_user->log.enabled = enable;
}

bool snp4_reset_all_tables(void * snp4_handle, const char ** error_str)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4ReturnType rt;

  // Look up the number of tables in the design
  uint32_t num_tables;
  rt = snp4_user->intf->target.get_table_count(&snp4_user->target, &num_tables);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto done;
  }

  // Reset all of the tables in the design
  for (uint32_t i = 0; i < num_tables; i++) {
    XilVitisNetP4TableCtx * table;
    rt = snp4_user->intf->target.get_table_by_index(&snp4_user->target, i, &table);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      goto done;
    }

    rt = snp4_user->intf->table.reset(table);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      goto done;
    }
  }

 done:
  if (error_str != NULL) {
    *error_str = snp4_user->intf->common.return_type_to_string(rt);
  }
  return rt == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_reset_one_table(void * snp4_handle, const char * table_name, const char ** error_str)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4ReturnType rt;

  XilVitisNetP4TableCtx * table;
  rt = snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table);
  if (rt == XIL_VITIS_NET_P4_SUCCESS) {
    rt = snp4_user->intf->table.reset(table);
  }

  if (error_str != NULL) {
    *error_str = snp4_user->intf->common.return_type_to_string(rt);
  }
  return rt == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_table_insert_kma(void * snp4_handle,
			   const char * table_name,
			   uint8_t * key,
			   uint8_t * mask,
			   const char * action_name,
			   uint8_t * params,
			   uint32_t priority,
			   bool replace,
                           const char ** error_str)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4ReturnType rt;

  // Get a handle for the target table
  XilVitisNetP4TableCtx * table;
  rt = snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto done;
  }

  // Convert the action name to an id
  uint32_t action_id;
  rt = snp4_user->intf->table.get_action_id(table, (char *)action_name, &action_id);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto done;
  }

  XilVitisNetP4TableMode table_mode;
  rt = snp4_user->intf->table.get_mode(table, &table_mode);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto done;
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
    rt = snp4_user->intf->table.update(table, key, mask, action_id, params);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      goto done;
    }
  } else {
    /* Insert an entirely new entry */
    rt = snp4_user->intf->table.insert(table, key, mask, priority, action_id, params);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      goto done;
    }
  }

 done:
  if (error_str != NULL) {
    *error_str = snp4_user->intf->common.return_type_to_string(rt);
  }
  return rt == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_table_delete_k(void * snp4_handle,
			 const char * table_name,
			 uint8_t * key,
			 uint8_t * mask,
                         const char ** error_str)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4ReturnType rt;

  // Get a handle for the target table
  XilVitisNetP4TableCtx * table;
  rt = snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto done;
  }

  XilVitisNetP4TableMode table_mode;
  rt = snp4_user->intf->table.get_mode(table, &table_mode);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    goto done;
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

  rt = snp4_user->intf->table.delete(table, key, mask);

 done:
  if (error_str != NULL) {
    *error_str = snp4_user->intf->common.return_type_to_string(rt);
  }
  return rt == XIL_VITIS_NET_P4_SUCCESS;
}

struct snp4_table_get_response {
  struct snp4_table_data key;
  struct {
    uint32_t id;
    struct snp4_table_data params;
  } action;
};

struct snp4_table_get_context {
  struct snp4_user_context * user;

  struct {
    const char * name;
    XilVitisNetP4TableCtx * handle;
    XilVitisNetP4TableMode mode;
  } table;

  bool (*callback)(struct snp4_table_get_context * ctx,
                   const struct snp4_table_get_response * resp);
  void * arg;
};

static bool _snp4_table_get_by_response(struct snp4_table_get_context * ctx)
{
  const struct vitis_net_p4_drv_intf * intf = ctx->user->intf;
  XilVitisNetP4ReturnType rt;

  rt = intf->target.get_table_by_name(
    &ctx->user->target, (char *)ctx->table.name, &ctx->table.handle);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  struct snp4_table_get_response resp = {0};
  rt = intf->table.get_key_size_bits(ctx->table.handle, &resp.key.width);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }
  resp.key.len = (resp.key.width + 8 - 1) / 8;

  uint8_t key[resp.key.len * 2];
  memset(key, 0, resp.key.len * 2);
  resp.key.value = key;
  resp.key.mask = &key[resp.key.len];

  rt = intf->table.get_action_params_size_bits(ctx->table.handle, &resp.action.params.width);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }
  resp.action.params.len = (resp.action.params.width + 8 - 1) / 8;

  uint8_t params[resp.action.params.len * 2];
  memset(params, 0, resp.action.params.len * 2);
  resp.action.params.value = params;
  resp.action.params.mask = &params[resp.action.params.len];

  // Certain table modes insist on a NULL mask parameter
  rt = intf->table.get_mode(ctx->table.handle, &ctx->table.mode);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  switch (ctx->table.mode) {
  case XIL_VITIS_NET_P4_TABLE_MODE_DCAM:
  case XIL_VITIS_NET_P4_TABLE_MODE_BCAM:
    // Mask parameter must be NULL for these table modes.
    resp.key.mask = NULL;
    break;

  default:
    // All other table modes require the mask.
    break;
  }

  uint32_t num_actions = 0;
  rt = intf->table.get_num_actions(ctx->table.handle, &num_actions);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  for (resp.action.id = 0; resp.action.id < num_actions; ++resp.action.id) {
    uint32_t position = 0;
    while (1) {
      rt = intf->table.get_by_response(
        ctx->table.handle, resp.action.id, resp.action.params.value, resp.action.params.mask,
        &position, resp.key.value, resp.key.mask);
      if (rt == XIL_VITIS_NET_P4_CAM_ERR_KEY_NOT_FOUND) {
        break;
      }

      if (rt != XIL_VITIS_NET_P4_SUCCESS) {
        return false;
      }

      if (!ctx->callback(ctx, &resp)) {
        return true;
      }
    }
  }

  return true;
}

struct snp4_table_for_each_context {
  bool (*callback)(const struct snp4_table_entry * entry, void * arg);
  void * arg;
};

static bool _snp4_table_for_each_entry(struct snp4_table_get_context * gctx,
                                       const struct snp4_table_get_response * resp)
{
  struct snp4_table_for_each_context * fctx = gctx->arg;
  struct snp4_table_entry entry = {
    .table_name = gctx->table.name,
    .key = {
      .width = resp->key.width,
      .len = resp->key.len,
    },
    .action = {
      .params = {
        .width = resp->action.params.width,
        .len = resp->action.params.len,
      },
    },
  };

  uint8_t key[resp->key.len * 2];
  entry.key.value = key;
  entry.key.mask = &key[resp->key.len];
  memcpy(entry.key.value, resp->key.value, resp->key.len);

  uint8_t params[resp->action.params.len * 2];
  entry.action.params.value = params;
  entry.action.params.mask = &params[resp->action.params.len];
  memset(entry.action.params.mask, 0xff, resp->action.params.len);

  uint8_t * mask = NULL;
  uint32_t * priority = NULL;
  switch (gctx->table.mode) {
  case XIL_VITIS_NET_P4_TABLE_MODE_DCAM:
  case XIL_VITIS_NET_P4_TABLE_MODE_BCAM:
    // Mask and priority parameters must be NULL for these table modes.
    memset(entry.key.mask, 0xff, resp->key.len); // Make sure the callback always has a key mask.
    break;

  default:
    // All other table modes require the mask and priority.
    memcpy(entry.key.mask, resp->key.mask, resp->key.len);
    mask = entry.key.mask;
    priority = &entry.priority;
    break;
  }

  uint32_t action_id;
  XilVitisNetP4ReturnType rt = gctx->user->intf->table.get_by_key(
    gctx->table.handle, entry.key.value, mask, priority,
    &action_id, entry.action.params.value);
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  char action_name[256] = {0};
  rt = gctx->user->intf->table.get_action_name(
    gctx->table.handle, action_id, action_name, sizeof(action_name));
  if (rt != XIL_VITIS_NET_P4_SUCCESS) {
    entry.action.name = "<buffer-too-small>";
  } else {
    entry.action.name = action_name;
  }

  return fctx->callback(&entry, fctx->arg);
}

bool snp4_table_for_each_entry(void * snp4_handle,
                               const char * table_name,
                               bool (*callback)(const struct snp4_table_entry * entry, void * arg),
                               void * arg)
{
  struct snp4_table_for_each_context fctx = {
    .callback = callback,
    .arg = arg,
  };
  struct snp4_table_get_context gctx = {
    .user = (struct snp4_user_context *)snp4_handle,
    .table = {
      .name = table_name,
    },
    .callback = _snp4_table_for_each_entry,
    .arg = &fctx,
  };
  return _snp4_table_get_by_response(&gctx);
}

bool snp4_table_ecc_counters_read(void * snp4_handle,
                                  const char * table_name,
                                  uint32_t * corrected_single_bit_errors,
                                  uint32_t * detected_double_bit_errors)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  XilVitisNetP4TableCtx * table;
  if (snp4_user->intf->target.get_table_by_name(&snp4_user->target, (char *)table_name, &table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  XilVitisNetP4ReturnType rt = snp4_user->intf->table.get_ecc_counters(table, corrected_single_bit_errors, detected_double_bit_errors);
  if (rt == XIL_VITIS_NET_P4_TABLE_ERR_FUNCTION_NOT_SUPPORTED) {
    /* Not all CAM IPs support ECC error counters. Rather than try to figure out which do, let the driver tell us and handle it. */
    *corrected_single_bit_errors = 0;
    *detected_double_bit_errors = 0;
    return true;
  }

  return rt == XIL_VITIS_NET_P4_SUCCESS;
}


static struct snp4_counter_block * snp4_counter_block_by_name(struct snp4_user_context * snp4_user, const char * block_name)
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
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->num_counters) {
    return false;
  }

  return snp4_user->intf->counter.simple_read(&block->ctx, index, value) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_simple_write(void * snp4_handle, const char * block_name, uint32_t index, uint64_t value)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->num_counters) {
    return false;
  }

  return snp4_user->intf->counter.simple_write(&block->ctx, index, value) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_combo_read(void * snp4_handle, const char * block_name, uint32_t index, uint64_t * packets, uint64_t * bytes)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->num_counters) {
    return false;
  }

  return snp4_user->intf->counter.combo_read(&block->ctx, index, packets, bytes) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_combo_write(void * snp4_handle, const char * block_name, uint32_t index, uint64_t packets, uint64_t bytes)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL || index >= block->num_counters) {
    return false;
  }

  return snp4_user->intf->counter.combo_write(&block->ctx, index, packets, bytes) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_simple_read(void * snp4_handle, const char * block_name, uint64_t * counts, size_t ncounts)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  if (ncounts > block->num_counters) {
    memset(&counts[block->num_counters], 0, sizeof(counts[0]) * (ncounts - block->num_counters));
    ncounts = block->num_counters;
  }

  return snp4_user->intf->counter.collect_simple_read(&block->ctx, 0, ncounts, counts) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_combo_read(void * snp4_handle, const char * block_name, uint64_t * packets, uint64_t * bytes, size_t ncounts)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  if (ncounts > block->num_counters) {
    memset(&packets[block->num_counters], 0, sizeof(packets[0]) * (ncounts - block->num_counters));
    memset(&bytes[block->num_counters], 0, sizeof(bytes[0]) * (ncounts - block->num_counters));
    ncounts = block->num_counters;
  }

  return snp4_user->intf->counter.collect_combo_read(&block->ctx, 0, ncounts, packets, bytes) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_reset(void * snp4_handle, const char * block_name)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_counter_block * block = snp4_counter_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  return snp4_user->intf->counter.reset(&block->ctx) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_counter_block_reset_all(void * snp4_handle)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  const XilVitisNetP4TargetConfig * tcfg = snp4_user->intf->target.config;
  for (unsigned int n = 0; n < tcfg->CounterListSize; ++n) {
    if (snp4_user->intf->counter.reset(&snp4_user->counter_blocks[n].ctx) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }
  return true;
}

static struct snp4_register_block * snp4_register_block_by_name(struct snp4_user_context * snp4_user, const char * block_name)
{
  const XilVitisNetP4TargetConfig * tcfg = snp4_user->intf->target.config;
  for (unsigned int n = 0; n < tcfg->RegisterListSize; ++n) {
    if (strcmp(block_name, tcfg->RegisterListPtr[n]->NameStringPtr) == 0) {
      return &snp4_user->register_blocks[n];
    }
  }
  return NULL;
}

bool snp4_register_block_reset(void * snp4_handle, const char * block_name)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_register_block * block = snp4_register_block_by_name(snp4_user, block_name);
  if (block == NULL) {
    return false;
  }

  return snp4_user->intf->registers.reset(&block->ctx) == XIL_VITIS_NET_P4_SUCCESS;
}

bool snp4_register_reset(void * snp4_handle, const char * block_name, unsigned int index, size_t count)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_register_block * block = snp4_register_block_by_name(snp4_user, block_name);
  if (block == NULL || index + count > block->num_registers) {
    return false;
  }

  uint32_t words[block->words_per_reg];
  for (unsigned int w = 0; w < block->words_per_reg; ++w) {
      words[w] = block->initial_data[w];
  }

  for (unsigned int n = 0; n < count; ++n, ++index) {
    XilVitisNetP4ReturnType rt = snp4_user->intf->registers.write(&block->ctx, index, (uint8_t*)words);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }

  return true;
}

bool snp4_register_read(void * snp4_handle, const char * block_name, unsigned int index, size_t count, mpz_t * data)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_register_block * block = snp4_register_block_by_name(snp4_user, block_name);
  if (block == NULL || index + count > block->num_registers) {
    return false;
  }

  uint32_t words[block->words_per_reg];
  for (unsigned int n = 0; n < count; ++n, ++index) {
    XilVitisNetP4ReturnType rt = snp4_user->intf->registers.read(&block->ctx, index, (uint8_t*)words);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
    mpz_import(data[n], block->words_per_reg, -1, sizeof(words[0]), 0, 0, words);
  }

  return true;
}

bool snp4_register_write(void * snp4_handle, const char * block_name, unsigned int index, size_t count, const mpz_t * data)
{
  struct snp4_user_context * snp4_user = (struct snp4_user_context *) snp4_handle;
  struct snp4_register_block * block = snp4_register_block_by_name(snp4_user, block_name);
  if (block == NULL || index + count > block->num_registers) {
    return false;
  }

  uint32_t words[block->words_per_reg];
  for (unsigned int n = 0; n < count; ++n, ++index) {
    memset(words, 0, sizeof(words[0])*block->words_per_reg);
    mpz_export(words, NULL, -1, sizeof(words[0]), 0, 0, data[n]);
    XilVitisNetP4ReturnType rt = snp4_user->intf->registers.write(&block->ctx, index, (uint8_t*)words);
    if (rt != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }

  return true;
}
