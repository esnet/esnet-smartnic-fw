if { $argc != 1 } {
    puts "Missing required arguments to this script."
    puts "Usage: xsdb versal_pcie_reset.tcl <hw_server_url>"
    puts "    hw_server_url: tcp:localhost:3121"
    puts "Please try again."
    exit 1
}

set PCIE_DELAY_MS  100
set hw_server_url [lindex $argv 0]

puts "Hardware server URL: $hw_server_url"

# Connect to the HW server that provides JTAG access.
connect -url tcp:$hw_server_url
targets -set -filter {name =~ "Versal *"}
puts "\nTargets: [targets]"

# Dump the current state of the JTAG registers.
puts "\nJTAG registers:"
foreach reg {extended_idcode usercode dna jtag_status error_status} {
    puts "----------------------------------------\n[device status -hex $reg]"
}

set USR_ACCESS_ADRR    0xf2014168
set JTAG_USERCODE_ADRR 0xf11a0008

puts "----------------------------------------"
puts "PLM_RTCA USR_ACCESS   => [mrd $USR_ACCESS_ADRR]"
puts "PMC_JTAG_CSR USERCODE => [mrd $JTAG_USERCODE_ADRR]"
puts "                      => [rrd pmc_tap]"
puts "----------------------------------------"

puts "\nAsserting reset of PCIe controllers."
rwr cpm5_crx rst_pcie0 1
rwr cpm5_crx rst_pcie1 1

# Wait for resets to propagate.
after $PCIE_DELAY_MS

puts "De-asserting reset of PCIe controllers."
rwr cpm5_crx rst_pcie0 0
rwr cpm5_crx rst_pcie1 0

# Wait for resets to clear.
after $PCIE_DELAY_MS
puts "Completed reset of PCIe controllers"
