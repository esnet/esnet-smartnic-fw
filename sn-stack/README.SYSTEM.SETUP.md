# Purpose and context for this document

This document is intended to provide an opinionated set of instructions for how to configure a new server so that it can be used as a platform for running ESnet SmartNIC applications in a production environment.

This document is written from the perspective of having the following distinct roles within an organization:
1. System Administrators
  * Has physical access to the server for card installation and cabling
  * Has `root` on the physical server
  * Responsible for hardware, cabling, LOM, BIOS/UEFI, OS install + patching, user management, system config
2. SmartNIC Service Account
  * Service account called `smartnic`
  * Not associated with an individual person
  * Owns the files related to the SmartNIC Application
  * No direct `root` / `sudo` privileges required
  * Member of `docker` group
    * Able to run privileged containers
    * Capable of directly accessing SmartNIC FPGA cards
3. SmartNIC Application Operators
  * Real people responsible with the operation of the SmartNIC application
  * Able to remotely log into the system
  * Possibly no `root` privileges
  * Has enough privileges to log in as or at least run commands as the `smartnic` service account
  * Responsible for installing, upgrading and monitoring the firmware and software that ultimately run on the SmartNIC FPGA cards.
  
**This document is targetted at System Administrator(s) and covers the items for 1 and 2 in the list above.  Creating the user accounts for item 3 is entirely at the discretion of the system administrators and is not covered here.**

If the roles described above do not match your situation, you may need or want to modify some of the setup described below.

# Hardware Installation

## FPGA Card Installation

Ensure server airflow is sufficient to cool the FPGA cards (refer to Xilinx documentation for requirements)

Record the FPGA card serial numbers along with which physical slot each card is plugged into in the server chassis
* This information will help in future steps where we create a mapping between the physical FPGA cards and the USB/JTAG interface that is used to program them.

In multi-socket, multi-card installations, it is recommended to spread the FPGA cards across PCIe slots connected to the two CPU sockets.  This helps spread the PCIe bandwidth usage within the server.  This may be more or less important for your use case depending on whether your design directs packets toward the host CPU.

Record your physical cards in a table similar to this one for future reference
```
 --------------- ----------- ---------------
| Physical Slot | Card Type | Card Serial # |
|---------------|-----------|---------------|
| Slot 1        | U55C      | XFL1......2V  |
| Slot 2        | U55C      | XFL1......0U  |
| Slot 4        | U55C      | XFL1......TC  |
| Slot 5        | U55C      | XFL1......Z0  |
 --------------- ----------- ---------------
```

## USB / JTAG connection

Connect the micro-USB connection of each of the U280 / U55C FPGA cards to an unused USB port on the server.  This connection is required by the ESnet SmartNIC firmware to load new FPGA bitfiles each time the FW stack starts.

## Aux Power Connection

It is recommended that wherever possible, you connect the AUX power connector of the U280 / U55C FPGA card to an appropriate connection on your server's power supply.

**NOTE** Refer to the documentation for your server and for the Alveo FPGA cards that you're installing to ensure that you have the correct AUX power cable pinout.  **Getting this wrong can cause damange to both your server and your FPGA cards.**

## QSFP Optics Module Selection and Installation

Some things to consider when choosing QSFP modules for use with the Alveo U280 / U55C FPGA cards:
* Higher power class optics (eg. LR) draw more power and generate more heat
  * These modules will place additional importance on AUX power and on the cooling capabilities of your server
  * Carefully monitor power and cooling
  * Watch out for card resets caused by over temp events
* Alveo FPGA cards have a very limited QSFP compatibility matrix
  * Consult the compatibility matrix for your card when selecting a QSFP
    * https://adaptivesupport.amd.com/s/article/000034838
    * **NOTE** in particular the requirement to support SFF-8636 rev. 2.5 or newer
  * If using QSFPs that are not listed, always test compatibility and monitor the SC console logs for error messages
  * Incompatible QSFP modules have been observed to cause the Alveo Satellite Controller to lock up, requiring a power cycle to recover

# BIOS / UEFI Setup

## Power Settings

Some servers may require specific settings for higher power modes when using FPGA card(s).  Look for settings related to "HPC mode" or similar which may increase the available power provided to the PCIe cards.

## Slot Bifurcation Settings

Some servers allow the PCIe lanes within a physical PCIe slot to be split (bifurcated) and treated as two separate slots.

The ESnet SmartNIC FPGA designs implement a single PCIe endpoint so your slot bifurcation configuration should be such that all of the PCIe lanes terminating on the FPGA card are all part of the same, single root port.  This means that slot bifurcation should typically be disabled for the slots holding FPGA cards.

Note, however that some servers make use of PCIe riser cards which physically split (bifurcate) the PCIe lanes on a connector into two distinct PCIe card slots.  In these scenarios, your BIOS configuration may have to have PCIe Slot Bifurcation enabled (to reflect the reality that the riser card has split the lanes to two different card slots).

Given the wide variation among server architectures, the correct setting might not be obvious or known without detailed information on the PCI bus topology or some experimentation.  If in doubt, please refer to your server documentation or contact your server support team for assistance.

## IO Virtualization
* VT-d enabled
* IOMMU enabled
* x2apic enabled

# Operating System Installation

Currently Recommended OS: Ubuntu Server 22.04

Beta/Testing/Next OS: Ubuntu Server 24.04

# Bootloader / Kernel Commandline Setup

The SmartNIC application stacks depend on certain kernel features being enabled at boot.  Enabling these features is done by editing the kernel command line that the `grub` bootloader provides to the kernel as it starts.

The required features are
* Hugepages
  * Used by the DPDK components within the SmartNIC application stack to allocate packet memory.
* IOMMU
  * Required by the `vfio-pci` kernel driver which is used to provide userspace access to the registers within the SmartNIC FPGA over PCIe.

To enable these features on the kernel command line, edit your `/etc/default/grub` file to add one of these lines depending on which CPU vendor you're using.

For an AMD CPU, use these settings
```
GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=32 amd_iommu=on iommu=pt"
```

For an Intel CPU, use these settings
```
GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=32 intel_iommu=on iommu=pt"
```

After editing the `/etc/default/grub` configuration, you **MUST** run this command for your changes to take effect on the next boot.
``` bash
sudo update-grub
```

**NOTE** The bootloader changes will not take effect until the system has been rebooted.  You can reboot now or defer the reboot until later in the bootstrapping sequence after updating the FPGA flash.

# Confirm that all USB/JTAG cables have been properly connected

The USB/JTAG connection to each FPGA card will show up as a quad-port USB-UART (FT4232H) manufactured by Future Technology Devices International (FTDI).

``` bash
$ lsusb -d 0403:6011
Bus 005 Device 006: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
Bus 005 Device 005: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
Bus 005 Device 004: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
Bus 005 Device 003: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
```
(example from a system with four Alveo au55c FPGA cards)

Each of these devices also provides more detailed information including the FPGA card Manufacturer (Xilinx), Product (A-U55C or A-U280) as well as the card's Serial Number.
``` bash
$ sudo lsusb -d 0403:6011 -vv 2>/dev/null | grep -Ei '(iManufacturer|iProduct|iSerial|^Bus)'
Bus 005 Device 006: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
  iManufacturer           1 Xilinx
  iProduct                2 A-U55C
  iSerial                 3 XFL1......Z0
Bus 005 Device 005: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
  iManufacturer           1 Xilinx
  iProduct                2 A-U55C
  iSerial                 3 XFL1......TC
Bus 005 Device 004: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
  iManufacturer           1 Xilinx
  iProduct                2 A-U55C
  iSerial                 3 XFL1......0U
Bus 005 Device 003: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
  iManufacturer           1 Xilinx
  iProduct                2 A-U55C
  iSerial                 3 XFL1......2V
```
(example from a system with four Alveo au55c FPGA cards -- your bus/device numbers may differ)

``` bash
$ sudo lsusb -d 0403:6011 -vv 2>/dev/null | grep -Ei '(iManufacturer|iProduct|iSerial|^Bus)'
Bus 001 Device 002: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
  iManufacturer           1 Xilinx
  iProduct                2 A-U280-P32G
  iSerial                 3 2177......4N
```
(example from a system with one Alveo au280 FPGA card -- your bus/device numbers may differ)

**NOTE** If any of the expected cards are missing from the output of either of these commands, double check the micro-USB connection on the FPGA card.  The micro-USB port on the back plate of the au55c card can sometimes be partially obstructed by the case metalwork, preventing proper seating of the USB plug.

# Docker Installation

Follow the official docker installation instructions
* https://docs.docker.com/engine/install/ubuntu/#install-using-the-repository

Success checks:
``` bash
sudo docker version
sudo docker compose version
sudo docker run --rm hello-world
```

# Create the `smartnic` service account

When setting up a production system to run SmartNIC firmware and software stacks, it is recommended to create a dedicated service account which can own and manage the SmartNIC stacks.  This user must be a member of the `docker` group in order to manage the SmartNIC stacks.

``` bash
sudo adduser --gecos "" --disabled-password --shell /bin/bash smartnic
sudo adduser smartnic docker
```

**NOTE** All of the remaining documentation assumes that the `smartnic` user will be the service user.  You can choose a different name for the account but you'll need to adapt various instructions and helper scripts to match.

Success checks:
``` bash
sudo -i -u smartnic docker run --rm hello-world
```

# Create a common directory tree to hold the `smartnic` firmware deployments

``` bash
sudo mkdir -p /usr/local/smartnic
sudo chown root:smartnic /usr/local/smartnic
sudo mkdir /usr/local/smartnic/{0..9}
sudo chown smartnic:smartnic /usr/local/smartnic/{0..9}
sudo chmod 775 /usr/local/smartnic/{0..9}
```

**NOTE** By convention, the smartnic firmware stack for the card in server physical slot X should be deployed under `/usr/local/smartnic/X` to make it easy to match up the deployed firmware with the physical card it should apply to.

# Install a trusted TLS certificate

The SmartNIC application stack provides secure configuration channels, encrypted with TLS.  Many applications will require an external controller to manage the SmartNIC application.  For maximum security and compatibility with TLS clients, it is recommended that you install a TLS certificate signed by a globally trusted CA.

The SmartNIC application stack includes example settings to allow it to easily refer to a TLS certificate and private key installed under `/etc/letsencrypt/<FQDN>` where it might be placed by popular ACME tooling.  If you install the TLS certificate to a custom path, or with custom naming, the SmartNIC application can be configured to find it in your preferred location.

As a fallback, in the absence of a provided TLS certificate, the SmartNIC stack will auto-generate a new self-signed TLS certificate on every restart.  Note that some TLS clients will refuse to connect to endpoints which provide a self-signed TLS certificate so operating in this fallback mode is *NOT* recommended for a production environment.

# Obtain SmartNIC Bootstrap Zip file

The next sections all depend on a build artifact that provides you with tools and FPGA flash images for commissioning the Xilinx FPGA cards to run as ESnet SmartNICs.  This build artifact can be obtained by:
1. The bootstrap zip file was provided to you directly
   * The developers working on your SmartNIC application would typically pre-build this zip file and provide it to the sysadmins who are bootstrapping the system
   * The bootstrap zip file in this case will be named something like:
     * `artifacts.esnet-smartnic-fw.[au280|au55c].ejfat.bootstrap.<build#>.zip`
     * `artifacts.esnet-smartnic-fw.bootstrap.0.zip`
   * This is the typical method for sysadmins
2. Building it yourself out of the `esnet-smartnic-fw` repo
   * This method is typically used by developers working on the SmartNIC application
   * Instructions for this method can be found in the `README.md` at the top of the `esnet-smartnic-fw` git repo
   * The bootstrap zip file in this case will be named `artifacts.esnet-smartnic-fw.bootstrap.0.zip`

Regardless of how you obtained your bootstrap zip file, the next step is to unzip it **on the server being bootstrapped**.
```
unzip artifacts.esnet-smartnic-fw.bootstrap.0.zip
```

This zip file contains the following files
```
$ tree sn-bootstrap/
sn-bootstrap/
├── README.SYSTEM.SETUP.md    <------------ copy of this document
├── README.SYSTEM.DECOM.md    <------------ instructions to decommission an ESnet SmartNIC system
├── SC_U280_4_3_31.zip        <------------ (optional) Satellite Controller Firmware Image (au280)
├── SC_U55C_7_1_24.zip        <------------ (optional) Satellite Controller Firmware Image (au55c)
├── esnet-smartnic.au280.mcs  <------------ FPGA card flash image for au280 FPGA card
├── esnet-smartnic.au55c.mcs  <------------ FPGA card flash image for au55c FPGA card
├── loadsc                    <------------ (optional) Xilinx tool for upgrading Satellite Controller
├── smartnic-system-setup_0.5.0_all.deb <-- Debian package providing udev and systemd units for card management
└── xbflash2                  <------------ Xilinx tool for programming FPGA card flash images
```
(version numbers above are latest as of March 2026)

# Prepare the system to manage ESnet SmartNIC FPGA cards

This section will install a set of `udev` and `systemd` configuration files which will prepare the system to manage ESnet SmartNIC FPGA cards.  This is provided by a `smartnic-system-setup` Debian package.

The `smartnic-system-setup` package contains `udev` and `systemd` services and scripts to automatically manage system-level aspects of the SmartNICs.
* udev: 99-smartnic-pci.rules
  * Binds to all variants of SmartNIC FPGA cards
  * Decodes SmartNIC PCIe VPD data
  * Activates `systemd` units to manage the PCIe root port above the SmartNIC
* systemd: smartnic-pcie-err-disable@.service
  * Disables Severe and Fatal Error Reporting for PCIe root ports above SmartNICs to allow safe reloading of FPGAs without a reboot
  * Automatically activated for all SmartNICs
* systemd: smartnic-pcie-power-control-disable@.service
  * Disables automatic power control for PCIe root ports above SmartNICs to prevent power-off during FPGA reloads
  * Automatically activated for all SmartNICs
* systemd: smartnic-stack-restart@.service
  * Ensures that active SmartNIC stacks are fully restarted on boot
  * Manually user-enabled

## Install the `smartnic-system-setup` Debian package on your system

``` bash
cd sn-bootstrap
sudo dpkg -i smartnic-system-setup_0.5.0_all.deb
```

# Convert FPGA cards from factory images to ESnet SmartNIC images

When the FPGA cards are delivered from the factory, they will be configured as generic FPGA cards.  This section will convert the FPGA cards into ESnet SmartNIC cards.

## Install prerequisites for the `xbflash2` tool

The `xbflash2` tool will be used to program the on-card flash storage of each FPGA card so that on power-up, it will be initialized as an ESnet SmartNIC.

This tool requires one or more packages on the host system as dependencies, install them with these commands
``` bash
sudo apt install --no-install-recommends libboost-program-options1.74.0
```

## Install `parallel` to allow faster execution of updates in parallel

Later long-running steps can be run in parallel.  This can save 30+ minutes when operating on multi-card systems.  Install a helper package to make this more ergonomic.

``` bash
sudo apt install parallel
```

## Write the SmartNIC image onto the flash storage of each FPGA card (if Factory/Golden Image)

Find out what type of FPGA cards you have
``` bash
$ lspci -Dd 10ee: 
0000:21:00.0 Processing accelerators: Xilinx Corporation Device 505c
0000:22:00.0 Processing accelerators: Xilinx Corporation Device 505c
0000:81:00.0 Processing accelerators: Xilinx Corporation Device 505c
0000:82:00.0 Processing accelerators: Xilinx Corporation Device 505c
```
(example from a system with four Alveo au55c FPGA cards)

**NOTE** The following commands will program all factory cards in parallel with delayed, interleaved console output.  Line-buffered IO here means that the progress updates don't come out very often.  Be patient even if it looks like it's not doing anything for multiple minutes.  This process takes ~10 minutes total to program all of your cards.  Doing this sequentially would take 10 minutes *per card*.

If your FPGA cards are au55C cards, they will have reported as `Xilinx Corporation Device 505c`.  Use these commands to program their flash.
``` bash
cd sn-bootstrap
lspci -Dd 10ee:505c | awk -F' ' '{ print $1 }' | \
  time \
  parallel --verbose --line-buffer --tag --no-run-if-empty \
    'printf "\n" | sudo ./xbflash2 program --spi --image esnet-smartnic.au55c.mcs --bar-offset 0x1f06000 --device {}'
```

If your FPGA cards are au280 cards, they will have reported as `Xilinx Corporation Device d00c`.  Use these commands to program their flash.
``` bash
cd sn-bootstrap
lspci -Dd 10ee:d00c | awk -F' ' '{ print $1 }' | \
  time \
  parallel --verbose --line-buffer --tag --no-run-if-empty \
    'printf "\n" | sudo ./xbflash2 program --spi --image esnet-smartnic.au280.mcs --bar-offset 0x40000 --device {}'
```

If your FPGA cards report as `Xilinx Corporation Device 903f` or `Xilinx Corporation Device 913f` then your cards are already ESnet SmartNIC cards.  Refer to the next section for how to update the flash on these cards.

## Updating the flash of an existing SmartNIC FPGA card (if already an ESnet SmartNIC Image)

If your FPGA cards reported as `Xilinx Corporation Device 903f` or `Xilinx Corporation Device 913f` in the previous section, then your cards are already ESnet SmartNIC cards.  You can still update them to the flash image provided in this bootstrap zip file.  This situation will occur if you are updating to a newer release of the ESnet SmartNIC flash image which modifies the PCIe BAR configuration or the set of PCIe features being advertised by the ESnet SmartNIC.

Example output for cards already running an ESnet SmartNIC image:
```
$ lspci -Dd 10ee:
0000:21:00.0 Network controller: Xilinx Corporation Device 903f
0000:21:00.1 Network controller: Xilinx Corporation Device 913f
0000:22:00.0 Network controller: Xilinx Corporation Device 903f
0000:22:00.1 Network controller: Xilinx Corporation Device 913f
0000:81:00.0 Network controller: Xilinx Corporation Device 903f
0000:81:00.1 Network controller: Xilinx Corporation Device 913f
0000:82:00.0 Network controller: Xilinx Corporation Device 903f
0000:82:00.1 Network controller: Xilinx Corporation Device 913f
```

Before you run the next command, you will need to edit the name of the `.mcs` file given in the command, to match the type of FPGA card you are updating. It will be one of these two filenames:
* au280 card: `esnet-smartnic.au280.mcs`
* au55c card: `esnet-smartnic.au55c.mcs`

``` bash
cd sn-bootstrap
lspci -Dd 10ee:903f | awk -F' ' '{ print $1 }' | \
  time \
  parallel --verbose --line-buffer --tag --no-run-if-empty \
    'printf "\n" | sudo ./xbflash2 program --spi --image esnet-smartnic.au280_or_au55c.mcs --bar 2 --bar-offset 0x20000 --device {}'
```

**NOTE** If you have an older ESnet SmartNIC version which lacks the Vital Product Data section in `lspci -vv` output, you may need to use `--bar-offset 0x340000` as the older versions positioned the Flash QSPI registers at a different offset.

## Cold boot the system

After flash programming is completed, cold boot the system to allow the FPGAs to load their configuration from flash.  This must be a proper cold boot (eg. `ipmitool chassis power cycle`) and **not** an ordinarly warm boot (eg. `shutdown -r now`) or the FPGAs will not reconfigure from flash.

``` bash
sudo ipmitool chassis power cycle
```

**DO NOT SKIP THIS STEP**

**NOTE** Although rare, some servers fail to properly perform a cold boot (ie. removing all power to the PCIe slots) using this sequence.  They may require different steps, or even physically removing the power cables from *all* power supply units (PSUs) on the chassis in order to force the FPGA cards to load from their flash.  In particular, this has been observed on at least one Supermicro server model.

## Verify that all FPGA cards are now reporting as ESnet SmartNIC cards

``` bash
$ lspci -Dd 10ee:
0000:21:00.0 Network controller: Xilinx Corporation Device 903f
0000:21:00.1 Network controller: Xilinx Corporation Device 913f
0000:22:00.0 Network controller: Xilinx Corporation Device 903f
0000:22:00.1 Network controller: Xilinx Corporation Device 913f
0000:81:00.0 Network controller: Xilinx Corporation Device 903f
0000:81:00.1 Network controller: Xilinx Corporation Device 913f
0000:82:00.0 Network controller: Xilinx Corporation Device 903f
0000:82:00.1 Network controller: Xilinx Corporation Device 913f
```
(example from a system with four Alveo au55c FPGA cards -- your bus addresses may differ)

Verify that all cards are now reporting as functions with device IDs in the form `9x3f`:
* `Xilinx Corporation Device 903f`
* `Xilinx Corporation Device 913f`

## Build the PCIe Address to Card Serial Number Map

Collect the PCIe Vital Product Data (VPD) for all FPGA cards
``` bash
$ sudo lspci -vvv -Dd 10ee:903f | awk '/^0000/ { print } /Vital Prod/,/Advanced Error/ { print }' | grep -v 'Advanced Error'
0000:21:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[VA] Vendor specific: Application   : ejfat
			[VB] Vendor specific: Build ID      : 98635
			[VR] Vendor specific: Build git repo: udplb
			[VH] Vendor specific: Build git hash: e023ec49
			[VT] Vendor specific: Build time    : 2026-04-01T05:07:27Z
			[VF] Vendor specific: Flash offset  : 0x20000
			[VC] Vendor specific: CMS offset    : 0x40000
			[PN] Part number: ALVEO U55C PQ
			[SN] Serial number: XFL1......2V
			[RM] Firmware version: 7.1.24
			[RV] Reserved: checksum good, 80 byte(s) reserved
		End
0000:22:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[...]
			[PN] Part number: ALVEO U55C PQ
			[SN] Serial number: XFL1......0U
			[RM] Firmware version: 7.1.24
			[RV] Reserved: checksum good, 80 byte(s) reserved
		End
0000:81:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[...]
			[PN] Part number: ALVEO U55C PQ
			[SN] Serial number: XFL1......Z0
			[RM] Firmware version: 7.1.24
			[RV] Reserved: checksum good, 80 byte(s) reserved
		End
0000:82:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[...]
			[PN] Part number: ALVEO U55C PQ
			[SN] Serial number: XFL1......TC
			[RM] Firmware version: 7.1.24
			[RV] Reserved: checksum good, 80 byte(s) reserved
		End
```
(example from a system with four Alveo au55c FPGA cards -- your build info and bus addresses may differ)

**NOTE** Older versions of the ESnet SmartNIC bootstrap flash images did not export Vital Product Data.  If you do not see the Vital Product Data information or if your VPD or your VPD section has fewer fields in it, it may indicate that you have an older version active.  Double check that your system has properly **cold booted** (ie. full removal of PCIe card power), pulling power cables if necessary for your system.  It is recommended that you have update to a version of the `.mcs` flash image that supports VPD before proceeding with the rest of these instructions.  If you are certain that the older, non-VPD version is the one you want to be running, proceed with caution as some of the subsequent steps may not be possible or safe without accurate information about your FPGA cards.

Another view of the same information can be seen with this command but from the perspective of the devices detected by udev + systemd
``` bash
$ lspci -Dd 10ee:903f | awk -F' ' '{ print $1 }' | sudo xargs -I{} systemctl list-units --full --quiet --legend=no --no-pager 'sys-devices-pci*-{}.device'
  sys-devices-pci0000:20-0000:20:01.1-0000:21:00.0.device loaded active plugged ESnet SmartNIC XFL1......2V
  sys-devices-pci0000:20-0000:20:01.2-0000:22:00.0.device loaded active plugged ESnet SmartNIC XFL1......0U
  sys-devices-pci0000:80-0000:80:01.1-0000:81:00.0.device loaded active plugged ESnet SmartNIC XFL1......Z0
  sys-devices-pci0000:80-0000:80:01.2-0000:82:00.0.device loaded active plugged ESnet SmartNIC XFL1......TC
```
(example from a system with four Alveo au55c FPGA cards -- your bus addresses may differ)

**NOTE** Record the relationship between the PCIe Bus Address and the VPD [SN] Serial number field.  This information will be needed by the SmartNIC Application Operators.

## Validate the Card Serial Number to Physical Slot Map that was captured during installation

If the VPD output in the previous section included the `[SN]` field, you can combine this information with the ACPI data provided by your BIOS/UEFI system firmware to confirm that the card mapping that you captured during installation matches the view from the running system.

Most (but not all) servers expose Slot records in their ACPI (aka DMI) data.  Slot records provide a mapping for:
* Physical Slot <-> PCIe Bus Address

VPD data (when available) provides a mapping for:
* PCIe Bus Address <-> Card Type, Serial Number and SC Firmware Version

We can combine these two mappings to create:
* Physical Slot <-> PCIe Bus Address <-> FPGA Card Type, Serial Number and SC FW Version

Query the ACPI/DMI Slot records
``` bash
$ sudo dmidecode -t 9 | awk '/(^System|Designation|Current Usage|ID:|Bus Address|^$)/ { print }'

System Slot Information
	Designation: PCIe Slot 3
	Current Usage: In Use
	ID: 3
	Bus Address: 0000:64:00.0

System Slot Information
	Designation: PCIe Slot 1
	Current Usage: In Use
	ID: 1
	Bus Address: 0000:21:00.0

System Slot Information
	Designation: PCIe Slot 2
	Current Usage: In Use
	ID: 2
	Bus Address: 0000:22:00.0

System Slot Information
	Designation: PCIe Slot 7
	Current Usage: Available
	ID: 7

System Slot Information
	Designation: PCIe Slot 8
	Current Usage: Available
	ID: 8

System Slot Information
	Designation: PCIe Slot 6
	Current Usage: In Use
	ID: 6
	Bus Address: 0000:a1:00.0

System Slot Information
	Designation: PCIe Slot 5
	Current Usage: In Use
	ID: 5
	Bus Address: 0000:81:00.0

System Slot Information
	Designation: PCIe Slot 4
	Current Usage: In Use
	ID: 4
	Bus Address: 0000:82:00.0
```
(example output for a system with 8 physical slots reported in the ACPI/DMI tables -- your output will likely look different)

Use the VPD data and ACPI/DMI Slot records to validate the records from the installation step and to build up the full view for:
```
 --------------- ----------- --------------- ------------------ -----------
| Physical Slot | Card Type | Card Serial # | PCIe Bus Address | SC FW Ver |
|---------------|-----------|---------------|------------------|-----------|
| Slot 1        | U55C      | XFL1......2V  | 0000:21:00.0     | 7.1.24    |
| Slot 2        | U55C      | XFL1......0U  | 0000:22:00.0     | 7.1.24    |
| Slot 4        | U55C      | XFL1......TC  | 0000:82:00.0     | 7.1.24    |
| Slot 5        | U55C      | XFL1......Z0  | 0000:81:00.0     | 7.1.24    |
 --------------- ----------- --------------- ------------------ -----------
```
(example from a system with 4 Alveo au55c FPGA cards -- your table will differ)

## Verify that the udev glue and systemd units are starting without error for all SmartNIC FPGA cards

Display the current status of all `smartnic-*` systemd units since boot.
``` bash
sudo systemctl status --all 'smartnic-*'
```

Display the full time-interleaved logs of all `smartnic-*` systemd units since boot
``` bash
sudo journalctl --output=short-iso-precise -u 'smartnic-*' --boot
```

# Upgrade the Alveo FPGA card's Satellite Controller (SC) firmware

The SmartNIC application stack requires a minimum firmware version on the Satellite Controller (SC).  If your SC firmware is older than these versions, you must upgrade it in order to use the SmartNIC application stack provided in this repository.
* au280: `4.3.31`
* au55c: `7.1.24`
(minimum required SC FW versions as of 2026-04)

**WARNING** This process has some risk of "bricking" (ie. rendering it unrecoverable / unusable) the Xilinx FPGA card.  If it is bricked, it will have to be returned/repaired via an RMA process with the vendor.  We have never had this happen to a U280 card (>200 individual SC FW upgrades over 80+ cards).  We have had one U55C FPGA card become bricked during an SC FW upgrade (at the time, this card was attached to QSFP optics that seemed to be causing communication problems but the root cause of the bricked card is still unknown as of March 2026).

The Satellite Controller (SC) is a microcontroller that is on the Xilinx FPGA card, adjacent to the FPGA ASIC itself.  The SC manages card initialization and monitoring.  The version of firmware running on the SC when the card comes from the factory is often quite old and is missing features and bug fixes.  Some of the bugs that we've seen in older SC firmware include spurious resets of the FPGA card, and an inability to read QSFP module information.  Other bugs are mentioned in various places in the Xilinx knowledge base.  If any of these bugs or missing features affects you, then upgrading the SC firmware may fix them.  If you have a fresh card, delivered from the factory, you will probably want to perform this upgrade at least once.

The implementation of the SC firmware update process is entirely within the closed-source firmware provided by Xilinx which runs on the Satellite controller.  Unfortunately, this means that we are unable to improve the robustness of the implementation.

**WARNING** If you are unsure of what you are doing, please consult your Xilinx support team for advice and assistance rather than following these instructions on your own.  Please include a pointer to this project's git repo in your Xilinx support request so that they have context for your request.

For general information about the Satellite Controller, refer to: https://xilinx.github.io/Alveo-Cards/master/management-specification/oob-intro.html
For general information about the Card Management Controller blocks, refer to: https://docs.amd.com/r/en-US/pg348-cms-subsystem/Introduction
For the latest Satellite Controller Firmware versions, refer to: https://adaptivesupport.amd.com/s/article/Alveo-Custom-Flow-Latest-CMS-IP-and-SC-FW
For details about the Xilinx Alveo FPGA card RMA process, refer to: https://adaptivesupport.amd.com/s/article/72533

**WARNING** Do NOT automate this sequence and run it repeatedly expecting it to be idempotent.  It will erase and re-write the SC firmware even if it's already running the desired version.  The `loadsc` tool does not detect/skip this scenario and automating this update step may repeatedly risk bricking your card for no benefit.

If you have read the warnings above, and would still like to upgrade the SC firmware on your Xilinx FPGA card, proceed with the steps in the following sections.

## Ensure that there are NO active application stacks running

**Do NOT skip this step**.  Any running application stacks which are active during the SC firmware update process will interfere with the communication path between the FPGA and the adjacent SC.  This interference may result in corruption of the SC firmware update messages, potentially resulting in a "bricked" FPGA card.

Follow the steps in the [Stop all running SmartNIC Application Stacks section of the README.SYSTEM.DECOM.md file](./README.SYSTEM.DECOM.md#stop-all-running-smartnic-application-stacks).

Verify that no remaining docker containers are running which may be interacting with the FPGA cards
```
sudo docker ps
```
**NOTE** For maximum safety, that command should return an empty list.  If that command *does* list any running containers, ensure that none of them are related to SmartNIC application stacks.  If you are unsure, **DO NOT PROCEED**.

## Identify the correct SC firmware file for your FPGA card

You will need to determine which FPGA card type (eg. U55C or U280) that you have in your system.  Note that your system may contain more than one type of FPGA card.  In that case, you will additionally need a clear view of the mapping between PCIe bus address and FPGA card type.  **DO NOT GUESS**.

Once you've identified the PCIe bus address and card type that you would like to inspect and possibly update, you will need to ensure that you have the matching SC firmware file for your card.  Depending on where you obtained your bootstrap zip file, it may or may not contain the relevant, matching SC firmware file for your card.

Look for a file called `SC_<U55C|U280>_<major>_<minor>_<patch>.[txt|zip]` in your `sn-bootstrap` directory that matches your FPGA card type.

If you do not have a matching SC firmware file already, or if you would like to check for the latest release from Xilinx, follow these steps:
* Navigate to https://adaptivesupport.amd.com/s/article/Alveo-Custom-Flow-Latest-CMS-IP-and-SC-FW
* Check the latest release version for your card
* If required, download the zip file for the matching SC firmware file
  * **NOTE** You may need to log in to be permitted to download files from this page

If you have a `.zip` file, you will need to unzip the file so that the `.txt` file within is extracted
```
$ cd sn-bootstrap
$ unzip SC_U55C_7_1_24.zip 
Archive:  SC_U55C_7_1_24.zip
   creating: SC_U55C_7_1_24/
  inflating: SC_U55C_7_1_24/SC_U55C_7_1_24.txt
```
(example for extracting the SC firmware for a U55C file from its zip file)

**NOTE** the `.txt` file may be extracted into a new directory.  Adjust the paths below to match the location of the `.txt` file as necessary.

## Read out the current SC firmware status

This step also confirms that we have working comms path through PCIe -> FPGA -> SC.
```
$ cd sn-bootstrap
$ sudo ./loadsc -d 0000:21:00.0 -b 2 -a 0x40000 -r

------------------------------------------------------------------------------------
CMS Satellite Controller Firmware Download Tool v2.3
------------------------------------------------------------------------------------

>> Aquiring BAR address of target card

   > PCIe DBDF                        0000:21:00.0 
   > PCIe BAR/Region                  2 
   > PCIe Vendor ID                   0x10EE

<< Aquiring BAR address of target card (success)

   < PCIe Device ID                   0x903F
   < PCIe BAR Address                 0xC4000000

>> Bring CMS Microblaze out of reset and establish comms with satellite controller

   > CMS Subsystem Base Address       0x00040000

<< Bring CMS Microblaze out of reset and establish comms with satellite controller (success)

   < CMS Firmware Version             1.5.26 (0x0C01051A)
   < CMS Hardware Version             Vivado 2023.2.2
   < SC  Firmware Version             7.1.23

------------------------------------------------------------------------------------
The current Satellite Controller firmware version is 7.1.23
------------------------------------------------------------------------------------

```
(example output from a U55C FPGA card)

**NOTE** Do NOT proceed to the next step if you get an error message from this read-out step.

## Program a newer version of the SC firmware to the card

**NOTE** Only perform this step if you've determined that your card requires an updated SC firmware version.  Upgrading may or may not be required, depending the details of the application you would like to run on your FPGA card.

**WARNING** Before running these commands, be absolutely sure that you are using the `.txt` file that matches your card type.  Installing the wrong SC firmware file onto your card may result in your card being "bricked" and requiring an RMA to replace/repair the card.  If you are unsure of what card type you have, **DO NOT PROCEED**.

```
$ cd sn-bootstrap
$ sudo ./loadsc -d 0000:21:00.0 -b 2 -a 0x40000 -f SC_U55C_7_1_24.txt

------------------------------------------------------------------------------------
CMS Satellite Controller Firmware Download Tool v2.3
------------------------------------------------------------------------------------

>> Aquiring BAR address of target card

   > PCIe DBDF                        0000:21:00.0 
   > PCIe BAR/Region                  2 
   > PCIe Vendor ID                   0x10EE

<< Aquiring BAR address of target card (success)

   < PCIe Device ID                   0x903F
   < PCIe BAR Address                 0xC4000000

>> Bring CMS Microblaze out of reset and establish comms with satellite controller

   > CMS Subsystem Base Address       0x00040000

>> Reading SC image file "SC_U55C_7_1_24.txt"
<< Reading SC image file "SC_U55C_7_1_24.txt" (success)

<< Bring CMS Microblaze out of reset and establish comms with satellite controller (success)

   < CMS Firmware Version             1.5.26 (0x0C01051A)
   < CMS Hardware Version             Vivado 2023.2.2
   < SC  Firmware Version             7.1.23

>> DownloadSequence_Initial (start)
<< DownloadSequence_Initial (success)
>> DownloadSequence_EraseFirmware (start)
<< DownloadSequence_EraseFirmware (success)
>> DownloadSequence_SendImage (start)
>> Image downloading: ..... 
<< DownloadSequence_SendImage (success)
>> DownloadSequence_JumpToResetVector (start)
<< DownloadSequence_JumpToResetVector (success)
------------------------------------------------------------------------------------
Satellite Controller firmware has been succesfully updated from 7.1.23 to 7.1.24
------------------------------------------------------------------------------------

```
(example output from a U55C FPGA card -- note the `SC_U55C_*` filename matches the U55C card type)

**NOTE** This process takes around 1 minute to run, and includes a somewhat stressful pause of at least 10s between `JumpToResetVector (start)` and `JumpToResetVector (success)` logs.  Be patient and **DO NOT INTERRUPT** this process.  It is unknown whether the SC will recover from an interrupted firmware download or whether it will become "bricked".

# Enable automatic SmartNIC application restart at boot

There are two possible approaches available to ensure correct startup of the SmartNIC application stacks on system boot.
1. Allow the `smartnic` service account to self-manage application stack startup on boot (Recommended)
   * Enables registration of persistent systemd units for execution on boot
   * Allows application operators to self-manage the startup state of their application stacks without coordination with sysadmins
2. Have `root` manage the systemd units to be executed on boot
   * Requires coordination with sysadmins whenever stacks should be temporarily disabled

**NOTE** One of these two options must be chosen, do not skip this step.  The `smartnic-stack-restart` on-boot job is essential to the correct operation of the SmartNIC application stack after a boot.  Docker does not (on its own) perform a properly sequenced startup of the containers within the SmartNIC application stack.  This job ensures startup is properly sequenced.

## Option # 1: Allow the `smartnic` service account to self-manage application stack startup on boot (Recommended)

Enable `linger` for the `smartnic` user to allow registration of persistent systemd user units which can run on boot
``` bash
sudo loginctl enable-linger smartnic
```

Confirm that the setting has been adjusted
``` bash
loginctl show-user smartnic --property=Linger
```
(should respond with `Linger=yes`)

Confirm that the user now has a persistent `systemd --user` daemon running along with a `dbus-daemon`
``` bash
sudo systemctl status --machine smartnic@ --user
```

With the `linger` option enabled, the application operators will be able to enable on-boot systemd jobs which will run as the `smartnic` user.
**NOTE** This is an example only and should not be done by `root` but rather by the application operators.  Shown here as general awareness in case this setup is not already well known.
``` bash
sudo su - smartnic
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
export DBUS_SESSION_BUS_ADDRESS="unix:path=${XDG_RUNTIME_DIR}/bus"

for card_slot in 1 2 4 5 ; do
  systemctl enable --user smartnic-stack-restart@$card_slot
done
```
(example for a system with four FPGA cards installed in physical slots 1,2,4 and 5 -- your physical slot numbers may differ)

Please communicate clearly to the application operators that they will be responsible for managing this on-boot systemd unit for each of the individual application stacks that they run (one per SmartNIC FPGA card)

## Option # 2: Have `root` manage the systemd units to be executed on boot

If the sysadmins have decided that stack auto-start at reboot should be exclusively managed by `root` on this system, the systemd on-boot jobs must be activated at the system level.

**WARNING** DO NOT run the commands in this section if you have already enabled the `linger` option for the `smartnic` service account in the previous section.  Either `root` or `smartnic` should manage the SmartNIC stack restarts on boot but *not both*.

Now we need the list of physical slots containing a Xilinx FPGA card from the info captured during card install.  For each slot number, activate a systemd unit to automatically restart the SmartNIC at system boot.

``` bash
for card_slot in 1 2 4 5 ; do
  sudo systemctl enable smartnic-stack-restart@$card_slot
done
```
(example for a system with four FPGA cards installed in physical slots 1,2,4 and 5 -- your physical slot numbers may differ)

Initially, there will not be any application installed under `/usr/local/smartnic/*` so these jobs won't be doing anything useful and might show up as failing.

# Check all the things before handing off to the smartnic application service owners

```
 ┌─────────────┬──────────────────────────────────────────┬───────────────────────────────────────────────────┬──────────────────────────────────────────────────────────────────────┐
 │  Category   │              Checklist Item              │               Verification Command                │                          Expected Result                             │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ OS          │ Ubuntu Server 22.04/24.04 installed      │ lsb_release -a                                    │ Release:	22.04                                                │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ Kernel      │ Kernel command line configured correctly │ cat /proc/cmdline                                 │                                                                      │
 │ Kernel      │ Hugepages enabled (32 x 1GB)             │ grep -i hugepages /proc/meminfo                   │ HugePages_Total:      32                                             │
 │ Kernel      │ IOMMU enabled                            │ sudo dmesg | grep -i -E "(DMAR|IOMMU)"            │                                                                      │
 │ Kernel      │ IOMMU working                            │ ls /sys/kernel/iommu_groups/ | wc -l              │ more than 2                                                          │
 │ Kernel      │ Grub config updated                      │ grep GRUB_CMDLINE_LINUX_DEFAULT /etc/default/grub │                                                                      │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ Docker      │ Docker installed                         │ sudo docker version                               │                                                                      │
 │ Docker      │ Docker Compose installed                 │ sudo docker compose version                       │                                                                      │
 │ Docker      │ Docker functional (as root)              │ sudo docker run --rm hello-world                  │ Hello from Docker!                                                   │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ User        │ smartnic user exists                     │ id smartnic                                       │ uid=xxx(smartnic) gid=xxx(smartnic) groups=xxx(smartnic),xxx(docker) │
 │ User        │ smartnic user in docker group            │ groups smartnic                                   │ smartnic : smartnic docker                                           │
 │ User        │ smartnic can run docker                  │ sudo -i -u smartnic docker run --rm hello-world   │ Hello from Docker!                                                   │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ Directories │ /usr/local/smartnic exists               │ test -d /usr/local/smartnic && echo OK            │ OK                                                                   │
 │ Directories │ Slot directories exist (0-9)             │ ls /usr/local/smartnic/                           │ 0  1  2  3  4  5  6  7  8  9                                         │
 │ Directories │ Ownership correct                        │ stat -c '%U:%G' /usr/local/smartnic/{0..9}        │ root:smartnic for each                                               │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ Packages    │ libboost-program-options installed       │ dpkg -l libboost-program-options1.74.0            │ ii  libboost-program-options1.74.0:amd64                             │
 │ Packages    │ smartnic-system-setup installed          │ dpkg -l smartnic-system-setup                     │ ii  smartnic-system-setup                                            │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ udev        │ SmartNIC udev rules installed            │ ls /usr/lib/udev/rules.d/*smartnic*               │ /usr/lib/udev/rules.d/90-smartnic-pci.rules                          │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ systemd     │ SmartNIC PCIe related units present      │ systemctl list-unit-files 'smartnic-pcie-*'       │ smartnic-pcie-err-disable@, smartnic-pcie-power-control-disable@     │
 │ systemd     │ Stack restart service enabled (per slot) │ systemctl is-enabled smartnic-stack-restart@<N>   │ enabled                                                              │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ PCIe        │ Xilinx FPGA cards detected               │ lspci -Dd 10ee:                                   │ 2 functions for each FPGA card at the expected PCIe addresses        │
 │ PCIe        │ Cards show as SmartNIC (post-flash)      │ lspci -Dd 10ee: | grep '9[0-3]3f$'                │ 2 functions for each FPGA card at the expected PCIe addresses        │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┤
 │ USB         │ JTAG adapters detected                   │ lsusb -d 0403:6011                                │ 1 FT4232H Quad HS USB-UART/FIFO IC per FPGA card                     │
 └─────────────┴──────────────────────────────────────────┴───────────────────────────────────────────────────┴──────────────────────────────────────────────────────────────────────┘
```

# Conclusion

Congratulations, you're all done commissioning this server to run ESnet SmartNIC applications.  After this point, you will no longer require `root` privileges to bring up the SmartNIC applications.

**NOTE** that even though `root` privileges aren't strictly required for running SmartNIC applications, it is still *very* useful for the application ops users to have `root` / `sudo` capabilities in order to debug any issues that might arise during service maintenance.

The users who are installing and managing the SmartNIC application will need a few facts that were collected during these steps:
* A list of FPGA cards
  * Physical Slot Number (from installation notes and ACPI/DMI Slot records)
  * Card Type
  * Card Serial Number
  * PCIe Bus Address (from lspci VPD output)
  * SC Firmware Version (from lspci VPD output)
* Path to TLS certificate and private key
* Instructions to either log in as the `smartnic` user directly or to be able to become that user via `sudo su - smartnic`

