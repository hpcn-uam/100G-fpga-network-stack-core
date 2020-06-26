

.PHONY:all
all: create_folder_noHBM
	make -C synthesis_results_noHMB

create_folder_noHBM:
	mkdir -p synthesis_results_noHMB
	cp Makefile.synthesis synthesis_results_noHMB/Makefile

.PHONY:hbm
hbm: create_folder_HBM
	make -C synthesis_results_HMB FPGAPART=xcu280-fsvh2892-2L-e

create_folder_HBM:
	mkdir -p synthesis_results_HMB
	cp Makefile.synthesis synthesis_results_HMB/Makefile

clean:
	rm -rf *.log *.jou

distclean:
	rm -rf synthesis_results_noHMB synthesis_results_HMB