One-time Setup of the Runtime Environment
=========================================

The smartnic runtime environment also requires `docker` and `docker compose` as described in README.md at the top of this repo.

Set up Xilinx Labtools
----------------------

In order to load a smartnic FPGA bitfile into the Xilinx U280 card, we need to make use of the Xilinx Labtools.  The instructions for setting up labtools can be found in a separate repository (https://github.com/esnet/xilinx-labtools-docker).  That repository will provide us with a docker container populated with the xilinx labtools.  That docker container will be used to program the FPGA bitfile.

Configuring the firmware runtime environment
--------------------------------------------

The firmware artifact produced by the build (see README.md at the top of this repo) should be transferred to the runtime system that hosts an FPGA card.

```
unzip artifacts.esnet-smartnic-fw.package.0.zip
cd sn-stack
# edit the .env file to provide sane values for
#    FPGA_PCIE_DEV=0000:d8:00
#    COMPOSE_PROFILES=smartnic-mgr-vfio-unlock
# and IFF you have more than one JTAG you also need a line like this
#    HW_TARGET_SERIAL=21760204S029A
```

Verify that the stack configuration is valid

```
docker compose config --quiet && echo "All good!"
```

If this prints anything other than "All good!" then your `.env` configuration file has errors.  Do not proceed until this step passes.

Converting from factory flash image to ESnet Smartnic flash image
-----------------------------------------------------------------

From the factory, the FPGA cards have only a "gold" bitfile in flash with the "user" partition of flash being blank.  The "gold" bitfile has a narrow PCIe memory window for BAR1 and BAR2 which is insufficient for the ESnet Smartnic platform.  Fixing this requires a one-time flash programming step to install an ESnet Smartnic bitfile into the FPGA "user" partition in flash.  This initial setup is done using the JTAG.

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
  * The "gold" partition is left untouched

Clean up by bringing down the running stack after flash writing has completed.
```
docker compose down -v --remove-orphans
```

**Perform a cold-boot (power cycle) of the server hosting the FPGA card**

It is essential that this is a proper power cycle and not simply a warm reboot.  Specifically **do not** use `shutdown -r now` but rather use something like `ipmitool chassis power cycle`.  Failure to perform a cold-boot here will result in an unusable card.


Normal Operation of the Runtime Environment
===========================================

**NOTE** the steps in this major section are only expected to work once you've completed the initial setup in "One-time Setup of the Runtime Environment".

Running the firmware
--------------------

Start up the full firmware docker stack like this
```
docker compose up -d
```

Verifying the bitfile download
------------------------------

```
docker compose logs smartnic-hw
```

Inspecting registers and interacting with the firmware
------------------------------------------------------

The firmware runtime environment exists inside of the `smartnic-fw` container.  Here, we exec a shell inside of that container and have a look around.

```
docker compose exec smartnic-fw bash
sn-cfg show device info
regio-esnet-smartnic dump dev0.bar2.syscfg
```

Stopping the runtime environment
--------------------------------

When we're finished using the smartnic runtime environment, we can stop and remove our docker containers.

```
docker compose down -v
```

(OPTIONAL) Updating the flash image to a new ESnet Smartnic flash image
-----------------------------------------------------------------------

The instructions in this section are used to **update** the Smartnic flash image **from an already working** Smartnic environment.  This update step is *optional* and only required if you want to change the contents of the FPGA card flash.  Normally, the "RAM" of the FPGA is loaded using JTAG during stack startup.

**NOTE** This will not work for the very first time ever programming the flash.  See "Converting from factory flash image to ESnet Smartnic flash image" section above for first-time setup.

Start up a any properly configured stack which will allow us to write the flash using a fast algorithm over PCIe.

```
docker compose up -d
```

Confirm that PCIe register IO is working in your stack by querying the version registers.

```
docker compose exec smartnic-fw sn-cfg show device info
```
Confirm that the "DNA" register is **not** showing 0xfffff... as its contents.

Start the flash update service to write the currently active FPGA bitfile into the persistent flash on the FPGA card.  This takes approximately 7-8 minutes. This process should not be interrupted.
```
docker compose --profile smartnic-flash run --rm smartnic-flash-update
```

Bring down the running stack after flash writing has completed.
```
docker compose down -v --remove-orphans
```

(OPTIONAL) Remove the ESnet Smartnic flash image from the FPGA card to revert to factory image
----------------------------------------------------------------------------------------------

The instructions in this section are used to **remove** the Smartnic flash image **from an already working** Smartnic environment.  This removal step is *optional* and only required if you want to reset the contents of the FPGA card flash back to the factory bitfile.  If you want to keep using the card as an ESnet Smartnic, **do not** perform these operations or you'll have to re-do the  "Converting from factory flash image to ESnet Smartnic flash image" section above.

Start up a any properly configured stack which will allow us to write the flash using a fast algorithm over PCIe.

```
docker compose up -d
```

Confirm that PCIe register IO is working in your stack by querying the version registers.

```
docker compose exec smartnic-fw sn-cfg show device info
```
Confirm that the "DNA" register is **not** showing 0xfffff... as its contents.

Start the flash remove service to erase the ESnet Smartnic image from the "user" partition of the FPGA card flash.  This takes less than 1 minute. This process should not be interrupted.
```
docker compose --profile smartnic-flash run --rm smartnic-flash-remove
```

Bring down the running stack after flash reset is completed.
```
docker compose down -v --remove-orphans
```

Using the sn-cfg tool
=====================

The sn-cfg tool provides subcommands to help you accomplish many common tasks for inspecting and configuring the smartnic platform components.

All commands described below are expected to be executed within the `smartnic-fw` container environment.  Use this command to enter the appropriate environment.
```
docker compose exec smartnic-fw bash
```

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

The QDMA block is responsible for managing all DMA queues used for transferring packets and/or events bidirectionally between the U280 card and the Host CPU over the PCIe bus.  In order for any DMA transfers to be allowed on either of the PCIe Physical Functions (PF), an appropriate number of DMA Queue IDs must be provisioned.  This can be done using the `configure host` subcommand.

Configure the number of queues allocated to each of the PCIe Physical Functions
```
sn-cfg configure host --host-id 0 --dma-base-queue 0 --dma-num-queues 1
sn-cfg configure host --host-id 1 --dma-base-queue 1 --dma-num-queues 1
```
This assigns 1 QID to Host PF0 and 1 QID to Host PF1.  The `configure host` subcommand also takes care of configuring the RSS entropy -> QID map with an equal weighted distribution of all allocated queues.  If you're unsure of how many QIDs to allocate, using `--dma-num-queues 1` here is your best choice.

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

Configuring the smartnic platform ingress/egress/bypass switch port remapping functions with the "configure switch" subcommand
------------------------------------------------------------------------------------------------------------------------------

The smartnic platform implements reconfigurable ingress and egress port remapping, connections and redirecting.  You can inspect and modify these configuration points using the "configure switch" subcommand.

Most of the `configure switch` subcommands take one or more port bindings as parameters.  The port bindings are of the form:
```
<port>:<port-connector>
```
Where:
* `<port>` is one of
  * port0  -- 100G port 0
  * port1  -- 100G port 1
  * host0  -- DMA over PCIe Physical Function 0 (PF0)
  * host1  -- DMA over PCIe Physical Function 1 (PF1)
* `<port-connector>` is context dependent and is one of
  * port0
  * port1
  * host0
  * host1
  * bypass -- a high bandwidth channel through the smartnic which does **NOT** pass through the user's application
  * app0   -- user application port 0 (typically a p4 program ingress)
  * app1   -- user application port 1 (only available when user implements it in verilog)
  * drop   -- infinite blackhole that discards all packets sent to it

The following configuration settings provide you with a normal operating mode.  You almost certainly want to apply exactly these settings on startup.
```
# 1:1 Source port setting: packets all appear to be coming from their actual source ports, normal mode, no fakery (you want this)
sn-cfg configure switch -s host0:host0 \
                        -s host1:host1 \
			-s port0:port0 \
			-s port1:port1

# 1:1 Egress port setting: destination ports selected by the application pipeline are mapped normally, normal mode, no fakery (you want this)

# app0 = user's P4 program (you definitely have an app0) (you want this)
sn-cfg configure switch -e app0:host0:host0 \
                        -e app0:host1:host1 \
			-e app0:port0:port0 \
			-e app0:port1:port1
# app1 = user's custom RTL (you only have an app1 if you have written custom FPGA code as part of the HW build step) (you want this even if you don't have an app1 in your design, it's safe)
sn-cfg configure switch -e app1:host0:host0 \
                        -e app1:host1:host1 \
			-e app1:port0:port0 \
			-e app1:port1:port1

# Bypass path switching: when connected to the bypass, "wire" host0 <-> port0 and "wire" host1 <-> port1 (you probably want this, it only gets used if you connect ingress ports to bypass)
sn-cfg configure switch -e bypass:host0:port0 \
                        -e bypass:host1:port1 \
			-e bypass:port0:host0 \
			-e bypass:port1:host1

# Ingress port connectivity:
#   * connect 100G port0/1 directly into your P4 program so that your p4 program handles all 100G Rx packets
#   * connect host0/1 to the bypass path so host software sends directly out of each 100G port via the bypass (useful for injecting LLDP)
sn-cfg configure switch -i host0:bypass \
                        -i host1:bypass \
			-i port0:app0 \
			-i port1:app0
```

Skipping the p4 program and wiring the host ports to 100G ports for debugging
-----------------------------------------------------------------------------

This **optional** configuration snippet allows you to *entirely bypass the p4 program* contained in the smartnic and deliver all packets directly to the host software.  This is often useful for debug if you have reason to think your p4 program is mishandling the packets.

```
# Bypass path switching: when connected to the bypass, "wire" host0 <-> port0 and "wire" host1 <-> port1
sn-cfg configure switch -e bypass:host0:port0 \
                        -e bypass:host1:port1 \
			-e bypass:port0:host0 \
			-e bypass:port1:host1

# Ingress port connections: connect all ingress ports to the bypass path (THIS ENTIRELY SKIPS YOUR P4 PROGRAM and is only for debug/testing that packets are flowing)
sn-cfg configure switch -i host0:bypass \
                        -i host1:bypass \
			-i port0:bypass \
			-i port1:bypass
```

**NOTE** Don't forget to restore these settings after you're finished debugging.  Any packets that follow the bypass path will not be processed by the user's p4 program.

Display the current configuration status
----------------------------------------

```
sn-cfg show switch config
```

Using the sn-p4 tool
====================

The user's p4 application embedded within the smartnic design may have configurable lookup tables which are used during the wire-speed execution of the packet processing pipeline.  The sn-p4 tool provides subcommands to help you to manage the rules in all of the lookup tables defined in your p4 program.

All commands described below are expected to be executed within the `smartnic-fw` container environment.  Use this command to enter the appropriate environment.
```
docker compose exec smartnic-fw bash
```

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
root@smartnic-fw:/# sn-cfg configure host --host-id 0 --dma-base-queue 0 --dma-num-queues 1
root@smartnic-fw:/# sn-cfg configure host --host-id 1 --dma-base-queue 1 --dma-num-queues 1
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

A hardware application can optionally supply their own test suites and library. When the selected `sn-hw/artifacts.<board>.<app_name>.0.zip` archive contains a `robot-framework.tar.bz2` tarball, the tarball will be extracted into the `sn-stack/test/hw` directory for inclusion into the `sn-stack` build artifact. This process will only be automatic during a Gitlab CI build, but can be manually performed by executing the `build-setup-hw-test.sh` script prior to running the `build.sh` script to build the container.

A regular test run is started with:
```
cd sn-stack
./test/run.sh
```
Extra options can be passed to both the docker and robot command lines. Refer to `./test/run.sh --help` for details.

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

In both cases, any new Python package dependencies added to a `pip-requirements.txt` file can be installed into the container prior to executing the tests when the `--pip-install` (`-p`) option is passed to `./test/run.sh`. Note that this doesn't modify the container image, only the running instance. Adding new dependencies is only possible when network connectivity is available and the Python package index is accessible by the container.
