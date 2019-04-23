TOPDIR=$(shell pwd)
TOESCR=$(TOPDIR)/hls/TOE
IPERFSRC=$(TOPDIR)/hls/iperf2_tcp
ECHOSRC=$(TOPDIR)/hls/echo_replay
ARPSRC=$(TOPDIR)/hls/arp_server
ETHSRC=$(TOPDIR)/hls/ethernet_inserter
ICMPSRC=$(TOPDIR)/hls/icmp_server
PKTSRC=$(TOPDIR)/hls/packet_handler
TCLDIR=$(TOPDIR)/scripts


project = TOE_hls_prj IPERF2_TCP_hls_prj ECHOSERVER_hls_prj ARP_hls_prj \
	      ETH_inserter_hls_prj ICMP_hls_prj PKT_HANDLER_prj

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

IPERF2_TCP_hls_prj: $(shell find $(IPERFSRC) -type f) \
			 $(TCLDIR)/iperf2_tcp_script.tcl
	rm -rf 	IPERF2_TCP_hls_prj
	vivado_hls -f $(TCLDIR)/iperf2_tcp_script.tcl -tclargs $(project)

ECHOSERVER_hls_prj: $(shell find $(ECHOSRC) -type f) \
			 $(TCLDIR)/echo_server_script.tcl
	rm -rf 	ECHOSERVER_hls_prj
	vivado_hls -f $(TCLDIR)/echo_server_script.tcl -tclargs $(project)

ARP_hls_prj: $(shell find $(ARPSRC) -type f) \
			 $(TCLDIR)/arp_script.tcl
	rm -rf 	ARP_hls_prj
	vivado_hls -f $(TCLDIR)/arp_script.tcl -tclargs $(project)	

ETH_inserter_hls_prj: $(shell find $(ETHSRC) -type f) \
			 $(TCLDIR)/ethernet_inserter_script.tcl
	rm -rf 	ETH_inserter_hls_prj
	vivado_hls -f $(TCLDIR)/ethernet_inserter_script.tcl -tclargs $(project)

ICMP_hls_prj: $(shell find $(ICMPSRC) -type f) \
			 $(TCLDIR)/icmp_script.tcl
	rm -rf 	ICMP_hls_prj
	vivado_hls -f $(TCLDIR)/icmp_script.tcl -tclargs $(project)

PKT_HANDLER_prj: $(shell find $(PKTSRC) -type f) \
			 $(TCLDIR)/packet_handler_script.tcl
	rm -rf 	PKT_HANDLER_prj
	vivado_hls -f $(TCLDIR)/packet_handler_script.tcl -tclargs $(project)

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