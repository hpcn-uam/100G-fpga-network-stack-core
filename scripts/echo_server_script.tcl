# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}

set_top echo_server_application

add_files ${root_folder}/hls/echo_replay/echo_server_application.cpp
add_files -tb ${root_folder}/hls/echo_replay/test_echo_server_application.cpp

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2


csynth_design

export_design -rtl verilog -format ip_catalog -display_name "echo server" -vendor "hpcn-uam.es" -version "1.0"

exit