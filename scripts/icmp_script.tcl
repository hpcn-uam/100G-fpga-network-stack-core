open_project ICMP_hls_prj

set_top icmp_server

set root_dir [exec pwd]

add_files hls/icmp_server/icmp_server.cpp
#add_files hls/TOE/common_utilities/common_utilities.cpp
add_files -tb hls/icmp_server/test_icmp_server.cpp -cflags "-Ihls/TOE/testbench/."
add_files -tb hls/TOE/testbench/pcap2stream.cpp -cflags "-Ihls/TOE/testbench/."
add_files -tb hls/TOE/testbench/pcap.cpp -cflags "-Ihls/TOE/testbench/."

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2
#csim_design -argv "${root_dir}/hls/icmp_server/icmp.pcap ${root_dir}/hls/icmp_server/icmp_golden.pcap"

csynth_design

export_design -rtl verilog -format ip_catalog
exit