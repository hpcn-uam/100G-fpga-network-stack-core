

.PHONY:all
all: nohbm hbm

.PHONY:nohbm
nohbm:
	mkdir -p synthesis_results_noHBM
	cp Makefile.synthesis synthesis_results_noHBM/Makefile
	make -C synthesis_results_noHBM -j4

.PHONY:hbm
hbm:
	mkdir -p synthesis_results_HBM
	cp Makefile.synthesis synthesis_results_HBM/Makefile
	make -C synthesis_results_HBM FPGAPART=xcu280-fsvh2892-2L-e -j4

clean:
	rm -rf *.log *.jou

distclean:
	rm -rf synthesis_results_*