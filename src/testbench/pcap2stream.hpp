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

unsigned keep_to_length(ap_uint<ETH_INTERFACE_WIDTH/8> keep);

int stream2pcap(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				bool 								microseconds,		// 1: microseconds precision 0: nanoseconds precision 
				stream<axiWord>&					input_data,			// output data
				bool 								close_file
);