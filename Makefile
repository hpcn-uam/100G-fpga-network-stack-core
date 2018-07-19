TOPDIR=$(shell pwd)
TOESCR=$(TOPDIR)/toe_src
IPERFSRC=$(TOPDIR)/iperf_client_src
ECHOSRC=$(TOPDIR)/echo_server_src
TCLDIR=$(TOPDIR)/scripts


project = TOE_hls_prj IPERF_hls_prj ECHOSERVER_hls_prj

all: build

build: $(project)
	@echo -e "\e[94mIP Completed: $(project)\e[39m"


clean:
	rm -rf *.log *.jou file* *.bak vivado*.str synlog.tcl .Xil fsm_encoding.os

distclean: clean
	rm -rf $(project)


TOE_hls_prj: $(shell find $(TOESCR) -type f) \
			 $(TCLDIR)/toe_script.tcl
	rm -rf 	TOE_hls_prj
	vivado_hls -f $(TCLDIR)/toe_script.tcl -tclargs $(project)

IPERF_hls_prj: $(shell find $(IPERFSRC) -type f) \
			 $(TCLDIR)/iperf_script.tcl
	rm -rf 	IPERF_hls_prj
	vivado_hls -f $(TCLDIR)/iperf_script.tcl -tclargs $(project)

ECHOSERVER_hls_prj: $(shell find $(IPERFSRC) -type f) \
			 $(TCLDIR)/echo_server_script.tcl
	rm -rf 	ECHOSERVER_hls_prj
	vivado_hls -f $(TCLDIR)/echo_server_script.tcl -tclargs $(project)

.PHONY: list help
list:
	@(make -rpn | sed -n -e '/^$$/ { n ; /^[^ .#][^% ]*:/p ; }' | sort | egrep --color '^[^ ]*:' )


help:
	@echo "The basic usage of this makefile is:"
	@echo -e " 1) Create the IPs"
	@echo -e "    \e[94mmake $(project)\e[39m"
	@echo ""
	@echo "Remember that you can always review this help with"
	@echo -e "    \e[94mmake help\e[39m"