

.PHONY:all
all: create_folder
	make -C synthesis_results

create_folder:
	mkdir -p synthesis_results
	cp Makefile.synthesis synthesis_results/Makefile

clean:
	rm -rf *.log *.jou

distclean:
	rm -rf synthesis_results