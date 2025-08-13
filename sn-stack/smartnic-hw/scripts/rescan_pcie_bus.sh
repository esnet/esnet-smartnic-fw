#!/bin/bash

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
echo "Locating and rescanning PCIe bus under parent root port for $1"
FPGA_PCIE_DEV=$1
shift

# Make note of any extra, ignored command line parameters
if [ "$#" -gt 0 ] ; then
    echo "WARNING: Ignoring extra command line parameters $@"
fi

PCI_ID=($(echo "${FPGA_PCIE_DEV}" | tr ':' ' '))
PCI_BUS=$(printf '%d' "0x${PCI_ID[1]}")
printf '  %-12s  %2s   %2s\n' 'Root Port' 'Lo' 'Hi'
printf '  %-12s  %2s   %2s\n' '------------' '--' '--'
for path in $(find -L /sys/bus/pci/devices/ -mindepth 2 -maxdepth 2 -path "*/${PCI_ID[0]}:*/subordinate_bus_number" -printf '%h\n' | sort); do
    dev=$(basename "${path}")
    sec=$(<"${path}/secondary_bus_number")
    sub=$(<"${path}/subordinate_bus_number")
    printf '  %12s: %02x - %02x' "${dev}" "${sec}" "${sub}"

    if (( ${sec} == ${PCI_BUS} && ${sub} == ${PCI_BUS} )); then # PCIe root port with a single endpoint
        echo "  <====  matched, rescanning"
        echo 1 > ${path}/rescan
    else
        echo '  skip'
    fi
done
