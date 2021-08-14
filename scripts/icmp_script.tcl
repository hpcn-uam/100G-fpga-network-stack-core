# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}

set_top icmp_server

add_files ${root_folder}/hls/icmp_server/icmp_server.cpp
add_files -tb ${root_folder}/hls/icmp_server/test_icmp_server.cpp -cflags "-Ihls/TOE/testbench/."
add_files -tb ${root_folder}/hls/TOE/testbench/pcap2stream.cpp -cflags "-Ihls/TOE/testbench/."
add_files -tb ${root_folder}/hls/TOE/testbench/pcap.cpp -cflags "-Ihls/TOE/testbench/."
add_files -tb ${root_folder}/hls/icmp_server/icmp.pcap
add_files -tb ${root_folder}/hls/icmp_server/icmp_golden.pcap

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2
#csim_design -argv {icmp.pcap icmp_golden.pcap}

csynth_design

export_design -rtl verilog -format ip_catalog
exit