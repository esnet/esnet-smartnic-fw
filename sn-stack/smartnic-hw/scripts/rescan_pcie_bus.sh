#!/bin/bash

set -eo pipefail

function usage() {
    echo ""
    echo "Usage: $(basename $0) <pcie_device_addr>"
    echo "  pcie_device_addr: pcie domain:bus:device (e.g. 0000:81:00)"
    echo ""
}

# Make sure the caller has provided the required parameters
if [ "$#" -lt 1 ] ; then
    echo "ERROR: Missing required parameter(s)"
    usage
    exit 1
fi

# Grab the FPGA device address
echo ""
echo "Locating PCIe bus under parent root port for $1"
FPGA_PCIE_DEV=$1
shift

# Make note of any extra, ignored command line parameters
if [ "$#" -gt 0 ] ; then
    echo "WARNING: Ignoring extra command line parameters $@"
fi

PCI_DEVICES='/sys/bus/pci/devices'
PCI_ID=($(echo "${FPGA_PCIE_DEV}" | tr ':' ' '))
PCI_BUS=$(printf '%d' "0x${PCI_ID[1]}")
printf '  %-12s  %2s   %2s\n' 'Root Port' 'Lo' 'Hi'
printf '  %-12s  %2s   %2s\n' '------------' '--' '--'
root_port=''
bus_path=''
for path in $(find -L "${PCI_DEVICES}" -mindepth 2 -maxdepth 2 -path "*/${PCI_ID[0]}:*/subordinate_bus_number" -printf '%h\n' | sort); do
    dev=$(basename "${path}")
    sec=$(<"${path}/secondary_bus_number")
    sub=$(<"${path}/subordinate_bus_number")
    printf '  %12s: %02x - %02x' "${dev}" "${sec}" "${sub}"

    if (( ${sec} == ${PCI_BUS} && ${sub} == ${PCI_BUS} )); then # PCIe root port with a single endpoint
        echo '  <====  matched'
        root_port="${dev}"
        bus_path="${path}/pci_bus/${PCI_ID[0]}:${PCI_ID[1]}"
    else
        echo '  skip'
    fi
done

if [[ "${root_port}" == "" ]] || [[ "${bus_path}" == "" ]]; then
    echo "ERROR: Failed to determine a root port to rescan"
    exit 1
fi

find_devices() {
    find "${PCI_DEVICES}" -mindepth 1 -maxdepth 1 -name "${FPGA_PCIE_DEV}.*" -printf '%P\n' | sort -n
}

FUNCTIONS=( 0 1 )
for ((r=1; r<=3; r++)); do
    echo "Rescanning PCIe bus under parent root port ${root_port} [attempt ${r}]"
    echo 1 >"${bus_path}/rescan"

    echo "==> Waiting for uevent processing to settle"
    udevadm settle
    echo "==> Done settling uevents"

    echo "==> Waiting for devices to become present in udev database"
    for ((i=1; i<=10; i++)); do
        func_count=0
        for func in "${FUNCTIONS[@]}"; do
            dev_path="${PCI_DEVICES}/${FPGA_PCIE_DEV}.${func}"
            if udevadm info "${dev_path}" &>/dev/null; then
                func_count=$((func_count + 1))
            fi
        done

        if ((func_count == ${#FUNCTIONS[@]})); then
            devices=( $(find_devices) )
            echo "====> Done rescan of PCIe root port ${root_port}.  Devices found: ${devices[@]}"
            exit 0
        fi
        sleep 1s
    done

    devices=( $(find_devices) )
    echo "====> Timed out waiting for rescan of PCIe root port ${root_port}.  Devices found: ${devices[@]}"
done

devices=( $(find_devices) )
echo "Failed rescanning of PCIe root port ${root_port}.  Devices found: ${devices[@]}"
exit 1
