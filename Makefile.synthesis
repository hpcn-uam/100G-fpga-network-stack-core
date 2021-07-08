TOPDIR=$(shell pwd)/../
TOESCR=$(TOPDIR)/hls/TOE
IPERFSRC=$(TOPDIR)/hls/iperf2_tcp
ECHOSRC=$(TOPDIR)/hls/echo_replay
ARPSRC=$(TOPDIR)/hls/arp_server
ETHSRC=$(TOPDIR)/hls/ethernet_inserter
ICMPSRC=$(TOPDIR)/hls/icmp_server
PKTSRC=$(TOPDIR)/hls/packet_handler
USRSRC=$(TOPDIR)/hls/user_abstraction
UDPSRC=$(TOPDIR)/hls/UDP
PORTSRC=$(TOPDIR)/hls/port_handler
TCLDIR=$(TOPDIR)/scripts


FPGAPART ?= xcu250-figd2104-2L-e

project = ARP_hls_prj ETH_inserter_hls_prj ICMP_hls_prj PKT_HANDLER_prj UDP_prj


all: build

build: $(project)
	@echo -e "\e[94mIP Completed: $(project)\e[39m"

clean:
	rm -rf *.log *.jou file* *.bak vivado*.str synlog.tcl .Xil fsm_encoding.os

distclean: clean
	rm -rf $(project)


TOE_hls_prj: $(shell find $(TOESCR) -type f) \
			 $(TCLDIR)/toe_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/toe_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

IPERF2_TCP_hls_prj: $(shell find $(IPERFSRC) -type f) \
			 $(TCLDIR)/iperf2_tcp_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/iperf2_tcp_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

ECHOSERVER_hls_prj: $(shell find $(ECHOSRC) -type f) $(TCLDIR)/echo_server_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/echo_server_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

ARP_hls_prj: $(shell find $(ARPSRC) -type f) $(TCLDIR)/arp_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/arp_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

ETH_inserter_hls_prj: $(shell find $(ETHSRC) -type f) $(TCLDIR)/ethernet_inserter_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/ethernet_inserter_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

ICMP_hls_prj: $(shell find $(ICMPSRC) -type f) $(TCLDIR)/icmp_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/icmp_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

PKT_HANDLER_prj: $(shell find $(PKTSRC) -type f) $(TCLDIR)/packet_handler_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/packet_handler_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

userAbstraction_prj: $(shell find $(USRSRC) -type f) $(TCLDIR)/userAbstraction.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/userAbstraction.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

portHandler_prj: $(shell find $(PORTSRC) -type f) $(TCLDIR)/portHandler.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/portHandler.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

UDP_prj: $(shell find $(UDPSRC) -type f) $(TCLDIR)/udp_script.tcl
	rm -rf $@
	vitis_hls -f $(TCLDIR)/udp_script.tcl -tclargs $(TOPDIR) $@ $(FPGAPART)

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