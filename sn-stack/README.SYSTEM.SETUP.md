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
 -------------------------------------------
| Card Type | Card Serial # | Physical Slot |
|-----------|---------------|---------------|
| U55C      | XFL1......2V  | Slot 1        |
| U55C      | XFL1......0U  | Slot 2        |
| U55C      | XFL1......Z0  | Slot 4        |
| U55C      | XFL1......TC  | Slot 5        |
 -------------------------------------------
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
```

**NOTE** By convention, the smartnic firmware stack for the card in server physical slot X should be deployed under `/usr/local/smartnic/X` to make it easy to match up the deployed firmware with the physical card it should apply to.

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
├── SC_U280_4_3_31.zip        <------------ (optional) Satellite Controller Firmware Image (au280)
├── SC_U55C_7_1_24.zip        <------------ (optional) Satellite Controller Firmware Image (au55c)
├── esnet-smartnic.au280.mcs  <------------ FPGA card flash image for au280 FPGA card
├── esnet-smartnic.au55c.mcs  <------------ FPGA card flash image for au55c FPGA card
├── loadsc                    <------------ (optional) Xilinx tool for upgrading Satellite Controller
├── smartnic-system-setup_0.1.0_all.deb <-- Debian package providing udev and systemd units for card management
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
sudo dpkg -i smartnic-system-setup_0.1.0_all.deb
```

# Convert FPGA cards from factory images to ESnet SmartNIC images

When the FPGA cards are delivered from the factory, they will be configured as generic FPGA cards.  This section will convert the FPGA cards into ESnet SmartNIC cards.

## Install prerequisites for the `xbflash2` tool

The `xbflash2` tool will be used to program the on-card flash storage of each FPGA card so that on power-up, it will be initialized as an ESnet SmartNIC.

This tool requires one or more packages on the host system as dependencies, install them with these commands
``` bash
sudo apt install --no-install-recommends libboost-program-options1.74.0
```

## Write the SmartNIC image onto the flash storage of each FPGA card

**NOTE** This step will take approx. 10 min per card if run using the loops below.  Running multiple xbflash2 processes on different cards in parallel is safe and can speed this up when you have multiple cards.

Find out what type of FPGA cards you have
``` bash
$ lspci -Dd 10ee: 
0000:21:00.0 Processing accelerators: Xilinx Corporation Device 505c
0000:22:00.0 Processing accelerators: Xilinx Corporation Device 505c
0000:81:00.0 Processing accelerators: Xilinx Corporation Device 505c
0000:82:00.0 Processing accelerators: Xilinx Corporation Device 505c
```
(example from a system with four Alveo au55c FPGA cards)

If your FPGA cards are au55C cards, they will have reported as `Xilinx Corporation Device 505c`.  Use these commands to program their flash.
``` bash
cd sn-bootstrap
for card_addr in $(lspci -Dd 10ee:505c | awk -F' ' '{ print $1 }') ; do
  printf "\n" | sudo ./xbflash2 program --spi --image esnet-smartnic.au55c.mcs --bar-offset 0x1f06000 -d $card_addr
done
```

If your FPGA cards are au280 cards, they will have reported as `Xilinx Corporation Device 500c`.  Use these commands to program their flash.
``` bash
cd sn-bootstrap
for card_addr in $(lspci -Dd 10ee:500c | awk -F' ' '{ print $1 }') ; do
  printf "\n" | sudo ./xbflash2 program --spi --image esnet-smartnic.au280.mcs --bar-offset 0x40000 -d $card_addr
done
```

## Cold boot the system

After flash programming is completed, cold boot the system to allow the FPGAs to load their configuration from flash.  This must be a proper cold boot (eg. `ipmitool chassis power cycle`) and **not** an ordinarly warm boot (eg. `shutdown -r now`) or the FPGAs will not reconfigure from flash.

``` bash
sudo ipmitool chassis power cycle
```

**DO NOT SKIP THIS STEP**

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
			[SN] Serial number: XFL1......2V
			[RV] Reserved: checksum good, 0 byte(s) reserved
		End
0000:22:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[SN] Serial number: XFL1......0U
			[RV] Reserved: checksum good, 0 byte(s) reserved
		End
0000:81:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[SN] Serial number: XFL1......Z0
			[RV] Reserved: checksum good, 0 byte(s) reserved
		End
0000:82:00.0 Network controller: Xilinx Corporation Device 903f
	Capabilities: [b0] Vital Product Data
		Product Name: ESnet SmartNIC
		Read-only fields:
			[SN] Serial number: XFL1......TC
			[RV] Reserved: checksum good, 0 byte(s) reserved
		End
```
(example from a system with four Alveo au55c FPGA cards -- your bus addresses may differ)

**NOTE** Record the relationship between the PCIe Bus Address and the VPD [SN] Serial number field.  This information will be needed by the SmartNIC Application Operators.

## Verify that the udev glue and systemd units are starting without error for all SmartNIC FPGA cards

Display the current status of all `smartnic-*` systemd units since boot.
``` bash
sudo systemctl status --all 'smartnic-*'
```

Display the full time-interleaved logs of all `smartnic-*` systemd units since boot
``` bash
sudo journalctl --output=short-iso-precise -u 'smartnic-*' --boot
```

# Enable automatic SmartNIC application restart at boot

Now we need the list of physical slots containing a Xilinx FPGA card from the info captured during card install.  For each slot number, activate a systemd unit to automatically restart the SmartNIC at system boot.

**NOTE** This on-boot job is essential to the correct operation of the SmartNIC application stack after a boot.  Docker does not (on its own) perform a properly sequenced startup of the containers within the SmartNIC application stack.  This job ensures startup is properly sequenced.

``` bash
for card_slot in 1 2 4 5 ; do
  sudo systemctl enable smartnic-stack-restart@$card_slot
done
```
(example for a system with four FPGA cards installed in physical slots 1,2,4 and 5 -- your physical slot numbers may differ)

Initially, there will not be any application installed under `/usr/local/smartnic/*` so these jobs won't be doing anything useful and might show up as failing.  These jobs should still be enabled during commissioning since they require root privs and will be required by the SmartNIC Application Operators.

# Check all the things before handing off to the smartnic application service owners

```
 ┌─────────────┬──────────────────────────────────────────┬───────────────────────────────────────────────────┐
 │  Category   │              Checklist Item              │               Verification Command                │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ OS          │ Ubuntu Server 22.04/24.04 installed      │ lsb_release -a                                    │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Kernel      │ Kernel command line configured correctly │ cat /proc/cmdline                                 │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Kernel      │ Hugepages enabled (32 x 1GB)             │ grep -i hugepages /proc/meminfo                   │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Kernel      │ IOMMU enabled                            │ dmesg | grep -i -E "(DMAR|IOMMU)"                 │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Kernel      │ Grub config updated                      │ grep GRUB_CMDLINE_LINUX_DEFAULT /etc/default/grub │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Docker      │ Docker installed                         │ docker version                                    │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Docker      │ Docker Compose installed                 │ docker compose version                            │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Docker      │ Docker functional                        │ docker run --rm hello-world                       │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ User        │ smartnic user exists                     │ id smartnic                                       │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ User        │ smartnic user in docker group            │ groups smartnic | grep docker                     │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ User        │ smartnic can run docker                  │ sudo -u smartnic docker ps                        │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Directories │ /usr/local/smartnic exists               │ test -d /usr/local/smartnic && echo OK            │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Directories │ Slot directories exist (0-9)             │ ls -la /usr/local/smartnic/                       │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Directories │ Ownership correct                        │ stat -c '%U:%G' /usr/local/smartnic/{0..9}        │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Packages    │ libboost-program-options installed       │ dpkg -l libboost-program-options1.74.0            │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Packages    │ xbflash2 installed                       │ which xbflash2                                    │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ Packages    │ smartnic-system-setup installed          │ dpkg -l smartnic-system-setup                     │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ udev        │ SmartNIC udev rules installed            │ ls /etc/udev/rules.d/*smartnic*                   │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ systemd     │ PCIe error disable service exists        │ systemctl cat smartnic-pcie-err-disable@.service  │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ systemd     │ Stack restart service enabled (per slot) │ systemctl is-enabled smartnic-stack-restart@<N>   │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ PCIe        │ Xilinx FPGA cards detected               │ lspci -Dd 10ee:                                   │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ PCIe        │ Cards show as SmartNIC (post-flash)      │ lspci -Dd 10ee:903f -Dd 10ee:913f                 │
 ├─────────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ USB         │ JTAG adapters detected                   │ lsusb | grep -i xilinx                            │
 └─────────────┴──────────────────────────────────────────┴───────────────────────────────────────────────────┘
 ```

# Conclusion

Congratulations, you're all done commissioning this server to run ESnet SmartNIC applications.  After this point, you will no longer require `root` privileges to bring up the SmartNIC applications.

**NOTE** that even though `root` privileges aren't strictly required for running SmartNIC applications, it is still *very* useful for the application ops users to have `root` / `sudo` capabilities in order to debug any issues that might arise during service maintenance.

The users who are installing and managing the SmartNIC application will need a few facts that were collected during these steps:
* A list of FPGA cards
  * Physical Slot Number
  * PCIe Bus Address (from lspci VPD output)
  * Card Type
  * Card Serial Number
* Instructions to either log in as the `smartnic` user directly or to be able to become that user via `sudo su - smartnic`

