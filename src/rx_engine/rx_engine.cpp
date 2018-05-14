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

#include "rx_engine.hpp"

using namespace hls;
using namespace std;

void combine_words(
					axiWord currentWord, 
					axiWord previousWord, 
					ap_uint<4> ip_headerlen,
					axiWord* sendWord){

#pragma HLS INLINE
	switch(ip_headerlen) {
		case 5:
			sendWord->data( 447,   0) 	= previousWord.data(511,  64);
			sendWord->keep(  55,   0) 	= previousWord.keep( 63,   8);
			sendWord->data( 511, 448) 	= currentWord.data(  63,   0);
			sendWord->keep(  63,  56)	= currentWord.keep(   7,   0);
			break;
		case 6:
			sendWord->data( 415,   0) 	= previousWord.data(511,  96);
			sendWord->keep(  51,   0) 	= previousWord.keep( 63,  12);
			sendWord->data( 511, 416) 	= currentWord.data(  95,   0);
			sendWord->keep(  63,  52)	= currentWord.keep(  11,   0);
			break;
		case 7:
			sendWord->data( 383,   0) 	= previousWord.data(511, 128);
			sendWord->keep(  47,   0) 	= previousWord.keep( 63,  16);
			sendWord->data( 511, 384) 	= currentWord.data( 127,   0);
			sendWord->keep(  63,  48)	= currentWord.keep(  15,   0);
			break;
		case 8:
			sendWord->data( 351,   0) 	= previousWord.data(511, 160);
			sendWord->keep(  43,   0) 	= previousWord.keep( 63,  20);
			sendWord->data( 511, 352) 	= currentWord.data( 159,   0);
			sendWord->keep(  63,  44)	= currentWord.keep(  19,   0);
			break;
		case 9:
			sendWord->data( 319,   0) 	= previousWord.data(511, 192);
			sendWord->keep(  39,   0) 	= previousWord.keep( 63,  36);
			sendWord->data( 511, 320) 	= currentWord.data( 191,   0);
			sendWord->keep(  63,  40)	= currentWord.keep(  23,   0);
			break;
		case 10:
			sendWord->data( 287,   0) 	= previousWord.data(511, 224);
			sendWord->keep(  35,   0) 	= previousWord.keep( 63,  28);
			sendWord->data( 511, 288) 	= currentWord.data( 223,   0);
			sendWord->keep(  63,  36)	= currentWord.keep(  27,   0);
			break;
		case 11:
			sendWord->data( 255,   0) 	= previousWord.data(511, 256);
			sendWord->keep(  31,   0) 	= previousWord.keep( 63,  32);
			sendWord->data( 511, 256) 	= currentWord.data( 255,   0);
			sendWord->keep(  63,  32)	= currentWord.keep(  31,   0);
			break;
		case 12:
			sendWord->data( 223,   0) 	= previousWord.data(511, 288);
			sendWord->keep(  27,   0) 	= previousWord.keep( 63,  36);
			sendWord->data( 511, 224) 	= currentWord.data( 287,   0);
			sendWord->keep(  63,  28)	= currentWord.keep(  35,   0);
			break;
		case 13:
			sendWord->data( 191,   0) 	= previousWord.data(511, 320);
			sendWord->keep(  23,   0) 	= previousWord.keep( 63,  40);
			sendWord->data( 511, 192) 	= currentWord.data( 319,   0);
			sendWord->keep(  63,  24)	= currentWord.keep(  39,   0);
			break;
		case 14:
			sendWord->data( 159,   0) 	= previousWord.data(511, 352);
			sendWord->keep(  19,   0) 	= previousWord.keep( 63,  44);
			sendWord->data( 511, 160) 	= currentWord.data( 351,   0);
			sendWord->keep(  63,  20)	= currentWord.keep(  43,   0);
			break;
		case 15:
			sendWord->data( 127,   0) 	= previousWord.data(511, 384);
			sendWord->keep(  15,   0) 	= previousWord.keep( 63,  48);
			sendWord->data( 511, 128) 	= currentWord.data( 383,   0);
			sendWord->keep(  63,  16)	= currentWord.keep(  47,   0);
			break;
		default:
			cout << "Error the offset is not valid" << endl;
			break;
	}

}

/** @ingroup rx_engine
 * Extracts tcpLength from IP header, removes the header and prepends the IP addresses to the payload,
 * such that the output can be used for the TCP pseudo header creation
 * The TCP length is computed from the total length and the IP header length
 * @param[in]		dataIn, incoming data stream
 * @param[out]		dataOut, outgoing data stream
 * @param[out]		tcpLenFifoOut, the TCP length is stored into this FIFO
 */

/**
* In this function the main drawback is dealing with the alignment of the pseudo header
* before the TCP header. To do this is necessary to detect IP header size, after that 
* a multiplexor is needed to output the aligned word.
* In the following figure two output word are drawn. The first one is when IP does not have
* any option. The second one is when IP is plenty of options (ip_headerlen=15). It also boundaries 
* are shown.
*
*	Fig 1.a. Output word without IP options
*
*   +--------------------------------------------------------------+
*   | current   |                                           |pseudo |
*   | word      |          previous word  (511 , 160)       |header |
*   | (63,0)    |                                           |       |
*   +---------------------------------------------------------------+
*  512         448                                         96       0
*
*	Fig 1.b. Output word plenty of options IP header size(60 bytes)
*
*   +---------------------------------------------------------------+
*   |                                             | previous|pseudo |
*   |          current word (383,0)               |    word |header |
*   |                                             |(511,480)|       |
*   +---------------------------------------------------------------+
*  512                                           128       96       0
*
*	Fig 1.c. Generalisation of the output word.
*
*   +---------------------------------------------------------------+
*   |                                   |    previous word  |pseudo |
*   |          current word (y,0)       |       (511,x)     |header |
*   |                                   |                   |       |
*   +---------------------------------------------------------------+
*  512                                512-y                 96      0
*
*   x = prevword_right_boundary
*	y = currword_left_boundary
*/

void rxTCP_pseudoheader_insert(
								stream<axiWord>&			dataIn,
								stream<axiWord>&			dataOut) 
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	axiWord 				currWord;
	axiWord 				sendWord;
	ap_uint<16>				tcpTotalLen;
	static axiWord 			prevWord;
	static ap_uint<4> 		ip_headerlen;
	static ap_uint<16>		ipTotalLen;
	static ap_uint<6>		keep_extra;
	static ap_uint<32>		ip_dst;
	static ap_uint<32>		ip_src;
	static ap_uint<1>		pseudo_header = 0;
	static ap_uint<1> 		extra_word=0;

	enum pseudo_header_state {IP_HEADER ,TCP_PAYLOAD};
	static pseudo_header_state fsm_state = IP_HEADER;

	if (extra_word) {
		extra_word 	  = 0;
		currWord.data = 0;
		currWord.keep = 0;
		combine_words( currWord, prevWord, ip_headerlen, &sendWord);
		sendWord.last 			= 1;
		dataOut.write(sendWord);
	}
	else if (!dataIn.empty()){
		switch (fsm_state){
			case (IP_HEADER):
				dataIn.read(currWord);
				ip_headerlen 		= currWord.data( 3, 0); 	// Read IP header len
				ipTotalLen(7, 0) 	= currWord.data(31, 24);    // Read IP total len
				ipTotalLen(15, 8) 	= currWord.data(23, 16);
				ip_src 				= currWord.data(127,96);
				ip_dst 				= currWord.data(159,128);

				keep_extra = 8 + (ip_headerlen-5) * 4;
				prevWord = currWord;
				if (currWord.last){
					currWord.data = 0;
					currWord.keep = 0;

				 	combine_words( currWord, prevWord, ip_headerlen, &sendWord);
					
					tcpTotalLen = ipTotalLen - (ip_headerlen *4);
					sendWord.data( 63 ,0) 	= (ip_dst,ip_src);
					sendWord.data( 79 ,64)	= 0x0600;
					sendWord.data(87 ,80) 	= tcpTotalLen(15,8);
					sendWord.data(95 ,88) 	= tcpTotalLen( 7,0);
					sendWord.keep(11,0) 	= 0xFFF;
					sendWord.last 			= 1;
					
					dataOut.write(sendWord);
				}
				else{
					pseudo_header = 1;
					fsm_state = TCP_PAYLOAD;
				}
				break;
			case (TCP_PAYLOAD) :
				dataIn.read(currWord);
				combine_words( currWord, prevWord, ip_headerlen, &sendWord);
				if (pseudo_header){
					pseudo_header = 0;
					tcpTotalLen = ipTotalLen - (ip_headerlen *4);
					sendWord.data( 63 ,0) 	= (ip_dst,ip_src);
					sendWord.data( 79 ,64)	= 0x0600;
					sendWord.data(87 ,80) 	= tcpTotalLen(15,8);
					sendWord.data(95 ,88) 	= tcpTotalLen( 7,0);
					sendWord.keep(11,0) 	= 0xFFF;
				}
				sendWord.last = 0;
				prevWord = currWord;
				if (currWord.last){
					if (currWord.keep.bit(keep_extra)) {
						extra_word = 1;
					}
					else {
						sendWord.last=1;
					}
					fsm_state = IP_HEADER;
				}
				dataOut.write(sendWord);
			 	break;
			default:
				fsm_state = IP_HEADER;
				break;
		}
	}
}

/** @ingroup rx_engine
 *  This module gets the packet at Pseudo TCP layer.
 *  First of all, it removes the pseudo TCP header and forward the payload if any.
 *  If the res_checksum is 0, the following action are performed
 *  It extracts some metadata and the IP tuples from
 *  the TCP pseudo packet and writes it to @p metaDataFifoOut
 *  and @p tupleFifoOut
 *  Additionally, it sends the destination port number to the @ref port_table
 *  to check if the port is open.
 *  @param[in]		pseudo_packet
 *  @param[in]		res_checksum
 *  @param[out]		payload
 *  @param[out]		payload
 *  @param[out]		metaDataFifoOut
 *  @param[out]		tupleFifoOut
 *  @param[out]		portTableOut
 */
void rxEng_Generate_Metadata(
							stream<axiWord>&				pseudo_packet,
							stream<ap_uint<16> >&			res_checksum,
							stream<axiWord>&				payload,
							stream<bool>&					correct_checksum,				
							stream<rxEngineMetaData>&		metaDataFifoOut,		// TODO, maybe merge with tupleFifoOut
							stream<fourTuple>&				tupleFifoOut,			// TODO, maybe merge with metaDataFifoOut
							stream<ap_uint<16> >&			portTableOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	axiWord 				currWord;
	axiWord 				sendWord;
	static axiWord 			prevWord;

	static rxEngineMetaData rxMetaInfo;
	static fourTuple 		rxTupleInfo;
	static bool 			first_word=true;
	static ap_uint<16> 		dstPort;
	static ap_uint<4> 		tcp_offset;
	static bool 			short_packet = false;
	static bool 			extra_trans  = false;

	static ap_uint<16> 		payload_length;

	ap_uint<16> 			rtl_checksum;
	bool 					correct_checksum_i;


	if (!pseudo_packet.empty() && !short_packet && !extra_trans){
		
		pseudo_packet.read(currWord);
		
		if (first_word){
			first_word=false;
			// Get four tuple info
			dstPort 			= currWord.data(127,112);
			// Do not swap info
			rxTupleInfo.srcIp	= currWord.data(31 ,  0);
			rxTupleInfo.dstIp	= currWord.data(63 , 32);
			rxTupleInfo.srcPort	= currWord.data(111, 96);
			rxTupleInfo.dstPort	= currWord.data(127,112);
			tcp_offset 			= currWord.data(199 ,196);
			payload_length 		= (currWord.data(87 ,80),currWord.data(95 ,88)) - tcp_offset * 4;
			// Get Meta Info
			rxMetaInfo.seqNumb	= ((ap_uint<8>)currWord.data(135,128),(ap_uint<8>)currWord.data(143,136),(ap_uint<8>)currWord.data(151,144),(ap_uint<8>)currWord.data(159,152));
			rxMetaInfo.ackNumb	= ((ap_uint<8>)currWord.data(167,160),(ap_uint<8>)currWord.data(175,168),(ap_uint<8>)currWord.data(183,176),(ap_uint<8>)currWord.data(191,184));
			rxMetaInfo.winSize	= (currWord.data(215,208),currWord.data(223,216));
			rxMetaInfo.length 	= payload_length;
			rxMetaInfo.cwr 		= currWord.data.bit(207);
			rxMetaInfo.ecn 		= currWord.data.bit(206);
			rxMetaInfo.urg 		= currWord.data.bit(205);
			rxMetaInfo.ack 		= currWord.data.bit(204);
			rxMetaInfo.psh 		= currWord.data.bit(203);
			rxMetaInfo.rst 		= currWord.data.bit(202);
			rxMetaInfo.syn 		= currWord.data.bit(201);
			rxMetaInfo.fin 		= currWord.data.bit(200);

			short_packet 		= currWord.last;

		}
		else{ // TODO: if tcp_offset > 13 the payload starts in the second transaction
			sendWord.data = ((currWord.data(tcp_offset*32-1 + 96,0)) , prevWord.data(511,tcp_offset*32 + 96));
			sendWord.keep = ((currWord.keep(tcp_offset* 4-1 + 12,0)) , prevWord.keep(63, tcp_offset* 4 + 12));

			if (currWord.last && currWord.keep.bit((int)(tcp_offset* 4 + 12))){
				sendWord.last 	= 0;
				extra_trans  	= true;
			}
			else
				sendWord.last = currWord.last;
			payload.write(sendWord);
		}

		prevWord = currWord;

		if(currWord.last){
			first_word=true;
		}
	}
	else if(!res_checksum.empty()){
		res_checksum.read(rtl_checksum);
		correct_checksum_i = (rtl_checksum==0);				// If the compute checksum is equal to zero, then the packet must be forwarded
		
		if (payload_length !=0)								// If there is payload send the result of the checksum
			correct_checksum.write(correct_checksum_i);
		
		if (correct_checksum_i){
			metaDataFifoOut.write(rxMetaInfo);
			tupleFifoOut.write(rxTupleInfo);
			portTableOut.write(dstPort);
		}

		if (short_packet){
			short_packet = false;
			if (payload_length !=0){
				sendWord 	  		= axiWord(0,0,1);
				sendWord.data 		= prevWord.data(511,tcp_offset*32 + 96);
				sendWord.keep 		= prevWord.keep(63, tcp_offset* 4 + 12);
				payload.write(sendWord);
			}
		}
		else if(extra_trans){
			extra_trans = false;
			sendWord 	  		= axiWord(0,0,1);
			sendWord.data 		= prevWord.data(511,tcp_offset*32 + 96);
			sendWord.keep 		= prevWord.keep(63, tcp_offset* 4 + 12);
			payload.write(sendWord);
		}
	}
	


}
	

/** @ingroup rx_engine
 *  For each packet it reads the valid value from @param correct_checksum
 *  If the packet checksum is correct the data stream is forwarded.
 *  If it is not correct it is dropped
 *  @param[in]		dataIn, incoming data stream
 *  @param[in]		correct_checksum, packet checksum if is "true" the incoming packet is correct if is not discard the packet
 *  @param[out]		dataOut, outgoing data stream
 */
void rxTcpInvalidDropper(
							stream<axiWord>&			dataIn,
							stream<bool >&				correct_checksum,
							stream<axiWord>&			dataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	axiWord currWord;

	static bool reading_data 	= false;
	static bool forward_packet 	= false;

	if (!correct_checksum.empty() && !reading_data){
		correct_checksum.read(forward_packet); 			// if it is "true" the packet has to be forwarded
		reading_data = true;
	}
	else if (!dataIn.empty() && reading_data){
		dataIn.read(currWord);

		if (forward_packet){
			dataOut.write(currWord);
		}
		if (currWord.last){
			reading_data = false;
		}
	}
}


/**
 * @brief      { function_description }
 *
 * @param[In]    metaDataFifoIn           Metadata of the incomming packet
 * @param[In]    tupleBufferIn            The tuple buffer in
 * @param[In]    portTable2rxEng_rsp      Port is open?
 * @param[In]    sLookup2rxEng_rsp        Look up response, it carries if the four tuple is in the table and its ID
 * @param[Out]   rxEng2sLookup_req        Session look up, it carries the four tuple and if creation is allowed
 * @param[Out]   rxEng2eventEng_setEvent  Set event
 * @param[Out]   dropDataFifoOut          The drop data fifo out
 * @param[Out]   fsmMetaDataFifo          The fsm meta data fifo
 */
void rxMetadataHandler(	
			stream<rxEngineMetaData>&				metaDataFifoIn,
			stream<fourTuple>&						tupleBufferIn,
			stream<bool>&							portTable2rxEng_rsp,
			stream<sessionLookupReply>&				sLookup2rxEng_rsp,
			stream<sessionLookupQuery>&				rxEng2sLookup_req,
			stream<extendedEvent>&					rxEng2eventEng_setEvent,
			stream<bool>&							dropDataFifoOut,
			stream<rxFsmMetaData>&					fsmMetaDataFifo)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum mhStateType {META, LOOKUP};
	
	static rxEngineMetaData 	mh_meta;
	static sessionLookupReply 	mh_lup;
	static mhStateType 			mh_state = META;
	static ap_uint<32> 			mh_srcIpAddress;
	static ap_uint<16> 			mh_dstIpPort;
	fourTuple 					tuple;
	fourTuple 					switchedTuple;
	bool 						portIsOpen;

	switch (mh_state) {
		case META:
			if (!metaDataFifoIn.empty() && !tupleBufferIn.empty() && !portTable2rxEng_rsp.empty()) {
				metaDataFifoIn.read(mh_meta);
				tupleBufferIn.read(tuple);
				portTable2rxEng_rsp.read(portIsOpen);
				mh_srcIpAddress = byteSwap32(tuple.srcIp);			// Swap source IP and destination port
				mh_dstIpPort 	= byteSwap16(tuple.dstPort);

				if (!portIsOpen) {									// Check if port is closed
					// SEND RST+ACK
					if (!mh_meta.rst) {
						// send necessary tuple through event
						switchedTuple.srcIp 	= tuple.dstIp;
						switchedTuple.dstIp 	= tuple.srcIp;
						switchedTuple.srcPort 	= tuple.dstPort;
						switchedTuple.dstPort 	= tuple.srcPort;
						if (mh_meta.syn || mh_meta.fin) {
							rxEng2eventEng_setEvent.write(extendedEvent(rstEvent(mh_meta.seqNumb+mh_meta.length+1), switchedTuple)); //always 0
						}
						else {
							rxEng2eventEng_setEvent.write(extendedEvent(rstEvent(mh_meta.seqNumb+mh_meta.length), switchedTuple));
						}
					} 
					//else ignore => do nothing
					if (mh_meta.length != 0) {			// Drop payload because port is closed
						dropDataFifoOut.write(true);
					}
				}
				else { // Port is open. Make session lookup, only allow creation of new entry when SYN or SYN_ACK
					rxEng2sLookup_req.write(sessionLookupQuery(tuple, (mh_meta.syn && !mh_meta.rst && !mh_meta.fin)));
					mh_state = LOOKUP;
				}
			}
			break;
		case LOOKUP: //BIG delay here, waiting for LOOKup
			if (!sLookup2rxEng_rsp.empty()) {
				sLookup2rxEng_rsp.read(mh_lup);
				if (mh_lup.hit) {
					//Write out lup and meta
					fsmMetaDataFifo.write(rxFsmMetaData(mh_lup.sessionID, mh_srcIpAddress, mh_dstIpPort, mh_meta));
				}
				if (mh_meta.length != 0) {
					dropDataFifoOut.write(!mh_lup.hit);
				}
	//			if (!mh_lup.hit)
	//			{
	//				// Port is Open, but we have no sessionID, that matches or is free
	//				// For SYN we should time out, for everything else sent RST TODO
	//				if (mh_meta.length != 0) {
	//					dropDataFifoOut.write(true); // always write???
	//				}
	//				//mh_state = META;
	//			}
	//			else {
	//				//Write out lup and meta
	//				fsmMetaDataFifo.write(rxFsmMetaData(mh_lup.sessionID, mh_srcIpAddress, mh_dstIpPort, mh_meta));
	//
	//				// read state
	//				rxEng2stateTable_upd_req.write(stateQuery(mh_lup.sessionID));
	//				// read rxSar & txSar
	//				if (!(mh_meta.syn && !mh_meta.rst && !mh_meta.fin)) // Do not read rx_sar for SYN(+ACK)(+ANYTHING) => (!syn || rst || fin
	//				{
	//					rxEng2rxSar_upd_req.write(rxSarRecvd(mh_lup.sessionID));
	//				}
	//				if (mh_meta.ack) // Do not read for SYN (ACK+ANYTHING)
	//				{
	//					rxEng2txSar_upd_req.write(rxTxSarQuery(mh_lup.sessionID));
	//				}
	//				//mh_state = META;
	//			//}
				mh_state = META;
			}

			break;
	}//switch
}

/** @ingroup rx_engine
 * The module contains 2 state machines nested into each other. The outer state machine
 * loads the metadata and does the session lookup. The inner state machine then evaluates all
 * this data. This inner state machine mostly represents the TCP state machine and contains
 * all the logic how to update the metadata, what events are triggered and so on. It is the key
 * part of the @ref rx_engine.
 * @param[in]	metaDataFifoIn
 * @param[in]	sLookup2rxEng_rsp
 * @param[in]	stateTable2rxEng_upd_rsp
 * @param[in]	portTable2rxEng_rsp
 * @param[in]	tupleBufferIn
 * @param[in]	rxSar2rxEng_upd_rsp
 * @param[in]	txSar2rxEng_upd_rsp
 * @param[out]	rxEng2sLookup_req
 * @param[out]	rxEng2stateTable_req
 * @param[out]	rxEng2rxSar_upd_req
 * @param[out]	rxEng2txSar_upd_req
 * @param[out]	rxEng2timer_clearRetransmitTimer
 * @param[out]	rxEng2timer_setCloseTimer
 * @param[out]	openConStatusOut
 * @param[out]	rxEng2eventEng_setEvent
 * @param[out]	dropDataFifoOut
 * @param[out]	rxBufferWriteCmd
 * @param[out]	rxEng2rxApp_notification
 */

void rxTcpFSM(			stream<rxFsmMetaData>&					fsmMetaDataFifo,
						stream<sessionState>&					stateTable2rxEng_upd_rsp,
						stream<rxSarEntry>&						rxSar2rxEng_upd_rsp,
						stream<rxTxSarReply>&					txSar2rxEng_upd_rsp,
						stream<stateQuery>&						rxEng2stateTable_upd_req,
						stream<rxSarRecvd>&						rxEng2rxSar_upd_req,
						stream<rxTxSarQuery>&					rxEng2txSar_upd_req,
						stream<rxRetransmitTimerUpdate>&		rxEng2timer_clearRetransmitTimer,
						stream<ap_uint<16> >&					rxEng2timer_clearProbeTimer,
						stream<ap_uint<16> >&					rxEng2timer_setCloseTimer,
						stream<openStatus>&						openConStatusOut,
						stream<event>&							rxEng2eventEng_setEvent,
						stream<bool>&							dropDataFifoOut,
#if (!RX_DDR_BYPASS)
						stream<mmCmd>&							rxBufferWriteCmd,
#endif
						stream<appNotification>&				rxEng2rxApp_notification, 	// The notification are use both with DDR or no DDR
						stream<txApp_client_status>& 			rxEng2txApp_client_notification				
						)	
{
#pragma HLS LATENCY max=2
#pragma HLS INLINE off
#pragma HLS pipeline II=1


	enum fsmStateType {LOAD, TRANSITION};
	static fsmStateType fsm_state = LOAD;

	static rxFsmMetaData fsm_meta;
	static bool fsm_txSarRequest = false;


	static ap_uint<4> control_bits = 0;
	sessionState tcpState;
	rxSarEntry rxSar;
	rxTxSarReply txSar;


	switch(fsm_state) {
		case LOAD:
			if (!fsmMetaDataFifo.empty()) {
				fsmMetaDataFifo.read(fsm_meta);
				
				rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID)); // query the current session state
				rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID)); 		// Always read rxSar, even though not required for SYN-ACK. Query 
				
				if (fsm_meta.meta.ack) {// Do not read for SYN (ACK+ANYTHING)
					rxEng2txSar_upd_req.write(rxTxSarQuery(fsm_meta.sessionID)); // read txSar
					fsm_txSarRequest  = true;
				}

				control_bits[0] = fsm_meta.meta.ack; 	// Compose selection signal
				control_bits[1] = fsm_meta.meta.syn;
				control_bits[2] = fsm_meta.meta.fin;
				control_bits[3] = fsm_meta.meta.rst;
				
				fsm_state = TRANSITION;
			}
			break;
		case TRANSITION:
			// Check if transition to LOAD occurs
			if (!stateTable2rxEng_upd_rsp.empty() && !rxSar2rxEng_upd_rsp.empty()
							&& !(fsm_txSarRequest && txSar2rxEng_upd_rsp.empty())) {

				if (fsm_txSarRequest) {
					txSar2rxEng_upd_rsp.read(txSar); // FIXME for default it was a non-block read. Why?
				}

				fsm_txSarRequest = false;

				stateTable2rxEng_upd_rsp.read(tcpState);
				rxSar2rxEng_upd_rsp.read(rxSar);
				
				fsm_state = LOAD;

			} // When all responses needed are valid proceed
			
			switch (control_bits) {
				case 1: //ACK
					if (fsm_state == LOAD) {
						rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, (fsm_meta.meta.ackNumb == txSar.nextByte))); 		// Reset Retransmit Timer
						if (tcpState == ESTABLISHED || tcpState == SYN_RECEIVED || tcpState == FIN_WAIT_1 || tcpState == CLOSING || tcpState == LAST_ACK) {
							// Check if new ACK arrived
							if (fsm_meta.meta.ackNumb == txSar.prevAck && txSar.prevAck != txSar.nextByte) {
								txSar.count++; // Not new ACK has arrived increase counter
							}
							else {
								// Notify probeTimer about new ACK
								if (fsm_meta.meta.ackNumb == txSar.nextByte){				// Only notify probe timer if ACK is the expected
									rxEng2timer_clearProbeTimer.write(fsm_meta.sessionID);
								}
								// Check for SlowStart & Increase Congestion Window
								if (txSar.cong_window <= (txSar.slowstart_threshold-MSS)) {
									txSar.cong_window += MSS;
								}
								else if (txSar.cong_window <= 0xF7FF) {
									txSar.cong_window += 365; //TODO replace by approx. of (MSS x MSS) / cong_window
								}
								txSar.count = 0;
							}
							// TX SAR
							if ((txSar.prevAck <= fsm_meta.meta.ackNumb && fsm_meta.meta.ackNumb <= txSar.nextByte)
									|| ((txSar.prevAck <= fsm_meta.meta.ackNumb || fsm_meta.meta.ackNumb <= txSar.nextByte) && txSar.nextByte < txSar.prevAck)) {
								rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, txSar.count)));
							}

							// Check if packet contains payload
							if (fsm_meta.meta.length != 0){
								ap_uint<32> newRecvd = fsm_meta.meta.seqNumb+fsm_meta.meta.length;
								// Second part makes sure that app pointer is not overtaken
								ap_uint<16> free_space = ((rxSar.appd - rxSar.recvd(15, 0)) - 1);
								// Check if segment is in order and if enough free space is available
								if ((fsm_meta.meta.seqNumb == rxSar.recvd) && (free_space > fsm_meta.meta.length)) {
									rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, newRecvd, 1));
									// Build memory address
									ap_uint<32> pkgAddr;
									pkgAddr(31, 30) = 0x0;
									pkgAddr(29, 16) = fsm_meta.sessionID(13, 0);
									pkgAddr(15, 0) = fsm_meta.meta.seqNumb(15, 0);					// TODO maybe align to the beginning of the buffer
#if (!RX_DDR_BYPASS)
									rxBufferWriteCmd.write(mmCmd(pkgAddr, fsm_meta.meta.length));
#endif
									// Only notify about  new data available
									rxEng2rxApp_notification.write(appNotification(fsm_meta.sessionID, fsm_meta.meta.length, fsm_meta.srcIpAddress, fsm_meta.dstIpPort));
									dropDataFifoOut.write(false);
								}
								else {
									dropDataFifoOut.write(true);
								}
							}
							if (txSar.count == 3) {
								rxEng2eventEng_setEvent.write(event(RT, fsm_meta.sessionID));
							}
							else if (fsm_meta.meta.length != 0) { // Send ACK
								rxEng2eventEng_setEvent.write(event(ACK, fsm_meta.sessionID));
							}
							

							if (fsm_meta.meta.ackNumb == txSar.nextByte) {
								switch (tcpState) {
									case SYN_RECEIVED:
										rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, ESTABLISHED, 1)); //TODO MAYBE REARRANGE
										// Notify the tx App about new client
										// TODO 1460 is the default value, but it could change if options are presented in the TCP header
										rxEng2txApp_client_notification.write(txApp_client_status(fsm_meta.sessionID, 0, 1460 , TCP_NODELAY, true)); 
										break;
									case CLOSING:
										rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, TIME_WAIT, 1));
										rxEng2timer_setCloseTimer.write(fsm_meta.sessionID);
										break;
									case LAST_ACK:
										rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, CLOSED, 1));
										break;
									default:
										//rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));	// TODO I think this is not necessary
										break;
								}
							}
							else { //we have to release the lock
								//reset rtTimer
								//rtTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID));
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1)); // or ESTABLISHED
							}
						} //end state if
						// TODO if timewait just send ACK, can it be time wait??
						else {// state == (CLOSED || SYN_SENT || CLOSE_WAIT || FIN_WAIT_2 || TIME_WAIT)
							// SENT RST, RFC 793: fig.11
							rxEng2eventEng_setEvent.write(rstEvent(fsm_meta.sessionID, fsm_meta.meta.seqNumb+fsm_meta.meta.length)); // noACK ?
							
							if (fsm_meta.meta.length != 0) { // if data is in the pipe it needs to be dropped
								dropDataFifoOut.write(true);
							}
							rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
						}
					}
					break;
				case 2: //SYN
					//if (!stateTable2rxEng_upd_rsp.empty())
					if (fsm_state == LOAD) {
						if (tcpState == CLOSED || tcpState == SYN_SENT) {// Actually this is LISTEN || SYN_SENT
							// Simultaneous open is supported due to (tcpState == SYN_SENT)
							// Initialize rxSar, SEQ + phantom byte, last '1' for makes sure appd is initialized
							rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1));
							// Initialize receive window
							rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, 0, fsm_meta.meta.winSize, txSar.cong_window, 0))); //TODO maybe include count check
							// Set SYN_ACK event
							rxEng2eventEng_setEvent.write(event(SYN_ACK, fsm_meta.sessionID));
							// Change State to SYN_RECEIVED
							rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, SYN_RECEIVED, 1));
						}
						else if (tcpState == SYN_RECEIVED) {// && mh_meta.seqNumb+1 == rxSar.recvd) // Maybe Check for seq
							// If it is the same SYN, we resent SYN-ACK, almost like quick RT, we could also wait for RT timer
							if (fsm_meta.meta.seqNumb+1 == rxSar.recvd) {
								// Retransmit SYN_ACK
								rxEng2eventEng_setEvent.write(event(SYN_ACK, fsm_meta.sessionID, 1));
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
							}
							else { // Sent RST, RFC 793: fig.9 (old) duplicate SYN(+ACK)
								rxEng2eventEng_setEvent.write(rstEvent(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1)); //length == 0
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, CLOSED, 1));
							}
						}
						else {// Any synchronized state
							// Unexpected SYN arrived, reply with normal ACK, RFC 793: fig.10
							rxEng2eventEng_setEvent.write(event(ACK_NODELAY, fsm_meta.sessionID));
							// TODo send RST, has no ACK??
							// Respond with RST, no ACK, seq ==
							//eventEngine.write(rstEvent(mh_meta.seqNumb, mh_meta.length, true));
							//rxEng2eventEng_setEvent.write(rstEvent(fsm_meta.sessionID, fsm_meta.meta.seqNumb+fsm_meta.meta.length+1)); // FIXME is that correct? MR
							rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
						}
					}
					break;
				case 3: //SYN_ACK
					if (fsm_state == LOAD) {
						rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, (fsm_meta.meta.ackNumb == txSar.nextByte))); // Clear SYN retransmission time if ack number is correct
						
						if (tcpState == SYN_SENT) { // A SYN was already send, ack number has to be check, if is correct send ACK is not send a RST
							if (fsm_meta.meta.ackNumb == txSar.nextByte) { // SYN-ACK is correct
								
								rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1)); //initialize rx_sar, SEQ + phantom byte, last '1' for appd init
								rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, 0))); //CHANGE this was added //TODO maybe include count check
								rxEng2eventEng_setEvent.write(event(ACK_NODELAY, fsm_meta.sessionID)); 				// set ACK event
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, ESTABLISHED, 1)); 	// Update TCP FSM to ESTABLISHED now data can be transfer 

								openConStatusOut.write(openStatus(fsm_meta.sessionID, true));
							}
							else{ //TODO is this the correct procedure?
								// Sent RST, RFC 793: fig.9 (old) duplicate SYN(+ACK)
								rxEng2eventEng_setEvent.write(rstEvent(fsm_meta.sessionID, fsm_meta.meta.seqNumb+fsm_meta.meta.length+1)); 
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, CLOSED, 1));
							}
						}
						else {
							// Unexpected SYN arrived, reply with normal ACK, RFC 793: fig.10
							rxEng2eventEng_setEvent.write(event(ACK_NODELAY, fsm_meta.sessionID));
							rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
						}
					}
					break;
				case 5: //FIN (_ACK)

					if (fsm_state == LOAD) {
						rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, (fsm_meta.meta.ackNumb == txSar.nextByte)));
						// Check state and if FIN in order, Current out of order FINs are not accepted
						if ((tcpState == ESTABLISHED || tcpState == FIN_WAIT_1 || tcpState == FIN_WAIT_2) && (rxSar.recvd == fsm_meta.meta.seqNumb)) {
							rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, txSar.count))); //TODO include count check

							// +1 for phantom byte, there might be data too
							rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+fsm_meta.meta.length+1, 1)); //diff to ACK

							// Clear the probe timer
							rxEng2timer_clearProbeTimer.write(fsm_meta.sessionID);

							// Check if there is payload
							if (fsm_meta.meta.length != 0) {
								ap_uint<32> pkgAddr;
								pkgAddr(31, 30) = 0x0;
								pkgAddr(29, 16) = fsm_meta.sessionID(13, 0);
								pkgAddr(15, 0) = fsm_meta.meta.seqNumb(15, 0);
#if (!RX_DDR_BYPASS)
								rxBufferWriteCmd.write(mmCmd(pkgAddr, fsm_meta.meta.length));
#endif
								// Tell Application new data is available and connection got closed
								rxEng2rxApp_notification.write(appNotification(fsm_meta.sessionID, fsm_meta.meta.length, fsm_meta.srcIpAddress, fsm_meta.dstIpPort, true));
								dropDataFifoOut.write(false);
							}
							else if (tcpState == ESTABLISHED) {
								// Tell Application connection got closed
								rxEng2rxApp_notification.write(appNotification(fsm_meta.sessionID, fsm_meta.srcIpAddress, fsm_meta.dstIpPort, true)); //CLOSE
							}
							// Update state
							if (tcpState == ESTABLISHED) {
								rxEng2eventEng_setEvent.write(event(FIN, fsm_meta.sessionID));
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, LAST_ACK, 1));
							}
							else {//FIN_WAIT_1 || FIN_WAIT_2
								if (fsm_meta.meta.ackNumb == txSar.nextByte) {//check if final FIN is ACK'd -> LAST_ACK
									rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, TIME_WAIT, 1));
									rxEng2timer_setCloseTimer.write(fsm_meta.sessionID);
								}
								else {
									rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, CLOSING, 1));
								}
								rxEng2eventEng_setEvent.write(event(ACK, fsm_meta.sessionID));
							}
						}
						else {// NOT (ESTABLISHED || FIN_WAIT_1 || FIN_WAIT_2)
							rxEng2eventEng_setEvent.write(event(ACK, fsm_meta.sessionID));
							rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
							// If there is payload we need to drop it
							if (fsm_meta.meta.length != 0) {
								dropDataFifoOut.write(true);
							}
						}
					}
					break;
				default: //TODO MAYBE load everything all the time
					// stateTable is locked, make sure it is released in at the end
					if (fsm_state == LOAD) {
						// Handle if RST
						if (fsm_meta.meta.rst) {
							if (tcpState == SYN_SENT) {//TODO this would be a RST,ACK i think
								if (fsm_meta.meta.ackNumb == txSar.nextByte) {// Check if matching SYN
									//tell application, could not open connection
									openConStatusOut.write(openStatus(fsm_meta.sessionID, false));
									rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, CLOSED, 1));
									rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, true));
								}
								else {
									// Ignore since not matching
									rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
								}
							}
							else {
								// Check if in window
								if (fsm_meta.meta.seqNumb == rxSar.recvd) {
									//tell application, RST occurred, abort
									rxEng2rxApp_notification.write(appNotification(fsm_meta.sessionID, fsm_meta.srcIpAddress, fsm_meta.dstIpPort, true)); //RESET
									rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, CLOSED, 1)); //TODO maybe some TIME_WAIT state
									rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, true));
								}
								else {
									// Ingore since not matching window
									rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
								}
							}
						}
						else {// Handle non RST bogus packages
						
							//TODO maybe sent RST ourselves, or simply ignore
							// For now ignore, sent ACK??
							//eventsOut.write(rstEvent(mh_meta.seqNumb, 0, true));
							rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
						}
					} 
					break;
			} //switch control_bits
		break;
	} //switch state
}

/** @ingroup rx_engine
 *	Drops packets if their metadata did not match / are invalid, as indicated by @param dropBuffer
 *	@param[in]		dataIn, incoming data stream
 *	@param[in]		dropFifoIn, Drop-FIFO indicating if packet needs to be dropped
 *	@param[out]		rxBufferDataOut, outgoing data stream
 */

void rxPacketDropper(stream<axiWord>&		dataIn,
					  stream<bool>&			dropFifoIn1,
					  stream<bool>&			dropFifoIn2,
					  stream<axiWord>&		rxBufferDataOut) {
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum tpfStateType {READ_DROP1, READ_DROP2, FWD, DROP};
	static tpfStateType tpf_state = READ_DROP1;

	bool drop;

	switch (tpf_state) {
	case READ_DROP1: //Drop1
		if (!dropFifoIn1.empty()) {
			dropFifoIn1.read(drop);
			if (drop) {
				tpf_state = DROP;
			}
			else {
				tpf_state = READ_DROP2;
			}
		}
		break;
	case READ_DROP2:
		if (!dropFifoIn2.empty()) {
			dropFifoIn2.read(drop);
			if (drop) {
				tpf_state = DROP;
			}
			else {
				tpf_state = FWD;
			}
		}
		break;
	case FWD:
		if(!dataIn.empty() && !rxBufferDataOut.full()) {
			axiWord currWord = dataIn.read();
			if (currWord.last) {
				tpf_state = READ_DROP1;
			}
			rxBufferDataOut.write(currWord);
		}
		break;
	case DROP:
		if(!dataIn.empty()) {
			axiWord currWord = dataIn.read();
			if (currWord.last) {
				tpf_state = READ_DROP1;
			}
		}
		break;
	} // switch
}

/** @ingroup rx_engine
 *  Delays the notifications to the application until the data is actually written to memory
 *  @param[in]		rxWriteStatusIn, the status which we get back from the DATA MOVER it indicates if the write was successful
 *  @param[in]		internalNotificationFifoIn, incoming notifications
 *  @param[out]		notificationOut, outgoing notifications
 *  @TODO Handle unsuccessful write to memory
 */

void rxAppNotificationDelayer(	
								stream<mmStatus>&				rxWriteStatusIn, 
								stream<appNotification>&		internalNotificationFifoIn,
								stream<appNotification>&		notificationOut, 
								stream<ap_uint<1> >&			doubleAccess) 
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	static stream<appNotification> rand_notificationBuffer("rand_notificationBuffer");
	#pragma HLS STREAM variable=rand_notificationBuffer depth=512 //depends on memory delay
	#pragma HLS DATA_PACK variable=rand_notificationBuffer

	static ap_uint<1>		rxAppNotificationDoubleAccessFlag = false;
	static ap_uint<5>		rand_fifoCount = 0;
	static mmStatus			rxAppNotificationStatus1;
	static appNotification	rxAppNotification;
	mmStatus				rxAppNotificationStatus2;


	if (rxAppNotificationDoubleAccessFlag == true) {
		if(!rxWriteStatusIn.empty()) {
			rxWriteStatusIn.read(rxAppNotificationStatus2);
			rand_fifoCount--;
			if (rxAppNotificationStatus1.okay && rxAppNotificationStatus2.okay)	// If one of both writes were wrong a retransmission is needed to fill the gap
				notificationOut.write(rxAppNotification);
			rxAppNotificationDoubleAccessFlag = false;
		}
	}
	else if (rxAppNotificationDoubleAccessFlag == false) {
		if(!rxWriteStatusIn.empty() && !rand_notificationBuffer.empty() && !doubleAccess.empty()) {
			rxWriteStatusIn.read(rxAppNotificationStatus1);
			rand_notificationBuffer.read(rxAppNotification);
			doubleAccess.read(rxAppNotificationDoubleAccessFlag);				// Read the double notification flag. If true then go and wait for the second status 	
			if (!rxAppNotificationDoubleAccessFlag) {							// if the memory access was not broken down in two for this segment
				rand_fifoCount--;
				if (rxAppNotificationStatus1.okay)
					notificationOut.write(rxAppNotification);					// Output the notification
			}
			//TODO else, we are screwed since the ACK is already sent
		}
		else if (!internalNotificationFifoIn.empty() && (rand_fifoCount < 31)) {
			internalNotificationFifoIn.read(rxAppNotification);
			if (rxAppNotification.length != 0) {
				rand_notificationBuffer.write(rxAppNotification);
				rand_fifoCount++;
			}
			else
				notificationOut.write(rxAppNotification);
		}
	}
}


void rxEventMerger(
					stream<extendedEvent>& in1, 
					stream<event>& in2, 
					stream<extendedEvent>& out)
{
	#pragma HLS PIPELINE II=1
	#pragma HLS INLINE

	if (!in1.empty()) {
		out.write(in1.read());
	}
	else if (!in2.empty()) {
		out.write(in2.read());
	}
}


/** @ingroup rx_engine
 *  The @ref rx_engine is processing the data packets on the receiving path.
 *  When a new packet enters the engine its TCP checksum is tested, afterwards the header is parsed
 *  and some more checks are done. Before it is evaluated by the main TCP state machine which triggers Events
 *  and updates the data structures depending on the packet. If the packet contains valid payload it is stored
 *  in memory and the application is notified about the new data.
 *  @param[in]		ipRxData						: Incoming packets from the interface (IP Layer)
 *  @param[in]		sLookup2rxEng_rsp				: 
 *  @param[in]		stateTable2rxEng_upd_rsp
 *  @param[in]		portTable2rxEng_rsp
 *  @param[in]		rxSar2rxEng_upd_rsp
 *  @param[in]		txSar2rxEng_upd_rsp
 *  @param[in]		rxBufferWriteStatus
 *  @param[out]		rxBufferWriteCmd
 *  @param[out]		rxBufferWriteData
 *  @param[out]		rxEng2sLookup_req
 *  @param[out]		rxEng2stateTable_upd_req
 *  @param[out]		rxEng2portTable_req
 *  @param[out]		rxEng2rxSar_upd_req
 *  @param[out]		rxEng2txSar_upd_req
 *  @param[out]		rxEng2timer_clearRetransmitTimer
 *  @param[out]		rxEng2timer_clearProbeTimer
 *  @param[out]		rxEng2timer_setCloseTimer
 *  @param[out]		openConStatusOut
 *  @param[out]		rxEng2eventEng_setEvent
 *  @param[out]		rxEng2rxApp_notification
 *  @param[out]		rxEng_pseudo_packet_to_checksum
 *  @param[in]		rxEng_pseudo_packet_res_checksum
 */

void rx_engine(	stream<axiWord>&					ipRxData,
				stream<sessionLookupReply>&			sLookup2rxEng_rsp,
				stream<sessionState>&				stateTable2rxEng_upd_rsp,
				stream<bool>&						portTable2rxEng_rsp,
				stream<rxSarEntry>&					rxSar2rxEng_upd_rsp,
				stream<rxTxSarReply>&				txSar2rxEng_upd_rsp,
#if (!RX_DDR_BYPASS)
				stream<mmStatus>&					rxBufferWriteStatus,
				stream<mmCmd>&						rxBufferWriteCmd,
#endif
				stream<axiWord>&					rxBufferWriteData,
				stream<sessionLookupQuery>&			rxEng2sLookup_req,
				stream<stateQuery>&					rxEng2stateTable_upd_req,
				stream<ap_uint<16> >&				rxEng2portTable_req,
				stream<rxSarRecvd>&					rxEng2rxSar_upd_req,
				stream<rxTxSarQuery>&				rxEng2txSar_upd_req,
				stream<rxRetransmitTimerUpdate>&	rxEng2timer_clearRetransmitTimer,
				stream<ap_uint<16> >&				rxEng2timer_clearProbeTimer,
				stream<ap_uint<16> >&				rxEng2timer_setCloseTimer,
				stream<openStatus>&					openConStatusOut,
				stream<extendedEvent>&				rxEng2eventEng_setEvent,
				stream<appNotification>&			rxEng2rxApp_notification,
				stream<txApp_client_status>& 		rxEng2txApp_client_notification,
				stream<axiWord>&					rxEng_pseudo_packet_to_checksum,
				stream<ap_uint<16> >&				rxEng_pseudo_packet_res_checksum)
{
#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INLINE 

	// Axi Streams
	static stream<axiWord>		rxEng_pseudo_packet("rxEng_pseudo_packet");
	#pragma HLS stream variable=rxEng_pseudo_packet depth=8
	#pragma HLS DATA_PACK variable=rxEng_pseudo_packet

	static stream<axiWord>		rxEng_pseudo_packet_to_metadata("rxEng_pseudo_packet_to_metadata");
	#pragma HLS stream variable=rxEng_pseudo_packet_to_metadata depth=8
	#pragma HLS DATA_PACK variable=rxEng_pseudo_packet_to_metadata

	static stream<axiWord>		rxEng_tcp_payload("rxEng_tcp_payload");
	#pragma HLS stream variable=rxEng_tcp_payload depth=256 //critical, tcp checksum computation
	#pragma HLS DATA_PACK variable=rxEng_tcp_payload

	static stream<axiWord>		rxEng_pkt_buffer("rxEng_pkt_buffer");
	#pragma HLS stream variable=rxEng_pkt_buffer depth=256
	#pragma HLS DATA_PACK variable=rxEng_pkt_buffer

	// Meta Streams/FIFOs
	static stream<bool >			rxEng_correct_checksum("rxEng_correct_checksum");
	#pragma HLS stream variable=rxEng_correct_checksum depth=2

	static stream<rxEngineMetaData>		rxEng_metaDataFifo("rx_metaDataFifo");
	#pragma HLS stream variable=rxEng_metaDataFifo depth=2
	#pragma HLS DATA_PACK variable=rxEng_metaDataFifo

	static stream<rxFsmMetaData>		rxEng_fsmMetaDataFifo("rxEng_fsmMetaDataFifo");
	static stream<fourTuple>			rxEng_tupleBuffer("rx_tupleBuffer");
	#pragma HLS stream variable=rxEng_tupleBuffer depth=2
	#pragma HLS DATA_PACK variable=rxEng_tupleBuffer
	
	static stream<ap_uint<16> >			rxEng_tcpLenFifo("rx_tcpLenFifo");
	#pragma HLS stream variable=rxEng_tcpLenFifo depth=2

	static stream<extendedEvent>		rxEng_metaHandlerEventFifo("rxEng_metaHandlerEventFifo");
	#pragma HLS stream variable=rxEng_metaHandlerEventFifo depth=2
	#pragma HLS DATA_PACK variable=rxEng_metaHandlerEventFifo

	static stream<event>				rxEng_fsmEventFifo("rxEng_fsmEventFifo");
	#pragma HLS stream variable=rxEng_fsmEventFifo depth=2
	#pragma HLS DATA_PACK variable=rxEng_fsmEventFifo

	static stream<bool>					rxEng_metaHandlerDropFifo("rxEng_metaHandlerDropFifo");
	#pragma HLS stream variable=rxEng_metaHandlerDropFifo depth=2
	#pragma HLS DATA_PACK variable=rxEng_metaHandlerDropFifo

	static stream<bool>					rxEng_fsmDropFifo("rxEng_fsmDropFifo");
	#pragma HLS stream variable=rxEng_fsmDropFifo depth=2
	#pragma HLS DATA_PACK variable=rxEng_fsmDropFifo

	static stream<appNotification> rx_internalNotificationFifo("rx_internalNotificationFifo");
	#pragma HLS stream variable=rx_internalNotificationFifo depth=8 //This depends on the memory delay
	#pragma HLS DATA_PACK variable=rx_internalNotificationFifo

	static stream<mmCmd> 					rxTcpFsm2wrAccessBreakdown("rxTcpFsm2wrAccessBreakdown");
	#pragma HLS stream variable=rxTcpFsm2wrAccessBreakdown depth=8
	#pragma HLS DATA_PACK variable=rxTcpFsm2wrAccessBreakdown

	static stream<axiWord> 					rxPkgDrop2rxMemWriter("rxPkgDrop2rxMemWriter");
	#pragma HLS stream variable=rxPkgDrop2rxMemWriter depth=16
	#pragma HLS DATA_PACK variable=rxPkgDrop2rxMemWriter

	static stream<ap_uint<1> >				rxEngDoubleAccess("rxEngDoubleAccess");
	#pragma HLS stream variable=rxEngDoubleAccess depth=8

	rxTCP_pseudoheader_insert( 
			ipRxData, 
			rxEng_pseudo_packet);

	DataBroadcast(
			rxEng_pseudo_packet,
			rxEng_pseudo_packet_to_checksum,
			rxEng_pseudo_packet_to_metadata);

	rxEng_Generate_Metadata(
			rxEng_pseudo_packet_to_metadata,
			rxEng_pseudo_packet_res_checksum,
			rxEng_tcp_payload, 
			rxEng_correct_checksum, 
			rxEng_metaDataFifo,
			rxEng_tupleBuffer, 
			rxEng2portTable_req);

	rxTcpInvalidDropper(
			rxEng_tcp_payload, 
			rxEng_correct_checksum, 
			rxEng_pkt_buffer);

	rxMetadataHandler(	
			rxEng_metaDataFifo,
			rxEng_tupleBuffer,
			portTable2rxEng_rsp,
			sLookup2rxEng_rsp,
			rxEng2sLookup_req,
			rxEng_metaHandlerEventFifo,
			rxEng_metaHandlerDropFifo,
			rxEng_fsmMetaDataFifo);

	rxTcpFSM(			
			rxEng_fsmMetaDataFifo,
			stateTable2rxEng_upd_rsp,
			rxSar2rxEng_upd_rsp,
			txSar2rxEng_upd_rsp,
			rxEng2stateTable_upd_req,
			rxEng2rxSar_upd_req,
			rxEng2txSar_upd_req,
			rxEng2timer_clearRetransmitTimer,
			rxEng2timer_clearProbeTimer,
			rxEng2timer_setCloseTimer,
			openConStatusOut,
			rxEng_fsmEventFifo,
			rxEng_fsmDropFifo,
#if (!RX_DDR_BYPASS)
			rxTcpFsm2wrAccessBreakdown,
			rx_internalNotificationFifo,
#else
			rxEng2rxApp_notification,
#endif
			rxEng2txApp_client_notification);


#if (!RX_DDR_BYPASS)
	rxPacketDropper(
			rxEng_pkt_buffer,
			rxEng_metaHandlerDropFifo,
			rxEng_fsmDropFifo,
			rxPkgDrop2rxMemWriter);

	Rx_Data_to_Memory(
			rxPkgDrop2rxMemWriter, 
			rxTcpFsm2wrAccessBreakdown, 
			rxBufferWriteCmd, 
			rxBufferWriteData,
			rxEngDoubleAccess);

	rxAppNotificationDelayer(
			rxBufferWriteStatus, 
			rx_internalNotificationFifo, 
			rxEng2rxApp_notification, 
			rxEngDoubleAccess);
#else
	rxPacketDropper(
			rxEng_pkt_buffer,
			rxEng_metaHandlerDropFifo,
			rxEng_fsmDropFifo,
			rxBufferWriteData);
#endif
	rxEventMerger(
			rxEng_metaHandlerEventFifo,
			rxEng_fsmEventFifo,
			rxEng2eventEng_setEvent);
}


