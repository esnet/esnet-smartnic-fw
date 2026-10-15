if { $argc != 2 } {
    puts "Missing required arguments to this script."
    puts "Usage: vivado -mode batch -source get_hw_device.tcl -tclargs <hw_server_url> <hw_target_serial>"
    puts "Please try again."
    exit 1
}

set hw_server_url [lindex $argv 0]
set hw_target_serial [lindex $argv 1]

set_msg_config -severity INFO -suppress
open_hw_manager
connect_hw_server -url $hw_server_url
current_hw_target [get_hw_targets [format "*/xilinx_tcf/Xilinx/%s" $hw_target_serial]]
open_hw_target
puts "[current_hw_device]"
