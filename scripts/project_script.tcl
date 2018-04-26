open_project toe_hls

set_top toe

add_files src/ack_delay/ack_delay.cpp
add_files src/close_timer/close_timer.cpp
add_files src/event_engine/event_engine.cpp
add_files src/port_table/port_table.cpp
add_files src/probe_timer/probe_timer.cpp
add_files src/retransmit_timer/retransmit_timer.cpp
add_files src/rx_app_if/rx_app_if.cpp
add_files src/rx_app_stream_if/rx_app_stream_if.cpp
add_files src/rx_engine/rx_engine.cpp
add_files src/rx_sar_table/rx_sar_table.cpp
add_files src/session_lookup_controller/session_lookup_controller.cpp
add_files src/state_table/state_table.cpp
add_files src/toe.cpp
add_files src/utilities.cpp
add_files src/tx_app_if/tx_app_if.cpp
add_files src/tx_app_interface/tx_app_interface.cpp
add_files src/tx_app_stream_if/tx_app_stream_if.cpp
add_files src/tx_engine/tx_engine.cpp
add_files src/tx_sar_table/tx_sar_table.cpp
add_files -tb src/testbench/dummy_memory.cpp
add_files -tb src/testbench/pcap.cpp
add_files -tb src/testbench/pcap2stream.cpp
add_files -tb src/testbench/test_toe.cpp

open_solution "solution1"
set_part {xcvu095-ffva2104-2-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2
csynth_design

export_design -rtl verilog -format ip_catalog
exit