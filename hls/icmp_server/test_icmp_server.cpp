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
#include "../TOE/testbench/pcap2stream.hpp"

using namespace hls;
using namespace std;

int main(int argc, char **argv) {


	stream<axiWord>						ipRxData("ipRxData");
	stream<axiWord>						ipTxData("ipTxData");
	stream<axiWord>						goldenData("goldenData");
	ap_uint<32>							myIpAddress = 0x0500a8c0;


	char *input_file;
	char *golden_input;


	if (argc < 3) {
		cerr << "[ERROR] missing arguments " __FILE__  << " <INPUT_PCAP_FILE> <GOLDEN_PCAP_FILE> " << endl;;
		return -1;
	}

	input_file 	 = argv[1];
	golden_input = argv[2];

	pcap2stream(input_file, false, ipRxData);

	for (int m = 0 ; m < 50000 ; m++){
		icmp_server(
	            ipRxData,
	            myIpAddress,
	            ipTxData);
	}

	pcap2stream(golden_input, false, goldenData);

	int wordCount = 0;
	int packets   = 0;
	while(!ipTxData.empty()){
		axiWord currWord;
		axiWord GoldenWord;
		ipTxData.read(currWord);
		goldenData.read(GoldenWord);
		int error = 0;
		for (int m=0; m <64 ; m++){
			if (currWord.data((m*8)+7,m*8)!= GoldenWord.data((m*8)+7,m*8)){
				cout << "Packet [" << dec << setw(4) << packets << "] Word [" << setw(3) << wordCount <<" ]Byte [" << setw(2) << dec << m << "] does not match generated ";
				cout << hex << currWord.data((m*8)+7,m*8) << " expected " << GoldenWord.data((m*8)+7,m*8) << endl;
				error++;
			}
		}
		if (error!=0)
			return -1;
		if (currWord.keep != GoldenWord.keep){
			cout << "keep does not match generated " << hex << setw(18) << currWord.keep << " expected " << setw(18) << GoldenWord.keep << endl;
			return -2;
		}
		if (currWord.last != GoldenWord.last){
			cout << "Error last does not match " << endl;
			return -3;
		}

		if (currWord.last){
			packets++;
			wordCount = 0;
		}
		else
			wordCount++;
	}

	cout << "Processed packets " << dec << packets << endl;

	return 0;
}
