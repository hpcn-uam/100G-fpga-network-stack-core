# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}

set_top ethernet_header_inserter

add_files ${root_folder}/hls/ethernet_inserter/ethernet_header_inserter.cpp
add_files -tb ${root_folder}/hls/ethernet_inserter/ethernet_header_inserter_test.cpp

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

csynth_design
export_design -rtl verilog -format ip_catalog
exit