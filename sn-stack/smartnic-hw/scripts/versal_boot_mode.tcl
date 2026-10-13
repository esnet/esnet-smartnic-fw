if { $argc != 3 } {
    puts "Missing required arguments to this script."
    puts "Usage: xsdb versal_boot_mode.tcl <hw_server_url> <boot_mode> <reload>"
    puts "    hw_server_url: tcp:localhost:3121"
    puts "    boot_mode:     JTAG | FLASH"
    puts "    reload:        yes | no"
    puts "Please try again."
    exit 1
}

set hw_server_url [lindex $argv 0]
set boot_mode [lindex $argv 1]
set reload [lindex $argv 2]

puts "Hardware server URL: $hw_server_url"
puts "Boot mode:           $boot_mode"
puts "Reload:              $reload"

# Connect to the HW server that provides JTAG access.
connect -url tcp:$hw_server_url
targets -set -filter {name =~ "Versal *"}
puts "\nTargets: [targets]"

# Dump the current state of the JTAG registers.
puts "\nJTAG registers:"
foreach reg {extended_idcode usercode dna jtag_status error_status} {
    puts "----------------------------------------\n[device status -hex $reg]"
}

# Dump the current state of relevant boot mode registers.
puts "\nBoot mode registers (before):"
puts "  crp rst_ps: [rrd crp rst_ps]"
puts "  crp boot_mode_por: [rrd crp boot_mode_por]"
puts "  crp boot_mode_user: [rrd crp boot_mode_user]"
puts "  pmc_global pmc_multi_boot: [rrd pmc_global pmc_multi_boot]"

# Change the boot mode.
switch $boot_mode {
    "JTAG" {
        # Set JTAG boot mode.
        rwr crp boot_mode_user 0x0100
        rwr pmc_global pmc_multi_boot 0
    }

    "FLASH" {
        # Set OSPI FLASH boot mode and select AVED partition 1.
        rwr crp boot_mode_user 0x8100
        rwr pmc_global pmc_multi_boot 0xfc1
    }

    default {
        puts "ERROR: Unknown boot mode '$boot_mode'."
        exit 1
    }
}

# Dump the updated state of relevant boot mode registers.
puts "\nBoot mode registers (after):"
puts "  crp boot_mode_user: [rrd crp boot_mode_user]"
puts "  pmc_global pmc_multi_boot: [rrd pmc_global pmc_multi_boot]"

# Trigger the SoC boot processor to reload.
if { $reload == "yes" } {
    targets -set -filter {name =~ "PMC"}
    rst
    puts "Reloaded SoC boot processor."
}
