# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}


set_top iperf2_client

add_files ${root_folder}/hls/iperf2_tcp/iperf_client.cpp
add_files ${root_folder}/hls/TOE/common_utilities/common_utilities.cpp

add_files -tb ${root_folder}/hls/iperf2_tcp/test_iperf_client.cpp

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

#config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog -display_name "iperf2 client" -vendor "hpcn-uam.es" -version "1.0"

exit