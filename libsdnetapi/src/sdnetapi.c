//
// Copyright (c) 2021, The Regents of the University of California,
// through Lawrence Berkeley National Laboratory (subject to receipt of
// any required approvals from the U.S. Dept. of Energy).  All rights
// reserved.
//

#include <stdio.h>		/* fprintf */
#include "sdnetapi.h"		/* API */
#include "sdnet_0_defs.h"	/* XilSdnetTargetConfig_sdnet_0 */
#include "sdnetio.h"		/* sdnetio_reg_* */
#include "unused.h"		/* UNUSED() */

struct sdnet_user_context {
  uintptr_t sdnet_base_addr;
};

static XilSdnetReturnType device_write(XilSdnetEnvIf *EnvIfPtr, XilSdnetAddressType address, uint32_t data) {
    // Validate inputs
    if (NULL == EnvIfPtr) {
        return XIL_SDNET_GENERAL_ERR_NULL_PARAM;
    }
    if (NULL == EnvIfPtr->UserCtx)	{
        return XIL_SDNET_GENERAL_ERR_INTERNAL_ASSERTION;
    }

    // Recover virtual base address from user context
    struct sdnet_user_context * user_ctx;
    user_ctx = (struct sdnet_user_context *)EnvIfPtr->UserCtx;

    // Do the operation
    sdnetio_reg_write(user_ctx->sdnet_base_addr, address, data);

    return XIL_SDNET_SUCCESS;
}

static XilSdnetReturnType device_read(XilSdnetEnvIf *EnvIfPtr, XilSdnetAddressType address, uint32_t *dataPtr) {
    // Validate inputs
    if (EnvIfPtr == NULL) {
        return XIL_SDNET_GENERAL_ERR_NULL_PARAM;
    }
    if (EnvIfPtr->UserCtx == NULL) {
        return XIL_SDNET_GENERAL_ERR_INTERNAL_ASSERTION;
    }
    if (dataPtr == NULL) {
        return XIL_SDNET_GENERAL_ERR_INTERNAL_ASSERTION;
    }

    // Recover virtual base address from user context
    struct sdnet_user_context * user_ctx;
    user_ctx = (struct sdnet_user_context *)EnvIfPtr->UserCtx;

    // Do the operation
    sdnetio_reg_read(user_ctx->sdnet_base_addr, address, dataPtr);

    return XIL_SDNET_SUCCESS;
}

static XilSdnetReturnType log_info(XilSdnetEnvIf *UNUSED(EnvIfPtr), const char *MessagePtr)
{
  fprintf(stdout, "%s\n", MessagePtr);

  return XIL_SDNET_SUCCESS;
}

static XilSdnetReturnType log_error(XilSdnetEnvIf *UNUSED(EnvIfPtr), const char *MessagePtr)
{
  fprintf(stderr, "%s\n", MessagePtr);

  return XIL_SDNET_SUCCESS;
}

void * sdnet_init(uintptr_t sdnet_base_addr)
{
  struct sdnet_user_context * sdnet_user;
  sdnet_user = (struct sdnet_user_context *) calloc(1, sizeof(struct sdnet_user_context));
  if (sdnet_user == NULL) {
    goto out_fail;
  }
  sdnet_user->sdnet_base_addr = sdnet_base_addr;

  XilSdnetEnvIf * sdnet_env;
  sdnet_env = (XilSdnetEnvIf *) calloc(1, sizeof(XilSdnetEnvIf));
  if (sdnet_env == NULL) {
    goto out_fail_user;
  }

  XilSdnetTargetCtx * sdnet_target;
  sdnet_target = (XilSdnetTargetCtx *) calloc(1, sizeof(XilSdnetEnvIf));
  if (sdnet_target == NULL) {
    goto out_fail_env;
  }

  // Initialize the sdnet env
  if (XilSdnetStubEnvIf(sdnet_env) != XIL_SDNET_SUCCESS) {
    goto out_fail_target;
  }
  sdnet_env->WordWrite32 = (XilSdnetWordWrite32Fp) &device_write;
  sdnet_env->WordRead32  = (XilSdnetWordRead32Fp)  &device_read;
  sdnet_env->UserCtx     = (XilSdnetUserCtxType)   sdnet_user;
  sdnet_env->LogError    = (XilSdnetLogFp)         &log_error;
  sdnet_env->LogInfo     = (XilSdnetLogFp)         &log_info;

  // Initialize the sdnet target
  if (XilSdnetTargetInit(sdnet_target, sdnet_env, &XilSdnetTargetConfig_sdnet_0) != XIL_SDNET_SUCCESS) {
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
  XilSdnetTargetCtx * sdnet_target = (XilSdnetTargetCtx *) sdnet_handle;

  if (XilSdnetTargetExit(sdnet_target) != XIL_SDNET_SUCCESS) {
    return false;
  }

  return true;
}

bool sdnet_reset_all_tables(void * sdnet_handle)
{
  XilSdnetTargetCtx * sdnet_target = (XilSdnetTargetCtx *) sdnet_handle;

  // Look up the number of tables in the design
  uint32_t num_tables;
  if (XilSdnetTargetGetTableCount(sdnet_target, &num_tables) != XIL_SDNET_SUCCESS) {
    return false;
  }

  // Reset all of the tables in the design
  for (uint32_t i = 0; i < num_tables; i++) {
    XilSdnetTableCtx * table;
    if (XilSdnetTargetGetTableByIndex(sdnet_target, i, &table) != XIL_SDNET_SUCCESS) {
      return false;
    }
    if (XilSdnetTableReset(table) != XIL_SDNET_SUCCESS) {
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
  XilSdnetTargetCtx * sdnet_target = (XilSdnetTargetCtx *) sdnet_handle;

  // Get a handle for the target table
  XilSdnetTableCtx * table;
  if (XilSdnetTargetGetTableByName(sdnet_target, (char *)table_name, &table) != XIL_SDNET_SUCCESS) {
    return false;
  }

  // Convert the action name to an id
  uint32_t action_id;
  if (XilSdnetTableGetActionId(table, (char *)action_name, &action_id) != XIL_SDNET_SUCCESS) {
    return false;
  }

  if (XilSdnetTableInsert(table, key, mask, priority, action_id, params) != XIL_SDNET_SUCCESS) {
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
  XilSdnetTargetCtx * sdnet_target = (XilSdnetTargetCtx *) sdnet_handle;

  // Get a handle for the target table
  XilSdnetTableCtx * table;
  if (XilSdnetTargetGetTableByName(sdnet_target, (char *)table_name, &table) != XIL_SDNET_SUCCESS) {
    return false;
  }

  if (XilSdnetTableDelete(table, key, mask) != XIL_SDNET_SUCCESS) {
    return false;
  }

  return true;
}
