/************************************************
BSD 3-Clause License

Copyright (c) 2019, HPCN Group, UAM Spain (hpcn-uam.es)
All rights reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************/


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
				bool& 								end_of_data,
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