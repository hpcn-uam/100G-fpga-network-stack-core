# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}


set_top port_handler
add_files ${root_folder}/hls/port_handler/port_handler.cpp


add_files -tb ${root_folder}/hls/port_handler/port_handler_tb.cpp -cflags ""
add_files -tb ${root_folder}/hls/TOE/testbench/pcap.cpp -cflags ""
add_files -tb ${root_folder}/hls/TOE/testbench/pcap2stream.cpp -cflags ""

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog

exit