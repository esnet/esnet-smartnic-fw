#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
priv_dir="$1"; shift
config_h="$1"; shift
cc_cmd="$@"

#-------------------------------------------------------------------------------
test_c="${priv_dir}/test_with_v2_counters.c"
cat <<_EOF >"${test_c}"
#include <stddef.h>
#include "p4_proc_decoder.h"
size_t offset(void) {
    return offsetof(struct p4_proc_decoder, drops_from_p4);
}
_EOF

with_v2_switch_counters=1
eval "${cc_cmd} -o ${test_c}.o -c ${test_c}" || with_v2_switch_counters=0

#-------------------------------------------------------------------------------
cat <<_EOF >"${config_h}"
#ifndef INCLUDE_REMGAP_CONFIG
#define INCLUDE_REMGAP_CONFIG

#define REGMAP_CONFIG_WITH_V2_SWITCH_COUNTERS ${with_v2_switch_counters}

#endif // INCLUDE_REMGAP_CONFIG
_EOF
