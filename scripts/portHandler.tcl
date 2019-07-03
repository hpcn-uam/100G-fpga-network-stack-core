open_project portHandler_prj

set_top port_handler
add_files hls/port_handler/port_handler.cpp


add_files -tb hls/port_handler/port_handler_tb.cpp -cflags ""
add_files -tb hls/TOE/testbench/pcap.cpp -cflags ""
add_files -tb hls/TOE/testbench/pcap2stream.cpp -cflags ""

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog

exit