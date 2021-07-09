/************************************************
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors 
may be used to endorse or promote products derived from this software 
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2015 Xilinx, Inc.
************************************************/

#include "ethernet_header_inserter.hpp"


void broadcaster_and_mac_request(
						stream<axiWord>&				dataIn,
						stream<ap_uint<32> >&			arpTableRequest,				
						stream<axiWordi>&				ip_header_out,
						stream<axiWordi>&				no_ip_header_out,
						ap_uint<32>&					regSubNetMask,
						ap_uint<32>&					regDefaultGateway)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum bmr_states {FIRST_WORD, REMAINING};
	static bmr_states bmr_fsm_state = FIRST_WORD;

//	static int 		word_count = 0;
	axiWord 		currWord;
	axiWordi 		sendWord;
	ap_uint<32> 	dst_ip_addr;

	switch (bmr_fsm_state){
		case FIRST_WORD : 
			if (!dataIn.empty()){							// There are data in the input stream
				dataIn.read(currWord);						// Reading input data
				dst_ip_addr = currWord.data(159,128);		// getting the IP address

				if ((dst_ip_addr & regSubNetMask) == (regDefaultGateway & regSubNetMask))		// Check if the destination address is in the server subnetwork and asks for dst_ip_addr MAC if not asks for default gateway MAC address
					arpTableRequest.write(dst_ip_addr);
				else
					arpTableRequest.write(regDefaultGateway);

				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.last = currWord.last;
				ip_header_out.write(sendWord); 				// Writing out first transaction
				if (!currWord.last)
					bmr_fsm_state = REMAINING;
			}
			break;
		case REMAINING : 
			if (!dataIn.empty()){							// There are data in the input stream
				dataIn.read(currWord);
				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.last = currWord.last;
				no_ip_header_out.write(sendWord);			// Writing out rest of transactions
				if (currWord.last)
					bmr_fsm_state = FIRST_WORD;
			}
			break;
	}
}

void handle_output(
						stream<arpTableReply>& 			arpTableReplay,
						stream<axiWordi>&				ip_header_checksum,
						stream<axiWordi>&				no_ip_header_out,

						ap_uint<48>&					myMacAddress,

						stream<axiWord>&				dataOut
						
						){
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum mwState {WAIT_LOOKUP, DROP_IP, DROP_NO_IP, WRITE_FIRST_TRANSACTION, WRITE_REMAINING , WRITE_EXTRA_LAST_WORD};
	typedef my_axis<112> axiremaining;

	static mwState mw_state = WAIT_LOOKUP;
	static axiremaining previous_word;
	
	axiWord sendWord;
	axiWordi current_ip_checksum;
	axiWordi current_no_ip;
	arpTableReply reply;

	switch (mw_state){
		case WAIT_LOOKUP:
			if (!arpTableReplay.empty()) {		// A valid response has been arrived
				arpTableReplay.read(reply);

				if (reply.hit){										// The MAC address related to IP destination address has been found
					previous_word.data( 47, 0) = reply.macAddress;		
					previous_word.data( 95,48) = myMacAddress;
					previous_word.data(111,96) = 0x0008;

					previous_word.keep(13,0) = 0x3FFF;

					mw_state = WRITE_FIRST_TRANSACTION;
				}
				else
					mw_state = DROP_IP;
			}
		break;
		case DROP_IP:
			if (!ip_header_checksum.empty()) {
				ip_header_checksum.read(current_ip_checksum);

				if (current_ip_checksum.last == 1)
					mw_state = WAIT_LOOKUP;
				else
					mw_state = DROP_NO_IP;
			}
		break;

		case DROP_NO_IP:
			if (!no_ip_header_out.empty()) {
				no_ip_header_out.read(current_no_ip);

				if (current_no_ip.last == 1)
					mw_state = WAIT_LOOKUP;
			}
		break;		

		case WRITE_FIRST_TRANSACTION: 
			if (!ip_header_checksum.empty()) {
				ip_header_checksum.read(current_ip_checksum);

				sendWord.data( 111,   0) 	= previous_word.data;					// Insert Ethernet header
				sendWord.keep(  13,   0) 	= previous_word.keep;

				sendWord.data( 511, 112) 	= current_ip_checksum.data(399,0);		// Compose output word
				sendWord.keep(  63,  14) 	= current_ip_checksum.keep( 49,0);

				previous_word.data 			= current_ip_checksum.data(511,400);
				previous_word.keep 			= current_ip_checksum.keep(63,50);
				sendWord.last 				= 0;

				if (current_ip_checksum.last == 1){
					if (current_ip_checksum.keep[50]){

						mw_state = WRITE_EXTRA_LAST_WORD;
					}									// If the current word has more than 50 bytes a extra transaction for remaining data is needed
					else{
						sendWord.last 	= 1;
						mw_state = WAIT_LOOKUP;
					}
				}
				else {
					mw_state = WRITE_REMAINING;
				}

				dataOut.write(sendWord);
			}
		break;

		case WRITE_REMAINING: 
			if (!no_ip_header_out.empty()) {
				no_ip_header_out.read(current_no_ip);

				sendWord.data( 111,   0) 	= previous_word.data;
				sendWord.keep(  13,   0) 	= previous_word.keep;

				sendWord.data( 511, 112) 	= current_no_ip.data(399,0);		// Compose output word
				sendWord.keep(  63,  14) 	= current_no_ip.keep( 49,0);

				previous_word.data 			= current_no_ip.data(511,400);
				previous_word.keep 			= current_no_ip.keep(63,50);
				sendWord.last 				= 0;
				
				if (current_no_ip.last == 1){
					if (current_no_ip.keep.bit(50)){

						mw_state = WRITE_EXTRA_LAST_WORD;
					}									// If the current word has more than 50 bytes a extra transaction for remaining data is needed
					else{
						sendWord.last 	= 1;
						mw_state = WAIT_LOOKUP;
					}
				}

				dataOut.write(sendWord);
			}
		break;

		case WRITE_EXTRA_LAST_WORD:
			sendWord.data( 111,   0) 	= previous_word.data;
			sendWord.keep(  13,   0) 	= previous_word.keep;
			sendWord.data( 511, 112) 	= 0;
			sendWord.keep(  63,  14) 	= 0;
			sendWord.last 				= 1;
			dataOut.write(sendWord);
			mw_state = WAIT_LOOKUP;

		break;

		default:
			mw_state = WAIT_LOOKUP;
		break;
	}

}

void compute_and_insert_ip_checksum (
						stream<axiWordi>&			dataIn,
						stream<axiWordi>&			dataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	static ap_uint<16> ip_ops[30];
#pragma HLS ARRAY_PARTITION variable=ip_ops complete dim=1
	static ap_uint<17> ip_sums_L1[15];
	static ap_uint<18> ip_sums_L2[8];
	static ap_uint<19> ip_sums_L3[4] = {0, 0, 0, 0};
	static ap_uint<20> ip_sums_L4[2];
	static ap_uint<21> ip_sums_L5;
	static ap_uint<17> final_sum;

	static ap_uint<16> ip_chksum;


	static ap_uint<4> ipHeaderLen = 0;
	axiWordi currWord;
	ap_uint<16> temp;

   if (!dataIn.empty())
	{
		dataIn.read(currWord); //512 bits word

		ipHeaderLen = currWord.data.range(3, 0);

		data_ops: for (int i = 0; i < 30; i++)
		{
		#pragma HLS unroll
			temp(7, 0) = currWord.data(i*16+15, i*16+8);
			temp(15, 8) = currWord.data(i*16+7, i*16);
			if (i < ipHeaderLen*2)
				ip_ops[i] = temp;
			else
				ip_ops[i] = 0;
		}

		ip_ops[5] = 0;

		//adder tree
		for (int i = 0; i < 15; i++) {
		#pragma HLS unroll
			ip_sums_L1[i] = ip_ops[i*2] + ip_ops[i*2+1];
		}

		//adder tree L2
		for (int i = 0; i < 7; i++) {
		#pragma HLS unroll
			ip_sums_L2[i] = ip_sums_L1[i*2+1] + ip_sums_L1[i*2];
		}
		ip_sums_L2[7] = ip_sums_L1[14];

		//adder tree L3
		for (int i = 0; i < 4; i++) {
		#pragma HLS unroll
			ip_sums_L3[i] = ip_sums_L2[i*2+1] + ip_sums_L2[i*2];
		}

		ip_sums_L4[0] = ip_sums_L3[0*2+1] + ip_sums_L3[0*2];
		ip_sums_L4[1] = ip_sums_L3[1*2+1] + ip_sums_L3[1*2];

		ip_sums_L5 = ip_sums_L4[1] + ip_sums_L4[0];
		final_sum = ip_sums_L5.range(15,0) + ip_sums_L5.range(20,16);
		final_sum = final_sum.bit(16) + final_sum;
		ip_chksum = ~(final_sum.range(15,0)); // ones complement

		// switch WORD_N
		currWord.data(95, 88) = ip_chksum(7,0);
		currWord.data(87, 80) = ip_chksum(15,8);
		dataOut.write(currWord);

	}
}



/** @ingroup mac_ip_encode
 *  This module requests the MAC address of the destination IP address and inserts the Ethener header to the IP packet
 */
void ethernet_header_inserter(

					stream<axiWord>&			dataIn, 					// Input packet (ip aligned)
					stream<axiWord>&			dataOut,					// Output packet (ethernet aligned)
					
					stream<arpTableReply>&		arpTableReplay,					// ARP cache replay
					stream<ap_uint<32> >&		arpTableRequest,				// ARP cache request
					
					ap_uint<48>&				myMacAddress,				// Server MAC address
					ap_uint<32>&				regSubNetMask,				// Server subnet mask
					ap_uint<32>&				regDefaultGateway)			// Server default gateway
{

#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return

#pragma HLS INTERFACE axis register both port=dataIn
#pragma HLS INTERFACE axis register both port=dataOut

#pragma HLS INTERFACE axis register both port=arpTableReplay
#pragma HLS INTERFACE axis register both port=arpTableRequest	

#pragma HLS INTERFACE ap_none register port=myMacAddress
#pragma HLS INTERFACE ap_none register port=regSubNetMask
#pragma HLS INTERFACE ap_none register port=regDefaultGateway

	// FIFOs
	static stream<axiWordi> ip_header_out;
	#pragma HLS stream variable=ip_header_out depth=16 
	
	static stream<axiWordi> no_ip_header_out;
	#pragma HLS stream variable=no_ip_header_out depth=16 
	
	static stream<axiWordi> ip_header_checksum;
	#pragma HLS stream variable=ip_header_checksum depth=16 

	broadcaster_and_mac_request (
			dataIn, 
			arpTableRequest, 
			ip_header_out, 
			no_ip_header_out,
			regSubNetMask, 
			regDefaultGateway);

	compute_and_insert_ip_checksum(
			ip_header_out,
			ip_header_checksum);

	handle_output (
			arpTableReplay, 
			ip_header_checksum, 
			no_ip_header_out, 
			myMacAddress, 
			dataOut);
}
