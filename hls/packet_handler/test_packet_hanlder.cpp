/************************************************
BSD 3-Clause License

Copyright (c) 2019, HPCN Group, UAM Spain (hpcn-uam.es)
and Systems Group, ETH Zurich (systems.ethz.ch)
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
#include "icmp_server.hpp"
#include "/home/mario/Documents/cmac_100g/submodules/tcp_ip_cores/TOE/src/testbench/pcap2stream.hpp"

using namespace hls;
using namespace std;

void compute_checksum(	
									stream<axiWord>&			dataIn,
									stream<ap_uint<16> >&		pseudo_tcp_checksum,
									ap_uint<1>					source)
{

	static ap_uint<1> 	compute_checksum[2] = {0 , 0};
	static ap_uint<16> 	word_sum[32][2]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	ap_uint<17> 		ip_sums_L1[16];
	ap_uint<18> 		ip_sums_L2[8];
	ap_uint<19> 		ip_sums_L3[4];
	ap_uint<20> 		ip_sums_L4[2];
	ap_uint<21> 		ip_sums_L5;
	ap_uint<16> 		tmp;
	ap_uint<17> 		tmp1;
	ap_uint<17> 		tmp2;
	ap_uint<17> 		final_sum_r; 							// real add
	ap_uint<17> 		final_sum_o; 							// overflowed add
	ap_uint<16> 		res_checksum;
	axiWord 			currWord;

	if (!dataIn.empty() && !compute_checksum[source]){
		dataIn.read(currWord);

		first_level_sum : for (int i=0 ; i < 32 ; i++ ){
			tmp(7,0) 	= currWord.data((((i*2)+1)*8)+7,((i*2)+1)*8);
			tmp(15,8) 	= currWord.data(((i*2)*8)+7,(i*2)*8);
			tmp1 		= word_sum[i][source] + tmp;
			tmp2 		= word_sum[i][source] + tmp + 1;
			if (tmp1.bit(16)) 				// one's complement adder
				word_sum[i][source] = tmp2(15,0);
			else
				word_sum[i][source] = tmp1(15,0);
		}

		if(currWord.last){
			compute_checksum[source] = 1;
		}
	}
	else if(compute_checksum[source]) {
		//adder tree
		second_level_sum : for (int i = 0; i < 16; i++) {
			ip_sums_L1[i]= word_sum[i*2][source] + word_sum[i*2+1][source];
			word_sum[i*2][source]   = 0; // clear adder variable
			word_sum[i*2+1][source] = 0;
		}
		//adder tree L2
		third_level_sum : for (int i = 0; i < 8; i++) {
			ip_sums_L2[i] = ip_sums_L1[i*2+1] + ip_sums_L1[i*2];
		}
		//adder tree L3
		fourth_level_sum : for (int i = 0; i < 4; i++) {
			ip_sums_L3[i] = ip_sums_L2[i*2+1] + ip_sums_L2[i*2];
		}

		ip_sums_L4[0] = ip_sums_L3[1] + ip_sums_L3[0];
		ip_sums_L4[1] = ip_sums_L3[3] + ip_sums_L3[2];
		ip_sums_L5 = ip_sums_L4[1] + ip_sums_L4[0];

		final_sum_r = ip_sums_L5.range(15,0) + ip_sums_L5.range(20,16);
		final_sum_o = ip_sums_L5.range(15,0) + ip_sums_L5.range(20,16) + 1;

		if (final_sum_r.bit(16))
			res_checksum = ~(final_sum_o.range(15,0));
		else
			res_checksum = ~(final_sum_r.range(15,0));

		compute_checksum[source] = 0;
		pseudo_tcp_checksum.write(res_checksum);
	}
}


int main(int argc, char **argv) {


	stream<axiWord>						ipRxData("ipRxData");
	stream<axiWord>						in_packet_to_checksum("in_packet_to_checksum");
	stream<axiWord>						out_packet_to_checksum("out_packet_to_checksum");
	stream<axiWord>						ipTxData("ipTxData");

	stream<ap_uint<16> >				in_checksum("in_checksum");
	stream<ap_uint<16> >				out_checksum("out_checksum");

	char *input_file;
	char *output_file;

	int count = 0;


	if (argc < 3) {
		cerr << "[ERROR] missing arguments " __FILE__  << " <INPUT_PCAP_FILE> <OUTPUT_PCAP_FILE> " << endl;;
		return -1;
	}

	input_file 	= argv[1];
	output_file = argv[2];



	while (count < 500) {

		pcap2stream_step(input_file, false ,ipRxData);

		icmp_server(
			ipRxData,
			in_checksum,
			out_checksum,
			in_packet_to_checksum,
			out_packet_to_checksum,
			ipTxData);

		compute_checksum(	
			in_packet_to_checksum,
			in_checksum,
			0);

		compute_checksum(	
			out_packet_to_checksum,
			out_checksum,
			1);


			count++;
		stream2pcap(output_file,false,true,ipTxData,false);
	}

	stream2pcap(output_file,false,true,ipTxData,true);


	return 0;
}
