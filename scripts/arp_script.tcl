# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}

set_top arp_server
add_files ${root_folder}/hls/arp_server/arp_server.cpp

add_files -tb ${root_folder}/hls/arp_server/arp_server_tb.cpp
add_files -tb ${root_folder}/hls/TOE/testbench/pcap.cpp
add_files -tb ${root_folder}/hls/TOE/testbench/pcap2stream.cpp

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

#config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog

exit