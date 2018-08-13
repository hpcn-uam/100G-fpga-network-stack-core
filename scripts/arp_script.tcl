open_project ARP_hls_prj

set_top arp_server
add_files hls/arp_server/arp_server.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

csynth_design
export_design -rtl verilog -format ip_catalog

exit