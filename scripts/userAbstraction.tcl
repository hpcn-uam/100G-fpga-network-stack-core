open_project userAbstraction_prj

set_top user_abstraction
add_files hls/user_abstraction/user_abstraction.cpp
add_files hls/TOE/common_utilities/common_utilities.cpp
add_files -tb hls/user_abstraction/test_user_abstraction.cpp -cflags ""

open_solution "ultrascale_plus"
set_part {xcvu9p-flga2104-2l-e} -tool vivado
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

config_rtl -disable_start_propagation
csynth_design
export_design -rtl verilog -format ip_catalog

exit