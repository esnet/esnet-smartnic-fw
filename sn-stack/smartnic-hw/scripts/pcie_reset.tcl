if { $argc != 3 } {
    puts "Missing required arguments to this script."
    puts "Usage: vivado -mode batch -source pcie_reset.tcl -tclargs <hw_server_url> <hw_target_serial> <probes_path>"
    puts "Please try again."
    exit 1
}

set hw_server_url [lindex $argv 0]
set hw_target_serial [lindex $argv 1]
set probes_path [lindex $argv 2]

puts "Preparing to reset PCIe endpoint via VIO probes from: $probes_path"
open_hw_manager -verbose
connect_hw_server -url $hw_server_url -verbose
puts "Available Hardware Targets:"
foreach {target} [get_hw_targets] {
    puts "\t$target"
}
puts "Selecting Hardware Target Serial: $hw_target_serial"

current_hw_target -verbose [get_hw_targets -verbose [format "*/xilinx_tcf/Xilinx/%s" $hw_target_serial]]
puts "Selected Hardware Target: [current_hw_target]"
open_hw_target -verbose
puts "Selected Hardware Device: [current_hw_device]"

set_property PROBES.FILE $probes_path [current_hw_device]
refresh_hw_device [current_hw_device]
refresh_hw_vio [get_hw_vios hw_vio_1]

set __pcie_rstn_int [get_property INPUT_VALUE [get_hw_probes __pcie_rstn_int]]
set pcie_rstn_int [get_property INPUT_VALUE [get_hw_probes pcie_rstn_int]]
set jtag_rst [get_property OUTPUT_VALUE [get_hw_probes jtag_rst]]
puts "Initial state of VIOs:"
puts "    __pcie_rstn_int: $__pcie_rstn_int"
puts "    pcie_rstn_int:   $pcie_rstn_int"
puts "    jtag_rst:        $jtag_rst"

puts "Asserting reset of PCIe endpoint"
set_property OUTPUT_VALUE 1 [get_hw_probes jtag_rst]
commit_hw_vio [get_hw_vios hw_vio_1]
refresh_hw_vio [get_hw_vios hw_vio_1]

set __pcie_rstn_int [get_property INPUT_VALUE [get_hw_probes __pcie_rstn_int]]
set pcie_rstn_int [get_property INPUT_VALUE [get_hw_probes pcie_rstn_int]]
set jtag_rst [get_property OUTPUT_VALUE [get_hw_probes jtag_rst]]
puts "Asserted state of VIOs:"
puts "    __pcie_rstn_int: $__pcie_rstn_int"
puts "    pcie_rstn_int:   $pcie_rstn_int"
puts "    jtag_rst:        $jtag_rst"

puts "De-asserting reset of PCIe endpoint"
set_property OUTPUT_VALUE 0 [get_hw_probes jtag_rst]
commit_hw_vio [get_hw_vios hw_vio_1]
refresh_hw_vio [get_hw_vios hw_vio_1]

set __pcie_rstn_int [get_property INPUT_VALUE [get_hw_probes __pcie_rstn_int]]
set pcie_rstn_int [get_property INPUT_VALUE [get_hw_probes pcie_rstn_int]]
set jtag_rst [get_property OUTPUT_VALUE [get_hw_probes jtag_rst]]
puts "De-asserted state of VIOs:"
puts "    __pcie_rstn_int: $__pcie_rstn_int"
puts "    pcie_rstn_int:   $pcie_rstn_int"
puts "    jtag_rst:        $jtag_rst"

puts "Completed reset of PCIe endpoint"
