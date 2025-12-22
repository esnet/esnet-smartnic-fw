#!/bin/bash

function usage() {
    echo ""
    echo "Usage: $(basename $0) <hw_server_url> <hw_target_serial> <bitfile_path> <probes_path> <pcie_device_addr> [FORCE]"
    echo "  hw_server_url: host:port for xilinx hw_server (e.g. xilinx-hwserver:3121)"
    echo "  hw_target_serial: serial number of serial JTAG device (e.g. 21770205K01Y)"
    echo "     can be discovered with: lsusb -v -d 0403:6011 | grep iSerial"
    echo "     can be set to '*' to select the first serial JTAG device that is found"
    echo "  bitfile_path: full path to the .bit file to be loaded into the fpga"
    echo "  probes_path: full path to the .ltx debug probes file containing the VIOs for resetting the PCIe endpoint. If the file does not exist, the reset is skipped"
    echo "  pcie_device_addr: pcie domain:bus:device (e.g. 0000:81:00)"
    echo "  FORCE: optionally force a reload even if the USERCODE/UserID fields match"
    echo ""
}

# Make sure the caller has provided the required parameters
if [ "$#" -lt 5 ] ; then
    echo "ERROR: Missing required parameter(s)"
    usage
    exit 1
fi

# Note: This is intended as a temporary measure until the xilinx-labtools-docker container is updated.
if ! which udevadm &>dev/null; then
    apt update
    apt install -y udev
fi

# Grab URL pointing to the xilinx hw_server
echo "Using Xilinx hw_server URL: $1"
HW_SERVER_URL=$1
shift

# Grab the serial number for the USB-JTAG cable so that
# we can differentiate among them in case we have many
echo "Using USB-JTAG cable with serial: $1"
HW_TARGET_SERIAL=$1
shift

# Make sure the bitfile exists and is readable
if [ ! -e $1 ] ; then
    echo "ERROR: Bitfile does not exist: $1"
    exit 1
fi
if [ ! -r $1 ] ; then
    echo "ERROR: Bitfile at $1 is not readable"
    exit 1
fi

# Make sure the provided file looks like a xilinx bitfile
file -b $1 | grep 'Xilinx BIT data' 2>&1 > /dev/null
if [ $? -ne 0 ] ; then
    echo "ERROR: Provided file does not appear to be Xilinx BIT data"
    exit 1
fi

# Looks like we have a sane bitfile to work with
echo "Using target bitfile: $1"
echo "    $(file -b $1)"
BITFILE_PATH=$1
shift

echo "Using debug probes file: $1"
PROBES_PATH=$1
shift

# Grab the FPGA device address
echo "Expecting to reprogram PCIe device: $1"
FPGA_PCIE_DEV=$1
shift

# Check for the optional FORCE parameter
FORCE=0
if [ "$#" -ge 1 ] ; then
    if [ "x$1" = "xFORCE" ] ; then
	echo "NOTE: Using the FORCE.  FPGA will be reloaded even if USERCODE/UserID registers match."
	FORCE=1
	shift
    fi
fi

# Check if we're running on an AMD system and FORCE the FPGA to be reloaded even if USERCODE/UserID registers match
# This is necessary due to many AMD systems not providing a proper PCIe hot-reset to the cards
if ! [ -e $PROBES_PATH ]; then
    case $(lscpu --json | jq -r '.lscpu[] | select(.field == "Vendor ID:") | .data') in
        AuthenticAMD)
            echo "NOTE: Detected AMD CPU/chipset, enabling workaround for missing PCIe Hot Reset.  FPGA will be reloaded even if USERCODE/UserID registers match."
            FORCE=1
            ;;
        GenuineIntel)
            ;;
        *)
            ;;
    esac
fi

# Make note of any extra, ignored command line parameters
if [ "$#" -gt 0 ] ; then
    echo "WARNING: Ignoring extra command line parameters $@"
fi

# First, check if we are already running the correct FPGA version
/scripts/check_fpga_version.sh "$HW_SERVER_URL" "$HW_TARGET_SERIAL" "$BITFILE_PATH"
FPGA_VERSION_OK=$?

PCI_DEVICES='/sys/bus/pci/devices'

find_root_port() {
    local dev="$1"; shift
    local pci_id=($(echo "${dev}" | tr ':' ' '))
    local pci_bus=$(printf '%d' "0x${pci_id[1]}")

    for path in $(find -L "${PCI_DEVICES}" -mindepth 2 -maxdepth 2 \
                      -path "*/${pci_id[0]}:*/subordinate_bus_number" \
                      -printf '%h\n' | sort); do
        local root_port=$(basename "${path}")
        local sec=$(<"${path}/secondary_bus_number")
        local sub=$(<"${path}/subordinate_bus_number")

        # PCIe root port with a single endpoint
        if (( ${sec} == ${pci_bus} && ${sub} == ${pci_bus} )); then
            echo "${root_port}"
            return 0
        fi
    done

    return 1
}

remove_pcie_devices() {
    local dev_id="$1"; shift
    local devices=( $(find "${PCI_DEVICES}" -mindepth 1 -maxdepth 1 \
                          -name "${dev_id}.*" -printf '%P\n' | sort -n) )

    # Disconnect any devices from the kernel
    for dev in "${devices[@]}"; do
        local dev_path="${PCI_DEVICES}/${dev}"

        echo "Removing PCIe device ${dev}"
        echo 1 >"${dev_path}/remove"

        echo "Waiting for uevent processing to settle"
        udevadm settle
        echo "Done settling uevents"

        echo "Waiting for removal of PCIe device ${dev}"
        local remove_done=0
        for ((i=0; i<10; i++)); do
            if ! udevadm info "${dev_path}" &>/dev/null; then
                echo "==> Done removal of PCIe device ${dev}"
                remove_done=1
                break
            fi
            sleep 1s
        done

        if ((remove_done != 1)); then
            echo "==> Timed out waiting for removal of PCIe device ${dev}"
            exit 1
        fi
    done

    return 0
}

ROOT_PORT=$(find_root_port "${FPGA_PCIE_DEV}")
if [[ $? -eq 0 ]]; then
    # Removing all devices from a PCIe root port that supports automatic power management will
    # force the root port to transition from the D0 power state to D3hot.  When attempting a
    # rescan, the wakeup proceedure on the root port is triggered rather than actually searching
    # for devices as expected.  This results in no devices appearing and the need for a second
    # rescan to actually find them.  By disabling automatic power control on the root port, it
    # stays in the D0 state when it's devices are removed and does not require multiple rescans.
    echo "Disabling automatic power control on PCIe root port ${ROOT_PORT} for device ${FPGA_PCIE_DEV}"
    echo 'on' >"${PCI_DEVICES}/${ROOT_PORT}/power/control"
else
    echo "ERROR: Failed to find root port for PCIe device ${FPGA_PCIE_DEV}"
    exit 1
fi

if [[ $FORCE -eq 0 && $FPGA_VERSION_OK -eq 0 ]] ; then
    echo "Running and Target FPGA versions match"
else
    if [ $FPGA_VERSION_OK -eq 0 ] ; then
	echo "Running versions match but an FPGA reprogramming was FORCE'd anyway"
    else
	echo "Running version does not match Target version, reprogramming"
    fi

    remove_pcie_devices $FPGA_PCIE_DEV

    # Program the bitfile into the FPGA
    vivado_lab \
	-nolog \
	-nojournal \
	-tempDir /tmp/ \
	-mode batch \
	-notrace \
        -quiet \
	-source /scripts/program_card.tcl \
	-tclargs "$HW_SERVER_URL" "$HW_TARGET_SERIAL" "$BITFILE_PATH"
    if [ $? -ne 0 ] ; then
	echo "Failed to load FPGA, bailing out"
	exit 1
    fi

    # Re-check if we have all expected devices on the bus now
    /scripts/check_fpga_version.sh "$HW_SERVER_URL" "$HW_TARGET_SERIAL" "$BITFILE_PATH"
    FPGA_VERSION_OK=$?
    if [ $FPGA_VERSION_OK -eq 0 ] ; then
	echo "Running and Target FPGA versions match"
    else
	echo -n "Running version STILL does not match Target version.  "
	if [ $FORCE -eq 1 ] ; then
	    echo "Continuing anyway due to FORCE option."
	else
	    echo "Bailing out!"
	    exit 1
	fi
    fi
fi

if [ -e $PROBES_PATH ]; then
    remove_pcie_devices $FPGA_PCIE_DEV

    # Perform the reset of the PCIe endpoint via VIOs.
    vivado_lab \
        -nolog \
        -nojournal \
        -tempDir /tmp/ \
        -mode batch \
        -notrace \
        -quiet \
        -source /scripts/pcie_reset.tcl \
        -tclargs "$HW_SERVER_URL" "$HW_TARGET_SERIAL" "$PROBES_PATH"
    if [ $? -ne 0 ] ; then
        echo "Failed to reset PCIe endpoint, bailing out"
        exit 1
    fi
else
    echo "Skipping reset of PCIe endpoint due to not having probes"
fi

# Always rescan the PCIe bus under the parent port for this FPGA card
/scripts/rescan_pcie_bus.sh ${FPGA_PCIE_DEV}
