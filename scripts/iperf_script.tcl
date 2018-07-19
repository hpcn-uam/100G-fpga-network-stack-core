open_project IPERF_hls_prj

set_top iperf2_client

add_files iperf_client_src/iperf_client.cpp
add_files toe_src/common_utilities/common_utilities.cpp

add_files -tb iperf_client_src/test_iperf_client.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

csynth_design
export_design -rtl verilog -format ip_catalog -display_name "iperf2 client" -vendor "hpcn-uam.es" -version "1.0"

exit