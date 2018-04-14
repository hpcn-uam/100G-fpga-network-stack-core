TOPDIR=$(shell pwd)
SRCDIR=$(TOPDIR)/src
TCLDIR=$(TOPDIR)/scripts


project = toe_hls

all: build

build: $(project)/solution1/impl/ip/component.xml
	@echo -e "\e[94mIP Completed: $(project)\e[39m"


clean:
	rm -rf vivado*.log vivado*.jou file* *.bak synlog.tcl .Xil fsm_encoding.os

distclean: clean
	rm -rf $(project)

$(project)/solution1/impl/ip/component.xml: $(shell find $(SRCDIR) -type f) \
											$(TCLDIR)/project_script.tcl
	rm -rf 	$(project)
	vivado_hls -f $(TCLDIR)/project_script.tcl -tclargs $(project)


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