#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
intf_c="$1"; shift
ip_names=( "$@" )

#-------------------------------------------------------------------------------
cat <<EOF >"${intf_c}"
#include "vitisnetp4drv-intf.h"

EOF

for ip_name in "${ip_names[@]}"; do
    cat <<EOF >>"${intf_c}"
#include "vitisnetp4drv-intf-${ip_name}.h"
EOF
done

cat <<EOF >>"${intf_c}"

#define N_ENTRIES (sizeof(__table) / sizeof(__table[0]))

/*----------------------------------------------------------------------------*/
static const struct vitis_net_p4_drv_intf* const __table[] = {
EOF

for ip_name in "${ip_names[@]}"; do
    cat <<EOF >>"${intf_c}"
    &vitis_net_p4_drv_intf_${ip_name},
EOF
done

cat <<EOF >>"${intf_c}"
};

/*----------------------------------------------------------------------------*/
size_t vitis_net_p4_drv_intf_count(void)
{
    return N_ENTRIES;
}

/*----------------------------------------------------------------------------*/
const struct vitis_net_p4_drv_intf* vitis_net_p4_drv_intf_get(unsigned int idx)
{
    if (idx < N_ENTRIES)
        return __table[idx];
    return NULL;
}
EOF
