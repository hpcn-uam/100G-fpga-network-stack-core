open_project ECHOSERVER_hls_prj

set_top echo_server_application

add_files echo_server_src/echo_server_application.cpp
add_files -tb echo_server_src/test_echo_server_application.cpp

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2


csynth_design

export_design -rtl verilog -format ip_catalog -display_name "echo server" -vendor "hpcn-uam.es" -version "1.0"

exit