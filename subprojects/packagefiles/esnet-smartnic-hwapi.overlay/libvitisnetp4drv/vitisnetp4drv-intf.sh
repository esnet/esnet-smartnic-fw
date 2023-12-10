#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
ip_name="$1"; shift
intf_c="$1"; shift
intf_h="$1"; shift
intf_map="$1"; shift

#-------------------------------------------------------------------------------
intf_struct='vitis_net_p4_drv_intf'
intf_name="${intf_struct}_${ip_name}"

intf_h_base=$(basename "${intf_h}")
intf_def=${intf_h_base^^} # Convert to upper case.
intf_def=${intf_def//[-.]/_} # Replace selected chars with '_'.

#-------------------------------------------------------------------------------
cat <<EOF >"${intf_c}"
#include "${intf_h_base}"

#include "${ip_name}_defs.h"
#include "vitisnetp4_common.h"
#include "vitisnetp4_table.h"
#include "vitisnetp4_target.h"

/*----------------------------------------------------------------------------*/
static const struct ${intf_struct} __${intf_name} = {
    .common = {
        .stub_env_if = XilVitisNetP4StubEnvIf,
    },

    .table = {
        .reset = XilVitisNetP4TableReset,
        .update = XilVitisNetP4TableUpdate,
        .delete = XilVitisNetP4TableDelete,
        .insert = XilVitisNetP4TableInsert,
        .get_action_id = XilVitisNetP4TableGetActionId,
        .get_mode = XilVitisNetP4TableGetMode,
    },

    .target = {
        .config = &XilVitisNetP4TargetConfig_${ip_name},
        .init = XilVitisNetP4TargetInit,
        .exit = XilVitisNetP4TargetExit,
        .get_table_by_index = XilVitisNetP4TargetGetTableByIndex,
        .get_table_by_name = XilVitisNetP4TargetGetTableByName,
        .get_table_count = XilVitisNetP4TargetGetTableCount,
    },
};

/*----------------------------------------------------------------------------*/
const struct ${intf_struct}* ${intf_name}(void)
{
    return &__${intf_name};
}
EOF

#-------------------------------------------------------------------------------
cat <<EOF >"${intf_h}"
#ifndef ${intf_def}
#define ${intf_def}

#include "vitisnetp4drv-intf.h"

#ifdef __cplusplus
extern "C" {
#endif

const struct ${intf_struct}* ${intf_name}(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ${intf_def} */
EOF

#-------------------------------------------------------------------------------
cat <<EOF >"${intf_map}"
{
  /* Symbols to be exported. */
  global:
    ${intf_name};

  /* Default all symbols to hidden. */
  local:
    *;
};
EOF
