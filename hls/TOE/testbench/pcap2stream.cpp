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

#include <stdio.h>
#include <stdlib.h>
#include "pcap.h"
#include "pcap2stream.hpp"

stream<axiWord> input_data("Data_Read_from_pcap");
stream<axiWord> packet_without_ethernet("packet_without_ethernet");
axiWord transaction;

using namespace std;
using namespace hls;

#define DEBUG
//#define DEBUG1

void pcapPacketHandler (unsigned char *userData,  struct pcap_pkthdr* pkthdr, unsigned char *packet) {

	ap_uint<16> bytes_sent=0, bytes2send,last_trans;
	ap_uint<ETH_INTERFACE_WIDTH/8> aux_keep;
	ap_uint<ETH_INTERFACE_WIDTH> data_value;
	static ap_uint<16> pkt=0;

	for (int h=0 ; h< ETH_INTERFACE_WIDTH/8 ; h++){
		transaction.keep[h]=1;
	}
	transaction.last=0;

	bytes2send = (pkthdr->len);
#ifdef DEBUG1
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
					aux_keep[s]=1;
				else
					aux_keep[s]=0;
			}
			transaction.last=1;
			transaction.keep=aux_keep;
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
		cout << "Test Data [" << dec << pkt << "] Transaction [" << dec << h/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tComplete Data " << hex << transaction.data << "\t\tkeep " << transaction.keep << "\tlast " << transaction.last << endl;
		pkt = pkt + transaction.last;
#endif

		input_data.write(transaction);
	}
}


void remove_ethernet (
						stream<axiWord>&			dataIn,
					  	stream<axiWord>&			dataOut)
{

	axiWord currWord, prevWord, sendWord;
	prevWord.data = 0;
	prevWord.keep = 0;
	prevWord.last = 0;
	sendWord = prevWord;

	static int pkt_count = 0;
	int wordCount = 0;

	bool first_word = true;

	do{
		dataIn.read(currWord);
		//cout << "ETH In[" << dec << pkt_count << "] " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;

		if (first_word){
			first_word = false;
			if (currWord.last){
				sendWord.data(399,0) 	= currWord.data(511 , 112);
				sendWord.keep(49 ,0) 	= currWord.keep( 63 ,  14);
				sendWord.last 			= 1;
				dataOut.write(sendWord);
				//cout << "ETH Short[" << dec << pkt_count << "] " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
			}
		}
		else{
			sendWord.data(399,  0) 	= prevWord.data(511,112);
			sendWord.keep( 49,  0) 	= prevWord.keep( 63, 14);
			sendWord.data(511,400) 	= currWord.data(111,  0);
			sendWord.keep( 63, 50) 	= currWord.keep( 13,  0);
			sendWord.last=0;
			if (currWord.last){
				if (currWord.keep.bit(14)) {
					prevWord = currWord;
					dataOut.write(sendWord);
					//cout << "ETH Extra1[" << dec << pkt_count << "] " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
					sendWord.data = 0;
					sendWord.keep = 0;
					sendWord.last = 1; 
					sendWord.data(399,  0) 	= prevWord.data(511,112);
					sendWord.keep( 49,  0) 	= prevWord.keep( 63, 14);
					dataOut.write(sendWord);
					//cout << "ETH Extra2[" << dec << pkt_count << "] " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;

				}
				else {
					sendWord.last=1;
					dataOut.write(sendWord);
					//cout << "ETH Last[" << dec << pkt_count << "] " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
				}
			}
			else{
					dataOut.write(sendWord);
					//cout << "ETH Normal[" << dec << pkt_count << "] " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
			}
		}

		prevWord = currWord;
		

	} while (!currWord.last);
	pkt_count++;

}


int open_file (
				char 								*file2load,
				bool 								ethernet) {


	//cout << "File 2 load " << file2load << endl;

	/* Read packets from pcapfile */
	if(pcap_open (file2load,ethernet)) {
		cout << "Error opening the input file with name: " << file2load << endl;
		return -1;
	}
	//else
	//	cout <<"File open correct" << endl << endl;

	if (pcap_loop (0,                  /* Iterate over all the packets */
	                 pcapPacketHandler,  /* Routine invoked */
	                 NULL) < 0) {        /* Argument passed to the handler function (the buffer) */


	    pcap_close();
	    cout << "Error opening the input file"<< endl;
	    return -1;
  	}

	pcap_close();
	return 0;
}


void pcap2stream(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				stream<axiWord>&					output_data			// output data
) {

	axiWord 	currWord;

	if (open_file(file2load,ethernet)==0){
		while(!input_data.empty()){
			input_data.read(currWord);
			output_data.write(currWord);
		}
	}
}

/* It returns one complete packet each time is called
*
*/

void pcap2stream_step(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				bool& 								end_of_data,
				stream<axiWord>&					output_data			// output data
	){

	static bool error_opening_file = false;
	static bool file_open = false;

	stream<axiWord> currWord_Stream("data_to_remove_ethernet");

	axiWord 	currWord;

	if (!file_open) {
		file_open = true;
		if (open_file(file2load,ethernet)!=0){
			error_opening_file = true;
		}
	}


	if (!error_opening_file) {
		if (!input_data.empty()){
			do {
				input_data.read(currWord);
				output_data.write(currWord);
			} while (!currWord.last);
			end_of_data = false;
		}
		else {
			end_of_data = true;
		}
	}

}


unsigned keep_to_length(ap_uint<ETH_INTERFACE_WIDTH/8> keep){

	unsigned ones_count = 0;

	for (int i=0; i < ETH_INTERFACE_WIDTH/8 ; i++){
		if (keep.bit(i)){
			ones_count++;
		}
		else
			break;
	}

	return ones_count;

}


int stream2pcap(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				bool 								microseconds,		// 1: microseconds precision 0: nanoseconds precision 
				stream<axiWord>&					input_data,			// output data
				bool 								close_file
) {

	static bool file_open = false;
	static int 	pcap_open_write_return=0;

	axiWord 	currWord;

	static uint8_t packet [65536]={0x4A,0xFD,0x4B,0xE0,0x87,0xBD,0x0,0x0A,0x35,0x02,0x9D,0xE5,0x08,0x00}; // Include the Ethernet header
	static int pointer = 14;
	
	if (ethernet){
		pointer = 0;
	}

	if (!file_open){
		pcap_open_write_return = pcap_open_write(file2load,microseconds);
		file_open = true;
	}

	if (pcap_open_write_return !=0){
		return -1;
	}

	while (!input_data.empty()){
		input_data.read(currWord);
		//cout << "Stream to pcap: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
		for (int i =0 ; i<64 ; i++){
			if (currWord.keep.bit(i)){
				packet[pointer] = currWord.data(i*8+7,i*8);
				pointer++;
			}
		}
		if (currWord.last){
			//call write function
			pcap_WriteData(&packet[0],pointer);
			pointer = (ethernet) ? 0 : 14;
		}
	}

	if (close_file){
		pcap_close_write();
		file_open = false;
	}

	return 0;

}
