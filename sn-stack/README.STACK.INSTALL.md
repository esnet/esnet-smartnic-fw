One-time Setup of the Host Server
=================================

Server setup
------------
All of the steps for setting up a server to host one or more ESnet SmartNIC application stacks is contained in the `README.SYSTEM.SETUP.md` file found next to this file in the `esnet-smartnic-fw` repository.

The steps itemized in that document are typically performed by system adminstrators prior to handing off the system to the users operating the SmartNIC application stack.  Please ensure that all steps from that document are completed before proceeding.

You should have received a table of information from your sysadmins providing this collection of information about the ESnet SmartNIC FPGA cards installed and commissioned in your system.

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

You will need the information from the provided table when setting up the application stacks in later sections of this document.

Set up Xilinx Labtools
----------------------

In order to load a smartnic FPGA bitfile into the Xilinx U280 card, we need to make use of the Xilinx Labtools.  The instructions for setting up labtools can be found in a separate repository (https://github.com/esnet/xilinx-labtools-docker).  That repository will provide us with a docker container populated with the xilinx labtools.  That docker container will be used to program the FPGA bitfile.

**NOTE** This container build should be executed as the `smartnic` user on your server.  This will allow the container image to be located using the defaults within the SmartNIC application stack.  If you build it differently, or use non-default tags for the image, you will need to adjust the `.env` file in each SmartNIC container stack.

**NOTE** Once built, the `xilinx-labtools-docker` container should not be published outside of your organization as it contains proprietary tools that you have downloaded from AMD/Xilinx.  Publishing it to a **private** docker registry within your organization may be helpful if you will be deploying to multiple servers hosting SmartNIC FPGA cards.

Installing the ESnet SmartNIC application stack(s)
--------------------------------------------------

The steps from the `README.SYSTEM.SETUP.md` file will have pre-created a directory hierarchy under `/usr/local/smartnic/{0..9}`.  The numbers in that path are associated with the physical slot containing the related SmartNIC FPGA card.  Keeping the paths related to the physical slot numbers of the FPGA cards helps when diagnosing any issues on the server or on the network.  This document will assume that you have followed this recommendation.

You should have received (or built for yourself) a firmware artifact zip file which contains the SmartNIC application stack which will provision and manage a single SmartNIC FPGA card.  This file will be named in one of these forms, depending on how it was built:
* Typical self-built filename: `artifacts.esnet-smartnic-fw.package.0.zip`
* Typical production release filename: `artifacts.esnet-smartnic-fw.[au280|au55c].ejfat.package.<build#>.zip`

Extract this file under each of the applicable numbered directories under `/usr/local/smartnic`, matching the physical slot #'s holding your SmartNIC FPGA cards.
``` bash
cd /usr/local/smartnic/1
unzip ~/artifacts.esnet-smartnic-fw.au55c.ejfat.package.999999.zip
```
(example of extracting an au55c FPGA build of the ejfat application build # 999999 for a SmartNIC FPGA card located in physical slot #1)

You should end up with a tree that looks something like this for each SmartNIC FPGA card physical slot:
``` bash
$ tree -L 2 -d /usr/local/smartnic/1
/usr/local/smartnic/1
└── sn-stack
    ├── certs
    ├── debs
    ├── prometheus
    ├── scratch
    ├── smartnic-hw
    ├── smartnic-lldp
    ├── smartnic-vfio-unlock
    ├── test
    └── traefik
```
(abbreviated output, only top-level directories shown here for physical slot #1)

Configuring the firmware runtime environment
--------------------------------------------

Each of the SmartNIC application stacks requires configuration values to be set before you can use the firmware.

Your release zip file will typically contain a partially pre-populated `sn-stack/.env` file.  If present, this `.env` file should be used as the basis of your configuration.

If **no** `.env` file is present, you can create one by taking a copy of the always provided `example.env` file.
``` bash
cd /usr/local/smartnic/1/sn-stack
cp --no-clobber example.env .env
```

**NOTE** The `.env` file must be customized for each of the SmartNIC FPGA stacks individually.  If your system has more than one SmartNIC FPGA card installed, you will need to repeat this process with relevant settings for each card.  Refer to the information collected during the `README.SYSTEM.SETUP.md` workflow to assist with the required per-card customization.

**NOTE** The `sn-stack/example.env` file within the application zip file is the official reference for the available settings for a given release.  Please review this file, paying close attention to any differences between your previous running `.env` file and the new `example.env` file during upgrades as options may have been added, changed or deprecated in the new version.

The settings in the `.env` file configure and customize the following aspects of the application stack:
* Binding between PCIe Bus Address (`FPGA_PCIE_DEV=`) and FPGA Card Serial Number (`HW_TARGET_SERIAL=`)
  * Serial Number is used to identify the USB JTAG device used to program the FPGA
  * PCIe Bus Address is used to identify the PCIe device from the OS perspective
* Physical Slot Number (`UNIQUE=`)
  * Used to bind each SmartNIC FPGA stack to a unique TCP port when remote access is enabled
  * Used to identify this card's position within the server over LLDP
* Application Profile Options (`COMPOSE_PROFILE=`)
  * Selects which application stack components are started up
  * A full-featured stack includes
    * `smartnic-mgr-dpdk-portlink` which enables transmit of LLDP packets when the 100G ports are enabled
    * `smartnic-ingress` which enables a TLS-enabled reverse proxy allowing remote access to metrics and control plane APIs
    * `smartnic-metrics` which enables a Prometheus exporter allowing access to application metrics
* TLS paths for certificate and private key
  * This provides the TLS reverse proxy with a valid, trusted TLS certificate
* Authentication tokens for control plane APIs
  * All control plane APIs require authentication using bearer tokens
  * These tokens must be configured or the application stack will not start

Once you have reviewed and configured the settings in your `.env` file, verify that the stack configuration is valid
```
docker compose config --quiet && echo "All good!"
```

If this prints anything other than "All good!" then your `.env` configuration file has errors.  Do not proceed until this step passes.

Starting and stopping the SmartNIC application stack
====================================================

Automatically (re)starting the SmartNIC application stack at boot
-----------------------------------------------------------------

The SmartNIC appication stack requires special attention at server boot time.  This is required due to the various docker containers in the stack requiring a coordinated startup sequence.  In a default docker installation, the startup sequence after boot is disorderly.  To correct for this, a systemd unit (`smartnic-stack-restart@.service`) must be activated for each of the SmartNIC FPGA cards in the system.  As the name implies, the smartnic-stack-restart unit brings the stack down and then starts all the containers in the correct order.

There are two approaches to which user will run these stack restart jobs at boot.  See `README.SYSTEM.SETUP.md` section "Enable automatic SmartNIC application restart at boot", and coordinate with your system administrator about which method you will be using.

**ONLY IF** you are using the recommended option to have the `smartnic` user self-manage the application startup at boot (Option #1 from the other document), then you should activate a user-level systemd unit to restart each stack.
``` bash
sudo su - smartnic
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
export DBUS_SESSION_BUS_ADDRESS="unix:path=${XDG_RUNTIME_DIR}/bus"

for card_slot in 1 2 4 5 ; do
  systemctl enable --user smartnic-stack-restart@$card_slot
done
```
(example for a system with four FPGA cards installed in physical slots 1,2,4 and 5 -- your physical slot numbers may differ)


Running the SmartNIC application stack manually
-----------------------------------------------

Start up the full firmware docker stack using the `up` subcommand.
``` bash
sudo su - smartnic
for card_slot in 1 2 4 5 ; do
  docker compose --project-directory /usr/local/smartnic/$card_slot/sn-stack up -d
done
```
(example for a system with four FPGA cards installed in physical slots 1,2,4 and 5 -- your physical slot numbers may differ)

Inspecting registers and interacting with the firmware
------------------------------------------------------

The firmware runtime interactive environment exists inside of the `smartnic-fw` container.  Here, we `exec` a shell inside of that container and have a look around.

``` bash
sudo su - smartnic
cd /usr/local/smartnic/1/sn-stack
docker compose exec smartnic-fw bash
sn-cfg show device info
regio-esnet-smartnic dump dev0.bar2.syscfg
exit
```

Stopping the runtime environment
--------------------------------

When we're finished using the smartnic runtime environment, we can stop and remove our docker containers using the `down` subcommand.
``` bash
sudo su - smartnic
for card_slot in 1 2 4 5 ; do
  docker compose --project-directory /usr/local/smartnic/$card_slot/sn-stack down -v
done
docker compose down -v
```
(example for a system with four FPGA cards installed in physical slots 1,2,4 and 5 -- your physical slot numbers may differ)

Normal Operation of the Runtime Environment
===========================================

**NOTE** the steps in remainder of this document are only expected to work once you've completed the initial setup in "One-time Setup of the Runtime Environment" and started the SmartNIC application stack for each FPGA card.

**NOTE** if your SmartNIC application comes with its own control plane application, you will typically only need to start the application stack (see previous section).  The control plane application will typically take care of all remaining run-time configuration and monitoring.  Some of the commands documented in this section may be helpful for diagnosing any run-time issues you might encounter, so it's worthwhile becoming familiar with them even if you have a full control plane.

Using the sn-cfg tool
=====================

The sn-cfg tool provides subcommands to help you accomplish many common tasks for inspecting and configuring the smartnic platform components.

All commands described below are expected to be executed as the within the `smartnic-fw` container environment in the context of a single FPGA card (shown as `$CARD_SLOT` below).  Use this command sequence to enter the appropriate environment.
```
sudo su - smartnic
cd /usr/local/smartnic/$CARD_SLOT/sn-stack
docker compose exec smartnic-fw bash
```
(Substitute the appropriate `$CARD_SLOT` value from 0..9 for the FPGA card of interest)

Displaying device information with the "show device" subcommand
---------------------------------------------------------------

This will show information about the device such as the build version, build date/time, and serial number.

```
root@smartnic-fw:/# sn-cfg show device info
----------------------------------------
Device ID: 0
----------------------------------------
PCI:
    Bus ID:    0000:d8:00.0
    Vendor ID: 0x10ee
    Device ID: 0x903f
Build:
    Number: 0x0000e417
    Status: 0x10140810
    DNA[0]: 0x2c9082c5
    DNA[1]: 0x013b83c1
    DNA[2]: 0x40020000
Card:
    Name:                  ALVEO U280 PQ
    Profile:               U280
    Serial Number:         21770323Y043
    Revision:              1.0
    SC Version:            4.3.31
    Config Mode:           MASTER_SPI_X4
    Fan Presence:          P
    Total Power Available: 225W
    Cage Types:
    MAC Addresses:
        0: 00:0A:35:xx:xx:xx
        1: 00:0A:35:xx:xx:xx
```
The `Build Number` value is typically the unique build pipeline number that produced the embedded FPGA bitfile.
The `Build Status` value holds an encoded date/time (Oct 14 at 08:10am) which is when the embedded FPGA bitfile build was started.
The `Build DNA[]` value holds the factory programmed unique ID of the FPGA

```
root@smartnic-fw:/# sn-cfg show device status
----------------------------------------
Device ID: 0
----------------------------------------
Device Alarms:
    card:
        power_good: yes
Device Monitors:
    card:
             _12v_aux_avg: 12.11
        _12v_aux_i_in_avg: 1.396
        _12v_aux_i_in_ins: 1.385
        _12v_aux_i_in_max: 1.422
             _12v_aux_ins: 12.13
             _12v_aux_max: 12.14
             _12v_pex_avg: 12.06
             _12v_pex_ins: 12.09
             _12v_pex_max: 12.09
              _12v_sw_avg: 12.09
              _12v_sw_ins: 12.11
              _12v_sw_max: 12.12
[...]
               vccint_avg: 0.85
             vccint_i_avg: 13.75
             vccint_i_ins: 13.93
             vccint_i_max: 14.02
               vccint_ins: 0.851
               vccint_max: 0.851
    sysmon0:
        max_temperature: 58.98
             max_vccaux: 1.799
            max_vccbram: 0.8555
             max_vccint: 0.8496
        min_temperature: 52.51
             min_vccaux: 1.784
            min_vccbram: 0.8438
             min_vccint: 0.8408
            temperature: 55.5
                 vccaux: 1.79
                vccbram: 0.8496
                 vccint: 0.8467
                  vp_vn: 0
    sysmon1:
        max_temperature: 57.99
[...]
                 vccint: 0.8467
                 vuser0: 1.198
    sysmon2:
        max_temperature: 56.49
[...]
                 vccint: 0.8467
                 vuser0: 0
```

Inspecting and Configuring the CMAC (100G) Ports with the "show/configure port" subcommands
-------------------------------------------------------------------------------------------

Show the configuration (administrative state) of the 100G ports

```
root@smartnic-fw:/# sn-cfg show port config
----------------------------------------
Port ID: 0 on device ID 0
----------------------------------------
State:    enable
FEC:      none
Loopback: none
----------------------------------------
Port ID: 1 on device ID 0
----------------------------------------
State:    enable
FEC:      none
Loopback: none
```

Show the current operational state of the 100G ports

```
root@smartnic-fw:/# sn-cfg show port status
----------------------------------------
Port ID: 0 on device ID 0
----------------------------------------
Link: up
----------------------------------------
Port ID: 1 on device ID 0
----------------------------------------
Link: up
```

Show the current (non-zero) port statistics

```
root@smartnic-fw:/# sn-cfg show port stats
----------------------------------------
Port ID: 0 on device ID 0
----------------------------------------
        rx_broadcast: 402956        
        rx_multicast: 2581941       
     rx_pkt_64_bytes: 86200514      
 rx_pkt_65_127_bytes: 50704644337   
rx_pkt_128_255_bytes: 344660255002  
rx_pkt_256_511_bytes: 41321         
      rx_total_bytes: 51083551553232
 rx_total_good_bytes: 51083551553232
  rx_total_good_pkts: 395451141174  
       rx_total_pkts: 395451141174  
          rx_unicast: 395448156277  
             rx_vlan: 395451099853  
        tx_multicast: 61085         
tx_pkt_128_255_bytes: 61085         
      tx_total_bytes: 8368645       
 tx_total_good_bytes: 8368645       
  tx_total_good_pkts: 61085         
       tx_total_pkts: 61085         
----------------------------------------
Port ID: 1 on device ID 0
----------------------------------------
        tx_multicast: 61085  
tx_pkt_128_255_bytes: 61085  
      tx_total_bytes: 8368645
 tx_total_good_bytes: 8368645
  tx_total_good_pkts: 61085  
       tx_total_pkts: 61085  
```

Enable/Disable one or more (or all by default) 100G MAC interfaces using these commands:

```
sn-cfg configure port --state enable
sn-cfg configure port --state disable

sn-cfg configure port --port-id 0 --state enable
sn-cfg configure port --port-id 1 --state disable
```
Enabling a CMAC interface allows frames to pass (Rx/Tx) at the MAC layer.  These commands **do not affect** whether the underlying physical layer (PHY) is operational.

Enable/Disable Reed-Solomon Forward Error Correction (RS-FEC) on one or more (or all by default) 100G MAC interfaces using these commands:

```
sn-cfg configure port --fec reed-solomon
sn-cfg configure port --fec none

sn-cfg configure port --port-id 0 --fec reed-solomon
sn-cfg configure port --port-id 1 --fec none
```

**NOTE** The RS-FEC setting must be configured identically on **both ends of the physical link** or the link will not be established.

Display the current PHY status of one or more (or all by default) 100G MAC interfaces using these commands:
```
root@smartnic-fw:/# sn-cfg show port status
----------------------------------------
Port ID: 0 on device ID 0
----------------------------------------
Link: up
----------------------------------------
Port ID: 1 on device ID 0
----------------------------------------
Link: up

root@smartnic-fw:/# sn-cfg show port config
----------------------------------------
Port ID: 0 on device ID 0
----------------------------------------
State:    enable
FEC:      none
Loopback: none
----------------------------------------
Port ID: 1 on device ID 0
----------------------------------------
State:    enable
FEC:      none
Loopback: none

```

In the example output above, Port0 and Port1 PHY layers are **up**.  The MAC is enabled, RS-FEC is disabled.  This link is operational and should be passing packets normally.

If the port shows `Link: down`.  Possible causes for this are:
* No QSFP28 plugged into 100G the corresponding port the U280 card
* Wrong type of QSFP28 module plugged into 100G port
  * 100G QSFP28 SR4 or LR4 modules are supported
  * Some 100G AOC or DACs are known to work
  * QSFP+ 40G modules **are not supported**
  * QSFP 5G modules **are not supported**
* QSFP28 module improperly seated in the U280 card
  * Check if the QSFP28 module is inserted upside down and physically blocked from being fully inserted
  * Unplug/replug the module, ensuring that it is properly oriented and firmly seated
* Fiber not properly inserted
  * Unplug/replug the fiber connection at each end
* Far end is operating in 4x25G or 2x50G split mode
  * The smartnic platform **does not support** 4x25G or 2x50G mode
  * Only 100G mode is supported on each of the U280 100G interfaces
  * Configure far end in 100G mode
* Far end has RS-FEC (Reed-Solomon Forward Error Correction) enabled
  * The smartnic platform supports RS-FEC but it is disabled by default
  * Use the `sn-cfg configure port --fec reed-solomon` command to manualy enable RS-FEC on the CMAC ports
  * Alternatively, disable RS-FEC on the far end equipment

More detailed status can also be queried directly from the QSFP module using the `sn-cfg show module status` command.  This command will show signal levels as well as any active alarms indicated by the QSFP module.

Inspecting and Configuring the Host PCIe Queue DMA (QDMA) block with the "show/configure host" subcommands
----------------------------------------------------------------------------------------------------------

The QDMA block is responsible for managing all DMA queues used for transferring packets and/or events bidirectionally between the U280 card and the Host CPU over the PCIe bus.  In order for any DMA transfers to be allowed on either of the PCIe Physical Functions (PF), an appropriate number of DMA Queue IDs within a contiguous set must be provisioned.

As an intermediate development step towards providing future access to PCIe Virtual Functions (VF) via SR-IOV, the DMA channel associated with each PF has been further divided into subchannels.  A DMA subchannel consists of a range within the contiguous set of queue IDs allocated to the parent PF.  There are currently four DMA subchannels, one for the PF itself and the remaining three for the future VFs (named VF0, VF1 and VF2).  Each of the subchannels has a unique purpose within the smartnic datapath and are tied into it at differing points.  Please refer to the datapath diagrams provided in the FPGA repository (https://github.com/esnet/esnet-smartnic-hw/tree/main/docs) for details.

Configuration of the DMA queues for any subchannel can be done using the `configure host` subcommand as follows:
```
sn-cfg configure host --host-id 0 --reset-dma-queues --dma-queues pf:0:1
sn-cfg configure host --host-id 1 --reset-dma-queues --dma-queues pf:1:1
```
This assigns 1 QID to the PF subchannel of host PF0 and 1 QID to the PF subchannel of host PF1.  Any prior settings for all other subchannels are reset.  The `configure host` subcommand also takes care of configuring the RSS entropy -> QID map with an equal weighted distribution of all allocated queues.  If you're unsure of how many QIDs to allocate, using `<subchannel>:<base-queue>:1` here is your best choice.

Queues can be allocated to any combination of subchannels by prefixing the number of queues with the subchannel name (one of `pf`, `vf0`, `vf1` or `vf2`), a colon `:` and then finally the number of queues.  For example, assigning queues to both the PF and VF2 subchannels can be done as follows (using abbreviated option syntax):
```
sn-cfg configure host -i 0 -r -d pf:0:1 -d vf2:1:1
sn-cfg configure host -i 1 -r -d pf:2:1 -d vf2:3:1
```
This assigns 2 QIDs to each host PF and distributes them equally between the PF and VF2 subchannels.


Inspect the configuration of the QDMA block
```
sn-cfg show host config
```

Packet, byte and error counters are tracked for packets heading between the QDMA engine and the user application.  You can display them with this command:
```
sn-cfg show host stats
```
Refer to the `open-nic-shell` documentation for an explanation of exactly where in the FPGA design these statistics are measured.


Inspecting packet counters in the smartnic platform with the "show switch stats" subcommand
-------------------------------------------------------------------------------------------

The smartnic platform implements monitoring points in the datapath at various locations.  You an inspect these counters using this command:
```
sn-cfg show switch stats
```
Refer to the `esnet-smartnic-hw` documentation for an explanation of exactly where in the FPGA design these statistics are measured.

Configuring packet ingress/egress for the smartnic platform with the "configure switch" subcommand
--------------------------------------------------------------------------------------------------

The smartnic platform supports a simple switch for steering packets into the user application and back out.  You can modify this configuration using the `configure switch` subcommand.

An ingress selector is used to specify which interface to receive packets from and where to direct them for processing.  The selector is a triplet of the form <port-index>:<interface>:<destination> (see below).

An egress selector is used to specify which interface to transmit packets to after they've been processed.  The selector is a pair of the form <port-index>:<interface> (see below).

The port index in the selectors represent user application ports, one for each physical port supported by the FPGA card's hardware.

Interfaces are how packets are received and transmitted.  Each port supported by the user application can be independently mapped to an interface.  The supported interfaces are:
* physical: A 100G pluggable module.
* test: A DMA channel over PCIe to host software.

A processing destination specifies the path ingressing packets should take.  The available destinations are:
* app: User application processes each packet and decides what to do with it.  Processing occurs in a chain as follows:
  * Ingress P4 program.
  * Ingress custom RTL.
  * Egress custom RTL.
  * Egress P4 program.
* bypass: Simple wire path (no processing involved).  Packets are simply queued as-is for egress.
* drop: All packets are discarded (no ingress).

When using the `bypass` destination, an extra configuration option is available to specify how the ingress ports map to the egress ports.  This option does nothing unless at least one of the ingress selectors has been set to the `bypass` destination. The supported bypass modes are:
* straight: This is a one-to-one mapping of ingress and egress ports, such that:
  * ingress-port0 --> egress-port0
  * ingress-port1 --> egress-port1
* swap: This reverses the one-to-one mapping of ingress and egress ports, such that:
  * ingress-port0 --> egress-port1
  * ingress-port1 --> egress-port0


The following configuration settings provide you with a normal operating mode.  You almost certainly want to apply exactly these settings on startup.
```
# All egress packets are transmitted by the 100G pluggable module associated with each port.
sn-cfg configure switch -e 0:physical \
                        -e 1:physical

# Receive packets from the 100G pluggable module associated with each port and direct them to the user application for processing.
sn-cfg configure switch -i 0:physical:app \
			-i 1:physical:app \
                        -b straight
```

**NOTE** Don't forget to restore these settings after you're finished debugging.  Any packets that follow the bypass path will not be processed by the user application.

Display the current configuration status
----------------------------------------

```
sn-cfg show switch config
```

Using the sn-p4 tool
====================

The user's p4 application embedded within the smartnic design may have configurable lookup tables which are used during the wire-speed execution of the packet processing pipeline.  The sn-p4 tool provides subcommands to help you to manage the rules in all of the lookup tables defined in your p4 program.

All commands described below are expected to be executed within the `smartnic-fw` container environment in the context of a single FPGA card (shown as `$CARD_SLOT` below).  Use this command sequence to enter the appropriate environment.
```
sudo su - smartnic
cd /usr/local/smartnic/$CARD_SLOT/sn-stack
docker compose exec smartnic-fw bash
```
(Substitute the appropriate `$CARD_SLOT` value from 0..9 for the FPGA card of interest)

The `sn-p4` tool will automatically look for an environment variable called `SN_P4_CLI_ADDRESS` which can be set to the hostname or IP address of the `smartnic-p4` container that will perform all of the requested actions on the real hardware.  In the `smartnic-fw` container, this value will already be set for you.

By default, all operations performed by the `sn-p4` tool will target the ingress pipeline. A different pipeline can be operated on by using the `--pipeline-id` option to select an alternate. Use the `--help` option to see which pipelines are supported.

Inspecting the pipeline structure with the "show pipeline info" subcommand
--------------------------------------------------------------------------

The `show pipeline info` subcommand is used to display the pipeline structure, including table names, match fields (and their types), action names and the list of parameters for each action, as well as all counter blocks defined by the p4 program.  This information can be used to formulate new rule definitions for the other subcommands.

```
sn-p4 show pipeline info
```

Inserting a new rule into a table
---------------------------------

The `insert table rule` subcommand allows you to insert a new rule into a specified table.

```
sn-p4 insert table rule --table <table-name> --action <action-name> --match <match-expr> [--param <param-expr>] [--priority <prio-val>]
```
Where:
* `<table-name>` is the name of the table to be operated on
* `<action-name>` is the action that you would like to activate when this rule matches
* `<match-expr>` is a list of one or more space-separated match expressions which collectively define when this rule should match a given packet
  * The number and type of the match fields depends on the p4 definition of the table
  * When the table supports multiple matches, the entire list must be quoted to ensure it's passed to the option as a single value
  * The format of each match expression is one of:
    * For a "key and mask" type match, the format is `<key>&&&<mask>`, where `<key>` is a bit pattern to match, `<mask>` is the mask to apply prior to matching and `&&&` is the separator.
    * For a "longest prefix" type match, the format is `<key>/<prefix_length>`, where `<key>` is a bit pattern to match, `<prefix_length>` is the decimal number of upper bits (most significant) to use for building the mask to apply and `/` is the separator.
    * For a "range" type match, the format is `<lower>-><upper>`, where `<lower>` is the decimal start of the range, `<upper>` is the decimal top of the range and `->` is the separator.
    * Bit patterns used for specifying the key and mask fields have one of the following forms:
      * Hexadecimal: String of hex digits in the character set `[0-9a-fA-F]` prefixed with `0x`.
      * Binary: String of binary digits in the character set `[01]` prefixed with `0b`.
      * Decimal: String of decimal digits in the character set `[0-9]`.
* `<param-expr>` is a list of one or more space-separated parameter values which will be returned as a result when this rule matches a given packet
  * The number and type of the action parameters depends on the p4 definition of the action within the table
  * Some actions require zero parameters.  In this case, omit the optional `--param` option entirely.
  * When the action supports multiple parameters, the entire list must be quoted to ensure it's passed to the option as a single value
* `<prio-val>` is the priority to be used to resolve scenarios where multiple matches could occur
  * The `--priority` option is *required* for tables with CAM/TCAM type matches (prefix/range/ternary)
    * Refer to the output of the 'show pipeline info` subcommand to determine whether the table requires a priority.

**NOTE**: You can find details about your pipeline structure and valid names by running the `show pipeline info` subcommand.

Updating an existing rule within a table
----------------------------------------

The `insert table rule` subcommand described above also allows you to update the action and parameters for an existing rule within a table by specifying the `--replace` option.
```
sn-p4 insert table rule --replace --table <table-name> --action <action-name> --match <match-expr> [--param <param-expr>]
```

Removing previously inserted rules
----------------------------------

The `clear table` and `delete table rule` subcommands allow you to remove rules from tables with varying precision.

Clear all rules from *all tables* in the pipeline.
```
sn-p4 clear table
```

Clear all rules from a *single* specified table.
```
sn-p4 clear table --table <table-name>
```

Remove a specific rule from a specific table.
```
sn-p4 delete table rule --table <table-name> --match <match-expr>
```

Bulk changes of rules using a p4 behaviour model (p4bm) simulator rules file
----------------------------------------------------------------------------

Using the the `apply p4bm` subcommand, a list of pipeline modifications can be applied from a file.  A subset of the full p4bm simulator file format is supported by the `sn-p4` command.

```
sn-p4 apply p4bm <filename>
```

Supported actions within the p4bm file are:
* `table_add <table-name> <action-name> <match-expr> => <param-expr> [priority]`
  * Insert a rule
* `clear_all`
  * Clear all rules from all tables
* `table_clear <table-name>`
  * Clear all rules from a specified table

All comment characters `#` and text following them up to the end of the line are ignored.

Displaying counters defined by a p4 program
-------------------------------------------

If a p4 program defines one or more blocks of counters, their type and number of counters within each block can be found in the output of the `show pipeline info` subcommand.  Counters are automatically accumulated by `sn-p4-agent` and made available via the `show pipeline stats` subcommand.
```
sn-p4 show pipeline stats
```
**NOTE**: By default, the `show pipeline stats` subcommand suppresses 0 values in the output. To view all counters regardless of value, include the `--zeroes` option.

The accumulated value for all counters can be cleared with the `clear pipeline stats` subcommand.
```
sn-p4 clear pipeline stats
```

The `sn-p4-agent` server also acts as a Prometheus exporter, making all p4 counters available via HTTP on port `SN_P4_SERVER_PROMETHEUS_PORT` (default is 8000) for scraping and inclusion by a Prometheus data visualization tool such as [Grafana](https://grafana.com/oss/grafana/).
```
curl -s http://smartnic-p4:8000/metrics
```

Using the smartnic-dpdk container
=================================

The `sn-stack` environment can be started in a mode where the FPGA can be controlled by a DPDK application.  Running in this mode requires a few carefully ordered steps.

**NOTE** Running the SmartNIC stack in this mode requires specific knowledge of how a custom DPDK application will interact with the FPGA PCIe memory as well as the steps to bind the FPGA device to a kernel driver.  This is not the typical way that the SmartNIC stack is used.  This section is for advanced users who would like to take control of many of the low-level infrastructure details within the SmartNIC stack and is typically only applicable for users who are writing their own custom application.  If none of that applies to you, this section should be ignored and you will be better off running the stack in a different mode.

Broadly speaking, the steps required to bring up a DPDK application are as follows:
* Bind the `vfio-pci` kernel driver to each FPGA PCIe physical function (PF)
  * This is handled automatically by the sn-stack.
* Run a DPDK application with appropriate DPDK Environment Abstraction Layer (EAL) settings
  * Use `-a $SN_PCIE_DEV.0` to allow control of one or more specific FPGA PCIe PFs
  * Use `-d librte_net_qdma.so` to dynamically link the correct Userspace Polled-Mode Driver (PMD) for the smartnic QDMA engine
  * The EAL will
    * Open the PCIe PFs using the kernel's `vfio-pci` driver
	* Take the FPGA device out of reset
	* Open and map large memory regions for DMA using the kernel's `hugepages` driver
  * The application is responsible for assigning buffers to one or more of the FPGA's DMA queues
* Use the `sn-cfg` tool to configure some of the low-level hardware components in the FPGA
  * Configure the set of valid DMA queues in the FPGA (must match what is set in the DPDK application)
  * Bring up the physical ethernet ports

In the examples below, we will be running the `pktgen-dpdk` application to control packet tx/rx via the FPGA's PCIe physical functions.  This can be very useful for injecting packets into a design for testing behaviour on real hardware.

For more information about DPDK in general, see:
* http://core.dpdk.org/doc/

For more information about the `pktgen-dpdk` application, see:
* https://pktgen-dpdk.readthedocs.io/en/latest/index.html

Before you bring up the `sn-stack`, please ensure that you have uncommented this line in your `.env` file
```
COMPOSE_PROFILES=smartnic-mgr-dpdk-manual
```

If you changed this while the stack was already running, you'll need to restart the stack with:
```
docker compose down -v --remove-orphans
docker compose up -d
```

First, you'll need to start up the `pktgen` application to open the vfio-pci device for PF0 and PF1 and take the FPGA out of reset.
```
$ docker compose exec smartnic-dpdk bash
root@smartnic-dpdk:/# pktgen -a $SN_PCIE_DEV.0 -a $SN_PCIE_DEV.1 -l 4-8 -n 4 -d librte_net_qdma.so --file-prefix $SN_PCIE_DEV- -- -v -m [5:6].0 -m [7:8].1
Pktgen:/> help
```
NOTE: Leave this application running while doing the remaining setup steps.  The setup steps below must be re-run after each time you restart the pktgen application since the FPGA gets reset between runs.

Open a **separate** shell window which you will use for doing the low-level smartnic platform configuration.

Configure the Queue mappings for host PF0 and PF1 interfaces and bring up the physical ethernet ports using the `smartnic-fw` container.

```
$ docker compose exec smartnic-fw bash
root@smartnic-fw:/# sn-cfg configure host --host-id 0 --reset-dma-queues --dma-queues pf:0:1
root@smartnic-fw:/# sn-cfg configure host --host-id 1 --reset-dma-queues --dma-queues pf:1:1
root@smartnic-fw:/# sn-cfg show host config
root@smartnic-fw:/# sn-cfg configure port --state enable
root@smartnic-fw:/# sn-cfg show port status
```
Setting up the queue mappings tells the smartnic platform which QDMA queues to use for h2c and c2h packets.  Enabling the CMACs allows Rx and Tx packets to flow (look for `Link: up`).

Advanced usage of the pktgen-dpdk application
---------------------------------------------

Example of streaming packets out of an interface from a pcap file rather than generating the packets within the UI.
Note the `-s <P>:file.pcap` option where `P` refers to the port number to bind the pcap file to.

```
root@smartnic-dpdk:/# pktgen -a $SN_PCIE_DEV.0 -a $SN_PCIE_DEV.1 -l 4-8 -n 4 -d librte_net_qdma.so --file-prefix $SN_PCIE_DEV- -- -v -m [5:6].0 -m [7:8].1 -s 1:your_custom.pcap
Pktgen:/> port 1
Pktgen:/> page pcap
Pktgen:/> page main
Pktgen:/> start 1
Pktgen:/> stop 1
Pktgen:/> clr
```

Example of running a particular test case via a script rather than typing at the UI

```
cat <<_EOF > /tmp/test.pkt
clr
set 1 size 1400
set 1 count 1000000
enable 0 capture
start 1
disable 0 capture
_EOF
```

```
root@smartnic-dpdk:/# pktgen -a $SN_PCIE_DEV.0 -a SN_PCIE_DEV.1 -l 4-8 -n 4 -d librte_net_qdma.so --file-prefix $SN_PCIE_DEV- -- -v -m [5:6].0 -m [7:8].1 -f /tmp/test.pkt
```

Test Automation Framework
=========================
The `smartnic-fw-test` service incorporates the [Robot Framework](https://robotframework.org/robotframework/latest/RobotFrameworkUserGuide.html) as an engine for executing test suites. The service mounts the `sn-stack/test/fw` directory into the container as `/test/fw`. The `suites` and `library` sub-directories contain the tests to be executed and the Python classes implementing the tests, respectively.

**NOTE** This section is for advanced users, typically ones who have written their own custom SmartNIC appications.  It describes methods for hooking into the Test Automation Framework used by SmartNIC developers.  If this does not apply to your situation, this section should be skipped.

A hardware application can optionally supply their own test suites and library. When the selected `sn-hw/artifacts.<board>.<app_name>.0.zip` archive contains a `robot-framework.tar.bz2` tarball, the tarball will be extracted into the `sn-stack/test/hw` directory for inclusion into the `sn-stack` build artifact. This process will only be automatic during a Gitlab CI build, but can be manually performed by executing the `build-setup-hw-test.sh` script prior to running the `build.sh` script to build the container.

A regular test run is started with:
```
cd sn-stack
./run-tests.sh
```
Extra options can be passed to both the docker and robot command lines. Refer to `./run-tests.sh --help` for details.

A test run will produce three output files, located in `sn-stack/scratch`.
- `report.html`: Summarizes a test execution run and provides links into the verbose `log.html` file.
- `log.html`: Details the order of test execution and captures all logged output.
- `output.xml`: Robot Framework's native XML format. This file is the canonical output file from which all other formats are generated by the `rebot` tool.

Developing tests for automation
-------------------------------
There are two primary methods for developing tests:
1. Direct.

    With the direct method, the test source files within the `sn-stack` directory installed during deployment are modified in-place. While sufficient for making quick changes, this method is not recommended for longer term test development as it suffers from the possibility of losing all changes upon a re-deployment.

1. External.

    With the external method, a source tree containing test files is mounted into the container in place of the installed ones. This method is recommended for longer term test development as it allows keeping the sources separate and mitigates the danger of unexpected removals/overwrites.

    This method makes use of extra environment variables to point the container startup at alternate paths. They are:
    - `SN_FW_TEST_ROOT`: Path to alternate firmware test files to be mounted at `/test/fw` within the container.
    - `SN_HW_TEST_ROOT`: Path to alternate hardware test files to be mounted at `/test/hw` within the container.
    - `SN_APP_TEST_ROOT`: Path to alternate application test files to be mounted at `/test/app` within the container.

In both cases, any new Python package dependencies added to a `pip-requirements.txt` file can be installed into the container prior to executing the tests when the `--pip-install` (`-p`) option is passed to `./run-tests.sh`. Note that this doesn't modify the container image, only the running instance. Adding new dependencies is only possible when network connectivity is available and the Python package index is accessible by the container.


Rescuing a "missing" FPGA card using JTAG to program the card flash
===================================================================

**NOTE** If your FPGA card is present on the PCIe Bus (`lspci -Dd 10ee:`) this method is not applicable and you should instead use the sequence documented in `README.SYSTEM.SETUP.md` and program the flash using the `xbflash2` tool.  The sequence described in this document should only be used if your FPGA card is **NOT PRESENT** on the PCIe Bus.

Ensure that any running `sn-stack` instances have been stopped so that they don't interfere with the flash programming operation.
```
docker compose down -v --remove-orphans
```

Start the flash rescue service to program an ESnet Smartnic bitfile into the FPGA card "user" partition using the JTAG interface.  This takes approximately 20 minutes.  This process should not be interrupted.
```
docker compose --profile smartnic-flash run --rm smartnic-flash-rescue
```
This will:
* Use JTAG to write a small flash-programing helper bitfile into the FPGA
* Use JTAG to write the current version of the bitfile into the FPGA card's "user" partition in flash
  * Only the "user" partition of the flash is overwritten by this step
  * The "factory" / "golden" partition is left untouched

Clean up by bringing down the running stack after flash writing has completed.
```
docker compose down -v --remove-orphans
```

**Perform a cold-boot (power cycle) of the server hosting the FPGA card**

It is essential that this is a proper power cycle and not simply a warm reboot.  Specifically **do not** use `shutdown -r now` but rather use something like `ipmitool chassis power cycle`.  Failure to perform a cold-boot here will result in an unusable card.

