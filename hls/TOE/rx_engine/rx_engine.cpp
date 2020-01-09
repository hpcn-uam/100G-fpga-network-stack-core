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

#include "rx_engine.hpp"

using namespace hls;
using namespace std;

/** @ingroup rx_engine
 * Extracts tcpLength from IP header, removes the IP header and prepends the IP addresses to the payload,
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
*   +---------------------------------------------------------------+
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

void rxEngPseudoHeaderInsert(
								stream<axiWord>&			IpLevelPacket,
								stream<axiWord>&			TCP_PseudoPacket_i, 
								stream<axiWord>&			TCP_PseudoPacket_c) 
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
	static bool				pseudo_header = false;

	enum pseudo_header_state {IP_HEADER ,TCP_PAYLOAD, EXTRA_WORD};
	static pseudo_header_state fsm_state = IP_HEADER;


	switch (fsm_state){
		case (IP_HEADER):
			if (!IpLevelPacket.empty()){
				IpLevelPacket.read(currWord);
				ip_headerlen 	= currWord.data( 3, 0); 				// Read IP header len
				ipTotalLen 		= byteSwap16(currWord.data(31, 16));    // Read IP total len
				ip_src 			= currWord.data(127,96);
				ip_dst 			= currWord.data(159,128);

				keep_extra = 8 + (ip_headerlen-5) * 4;
				if (currWord.last){

				 	combine_words( axiWord(0,0,0), currWord, ip_headerlen, sendWord);
					
					tcpTotalLen = ipTotalLen - (ip_headerlen *4);
					sendWord.data(63 , 0) 	= (ip_dst,ip_src);
					sendWord.data(79 ,64)	= 0x0600;
					sendWord.data(95 ,80) 	= byteSwap16(tcpTotalLen);
					sendWord.keep(11 , 0) 	= 0xFFF;
					sendWord.last 			= 1;
					
					TCP_PseudoPacket_i.write(sendWord);
					TCP_PseudoPacket_c.write(sendWord);
				}
				else{
					pseudo_header = true;
					fsm_state = TCP_PAYLOAD;
				}
				prevWord = currWord;
			}
			break;
		case (TCP_PAYLOAD) :
			if (!IpLevelPacket.empty()){
				IpLevelPacket.read(currWord);
				combine_words( currWord, prevWord, ip_headerlen, sendWord);
				if (pseudo_header){
					pseudo_header = false;
					tcpTotalLen = ipTotalLen - (ip_headerlen *4);
					sendWord.data(63 , 0) 	= (ip_dst,ip_src);
					sendWord.data(79 ,64)	= 0x0600;
					sendWord.data(95 ,80) 	= byteSwap16(tcpTotalLen);
					sendWord.keep(11,0) 	= 0xFFF;
				}
				sendWord.last = 0;
				if (currWord.last){
					if (currWord.keep.bit(keep_extra)) {
						fsm_state = EXTRA_WORD;
					}
					else {
						sendWord.last = 1;
						fsm_state = IP_HEADER;
					}
				}
				prevWord = currWord;
				TCP_PseudoPacket_i.write(sendWord);
				TCP_PseudoPacket_c.write(sendWord);
			}
		 	break;
		case EXTRA_WORD:
			currWord.data = 0;
			currWord.keep = 0;
			combine_words( currWord, prevWord, ip_headerlen, sendWord);
			sendWord.last 			= 1;
			TCP_PseudoPacket_i.write(sendWord);
			TCP_PseudoPacket_c.write(sendWord);
			fsm_state = IP_HEADER;
			break;	

	}
}

/**
 * @brief      This module parses the TCP option only in the syn packet.
 * 			   If the packet is not a syn the metaInfo is forwarded directly.
 * 			   The parsing is done sequentially, that implies a variable latency 
 * 			   depending on where the Window Scale option is located.  	
 *
 * @param      metaDataFifoIn   The meta data fifo in
 * @param      metaDataFifoOut  The meta data fifo out
 */
#if WINDOW_SCALE
void rxParseTcpOptions (
							stream<rxEngPktMetaInfo>&		metaDataFifoIn,
							stream<rxEngPktMetaInfo>&		metaDataFifoOut
	){
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum rpto_states {READ_INFO, PARSE_DATA};
	static rpto_states rtpo_fsm_state = READ_INFO;
	static rxEngPktMetaInfo 		metaInfo;
	static ap_uint<6> 				byte_offset=0;	
	static ap_uint<9>				optionsSize;				
	ap_uint<9> 				bitOffset;	
	
	ap_uint<8> 				optionKind;	
	ap_uint<8> 				optionLength;
	ap_uint<4>				recv_window_scale = 0;	
	bool 					sendMeta = false;

	switch (rtpo_fsm_state) {
		case READ_INFO:
			if (!metaDataFifoIn.empty()){
				metaDataFifoIn.read(metaInfo);
		
				if ((metaInfo.tcpOffset > 5) && metaInfo.digest.syn) {
					byte_offset = 0;
					optionsSize 	= (metaInfo.tcpOffset - 5)*4;
					rtpo_fsm_state 	= PARSE_DATA;
				}
				else {
					metaDataFifoOut.write(metaInfo);
				}
			}
			
			break;

		case PARSE_DATA: 

			optionKind 	 = metaInfo.tcpOptions(7 , 0);
			optionLength = metaInfo.tcpOptions(15, 8);
			//std::cout << "RxParse. byte_offset " << std::dec << byte_offset << "\tOption Kind: " << std::hex << optionKind << "\toptionLength: " << std::dec << optionLength;
			//std::cout << "\toptionsSize " << optionsSize;

			/* If all the bytes where processed and Window Scale was not found send the current metaInfo*/
			if (optionsSize > byte_offset) {
				switch (optionKind){
					case 0:	// End of option List
						sendMeta = true;
						break;
					case 1:	// No Operation
						optionLength = 1;
						break;
					case 3: // Window Scale option
						sendMeta = true;
						if (optionLength == 3){ // Double check
							recv_window_scale = metaInfo.tcpOptions(19 ,16);
							if (metaInfo.tcpOptions(19 ,16) > WINDOW_SCALE_BITS) 		// When the other endpoint has a bigger window scale use local one
								recv_window_scale = (ap_uint<4>) WINDOW_SCALE_BITS;
							
							metaInfo.digest.recv_window_scale = recv_window_scale;
							//std::cout << "\t Window shift " << metaInfo.tcpOptions(bitOffset + 19 , bitOffset + 16);
						}
						break;	

					default:
					break;
						
				}
				metaInfo.tcpOptions = metaInfo.tcpOptions >> (optionLength*8);
				byte_offset = byte_offset + optionLength;
			}
			else {
				sendMeta = true;
			}

			std::cout << std::endl;


			if (sendMeta) {
				metaDataFifoOut.write(metaInfo);
				rtpo_fsm_state = READ_INFO;
			}
			
			break;	
	}

}

#endif
/** @ingroup rx_engine
 *  This module gets the packet at Pseudo TCP layer.
 *  First of all, it removes the pseudo TCP header and forward the payload if any.
 *  It also sends the metaData information to the following module.
 *  @param[in]		pseudoPacket
 *  @param[out]		payload
 *  @param[out]		metaDataFifoOut
 */
void rxEngGetMetaData(
							stream<axiWord>&				pseudoPacket,
							stream<rxEngPktMetaInfo>&		metaDataFifoOut,
							stream<axiWord>&				payload)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum regmd_states {FIRST_WORD, REMAINING_WORDS, EXTRA_WORD};
	static regmd_states regdm_fsm_state = FIRST_WORD;

	static axiWord 			prevWord;
	static ap_uint<6>		byte_offset;

	ap_uint<4> 				tcp_offset;
	ap_uint<16> 			payload_length;
	axiWord 				currWord;
	axiWord 				sendWord;
	rxEngPktMetaInfo 		rxMetaInfo;
	ap_uint<4> 				window_shift;

	switch (regdm_fsm_state){
		case FIRST_WORD:
			if (!pseudoPacket.empty()){
				pseudoPacket.read(currWord);
				/* Get the TCP Pseudo header total length and subtract the TCP header size. This value is the payload size*/
				tcp_offset 			= currWord.data(199 ,196);
				payload_length 		= byteSwap16(currWord.data(95 ,80)) - tcp_offset * 4; 
				// Get four tuple info but do not swap it
				rxMetaInfo.tuple.srcIp		= currWord.data(31 ,  0);
				rxMetaInfo.tuple.dstIp		= currWord.data(63 , 32);
				rxMetaInfo.tuple.srcPort	= currWord.data(111, 96);
				rxMetaInfo.tuple.dstPort	= currWord.data(127,112);
				// Get TCP meta Info swap the necessary words
				rxMetaInfo.digest.seqNumb	= byteSwap32(currWord.data(159,128));
				rxMetaInfo.digest.ackNumb	= byteSwap32(currWord.data(191,160));
				rxMetaInfo.digest.winSize	= byteSwap16(currWord.data(223,208));
				rxMetaInfo.digest.length 	= payload_length;
				rxMetaInfo.digest.cwr 		= currWord.data.bit(207);
				rxMetaInfo.digest.ecn 		= currWord.data.bit(206);
				rxMetaInfo.digest.urg 		= currWord.data.bit(205);
				rxMetaInfo.digest.ack 		= currWord.data.bit(204);
				rxMetaInfo.digest.psh 		= currWord.data.bit(203);
				rxMetaInfo.digest.rst 		= currWord.data.bit(202);
				rxMetaInfo.digest.syn 		= currWord.data.bit(201);
				rxMetaInfo.digest.fin 		= currWord.data.bit(200);

#if (WINDOW_SCALE)
				rxMetaInfo.digest.recv_window_scale = 0;						// Initialize window shift to 0
				rxMetaInfo.tcpOffset 		= tcp_offset;
				rxMetaInfo.tcpOptions 		= currWord.data(511,256);			// Get the possible options

				//rxMetaInfo.digest.recv_window_scale = 10;
#endif				

				/* Only send data when one-transaction packet has data */
				byte_offset = tcp_offset* 4 + 12;
				if (currWord.last){		
					if (payload_length != 0){ // one-transaction packet with any data
						sendWord.data 		= currWord.data(511, byte_offset*8);
						sendWord.keep 		= currWord.keep( 63,  byte_offset);
						sendWord.last 	  	= 1;
						payload.write(sendWord);
					}
				}
				else {
					regdm_fsm_state = REMAINING_WORDS;								// MR TODO: if tcp_offset > 13 the payload starts in the second transaction
				}
				prevWord = currWord;
				metaDataFifoOut.write(rxMetaInfo);
			}
			break;
		case REMAINING_WORDS:
			if (!pseudoPacket.empty()){
				pseudoPacket.read(currWord);
				
				// Compose the output word
				sendWord.data = ((currWord.data((byte_offset*8)-1 ,0)) , prevWord.data(511, byte_offset*8));
				sendWord.keep = ((currWord.keep(    byte_offset-1 ,0)) , prevWord.keep( 63,   byte_offset));

				sendWord.last 	= 0;
				if (currWord.last){
					if (currWord.keep.bit(byte_offset)){
						regdm_fsm_state = EXTRA_WORD;
					}
					else {
						sendWord.last 	= 1;
						regdm_fsm_state = FIRST_WORD;
					}
				}
				prevWord = currWord;
				payload.write(sendWord);
			}
			break;
		case EXTRA_WORD:
			sendWord.data 		= prevWord.data(511,byte_offset*8);
			sendWord.keep 		= prevWord.keep(63,   byte_offset);
			sendWord.last 	  	= 1;
			payload.write(sendWord);
			regdm_fsm_state = FIRST_WORD;
			break;
	}
}


/**
 * @ingroup    rx_engine
 *
 * This function reads the checksum @rtl_checksum coming from the RTL and the
 * @metaPacketInfoIn. 
 * If the rtl_checksum is zero, it means that the packet is valid 
 * and correct checksum will be set, and the @metaPacketInfoIn will be delivered.
 * If the checksum is not zero @drop_payload will be cleared and 
 * no metadata will be delivered
 *
 * @param      rtl_checksum      The resource checksum
 * @param      metaPacketInfoIn    The meta packet data
 * @param      drop_payload  The correct checksum
 * @param      portTableOut      The port table out
 */
void rxEngVerifyCheckSum (
		stream<ap_uint<16> >&			rtl_checksum,
		stream<rxEngPktMetaInfo>&		metaPacketInfoIn,
		stream<bool>&					drop_payload,
		stream<rxEngPktMetaInfo>&		metaPacketInfoOut,
		stream<ap_uint<16> >&			portTableOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum vc_states {READ_META_INFO, READ_CHECKSUM};
	static vc_states fsm_vc_state = READ_META_INFO;
	static rxEngPktMetaInfo	meta_VCS;

	ap_uint<16>			checksum_i;
	bool 				checksum_correct;

	switch (fsm_vc_state){
		case READ_META_INFO :
			if (!metaPacketInfoIn.empty()){
				metaPacketInfoIn.read(meta_VCS);
				fsm_vc_state = READ_CHECKSUM;
			}
			break;
		case READ_CHECKSUM :
			if (!rtl_checksum.empty()){
				rtl_checksum.read(checksum_i);
				checksum_correct = (checksum_i==0) ? true : false;	// Compare
				if (checksum_correct) {
					metaPacketInfoOut.write(meta_VCS);
					portTableOut.write(meta_VCS.tuple.dstPort);
				}
				if (meta_VCS.digest.length!=0)				
					drop_payload.write(!checksum_correct); // the value is inverted because it indicates dropping
				fsm_vc_state = READ_META_INFO;
			}

			break;
	}

}
	
/**
 * @ingroup    rx_engine
 * @brief      
 *
 * @param[In]    metaDataFifoIn           Metadata of the incomming packet
 * @param[In]    portTable2rxEng_rsp      Port is open?
 * @param[In]    sLookup2rxEng_rsp        Look up response, it carries if the four tuple is in the table and its ID
 * @param[Out]   rxEng2sLookup_req        Session look up, it carries the four tuple and if creation is allowed
 * @param[Out]   rxEng2eventEng_setEvent  Set event
 * @param[Out]   dropDataFifoOut          The drop data fifo out
 * @param[Out]   fsmMetaDataFifo          The fsm meta data fifo
 */
void rxEngMetadataHandler(	
			stream<rxEngPktMetaInfo>&				metaDataFifoIn,
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
	
	static rxEngPktMetaInfo 	mh_meta;
	static sessionLookupReply 	mh_lup;
	static mhStateType 			mh_state = META;
	static ap_uint<32> 			mh_srcIpAddress;
	static ap_uint<16> 			mh_dstIpPort;

	fourTuple 					switchedTuple;
	bool 						portIsOpen;

	switch (mh_state) {
		case META:
			if (!metaDataFifoIn.empty()&& !portTable2rxEng_rsp.empty()) {
				metaDataFifoIn.read(mh_meta);
				portTable2rxEng_rsp.read(portIsOpen);
				mh_srcIpAddress = byteSwap32(mh_meta.tuple.srcIp);			// Swap source IP and destination port
				mh_dstIpPort 	= byteSwap16(mh_meta.tuple.dstPort);

				if (!portIsOpen) {									// Check if port is closed
					// SEND RST+ACK
					if (!mh_meta.digest.rst) {
						// send necessary tuple through event
						switchedTuple.srcIp 	= mh_meta.tuple.dstIp;
						switchedTuple.dstIp 	= mh_meta.tuple.srcIp;
						switchedTuple.srcPort 	= mh_meta.tuple.dstPort;
						switchedTuple.dstPort 	= mh_meta.tuple.srcPort;
						if (mh_meta.digest.syn || mh_meta.digest.fin) {
							rxEng2eventEng_setEvent.write(extendedEvent(rstEvent(mh_meta.digest.seqNumb+mh_meta.digest.length+1), switchedTuple)); //always 0
						}
						else {
							rxEng2eventEng_setEvent.write(extendedEvent(rstEvent(mh_meta.digest.seqNumb+mh_meta.digest.length), switchedTuple));
						}
					} 
					//else ignore => do nothing
					if (mh_meta.digest.length != 0) {			// Drop payload because port is closed
						dropDataFifoOut.write(true);
					}
				}
				else { // Port is open. Make session lookup, only allow creation of new entry when SYN or SYN_ACK
					rxEng2sLookup_req.write(sessionLookupQuery(mh_meta.tuple, (mh_meta.digest.syn && !mh_meta.digest.rst && !mh_meta.digest.fin)));
					mh_state = LOOKUP;
				}
			}
			break;
		case LOOKUP: //BIG delay here, waiting for LOOKup
			if (!sLookup2rxEng_rsp.empty()) {
				sLookup2rxEng_rsp.read(mh_lup);
				if (mh_lup.hit) {
					//Write out lup and meta
					fsmMetaDataFifo.write(rxFsmMetaData(mh_lup.sessionID, mh_srcIpAddress, mh_dstIpPort, mh_meta.digest));
				}
				if (mh_meta.digest.length != 0) {
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

void rxEngTcpFSM(		
			stream<rxFsmMetaData>&					fsmMetaDataFifo,
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
#if (STATISTICS_MODULE)
			stream<rxStatsUpdate>&  				rxEngStatsUpdate,
#endif				
#if (!RX_DDR_BYPASS)
			stream<mmCmd>&							rxBufferWriteCmd,
#endif
			stream<appNotification>&				rxEng2rxApp_notification, 	// The notification are use both with DDR or no DDR
			stream<txApp_client_status>& 			rxEng2txApp_client_notification)	
{
#pragma HLS LATENCY max=2
#pragma HLS INLINE off
#pragma HLS pipeline II=1


	enum fsmStateType {LOAD, TRANSITION};
	static fsmStateType fsm_state = LOAD;
	static rxFsmMetaData fsm_meta;
	static bool fsm_txSarRequest = false;


	static ap_uint<4> 		control_bits = 0;
	sessionState 			tcpState;
	rxSarEntry 				rxSar;
	rxTxSarReply 			txSar;
	ap_uint<32> 			pkgAddr = 0;
	ap_uint<32> 			newRecvd;
	ap_uint<WINDOW_BITS> 	free_space;

	ap_uint<4>				rx_win_shift;	// used to computed the scale option for RX buffer
	ap_uint<4>				tx_win_shift;	// used to computed the scale option for TX buffer

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
					txSar2rxEng_upd_rsp.read(txSar); // FIXME by default it was a non-block read. Why?
				}

				fsm_txSarRequest = false;

				stateTable2rxEng_upd_rsp.read(tcpState);
				rxSar2rxEng_upd_rsp.read(rxSar);
				
				fsm_state = LOAD;

			} // When all responses needed are valid proceed
			
			switch (control_bits) {
				case 1: //ACK
					if (fsm_state == LOAD) {
						//std::cout << "RX_engine state " << std::dec << tcpState << "\tacknum " << std::hex << fsm_meta.meta.ackNumb <<  "\tat " << std::dec << simCycleCounter << std::endl;
						rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, (fsm_meta.meta.ackNumb == txSar.nextByte))); 		// Reset Retransmit Timer
						if (tcpState == ESTABLISHED || tcpState == SYN_RECEIVED || tcpState == FIN_WAIT_1 || tcpState == CLOSING || tcpState == LAST_ACK) {
							// Check if new ACK arrived
							if (fsm_meta.meta.ackNumb == txSar.prevAck && txSar.prevAck != txSar.nextByte) {
								// Not new ACK increase counter only if it does not contain data
								if (fsm_meta.meta.length == 0) {
									txSar.count++;
								}
							}
							else {
								// Notify probeTimer about new ACK
								rxEng2timer_clearProbeTimer.write(fsm_meta.sessionID);
								// Check for SlowStart & Increase Congestion Window
								if (txSar.cong_window <= (txSar.slowstart_threshold-MSS)) {
									txSar.cong_window += MSS;
								}
								else if (txSar.cong_window <= CONGESTION_WINDOW_MAX) {
									/*In theory (1/cong_window) should be added however increasing with floating/fixed point would increase memory usage */ 
									txSar.cong_window++;
								}
								txSar.count = 0;
								txSar.fastRetransmitted = false;
							}
							// TX SAR
							if ((txSar.prevAck <= fsm_meta.meta.ackNumb && fsm_meta.meta.ackNumb <= txSar.nextByte)
									|| ((txSar.prevAck <= fsm_meta.meta.ackNumb || fsm_meta.meta.ackNumb <= txSar.nextByte) && txSar.nextByte < txSar.prevAck)) {
#if (!WINDOW_SCALE)								
								rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, 
																		txSar.count, ((txSar.count == 3) || txSar.fastRetransmitted))));
#else
								rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, 
																		txSar.count, ((txSar.count == 3) || txSar.fastRetransmitted) , txSar.tx_win_shift)));
#endif							
							}

							// Check if packet contains payload
							if (fsm_meta.meta.length != 0){
								newRecvd = fsm_meta.meta.seqNumb+fsm_meta.meta.length;
								// Second part makes sure that app pointer is not overtaken
								free_space = ((rxSar.appd - rxSar.recvd(WINDOW_BITS-1, 0)) - 1);
								// Check if segment is in order and if enough free space is available
								if ((fsm_meta.meta.seqNumb == rxSar.recvd) && (free_space > fsm_meta.meta.length)) {
									rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, newRecvd, 1));
									// Build memory address
									
#if (!RX_DDR_BYPASS)
									pkgAddr(31, 30) = 0x0;
									pkgAddr(30, WINDOW_BITS)  	= fsm_meta.sessionID(13, 0);
									pkgAddr(WINDOW_BITS-1, 0) 	= fsm_meta.meta.seqNumb(WINDOW_BITS-1, 0);
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
#if FAST_RETRANSMIT							
							if ((txSar.count == 3) && !txSar.fastRetransmitted) {
								rxEng2eventEng_setEvent.write(event(RT, fsm_meta.sessionID));
							}
							else if (fsm_meta.meta.length != 0) { // Send ACK
#else				
							if (fsm_meta.meta.length != 0) {
#endif				
								rxEng2eventEng_setEvent.write(event(ACK, fsm_meta.sessionID));
							}
							

							if (fsm_meta.meta.ackNumb == txSar.nextByte) {
								// This is necessary to unlock stateTable
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
										rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1));
										break;
								}
							}
							else { //we have to release the lock
								//reset rtTimer
								//rtTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID));
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, tcpState, 1)); // or ESTABLISHED
							}
#if (STATISTICS_MODULE)							
							rxEngStatsUpdate.write(rxStatsUpdate(fsm_meta.sessionID,fsm_meta.meta.length));
#endif							
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
							
							//rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1));
							// Initialize rxSar, SEQ + phantom byte, last '1' for makes sure appd is initialized + Window scale if enable
#if (WINDOW_SCALE)							
							rx_win_shift = (fsm_meta.meta.recv_window_scale == 0) ? 0 : WINDOW_SCALE_BITS; 	// If the other side announces a WSopt we use WINDOW_SCALE_BITS
							tx_win_shift = fsm_meta.meta.recv_window_scale; // Keep at it is, since it was already verified
							//std::cout << std::endl << "SYN_ACK " << "rx_win_shift :" << std::dec << rx_win_shift << "\ttx_win_shift " << tx_win_shift << "\trecv_window_scale " << fsm_meta.meta.recv_window_scale << std::endl << std::endl; 
							rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1, rx_win_shift));
							// TX Sar table is initialized with the received window scale 
							rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, 0, fsm_meta.meta.winSize, txSar.cong_window, 0, false, true , rx_win_shift)));
#else
							rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1));
							rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, 0, fsm_meta.meta.winSize, txSar.cong_window, 0, false))); //TODO maybe include count check SYN_ACK event
#endif				
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
#if (STATISTICS_MODULE)							
							rxEngStatsUpdate.write(rxStatsUpdate(fsm_meta.sessionID,0,true,false,false));
#endif		
					}
					break;
				case 3: //SYN_ACK
					if (fsm_state == LOAD) {
						rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, (fsm_meta.meta.ackNumb == txSar.nextByte))); // Clear SYN retransmission time if ack number is correct
						
						if (tcpState == SYN_SENT) { // A SYN was already send, ack number has to be check, if is correct send ACK is not send a RST
							if (fsm_meta.meta.ackNumb == txSar.nextByte) { // SYN-ACK is correct
								
								rxEng2eventEng_setEvent.write(event(ACK_NODELAY, fsm_meta.sessionID)); 				// set ACK event
								rxEng2stateTable_upd_req.write(stateQuery(fsm_meta.sessionID, ESTABLISHED, 1)); 	// Update TCP FSM to ESTABLISHED now data can be transfer 
								//initialize rx_sar, SEQ + phantom byte, last '1' for appd init + Window scale if enable
#if (WINDOW_SCALE)							
								rx_win_shift = (fsm_meta.meta.recv_window_scale == 0) ? 0 : WINDOW_SCALE_BITS; 	// If the other side announces a WSopt we use WINDOW_SCALE_BITS
								tx_win_shift = fsm_meta.meta.recv_window_scale; // Keep at it is, since it was already verified
								//std::cout << std::endl << "SYN_ACK " << "rx_win_shift :" << std::dec << rx_win_shift << "\ttx_win_shift " << tx_win_shift << "\trecv_window_scale " << fsm_meta.meta.recv_window_scale << std::endl << std::endl; 
								rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1, rx_win_shift)); 
								// TX Sar table is initialized with the received window scale 
								rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, 0, false, true , tx_win_shift)));
#else								
								rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+1, 1, 1)); //initialize rx_sar, SEQ + phantom byte, last '1' for appd init
								rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, 0, false))); //CHANGE this was added //TODO maybe include count check
#endif
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
#if (STATISTICS_MODULE)							
							rxEngStatsUpdate.write(rxStatsUpdate(fsm_meta.sessionID,0,false,true,false));
#endif							
					}
					break;
				case 5: //FIN (_ACK)

					if (fsm_state == LOAD) {
						rxEng2timer_clearRetransmitTimer.write(rxRetransmitTimerUpdate(fsm_meta.sessionID, (fsm_meta.meta.ackNumb == txSar.nextByte)));
						// Check state and if FIN in order, Current out of order FINs are not accepted
						if ((tcpState == ESTABLISHED || tcpState == FIN_WAIT_1 || tcpState == FIN_WAIT_2) && (rxSar.recvd == fsm_meta.meta.seqNumb)) {
#if (WINDOW_SCALE)							
							rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, txSar.count, txSar.fastRetransmitted, tx_win_shift))); //TODO include count check
#else
							rxEng2txSar_upd_req.write((rxTxSarQuery(fsm_meta.sessionID, fsm_meta.meta.ackNumb, fsm_meta.meta.winSize, txSar.cong_window, txSar.count, txSar.fastRetransmitted))); //TODO include count check
#endif
							// +1 for phantom byte, there might be data too
							rxEng2rxSar_upd_req.write(rxSarRecvd(fsm_meta.sessionID, fsm_meta.meta.seqNumb+fsm_meta.meta.length+1, 1)); //diff to ACK

							// Clear the probe timer
							rxEng2timer_clearProbeTimer.write(fsm_meta.sessionID);

							// Check if there is payload
							if (fsm_meta.meta.length != 0) {
								pkgAddr(31, 30) = 0x0;
								pkgAddr(30, WINDOW_BITS)  	= fsm_meta.sessionID(13, 0);
								pkgAddr(WINDOW_BITS-1, 0) 	= fsm_meta.meta.seqNumb(WINDOW_BITS-1, 0);
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
									//std::cout << std::endl << "TCP FSM going to TIME_WAIT state " << std::endl << std::endl;
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
#if (STATISTICS_MODULE)							
							rxEngStatsUpdate.write(rxStatsUpdate(fsm_meta.sessionID,0,false,false,true));
#endif								
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
 *	@param[in]		VerifyChecksumDrop, Drop-FIFO indicating if packet needs to be dropped
 *	@param[in]		MetaHandlerDrop, Drop-FIFO indicating if packet needs to be dropped
 *	@param[in]		TCP_FSMDrop, Drop-FIFO indicating if packet needs to be dropped
 *	@param[out]		rxBufferDataOut, outgoing data stream
 */

void rxEngPacketDropper(
			stream<axiWord>&		dataIn,
			stream<bool>&			VerifyChecksumDrop,
			stream<bool>&			MetaHandlerDrop,
			stream<bool>&			TCP_FSMDrop,
			stream<axiWord>&		rxBufferDataOut) 
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	enum tpfStateType {RD_VERIFY_CHECKSUM, RD_META_HANDLER, RD_FSM_DROP, FWD, DROP};
	static tpfStateType tpf_state = RD_VERIFY_CHECKSUM;

	bool drop;
	axiWord currWord;

	switch (tpf_state) {
		case RD_VERIFY_CHECKSUM :
			if (!VerifyChecksumDrop.empty()){
				VerifyChecksumDrop.read(drop);
				tpf_state = (drop) ? DROP : RD_META_HANDLER;
			}
			break;
		case RD_META_HANDLER:
			if (!MetaHandlerDrop.empty()) {
				MetaHandlerDrop.read(drop);
				tpf_state = (drop) ? DROP : RD_FSM_DROP;
			}
			break;
		case RD_FSM_DROP:
			if (!TCP_FSMDrop.empty()) {
				TCP_FSMDrop.read(drop);
				tpf_state = (drop) ? DROP : FWD;
			}
			break;
		case FWD:
			if(!dataIn.empty() /*&& !rxBufferDataOut.full()*/) {
				dataIn.read(currWord);
				rxBufferDataOut.write(currWord);
				tpf_state = (currWord.last) ? RD_VERIFY_CHECKSUM : FWD;
			}
			break;
		case DROP:
			if(!dataIn.empty()) {
				dataIn.read(currWord);
				tpf_state = (currWord.last) ? RD_VERIFY_CHECKSUM : DROP;
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

void rxEngAppNotificationDelayer(	
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


void rxEngEventMerger(
					stream<extendedEvent>& in1, 
					stream<event>& in2, 
					stream<extendedEvent>& out)
{
	#pragma HLS PIPELINE II=1
	#pragma HLS INLINE off

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
#if (STATISTICS_MODULE)
				stream<rxStatsUpdate>&  			rxEngStatsUpdate,
#endif			
				stream<axiWord>&					rxEng_pseudo_packet_to_checksum,
				stream<ap_uint<16> >&				rxEng_pseudo_packet_res_checksum)
{
#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INLINE 

	// AXIS Streams

	static stream<axiWord>		rxEng_pseudo_packet_to_metadata("rxEng_pseudo_packet_to_metadata");
	#pragma HLS STREAM variable=rxEng_pseudo_packet_to_metadata depth=16
	#pragma HLS DATA_PACK variable=rxEng_pseudo_packet_to_metadata

	static stream<axiWord>		rxEng_tcp_payload("rxEng_tcp_payload");
	#pragma HLS STREAM variable=rxEng_tcp_payload depth=512 //critical, store the payload until is forwarded or dropped
	#pragma HLS DATA_PACK variable=rxEng_tcp_payload

#if (!RX_DDR_BYPASS)
	static stream<axiWord> 					rxPkgDrop2rxMemWriter("rxPkgDrop2rxMemWriter");
	#pragma HLS STREAM variable=rxPkgDrop2rxMemWriter depth=512
	#pragma HLS DATA_PACK variable=rxPkgDrop2rxMemWriter
#endif

	// Meta Streams/FIFOs
	static stream<bool>			rxEng_VerifyChecksumDrop("rxEng_VerifyChecksumDrop");
	#pragma HLS STREAM variable=rxEng_VerifyChecksumDrop depth=32

	static stream<rxEngPktMetaInfo>		rxEngMetaInfoFifo("rxEngMetaInfoFifo");
	#pragma HLS STREAM variable=rxEngMetaInfoFifo depth=8
	#pragma HLS DATA_PACK variable=rxEngMetaInfoFifo

	static stream<rxEngPktMetaInfo>		rxEngMetaInfoValid("rxEngMetaInfoValid");
	#pragma HLS STREAM variable=rxEngMetaInfoValid depth=32
	#pragma HLS DATA_PACK variable=rxEngMetaInfoValid

	static stream<rxFsmMetaData>		rxEng_fsmMetaDataFifo("rxEng_fsmMetaDataFifo");
	#pragma HLS STREAM variable=rxEng_fsmMetaDataFifo depth=8
	#pragma HLS DATA_PACK variable=rxEng_fsmMetaDataFifo

	static stream<extendedEvent>		rxEng_metaHandlerEventFifo("rxEng_metaHandlerEventFifo");
	#pragma HLS STREAM variable=rxEng_metaHandlerEventFifo depth=8
	#pragma HLS DATA_PACK variable=rxEng_metaHandlerEventFifo

	static stream<event>				rxEng_fsmEventFifo("rxEng_fsmEventFifo");
	#pragma HLS STREAM variable=rxEng_fsmEventFifo depth=8
	#pragma HLS DATA_PACK variable=rxEng_fsmEventFifo

	static stream<bool>					rxEng_metaHandlerDropFifo("rxEng_metaHandlerDropFifo");
	#pragma HLS STREAM variable=rxEng_metaHandlerDropFifo depth=32

	static stream<bool>					rxEng_fsmDropFifo("rxEng_fsmDropFifo");
	#pragma HLS STREAM variable=rxEng_fsmDropFifo depth=32

	static stream<appNotification> rx_internalNotificationFifo("rx_internalNotificationFifo");
	#pragma HLS STREAM variable=rx_internalNotificationFifo depth=8 //This depends on the memory delay
	#pragma HLS DATA_PACK variable=rx_internalNotificationFifo

	static stream<mmCmd> 					rxTcpFsm2wrAccessBreakdown("rxTcpFsm2wrAccessBreakdown");
	#pragma HLS STREAM variable=rxTcpFsm2wrAccessBreakdown depth=8
	#pragma HLS DATA_PACK variable=rxTcpFsm2wrAccessBreakdown

	static stream<ap_uint<1> >				rxEngDoubleAccess("rxEngDoubleAccess");
	#pragma HLS STREAM variable=rxEngDoubleAccess depth=8

#if WINDOW_SCALE
	static stream<rxEngPktMetaInfo>		rxEngMetaInfoBeforeWindow("rx_metaDataFoBeforeWindow");
	#pragma HLS STREAM variable=rxEngMetaInfoBeforeWindow depth=8
	#pragma HLS DATA_PACK variable=rxEngMetaInfoBeforeWindow
#endif	

	rxEngPseudoHeaderInsert( 
			ipRxData, 
			rxEng_pseudo_packet_to_metadata,
			rxEng_pseudo_packet_to_checksum);

	rxEngGetMetaData(
			rxEng_pseudo_packet_to_metadata,
#if WINDOW_SCALE
			rxEngMetaInfoBeforeWindow,
#else			
			rxEngMetaInfoFifo,
#endif			
			rxEng_tcp_payload);

#if WINDOW_SCALE
	rxParseTcpOptions (
			rxEngMetaInfoBeforeWindow,
			rxEngMetaInfoFifo);
#endif			

	rxEngVerifyCheckSum (
			rxEng_pseudo_packet_res_checksum,
			rxEngMetaInfoFifo,
			rxEng_VerifyChecksumDrop,
			rxEngMetaInfoValid,
			rxEng2portTable_req);

	rxEngMetadataHandler(	
			rxEngMetaInfoValid,
			portTable2rxEng_rsp,
			sLookup2rxEng_rsp,
			rxEng2sLookup_req,
			rxEng_metaHandlerEventFifo,
			rxEng_metaHandlerDropFifo,
			rxEng_fsmMetaDataFifo);

	rxEngTcpFSM(			
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
#if (STATISTICS_MODULE)
			rxEngStatsUpdate,
#endif			
#if (!RX_DDR_BYPASS)
			rxTcpFsm2wrAccessBreakdown,
			rx_internalNotificationFifo,
#else
			rxEng2rxApp_notification,
#endif
			rxEng2txApp_client_notification);


	rxEngPacketDropper(
			rxEng_tcp_payload,
			rxEng_VerifyChecksumDrop,
			rxEng_metaHandlerDropFifo,
			rxEng_fsmDropFifo,
#if (!RX_DDR_BYPASS)			
			rxPkgDrop2rxMemWriter);
#else	
			rxBufferWriteData);
#endif

#if (!RX_DDR_BYPASS)

	Rx_Data_to_Memory(
			rxPkgDrop2rxMemWriter, 
			rxTcpFsm2wrAccessBreakdown, 
			rxBufferWriteCmd, 
			rxBufferWriteData,
			rxEngDoubleAccess);

	rxEngAppNotificationDelayer(
			rxBufferWriteStatus, 
			rx_internalNotificationFifo, 
			rxEng2rxApp_notification, 
			rxEngDoubleAccess);

#endif
	rxEngEventMerger(
			rxEng_metaHandlerEventFifo,
			rxEng_fsmEventFifo,
			rxEng2eventEng_setEvent);
}


