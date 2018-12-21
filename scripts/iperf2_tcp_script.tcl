open_project IPERF2_TCP_hls_prj

set_top iperf2_client

add_files hls/iperf2_tcp/iperf_client.cpp
add_files hls/TOE/common_utilities/common_utilities.cpp

add_files -tb hls/iperf2_tcp/test_iperf_client.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog -display_name "iperf2 client" -vendor "hpcn-uam.es" -version "1.0"

exit