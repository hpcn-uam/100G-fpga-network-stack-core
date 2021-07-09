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

#include "packet_handler.hpp"


ap_uint<16> byteSwap16(ap_uint<16> inputVector) {
	return (inputVector.range(7,0), inputVector(15, 8));
}



/**
 * @brief      Shave off the Ethernet when is needed. (IPv4) packets
 *
 * @param      dataIn   The data in
 * @param      dataOut  The data out
 */
void ethernet_remover (			
			stream<axiWordi>&			dataIn,
			stream<axiWord>&			dataOut) {

#pragma HLS PIPELINE II=1

	enum er_states {FIRST_WORD , FWD , REMOVING, EXTRA};
	static er_states er_fsm_state = FIRST_WORD;

	axiWordi 			currWord;
	axiWord 			sendWord;
	static axiWordi 	prevWord;

	switch (er_fsm_state){
		case FIRST_WORD:
			if (!dataIn.empty()){
				dataIn.read(currWord);
				
				if (currWord.dest == 0){			// ARP packets must remain intact
					sendWord.data = currWord.data;
					sendWord.keep = currWord.keep;
					sendWord.dest = currWord.dest;
					sendWord.last = currWord.last;
					er_fsm_state 	= FWD;
				}
				else{								// No ARP packet, Re arrange the order in the output word
					sendWord.data(511,400) 	=  0;
					sendWord.keep( 63, 50) 	=  0;
					sendWord.data(399,  0) 	=  currWord.data (511,112);
					sendWord.keep( 49,  0) 	=  currWord.keep ( 63, 14);
					sendWord.dest 			=  currWord.dest;
					sendWord.last 			=  1;
					er_fsm_state 	= REMOVING;
				}

				if (currWord.last){				// If the packet is short stay in this state
					dataOut.write(sendWord);
					er_fsm_state = FIRST_WORD;
				}
				else if (currWord.dest == 0){	// ARP packets
					dataOut.write(sendWord);
				}

				prevWord = currWord;
			}
			break;
		case FWD:
			if (!dataIn.empty()){
				dataIn.read(currWord);
				if (currWord.last){
					er_fsm_state = FIRST_WORD;
				}
				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.dest = currWord.dest;
				sendWord.last = currWord.last;
				dataOut.write(sendWord);
			}
			break;
		case REMOVING:
			if (!dataIn.empty()){
				dataIn.read(currWord);

				sendWord.data(399,  0) 	=  prevWord.data(511,112);
				sendWord.keep( 49,  0) 	=  prevWord.keep( 63, 14);
				sendWord.data(511,400) 	=  currWord.data(111,  0);
				sendWord.keep( 63, 50) 	=  currWord.keep( 13,  0);
				sendWord.dest 			=  prevWord.dest;

				if (currWord.last){
					if (currWord.keep.bit(14)){							// When the input packet ends we have to check if all the input data
						er_fsm_state = EXTRA;							// was sent, if not an extra transaction is needed, but for the current
						sendWord.last 			=  0;					// transaction tlast must be 0
					}
					else {
						sendWord.last 			=  1;
						er_fsm_state = FIRST_WORD;
					}
				}
				else {
					sendWord.last 			=  0;
				}

				prevWord = currWord;
				dataOut.write(sendWord);
			}
			break;
		case EXTRA:
			sendWord.data(511,400) 	=  0;								// Send the remaining piece of information
			sendWord.keep( 63, 50) 	=  0;
			sendWord.data(399,  0) 	=  prevWord.data(511,112);
			sendWord.keep( 49,  0) 	=  prevWord.keep( 63, 14);
			sendWord.dest 			=  prevWord.dest;
			sendWord.last 			=  1;
			dataOut.write(sendWord);
			er_fsm_state = FIRST_WORD;
			break;
	}

}



/**
 * @brief      Packet identification, depending on its kind.
 * 			   If the packet does not belong to one of these 
 * 			   categories ARP, ICMP, TCP, UDP is dropped.
 * 			   It also a label for each kind of packet is added 
 * 			   in the tdest.
 *             Tdest : 0 ARP
 *                   : 1 ICMP
 *                   : 2 TCP
 *                   : 3 UDP  
 *
 * @param      dataIn   The data in
 * @param      dataOut  The data out
 */
void packet_identification(
			stream<axiWord>&			dataIn,
			stream<axiWordi>& 		dataOut) {


#pragma HLS PIPELINE II=1


	enum pi_states {FIRST_WORD , FWD ,DROP};
	static pi_states pi_fsm_state = FIRST_WORD;
	static dest_type tdest_r;
	
	dest_type tdest;
	axiWord   	currWord;
	axiWordi  	sendWord;
	ap_uint<16>	ethernetType;
	ap_uint<4>	ipVersion;
	ap_uint<8>	ipProtocol;
	bool		forward = true;

	switch (pi_fsm_state) {
		case FIRST_WORD :
			if (!dataIn.empty()){
				dataIn.read(currWord);
				ethernetType = byteSwap16(currWord.data(111,96));		// Get Ethernet type
				ipVersion    = currWord.data(119,116);					// Get IPv4
				ipProtocol   = currWord.data(191,184);					// Get protocol for IPv4 packets

				if (ethernetType == TYPE_ARP){
					tdest = 0;
					sendWord.dest = 0;
				}
				else if (ethernetType == TYPE_IPV4){
					if (ipVersion == 4){ 	// Double check
						if (ipProtocol == PROTO_ICMP ){
							tdest = 1;
						}
						else if (ipProtocol == PROTO_TCP ){
							tdest = 2;
						}
						else if (ipProtocol == PROTO_UDP ){
							tdest = 3;
						}
						else {
							forward = false;
						}
					}
				}
				else {
					forward = false;
				}

				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.last = currWord.last;
				sendWord.dest = tdest;										// Compose output word
				
				tdest_r 		= tdest;	// Save tdest

				if (forward){												// Evaluate if the packet has to be send or dropped
					dataOut.write(sendWord);
					pi_fsm_state = FWD;
				}
				else {
					pi_fsm_state = DROP;
				}

				if (currWord.last){
					pi_fsm_state = FIRST_WORD;
				}
			}
			break;
		case FWD :
			if (!dataIn.empty()){
				dataIn.read(currWord);
				
				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.last = currWord.last;
				sendWord.dest = tdest_r;
				dataOut.write(sendWord);
				
				if (currWord.last){											// Keep sending the packet until ends
					 pi_fsm_state = FIRST_WORD;
				}
			}
			break;
		case DROP :
			if (!dataIn.empty()){
				dataIn.read(currWord);
				if (currWord.last){											// Dropping the packet until ends
					 pi_fsm_state = FIRST_WORD;
			}
			break;
		}
	}	
}


/**
 * @brief      packet_handler: wrapper for packet identification and Ethernet remover
 *
 * @param      dataIn   Incoming data from the network interface, at Ethernet level
 * @param      dataOut  Output data. The tdest says which kind of packet it is.
 * 						The Ethernet header is shoved off for IPv4 packets
 *   
 */
void packet_handler(
			stream<axiWord>&			dataIn,
			stream<axiWord>&			dataOut) {

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

#pragma HLS INTERFACE axis register both port=dataIn name=s_axis
#pragma HLS INTERFACE axis register both port=dataOut name=m_axis

	static stream<axiWordi>     eth_level_pkt("eth_level_pkt");
	#pragma HLS STREAM variable=eth_level_pkt depth=16

	packet_identification(
			dataIn,
			eth_level_pkt); 

	ethernet_remover (
			eth_level_pkt,
			dataOut);

}
