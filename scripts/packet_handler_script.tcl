open_project PKT_HANDLER_prj

set_top packet_handler

add_files hls/packet_handler/packet_handler.cpp
add_files -tb hls/packet_handler/test_packet_hanlder.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

csynth_design

export_design -rtl verilog -format ip_catalog
exit