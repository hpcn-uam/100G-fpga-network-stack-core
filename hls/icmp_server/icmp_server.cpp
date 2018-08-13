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
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2018 Xilinx, Inc.
************************************************/
#include "icmp_server.hpp"


ap_uint<16> byteSwap16(ap_uint<16> inputVector) {
	return (inputVector.range(7,0), inputVector(15, 8));
}

void combine_words(
					axiWord 	currentWord, 
					axiWord 	previousWord, 
					ap_uint<4> 	ip_headerlen,
					axiWord& 	sendWord){

#pragma HLS INLINE
	switch(ip_headerlen) {
		case 5:
			sendWord.data( 351,   0) 	= previousWord.data(511, 160);
			sendWord.keep(  43,   0) 	= previousWord.keep( 63,  20);
			sendWord.data( 511, 352) 	= currentWord.data( 159,   0);
			sendWord.keep(  63,  44)	= currentWord.keep(  19,   0);
			break;
		case 6:
			sendWord.data( 319,   0) 	= previousWord.data(511, 192);
			sendWord.keep(  39,   0) 	= previousWord.keep( 63,  24);
			sendWord.data( 511, 320) 	= currentWord.data( 191,   0);
			sendWord.keep(  63,  40)	= currentWord.keep(  23,   0);
			break;
		case 7:
			sendWord.data( 287,   0) 	= previousWord.data(511, 224);
			sendWord.keep(  35,   0) 	= previousWord.keep( 63,  28);
			sendWord.data( 511, 288) 	= currentWord.data( 223,   0);
			sendWord.keep(  63,  36)	= currentWord.keep(  27,   0);
			break;
		case 8:
			sendWord.data( 255,   0) 	= previousWord.data(511, 256);
			sendWord.keep(  31,   0) 	= previousWord.keep( 63,  32);
			sendWord.data( 511, 256) 	= currentWord.data( 255,   0);
			sendWord.keep(  63,  32)	= currentWord.keep(  31,   0);
			break;
		case 9:
			sendWord.data( 223,   0) 	= previousWord.data(511, 288);
			sendWord.keep(  27,   0) 	= previousWord.keep( 63,  36);
			sendWord.data( 511, 224) 	= currentWord.data( 287,   0);
			sendWord.keep(  63,  28)	= currentWord.keep(  35,   0);
			break;
		case 10:
			sendWord.data( 191,   0) 	= previousWord.data(511, 320);
			sendWord.keep(  23,   0) 	= previousWord.keep( 63,  40);
			sendWord.data( 511, 192) 	= currentWord.data( 319,   0);
			sendWord.keep(  63,  24)	= currentWord.keep(  39,   0);
			break;
		case 11:
			sendWord.data( 159,   0) 	= previousWord.data(511, 352);
			sendWord.keep(  19,   0) 	= previousWord.keep( 63,  44);
			sendWord.data( 511, 160) 	= currentWord.data( 351,   0);
			sendWord.keep(  63,  20)	= currentWord.keep(  43,   0);
			break;
		case 12:
			sendWord.data( 127,   0) 	= previousWord.data(511, 384);
			sendWord.keep(  15,   0) 	= previousWord.keep( 63,  48);
			sendWord.data( 511, 128) 	= currentWord.data( 383,   0);
			sendWord.keep(  63,  16)	= currentWord.keep(  47,   0);
			break;
		case 13:
			sendWord.data(  95,   0) 	= previousWord.data(511, 416);
			sendWord.keep(  11,   0) 	= previousWord.keep( 63,  52);
			sendWord.data( 511,  96) 	= currentWord.data( 415,   0);
			sendWord.keep(  63,  12)	= currentWord.keep(  51,   0);
			break;
		case 14:
			sendWord.data(  63,   0) 	= previousWord.data(511, 448);
			sendWord.keep(   7,   0) 	= previousWord.keep( 63,  56);
			sendWord.data( 511,  64) 	= currentWord.data( 447,   0);
			sendWord.keep(  63,   8)	= currentWord.keep(  55,   0);
			break;
		case 15:
			sendWord.data(  31,   0) 	= previousWord.data(511, 480);
			sendWord.keep(   3,   0) 	= previousWord.keep( 63,  60);
			sendWord.data( 511,  32) 	= currentWord.data( 479,   0);
			sendWord.keep(  63,   4)	= currentWord.keep(  59,   0);
			break;
		default:
			//cout << "Error the offset is not valid" << endl;
			break;
	}

}

void remove_ip_header (
			stream<axiWord>& 		dataIn,
			stream<axiWord>& 		dataOutI,
			stream<axiWord>& 		dataOutC,
			stream<ipMetaData>& 	ipInfo) {

#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum rip_states {IP_PAYLOAD , REMAINING, WRITE_EXTRA};
	static rip_states rip_fsm = IP_PAYLOAD;

	axiWord currWord;
	axiWord sendWord;
	static axiWord prevWord;
	static ap_uint<4> 	ip_headerlen;

	ipMetaData 		ipMetaInfo;
	ap_uint<16> 	totalLen_i;

	switch (rip_fsm){
		case IP_PAYLOAD: 
			if (!dataIn.empty()){
				dataIn.read(currWord);
				ip_headerlen 			= currWord.data( 3, 0); 					// Read IP header len
				totalLen_i				= byteSwap16(currWord.data(31,16));
				ipMetaInfo.srcIP    	= currWord.data(159,128);					// Get my own IP
				ipMetaInfo.dstIP    	= currWord.data(127, 96);					// It will be the destination IP in the outgoing packet
				ipMetaInfo.totalLen 	= totalLen_i - ip_headerlen*4;
				if (currWord.last){
					combine_words (axiWord(0,0,0), currWord, ip_headerlen, sendWord);
					sendWord.last = 1;
					dataOutI.write(sendWord);
					dataOutC.write(sendWord);
				}
				else {
					rip_fsm = REMAINING;
				}

				ipInfo.write(ipMetaInfo);

				prevWord = currWord;


			}
			break;
		case REMAINING: 
			if (!dataIn.empty()){
				dataIn.read(currWord);

				combine_words (currWord , prevWord , ip_headerlen , sendWord);

				if (currWord.last){
					if (currWord.keep.bit(((int)(ip_headerlen*4)))){	// If this bit is valid a extra transaction is needed
						sendWord.last = 0;
						rip_fsm = WRITE_EXTRA;
					}
					else {
						sendWord.last = 1;
						rip_fsm = IP_PAYLOAD;
					}
				}
				else {
					sendWord.last = 0;
				}

				prevWord = currWord;
				dataOutI.write(sendWord);
				dataOutC.write(sendWord);
			}
			break;

		case WRITE_EXTRA:
			combine_words (axiWord(0,0,0), prevWord, ip_headerlen, sendWord);
			sendWord.last = 1;
			dataOutI.write(sendWord);
			dataOutC.write(sendWord);
			rip_fsm = IP_PAYLOAD;
			break;
	}
}

//Echo or Echo Reply Message
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |     Type      |     Code      |          Checksum             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |           Identifier          |        Sequence Number        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |     Data ...
//   +-+-+-+-+-
//   
//   Type
//      8 for echo message;
//      0 for echo reply message.
//      
//   Code
//
//      0
//
//   Checksum
//      The checksum is the 16-bit ones's complement of the one's
//      complement sum of the ICMP message starting with the ICMP Type.
//      For computing the checksum , the checksum field should be zero.
//      If the total length is odd, the received data is padded with one
//      octet of zeros for computing the checksum.
//
//   Identifier
//      If code = 0, an identifier to aid in matching echos and replies,
//      may be zero.
//      
//   Sequence Number
//      If code = 0, a sequence number to aid in matching echos and
//      replies, may be zero.
// Description
//
//      The data received in the echo message must be returned in the echo
//      reply message.


void analyze_icmp (
			stream<axiWord>& 		dataIn,
			stream<bool>&			drop,
			stream<axiWord>& 		dataOutI,
			stream<axiWord>& 		dataOutC){
#pragma HLS INLINE off
#pragma HLS pipeline II=1	

	enum 	an_states {ICMP_HEADER, REMAINING};
	static an_states an_fsm = ICMP_HEADER;
	static bool drop_i;
	axiWord 	currWord;

	ap_uint<8> icmpType;
	ap_uint<8> icmpCode;

	switch(an_fsm){
		case ICMP_HEADER:
			if (!dataIn.empty()){
				dataIn.read(currWord);
				icmpType 	= currWord.data( 7, 0);
				icmpCode 	= currWord.data(15, 8);

				if (icmpType == ECHO_REQUEST && (icmpCode == 0) ) {
					drop_i = false;
				}
				else {
					drop_i = true;
				}

				currWord.data( 7, 0) = ECHO_REPLY;
				currWord.data(31,16) = 0;				// Clear checksum
				
				drop.write(drop_i);

				dataOutI.write(currWord);

				// Only compute the output checksum if the packet is an ECHO REQUEST
				if (drop_i == false){	
					dataOutC.write(currWord);
				}
				if (currWord.last == 0){	// There is more data
					an_fsm = REMAINING;
				}

			}
			break;
		case REMAINING:
			if (!dataIn.empty()){
				dataIn.read(currWord);
				dataOutI.write(currWord);
				
				if (drop_i == false){
					dataOutC.write(currWord);
				}
				if (currWord.last){
					an_fsm = ICMP_HEADER;
				}
			}
			break;
	}


}

/** @ingroup icmp_server
 *  Reads the checksum, if it zero the packet is process if not is dropped
 *  @param[in]		dataIn
 *  @param[in]		checksum
 *  @param[out]		dataOut
 */
void dropper (
			stream<axiWord>& 		dataIn, 
			stream<ap_uint<16> >&	checksum,
			stream<bool>& 			drop,		// echo replay check 
			stream<ipMetaData>& 	ipInfoIn,
			stream<ipMetaData>& 	ipInfoOut,
			stream<axiWord>& 		dataOut) {

#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum drop_states{READ , DROP , FWD};
	static drop_states fsm_state = READ;
	static bool metaInfoAlreadySent = false;

	axiWord currWord;
	ap_uint<16>  checksum_i;
	bool drop_i;
	ipMetaData ipMetaData_i;

	switch (fsm_state){
		case READ : 
			if (!checksum.empty() && !drop.empty()){
				checksum.read(checksum_i);
				drop.read(drop_i);
				if ((checksum_i == 0) && (drop_i==false)){
					fsm_state = FWD;
				} 
				else {
					fsm_state = DROP;
				}
				metaInfoAlreadySent = false;
			}
			break;
		case DROP : 
			if (!dataIn.empty() && (metaInfoAlreadySent || !ipInfoIn.empty())){
				dataIn.read(currWord);
				if (currWord.last){
					fsm_state = READ;
				}

				if (!metaInfoAlreadySent){
					ipInfoIn.read(ipMetaData_i);
				}
				metaInfoAlreadySent = true;

			}
			break;
		case FWD : 
			if (!dataIn.empty() && (metaInfoAlreadySent || !ipInfoIn.empty())){
				dataIn.read(currWord);
				dataOut.write(currWord);

				if (!metaInfoAlreadySent){
					ipInfoIn.read(ipMetaData_i);
					ipInfoOut.write(ipMetaData_i);
				}
				metaInfoAlreadySent = true;

				if (currWord.last){
					fsm_state = READ;
				}
			}
			break;
	}

}

void ip_insertion (
			stream<axiWord>& 			dataIn, 
			stream<ipMetaData>& 		ipInfoIn,
			stream<ap_uint<16> >&		checksum,
			stream<axiWord>& 			dataOut) {

#pragma HLS INLINE off
#pragma HLS pipeline II=1	

	enum ii_states {IP_HEADER, READ_REMAINING, SEND_EXTRA};
	static ii_states ii_fsm = IP_HEADER;

	ipMetaData 		ipInfo_i;
	axiWord 		currWord;
	axiWord 		sendWord;
	ap_uint<16> 	checksum_i;
	static axiWord 	prevWord;


	switch (ii_fsm){
		case IP_HEADER:
			if (!dataIn.empty() && !ipInfoIn.empty() && !checksum.empty()){
				dataIn.read(currWord);
				ipInfoIn.read(ipInfo_i);
				checksum.read(checksum_i);

				sendWord.data(  7,  4) = 4;										// IP version
				sendWord.data(  3,  0) = 5;										// IP Header Len
				sendWord.data( 15,  8) = 0;										// IP Type of service
				sendWord.data( 31, 16) = byteSwap16(ipInfo_i.totalLen + 20);	// IP total Len
				sendWord.data( 63, 32) = 0; 									// IP Identification and fragment offset
				sendWord.data( 71, 64) = 128;									// IP time to live
				sendWord.data( 79, 72) = 1; 									// IP protocol
				sendWord.data( 95, 80) = 0;										// IP checksum
				sendWord.data(127, 96) = ipInfo_i.srcIP;						// My own IP address
				sendWord.data(159,128) = ipInfo_i.dstIP;						// Destination IP address
				sendWord.keep( 19,  0) = 0xFFFFF;

				sendWord.data(511,160) = currWord.data(351,  0);
				sendWord.data(191,176) = byteSwap16(checksum_i);				// Overwriting the checksum
				sendWord.keep( 63, 20) = currWord.keep( 43,  0);

				if (currWord.last) {
					if (currWord.keep.bit(44)){
						sendWord.last = 0;
						ii_fsm 	= SEND_EXTRA;
					} 
					else {
						sendWord.last = 1;
					}
					
				}
				else {
					sendWord.last = 0;
					ii_fsm 	= READ_REMAINING;
				}

				prevWord = currWord;
				dataOut.write(sendWord);
			}
			break;
		case READ_REMAINING:
			if (!dataIn.empty()){
				dataIn.read(currWord);

				sendWord.data(159,  0) = prevWord.data (511,352);
				sendWord.keep( 19,  0) = prevWord.keep ( 63, 44);
				sendWord.data(511,160) = currWord.data(351,  0);
				sendWord.keep( 63, 20) = currWord.keep( 43,  0);
				if (currWord.last) {
					if (currWord.keep.bit(44)){
						sendWord.last = 0;
						ii_fsm 	= SEND_EXTRA;
					}
					else {
						sendWord.last = 1;
						ii_fsm 	= IP_HEADER;
					}
				}
				else {
					sendWord.last = 0;
				}
				prevWord = currWord;
				dataOut.write(sendWord);

			}
			break;
		case SEND_EXTRA	:
				sendWord.data(159,  0) = prevWord.data (511,352);
				sendWord.keep( 19,  0) = prevWord.keep ( 63, 44);
				sendWord.data(511,160) = 0;
				sendWord.keep( 63, 20) = 0;
				sendWord.last = 1;
				dataOut.write(sendWord);
				ii_fsm 	= IP_HEADER;

			break;
	}


}


/** @ingroup icmp_server
 *  Main function
 *  @param[in]		dataIn
 *  @param[out]		dataOut
 */
void icmp_server(
			stream<axiWord>&			dataIn,
			stream<ap_uint<16> >&		input_icmp_checksum,
			stream<ap_uint<16> >&		output_icmp_checksum,

			//stream<axiWord>&	udpIn,
			//stream<axiWord>&	ttlIn,
			stream<axiWord>&			inputIcmp2checksum,
			stream<axiWord>&			outputIcmp2checksum,
			stream<axiWord>&			dataOut) {

#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return


#pragma HLS INTERFACE axis register both port=dataIn name=s_axis_icmp
#pragma HLS INTERFACE axis register both port=dataOut name=m_axis_icmp
#pragma HLS INTERFACE axis register both port=inputIcmp2checksum name=m_icmp_input_packet
#pragma HLS INTERFACE axis register both port=outputIcmp2checksum name=m_icmp_output_packet

#pragma HLS INTERFACE axis register both port=input_icmp_checksum name=s_icmp_input_packet_checksum
#pragma HLS INTERFACE axis register both port=output_icmp_checksum name=s_icmp_output_packet_checksum	


//#pragma HLS INTERFACE port=udpIn axis
//#pragma HLS INTERFACE port=ttlIn axis

	static stream<axiWord>			icmpLevel_i("icmpLevel_i");
	static stream<axiWord>			icmp2ipInsertion("icmp2ipInsertion");
	static stream<axiWord>			icmp2dropper("icmp2dropper");

	static stream<ipMetaData> 		ipInfo2dropper("ipInfo2dropper");
	static stream<ipMetaData> 		ipInfo2ipInsertion("ipInfo2ipInsertion");
	
	static stream<bool> 			icmpEchoReplayDrop("icmpEchoReplayDrop");

	#pragma HLS STREAM variable=icmp2ipInsertion depth=8
	#pragma HLS STREAM variable=icmpLevel_i depth=64
	#pragma HLS STREAM variable=icmp2dropper depth=64
	
	#pragma HLS STREAM variable=ipInfo2dropper depth=8
	#pragma HLS STREAM variable=ipInfo2ipInsertion depth=8
	
	#pragma HLS STREAM variable=icmpEchoReplayDrop depth=8



	remove_ip_header (
			dataIn,
			icmpLevel_i,
			inputIcmp2checksum,
			ipInfo2dropper);

	analyze_icmp (
			icmpLevel_i,
			icmpEchoReplayDrop,
			icmp2dropper,
			outputIcmp2checksum);

	dropper(
			icmp2dropper,
			input_icmp_checksum,
			icmpEchoReplayDrop,
			ipInfo2dropper,
			ipInfo2ipInsertion,
			icmp2ipInsertion);

	ip_insertion (
			icmp2ipInsertion, 
			ipInfo2ipInsertion,
			output_icmp_checksum,
			dataOut);

}


