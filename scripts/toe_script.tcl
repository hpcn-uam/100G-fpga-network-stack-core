# Get the root folder
set root_folder [lindex $argv 2]
# Get project name from the arguments
set proj_name [lindex $argv 3]
# Get FPGA part
set fpga_part [lindex $argv 4]
# Create project
open_project ${proj_name}

set_top toe

add_files ${root_folder}/hls/TOE/ack_delay/ack_delay.cpp
add_files ${root_folder}/hls/TOE/close_timer/close_timer.cpp
add_files ${root_folder}/hls/TOE/event_engine/event_engine.cpp
add_files ${root_folder}/hls/TOE/port_table/port_table.cpp
add_files ${root_folder}/hls/TOE/probe_timer/probe_timer.cpp
add_files ${root_folder}/hls/TOE/retransmit_timer/retransmit_timer.cpp
add_files ${root_folder}/hls/TOE/rx_app_stream_if/rx_app_stream_if.cpp
add_files ${root_folder}/hls/TOE/rx_engine/rx_engine.cpp
add_files ${root_folder}/hls/TOE/rx_sar_table/rx_sar_table.cpp
add_files ${root_folder}/hls/TOE/session_lookup_controller/session_lookup_controller.cpp
add_files ${root_folder}/hls/TOE/state_table/state_table.cpp
add_files ${root_folder}/hls/TOE/toe.cpp
add_files ${root_folder}/hls/TOE/common_utilities/common_utilities.cpp
add_files ${root_folder}/hls/TOE/memory_access/memory_access.cpp
add_files ${root_folder}/hls/TOE/tx_app_if/tx_app_if.cpp
add_files ${root_folder}/hls/TOE/tx_app_interface/tx_app_interface.cpp
add_files ${root_folder}/hls/TOE/tx_app_stream_if/tx_app_stream_if.cpp
add_files ${root_folder}/hls/TOE/tx_engine/tx_engine.cpp
add_files ${root_folder}/hls/TOE/tx_sar_table/tx_sar_table.cpp
add_files ${root_folder}/hls/TOE/statistics/statistics.cpp

add_files -tb ${root_folder}/hls/iperf2_tcp/iperf_client.cpp
add_files -tb ${root_folder}/hls/echo_replay/echo_server_application.cpp
add_files -tb ${root_folder}/hls/TOE/testbench/dummy_memory.cpp
add_files -tb ${root_folder}/hls/TOE/testbench/pcap.cpp
add_files -tb ${root_folder}/hls/TOE/testbench/pcap2stream.cpp
add_files -tb ${root_folder}/hls/TOE/testbench/test_toe.cpp

open_solution "ultrascale_plus"
set_part ${fpga_part}
create_clock -period 2.5 -name default
set_clock_uncertainty 0.2

config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog
#
exit