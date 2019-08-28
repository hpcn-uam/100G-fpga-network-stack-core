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

#include "ethernet_header_inserter.hpp"
#include <stdio.h>
#include <stdlib.h>
#include "pcap.h"

stream<axiWord> input_data;
axiWord transaction;

#define DEBUG
//#define DEBUG1

void pcapPacketHandler (unsigned char *userData,  struct pcap_pkthdr* pkthdr, unsigned char *packet) {

	ap_uint<16> bytes_sent=0, bytes2send,last_trans;
	ap_uint<ETH_INTERFACE_WIDTH/8> aux_strb;
	ap_uint<ETH_INTERFACE_WIDTH> data_value;

	for (int h=0 ; h< ETH_INTERFACE_WIDTH/8 ; h++){
		transaction.strb[h]=1;
	}
	transaction.last=0;

	bytes2send = (pkthdr->len);
#ifdef DEBUG
	cout << "Bytes to send " << bytes2send << endl;
#endif
	for (int h=0; h < bytes2send ; h+= ETH_INTERFACE_WIDTH/8){
		bytes_sent += ETH_INTERFACE_WIDTH/8;
		if (bytes_sent < bytes2send){
			data_value= *((ap_uint<ETH_INTERFACE_WIDTH> *)packet);
			packet+=ETH_INTERFACE_WIDTH/8;
		}
		else if (bytes_sent < (bytes2send + ETH_INTERFACE_WIDTH/8)){
			last_trans= ETH_INTERFACE_WIDTH/8 - bytes_sent + bytes2send;

			for (int s=0 ; s< ETH_INTERFACE_WIDTH/8; s++){
				if (s < last_trans)
					aux_strb[s]=1;
				else
					aux_strb[s]=0;
			}
			transaction.last=1;
			transaction.strb=aux_strb;
			data_value=0;
			for (int s=0; s < last_trans*8 ; s+=8){
				data_value.range(s+7,s)= *((ap_uint<8> *)packet);
				packet++;
			}
		}
#ifdef CHANGE_ENDIANESS
		for (int s=0; s< ETH_INTERFACE_WIDTH; s+=8){
			transaction.data.range(ETH_INTERFACE_WIDTH-1-s,ETH_INTERFACE_WIDTH-s-8)=data_value.range(s+7,s);
		}
#else
		transaction.data = data_value;
#endif
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << h/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tComplete Data " << hex << transaction.data << "\t\tstrb " << transaction.strb << "\tlast " << transaction.last << endl;
#endif

		input_data.write(transaction);
	}
}


void remove_ethernet (
						stream<axiWord>&			dataIn,
					  	stream<axiWord>&			dataOut)
{

	enum mwState {WAIT_PKT, WRITE_TRANSACTION , WRITE_EXTRA_LAST_WORD};
	static mwState mw_state = WAIT_PKT;

	axiWord currWord;
	static axiWord prevWord;
	static axiWord sendWord;


	static int wordCount = 0;

	if (!dataIn.empty()){
		do {
			switch (mw_state){
				case WAIT_PKT:
					wordCount = 0;
					if (!dataIn.empty()){
						dataIn.read(currWord);
						prevWord = {0,0,0};									// initialize
						prevWord.data(399,0) = currWord.data(511 , 112);
						prevWord.strb(49 ,0) = currWord.strb( 63 ,  14);
						prevWord.last = currWord.last;
						if (currWord.last == 1){
							sendWord = prevWord;
							dataOut.write(sendWord);
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << wordCount/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tShaved off Data " << hex << sendWord.data << "\t\tstrb " << sendWord.strb << "\tlast " << sendWord.last << endl;
#endif							
						}
						else{
							mw_state = WRITE_TRANSACTION;
						}
	
					}	
				break;
				case WRITE_TRANSACTION : 
					if (!dataIn.empty()){
						dataIn.read(currWord);
						wordCount++;
	
	
						sendWord.data(399,0) 	= prevWord.data(399,0);
						sendWord.strb(49 ,0) 	= prevWord.strb(49 ,0);
						sendWord.data(511,400) 	= currWord.data(111 ,  0);
						sendWord.strb( 63 ,50) 	= currWord.strb( 13 ,  0);
	
						prevWord = {0,0,0};									// initialize
						prevWord.data(399,0) 	= currWord.data(511 , 112);
						prevWord.strb(49 ,0) 	= currWord.strb( 63 ,  14);
						prevWord.last 		 	= currWord.last;

						sendWord.last 			= 0;
	
						if (currWord.last == 1) {
							if (currWord.strb.bit(14)){
								mw_state = WRITE_EXTRA_LAST_WORD;
							}
							else{
								wordCount = 0;
								sendWord.last 	= 1;
								mw_state = WAIT_PKT;
							}
						}
	
						dataOut.write(sendWord);
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << wordCount/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tShaved off Data " << hex << sendWord.data << "\t\tstrb " << sendWord.strb << "\tlast " << sendWord.last << endl;
#endif
					}
				break;
	
				case WRITE_EXTRA_LAST_WORD : 
					prevWord.last 		 	= 1;
					sendWord = prevWord;
					dataOut.write(sendWord);
					mw_state = WAIT_PKT;
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << wordCount/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tShaved off Data " << hex << sendWord.data << "\t\tstrb " << sendWord.strb << "\tlast " << sendWord.last << endl;
#endif
				break;
	
				default :
					mw_state = WAIT_PKT;
				break;
			}
		} while (sendWord.last == 0);
	}


}


int main(int argc, char **argv){

	stream<axiWord> output_data;
	axiWord value_out;
	ap_uint<32> pkt_rx;
	ap_uint<16> h=0;

	stream<axiWord> without_ethernet;

	stream<axiWord> ip_checksum_input;
	stream<axiWord> ip_checksum_golden;

	axiWord 		aux;
	axiWord 		aux1;
	
	stream<axiWord> ip_checksum_output;


	char file2load[500]="/home/mario/Documents/cmac_100g/submodules/tcp_ip_cores/mac_ip_encode/";
	char default_file[50]="prueba2.pcap";
	char file[100];

	if (argc == 2){
		strcat(file2load,argv[1]);
	}
	else {
		strcat(file2load,default_file);
	}

	cout << "File 2 load " << file2load << endl;

	/* Read packets from pcapfile */
	if(pcap_open (file2load)) {
		cout << "Error opening the input file"<< endl;
		return -1;
	}
	else
		cout <<"File open correct" << endl << endl;

	if (pcap_loop (0,                  /* Iterate over all the packets */
	                 pcapPacketHandler,  /* Routine invoked */
	                 NULL) < 0) {        /* Argument passed to the handler function (the buffer) */


	    pcap_close();
	    cout << "Error opening the input file"<< endl;
	    return -1;
	  }

	cout << endl;
	pcap_close();

	while(!input_data.empty()){
		remove_ethernet(input_data,without_ethernet);
		//first_test (input_data,output_data,&pkt_rx);
	}

	while(!without_ethernet.empty()){

		without_ethernet.read(aux);

		ip_checksum_input.write(aux);
		ip_checksum_golden.write(aux);

	}
		/* Execute the hls code */
	while(!ip_checksum_input.empty()){
		compute_and_insert_ip_checksum(ip_checksum_input,ip_checksum_output);
	}
		/* Read results */

	int wordCount=0;
	ap_uint<16> 	correct_checksum;
	ap_uint<16> 	computed_checksum;

	while(!ip_checksum_output.empty()){
		ip_checksum_output.read(aux);
		ip_checksum_golden.read(aux1);

	if (wordCount ==  0){
		correct_checksum(15 , 8) = aux1.data(87 , 80);
		correct_checksum( 7 , 0) = aux1.data(95 , 88);
		computed_checksum(15 , 8) = aux.data(87 , 80);
		computed_checksum( 7 , 0) = aux.data(95 , 88);
		if (computed_checksum != computed_checksum){
			cout << "Checksum error" << endl;
		}
#ifdef DEBUG
		cout << "Checksum {correct,computed} : {" << hex << correct_checksum << " , " << computed_checksum << "}" << endl;
#endif
	}

	if(aux1.last == 1){
		wordCount = 0;
	}
	else
		wordCount++;
//		if (aux.data != aux1.data){
//			cout << "Error in word " << wordCount << endl;
//		}


	}


	return 0;
}
