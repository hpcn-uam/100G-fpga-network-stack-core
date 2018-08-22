open_project ICMP_hls_prj

set_top icmp_server

add_files hls/icmp_server/icmp_server.cpp
add_files hls/TOE/common_utilities/common_utilities.cpp
add_files -tb hls/icmp_server/test_icmp_server.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

csynth_design

export_design -rtl verilog -format ip_catalog
exit