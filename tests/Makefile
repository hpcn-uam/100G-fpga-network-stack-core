IVERILOG ?= iverilog
VVP ?= vvp

BUILD_DIR := build
TESTBENCH := size_checker_backpressure_tb

.PHONY: all clean
all: size_checker_backpressure.vcd

$(BUILD_DIR)/$(TESTBENCH): $(TESTBENCH).sv ../RTL/size_checker.v
	mkdir -p $(BUILD_DIR)
	$(IVERILOG) -g2012 -s $(TESTBENCH) -o $@ $^

size_checker_backpressure.vcd: $(BUILD_DIR)/$(TESTBENCH)
	$(VVP) $<

clean:
	rm -rf $(BUILD_DIR) size_checker_backpressure.vcd
