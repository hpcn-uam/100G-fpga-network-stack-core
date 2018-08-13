TOPDIR=$(shell pwd)
TOESCR=$(TOPDIR)/toe_src
IPERFSRC=$(TOPDIR)/iperf_client_src
ECHOSRC=$(TOPDIR)/echo_server_src
ARPSRC=$(TOPDIR)/hls/arp_server
ETHSRC=$(TOPDIR)/hls/ethernet_inserter
ICMPSRC=$(TOPDIR)/hls/icmp_server
PKTSRC=$(TOPDIR)/hls/packet_handler
TCLDIR=$(TOPDIR)/scripts


project = TOE_hls_prj IPERF_hls_prj ECHOSERVER_hls_prj ARP_hls_prj \
	      ETH_inserter_hls_prj ICMP_hls_prj PKT_HANDLER_prj

all: folder build
	

build: $(project)
	@echo -e "\e[94mIP Completed: $(project)\e[39m"

folder:
	mkdir projects -p

clean:
	rm -rf *.log *.jou file* *.bak vivado*.str synlog.tcl .Xil fsm_encoding.os

distclean: clean
	rm -rf projects


TOE_hls_prj: $(shell find $(TOESCR) -type f) \
			 $(TCLDIR)/toe_script.tcl
	rm -rf 	projects/TOE_hls_prj
	vivado_hls -f $(TCLDIR)/toe_script.tcl -tclargs $(project)
	#mv TOE_hls_prj projects

IPERF_hls_prj: $(shell find $(IPERFSRC) -type f) \
			 $(TCLDIR)/iperf_script.tcl
	rm -rf 	projects/IPERF_hls_prj
	vivado_hls -f $(TCLDIR)/iperf_script.tcl -tclargs $(project)
	#mv IPERF_hls_prj projects

ECHOSERVER_hls_prj: $(shell find $(ECHOSRC) -type f) \
			 $(TCLDIR)/echo_server_script.tcl
	rm -rf 	projects/ECHOSERVER_hls_prj
	vivado_hls -f $(TCLDIR)/echo_server_script.tcl -tclargs $(project)
	#mv ECHOSERVER_hls_prj projects 

ARP_hls_prj: $(shell find $(ARPSRC) -type f) \
			 $(TCLDIR)/arp_script.tcl
	rm -rf 	projects/ARP_hls_prj
	vivado_hls -f $(TCLDIR)/arp_script.tcl -tclargs $(project)	
	#mv ARP_hls_prj projects

ETH_inserter_hls_prj: $(shell find $(ETHSRC) -type f) \
			 $(TCLDIR)/ethernet_inserter_script.tcl
	rm -rf 	projects/ETH_inserter_hls_prj
	vivado_hls -f $(TCLDIR)/ethernet_inserter_script.tcl -tclargs $(project)
	#mv ETH_inserter_hls_prj projects		

ICMP_hls_prj: $(shell find $(ICMPSRC) -type f) \
			 $(TCLDIR)/icmp_script.tcl
	rm -rf 	projects/ICMP_hls_prj
	vivado_hls -f $(TCLDIR)/icmp_script.tcl -tclargs $(project)
	#mv ICMP_hls_prj projects

PKT_HANDLER_prj: $(shell find $(PKTSRC) -type f) \
			 $(TCLDIR)/packet_handler_script.tcl
	rm -rf 	projects/PKT_HANDLER_prj
	vivado_hls -f $(TCLDIR)/packet_handler_script.tcl -tclargs $(project)
	#mv PKT_HANDLER_prj projects

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