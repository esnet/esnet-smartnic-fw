//
// Copyright (c) 2021, The Regents of the University of California,
// through Lawrence Berkeley National Laboratory (subject to receipt of
// any required approvals from the U.S. Dept. of Energy).  All rights
// reserved.
//

#include <stdio.h>		/* fprintf */
#include "sdnetapi.h"		/* API */
#include "sdnet_0_defs.h"	/* XilVitisNetP4TargetConfig_sdnet_0 */
#include "sdnetio.h"		/* sdnetio_reg_* */
#include "unused.h"		/* UNUSED() */

struct sdnet_user_context {
  uintptr_t sdnet_base_addr;
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
    struct sdnet_user_context * user_ctx;
    user_ctx = (struct sdnet_user_context *)EnvIfPtr->UserCtx;

    // Do the operation
    sdnetio_reg_write(user_ctx->sdnet_base_addr, address, data);

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
    struct sdnet_user_context * user_ctx;
    user_ctx = (struct sdnet_user_context *)EnvIfPtr->UserCtx;

    // Do the operation
    sdnetio_reg_read(user_ctx->sdnet_base_addr, address, dataPtr);

    return XIL_VITIS_NET_P4_SUCCESS;
}

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

void * sdnet_init(uintptr_t sdnet_base_addr)
{
  struct sdnet_user_context * sdnet_user;
  sdnet_user = (struct sdnet_user_context *) calloc(1, sizeof(struct sdnet_user_context));
  if (sdnet_user == NULL) {
    goto out_fail;
  }
  sdnet_user->sdnet_base_addr = sdnet_base_addr;

  XilVitisNetP4EnvIf * sdnet_env;
  sdnet_env = (XilVitisNetP4EnvIf *) calloc(1, sizeof(XilVitisNetP4EnvIf));
  if (sdnet_env == NULL) {
    goto out_fail_user;
  }

  XilVitisNetP4TargetCtx * sdnet_target;
  sdnet_target = (XilVitisNetP4TargetCtx *) calloc(1, sizeof(XilVitisNetP4EnvIf));
  if (sdnet_target == NULL) {
    goto out_fail_env;
  }

  // Initialize the sdnet env
  if (XilVitisNetP4StubEnvIf(sdnet_env) != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_target;
  }
  sdnet_env->WordWrite32 = (XilVitisNetP4WordWrite32Fp) &device_write;
  sdnet_env->WordRead32  = (XilVitisNetP4WordRead32Fp)  &device_read;
  sdnet_env->UserCtx     = (XilVitisNetP4UserCtxType)   sdnet_user;
  sdnet_env->LogError    = (XilVitisNetP4LogFp)         &log_error;
  sdnet_env->LogInfo     = (XilVitisNetP4LogFp)         &log_info;

  // Initialize the sdnet target
  if (XilVitisNetP4TargetInit(sdnet_target, sdnet_env, &XilVitisNetP4TargetConfig_sdnet_0) != XIL_VITIS_NET_P4_SUCCESS) {
    goto out_fail_target;
  }

  return (void *) sdnet_target;

 out_fail_target:
  free(sdnet_target);
 out_fail_env:
  free(sdnet_env);
 out_fail_user:
  free(sdnet_user);
 out_fail:
  return NULL;
}

bool sdnet_deinit(void * sdnet_handle)
{
  XilVitisNetP4TargetCtx * sdnet_target = (XilVitisNetP4TargetCtx *) sdnet_handle;

  if (XilVitisNetP4TargetExit(sdnet_target) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  return true;
}

bool sdnet_reset_all_tables(void * sdnet_handle)
{
  XilVitisNetP4TargetCtx * sdnet_target = (XilVitisNetP4TargetCtx *) sdnet_handle;

  // Look up the number of tables in the design
  uint32_t num_tables;
  if (XilVitisNetP4TargetGetTableCount(sdnet_target, &num_tables) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  // Reset all of the tables in the design
  for (uint32_t i = 0; i < num_tables; i++) {
    XilVitisNetP4TableCtx * table;
    if (XilVitisNetP4TargetGetTableByIndex(sdnet_target, i, &table) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
    if (XilVitisNetP4TableReset(table) != XIL_VITIS_NET_P4_SUCCESS) {
      return false;
    }
  }

  return true;
}

bool sdnet_table_insert_kma(void * sdnet_handle,
			    const char * table_name,
			    uint8_t * key,
			    size_t UNUSED(key_len),
			    uint8_t * mask,
			    size_t UNUSED(mask_len),
			    const char * action_name,
			    uint8_t * params,
			    size_t UNUSED(params_len),
			    uint32_t priority)
{
  XilVitisNetP4TargetCtx * sdnet_target = (XilVitisNetP4TargetCtx *) sdnet_handle;

  // Get a handle for the target table
  XilVitisNetP4TableCtx * table;
  if (XilVitisNetP4TargetGetTableByName(sdnet_target, (char *)table_name, &table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  // Convert the action name to an id
  uint32_t action_id;
  if (XilVitisNetP4TableGetActionId(table, (char *)action_name, &action_id) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  XilVitisNetP4TableMode table_mode;
  if (XilVitisNetP4TableGetMode(table, &table_mode) != XIL_VITIS_NET_P4_SUCCESS) {
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

  if (XilVitisNetP4TableInsert(table, key, mask, priority, action_id, params) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  return true;
}

bool sdnet_table_delete_k(void * sdnet_handle,
			  const char * table_name,
			  uint8_t * key,
			  size_t    UNUSED(key_len),
			  uint8_t * mask,
			  size_t    UNUSED(mask_len))
{
  XilVitisNetP4TargetCtx * sdnet_target = (XilVitisNetP4TargetCtx *) sdnet_handle;

  // Get a handle for the target table
  XilVitisNetP4TableCtx * table;
  if (XilVitisNetP4TargetGetTableByName(sdnet_target, (char *)table_name, &table) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  XilVitisNetP4TableMode table_mode;
  if (XilVitisNetP4TableGetMode(table, &table_mode) != XIL_VITIS_NET_P4_SUCCESS) {
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

  if (XilVitisNetP4TableDelete(table, key, mask) != XIL_VITIS_NET_P4_SUCCESS) {
    return false;
  }

  return true;
}
