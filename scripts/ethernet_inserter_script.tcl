open_project ETH_inserter_hls_prj

set_top ethernet_header_inserter

add_files hls/ethernet_inserter/ethernet_header_inserter.cpp
add_files -tb hls/ethernet_inserter/ethernet_header_inserter_test.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

csynth_design
export_design -rtl verilog -format ip_catalog
exit