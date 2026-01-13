if { $argc != 3 } {
    puts "Missing required arguments to this script."
    puts "Usage: vivado -mode batch -source program_card.tcl -tclargs <hw_server_url> <hw_target_serial> <bitfile_path>"
    puts "Please try again."
    exit 1
}

set hw_server_url [lindex $argv 0]
set hw_target_serial [lindex $argv 1]
set bitfile_path [lindex $argv 2]

puts "About to flash the following bitfile: $bitfile_path"
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

# Note: FPGA configuration works at the high speed but VIO/ILAs may not
puts "Increasing JTAG clock frequency to 30MHz (max)"
set_property PARAM.FREQUENCY 30000000 [current_hw_target]

set_property PROGRAM.FILE $bitfile_path [current_hw_device]
program_hw_device [current_hw_device]

# TODO this also seems to work at 15MHz so maybe worth bumping this up in the future after retesting
puts "Decreasing JTAG clock frequency back down to 10MHz so that VIO/ILA access works reliably"
set_property PARAM.FREQUENCY 10000000 [current_hw_target]
