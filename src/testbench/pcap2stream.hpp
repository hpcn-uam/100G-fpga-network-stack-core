#ifndef _PCAP2STREAM_H_
#define _PCAP2STREAM_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include "../toe.hpp"


void pcap2stream(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				stream<axiWord>&					output_data			// output data
);

void pcap2stream_step(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				stream<axiWord>&					output_data			// output data
	);

#endif
