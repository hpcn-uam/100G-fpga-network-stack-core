/************************************************
Copyright (c) 2016, Xilinx, Inc.
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

#include "arp_server.hpp"
/** @ingroup arp_server
 *
 */
void arp_pkg_receiver(
	  	stream<axiWord>&			arpDataIn,
      	stream<arpReplyMeta>& 		arpReplyMetaFifo,
      	stream<arpTableEntry>& 		arpTableInsertFifo,
      	ap_uint<32>&             	myIpAddress) {

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

  static ap_uint<3> 	wordCount = 0;
  static ap_uint<16>	opCode;
  static ap_uint<32>	protoAddrDst;
  static ap_uint<32>	inputIP;
  static arpReplyMeta 	meta;
  
  axiWord currWord;

  /* 100 Gb/s implementation */ 

 	if (!arpDataIn.empty()) {
		arpDataIn.read(currWord);

		if (wordCount==0) {
			meta.srcMac  		= currWord.data(95, 48);
			meta.ethType 		= currWord.data(111, 96);
			meta.hwType 		= currWord.data(127, 112);
			meta.protoType 		= currWord.data(143, 128);
			meta.hwLen 			= currWord.data(151, 144);
			meta.protoLen 		= currWord.data(159, 152);
			opCode 				= currWord.data(175, 160);
			meta.hwAddrSrc 		= currWord.data(223, 176);
			meta.protoAddrSrc 	= currWord.data(254, 224);
			protoAddrDst 		= currWord.data(335, 304);

			if (currWord.last == 1) {
				if ((opCode == REQUEST) && (protoAddrDst == myIpAddress))
				  arpReplyMetaFifo.write(meta);
				else {
					if ((opCode == REPLY) && (protoAddrDst == myIpAddress))
						arpTableInsertFifo.write(arpTableEntry(meta.hwAddrSrc, meta.protoAddrSrc, true));
				}
				wordCount = 0;
			}
			else
				wordCount++;
		}
	}
}

/** @ingroup arp_server
 *
 */
void arp_pkg_sender(
		stream<arpReplyMeta>&     arpReplyMetaFifo,
        stream<ap_uint<32> >&     arpRequestMetaFifo,
        stream<axiWord>&          arpDataOut,
        ap_uint<48>&			  myMacAddress,
        ap_uint<32>&              myIpAddress) {

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	enum arpSendStateType {ARP_IDLE, ARP_REPLY, ARP_SENTRQ};
  	static arpSendStateType aps_fsmState = ARP_IDLE;
  
  	static arpReplyMeta		replyMeta;
  	static ap_uint<32>		inputIP;
  
  	axiWord sendWord;
  
  	switch (aps_fsmState) {
   		case ARP_IDLE:
	    	if (!arpReplyMetaFifo.empty()){
	    	  arpReplyMetaFifo.read(replyMeta);
	    	  aps_fsmState = ARP_REPLY;
	    	}
	    	else if (!arpRequestMetaFifo.empty()){
	    	  arpRequestMetaFifo.read(inputIP);
	    	  aps_fsmState = ARP_SENTRQ;
	    	}
    		break;
  		case ARP_SENTRQ:
			sendWord.data(47, 0)  	= BROADCAST_MAC;
			sendWord.data(95, 48) 	= myMacAddress;			// Sorce MAC
			sendWord.data(111, 96) 	= 0x0608;				// Ethertype

			sendWord.data(127, 112) = 0x0100;				// Hardware type
			sendWord.data(143, 128) = 0x0008;				// Protocol type IP Address
			sendWord.data(151, 144) = 6;					// HW Address Length
			sendWord.data(159, 152) = 4;					// Protocol Address Length
			sendWord.data(175, 160) = REQUEST;
			sendWord.data(223, 176) = myMacAddress;
			sendWord.data(255, 224) = myIpAddress;			// MY_IP_ADDR;
			sendWord.data(303, 256) = 0;					// Sought-after MAC pt.1
			sendWord.data(335, 304) = inputIP;
			sendWord.data(383, 336) = 0;					// Sought-after MAC pt.1
			sendWord.data(511, 384) = 0x454546464F43;		// padding

			sendWord.keep = 0x0FFFFFFFFFFFFFFF;
			sendWord.last = 1;
			aps_fsmState = ARP_IDLE;

			arpDataOut.write(sendWord);
			break;
  		case ARP_REPLY:

			sendWord.data(47, 0)  	= replyMeta.srcMac;
			sendWord.data(95, 48) 	= myMacAddress;				// Sorce MAC
			sendWord.data(111, 96) 	= replyMeta.ethType;		// Ethertype

			sendWord.data(127, 112) = replyMeta.hwType;			// Hardware type
			sendWord.data(143, 128) = replyMeta.protoType;		// Protocol type IP Address
			sendWord.data(151, 144) = replyMeta.hwLen;			// HW Address Length
			sendWord.data(159, 152) = replyMeta.protoLen;		// Protocol Address Length
			sendWord.data(175, 160) = REPLY;
			sendWord.data(223, 176) = myMacAddress;
			sendWord.data(255, 224) = myIpAddress;				//MY_IP_ADDR;
			sendWord.data(303, 256) = replyMeta.hwAddrSrc;		// Sought-after MAC pt.1
			sendWord.data(335, 304) = replyMeta.protoAddrSrc;
			sendWord.data(383, 336) = 0;						// Sought-after MAC pt.1
			sendWord.data(511, 384) = 0x454546464F43;			// padding

			sendWord.keep = 0x0FFFFFFFFFFFFFFF;
			sendWord.last = 1;
			aps_fsmState = ARP_IDLE;

			arpDataOut.write(sendWord);		  		
			break;
  	} //switch
}

/** @ingroup arp_server
 *
 */
void arp_table( 
		stream<arpTableEntry>& 		arpTableInsertFifo,
        stream<ap_uint<32> >& 		macIpEncode_req,
        stream<arpTableReply>& 		macIpEncode_rsp,
        stream<ap_uint<32> >& 		arpRequestMetaFifo){

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	static 	arpTableEntry		arpTable[256];
	#pragma HLS RESOURCE variable=arpTable core=RAM_1P_BRAM
	#pragma HLS DEPENDENCE variable=arpTable inter false

	ap_uint<32>			query_ip;
	arpTableEntry		currEntry;


	if (!arpTableInsertFifo.empty())
	{
		arpTableInsertFifo.read(currEntry);
		arpTable[currEntry.ipAddress(31,24)] = currEntry;
	}
	else if (!macIpEncode_req.empty())
	{
		macIpEncode_req.read(query_ip);
		currEntry = arpTable[query_ip(31,24)];
		if (!currEntry.valid)
		{
			// send ARP request
			arpRequestMetaFifo.write(query_ip);
		}
		macIpEncode_rsp.write(arpTableReply(currEntry.macAddress, currEntry.valid));
	}

}

#ifdef SCANNING

void genInitARP (
		ap_uint<32>& 				myIpAddress,
		stream<ap_uint<32> >& 		macIpEncodeIn,
		stream<arpTableReply>&    	macIpEncode_rsp_i,
		stream<arpTableReply>&    	macIpEncode_rsp_o,
		stream<ap_uint<32> >& 		macIpEncodeOut){

#pragma HLS PIPELINE II=1
#pragma INLINE off

	enum gia_states {SETUP, WAIT_TIME, GEN_IP, CONSUME,FWD};
	static gia_states gia_fsm_state = SETUP;
#pragma HLS RESET variable=gia_fsm_state


	static ap_uint<8> 	ip_lsb;
	static ap_uint<10>	time_counter;
	ap_uint<32>			ip_aux;
	arpTableReply 		macEnc_i;
	static ap_uint<9>	encode_rsp;

	switch (gia_fsm_state){
		case SETUP: 
			ip_lsb = 0;
			time_counter = 0;
			encode_rsp = 0;
			gia_fsm_state = WAIT_TIME;
			break;
		case WAIT_TIME:	
			time_counter++;
			if (time_counter == 99)
				gia_fsm_state = GEN_IP;
			break;
		case GEN_IP:
			ip_aux =  (ip_lsb,myIpAddress(23,0));
			if (ip_lsb != myIpAddress(31,24)){
				macIpEncodeOut.write(ip_aux);
			}
			if (ip_lsb == 0xFF){
				gia_fsm_state = CONSUME;
			}
			ip_lsb++;
			if (!macIpEncode_rsp_i.empty()){
				macIpEncode_rsp_i.read(macEnc_i);
				encode_rsp++;
			}
			break;
		case CONSUME:
			if (!macIpEncode_rsp_i.empty()){
				macIpEncode_rsp_i.read(macEnc_i);
				encode_rsp++;
				if (encode_rsp == 255) {
					gia_fsm_state = FWD;
				}
			}
			break;	
		case FWD: 
			if (!macIpEncodeIn.empty()){
				macIpEncodeOut.write(macIpEncodeIn.read());
			}
			if (!macIpEncode_rsp_i.empty()){
				macIpEncode_rsp_i.read(macEnc_i);
				macIpEncode_rsp_o.write(macEnc_i);
			}
			break;
	}

}

#endif

/** @ingroup arp_server
 *
 */
void arp_server(  
		stream<axiWord>&          		arpDataIn,
    	stream<ap_uint<32> >&     		macIpEncode_req,
		stream<axiWord>&          		arpDataOut,
		stream<arpTableReply>&    		macIpEncode_rsp,
		ap_uint<48>& 					myMacAddress,
		ap_uint<32>& 					myIpAddress)	{


#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

#pragma HLS INTERFACE ap_stable register port=myMacAddress
#pragma HLS INTERFACE ap_stable register port=myIpAddress

#pragma HLS INTERFACE axis register both port=arpDataIn
#pragma HLS INTERFACE axis register both port=arpDataOut
#pragma HLS INTERFACE axis register both port=macIpEncode_req
#pragma HLS INTERFACE axis register both port=macIpEncode_rsp

#pragma HLS DATA_PACK variable=macIpEncode_rsp


  static stream<arpReplyMeta>     arpReplyMetaFifo("arpReplyMetaFifo");
#pragma HLS STREAM variable=arpReplyMetaFifo depth=4
#pragma HLS DATA_PACK variable=arpReplyMetaFifo
  
  static stream<ap_uint<32> >   arpRequestMetaFifo("arpRequestMetaFifo");
#pragma HLS STREAM variable=arpRequestMetaFifo depth=4
  //#pragma HLS DATA_PACK variable=arpRequestMetaFifo

  static stream<arpTableEntry>    arpTableInsertFifo("arpTableInsertFifo");
#pragma HLS STREAM variable=arpTableInsertFifo depth=4
#pragma HLS DATA_PACK variable=arpTableInsertFifo

#if SCANNING
 	static stream<ap_uint<32> >     		macIpEncode_i("macIpEncode_i");
 	#pragma HLS STREAM variable=macIpEncode_i depth=4

 	static stream<arpTableReply> 		macIpEncode_rsp_i("macIpEncode_rsp_i");
 	#pragma HLS STREAM variable=macIpEncode_rsp_i depth=4
 	
	genInitARP (
		myIpAddress,
		macIpEncode_req,
		macIpEncode_rsp_i,
		macIpEncode_rsp,
		macIpEncode_i);
 #endif		

  	arp_pkg_receiver(
  		arpDataIn, 
  		arpReplyMetaFifo, 
  		arpTableInsertFifo, 
  		myIpAddress);
  
  	arp_pkg_sender(
  		arpReplyMetaFifo, 
  		arpRequestMetaFifo, 
  		arpDataOut, 
  		myMacAddress, 
  		myIpAddress);

  	arp_table(
  		arpTableInsertFifo, 
#if SCANNING
  		macIpEncode_i,
  		macIpEncode_rsp_i,
#else  		
  		macIpEncode_req, 
  		macIpEncode_rsp, 
#endif  		
  		arpRequestMetaFifo);
}
