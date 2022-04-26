/************************************************
Copyright (c) 2020, Xilinx, Inc.
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
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2020 Xilinx, Inc.
************************************************/

#include "arp_server.hpp"

/** @ingroup    arp_server
 *
 * @brief      Parse ARP packets and generate a transaction with
 *             metatada if the packet is a REQUEST
 *             Generate a transaction to update the ARP table if 
 *             the packet is a REPLAY
 *
 * @param      arpDataIn           Incoming ARP packet at Ethernet level
 * @param      arpReplyOut    	   Internal metatadata to generate a replay
 * @param      arpTableInsertOut   Insert a new element in the ARP table
 * @param      myIpAddress         My IP address
 */
void arp_pkg_receiver(
	  	stream<axiWord>&			arpDataIn,
      	stream<arpReplyMeta>& 		arpReplyOut,
      	stream<arpTableEntry>& 		arpTableInsertOut,
      	ap_uint<32>&             	myIpAddress) {

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

  static ap_uint<4> 	wordCount = 0;
  ap_uint<16>	opCode;
  ap_uint<32>	protoAddrDst;
  arpReplyMeta 	meta;
  axiWord currWord;

 	if (!arpDataIn.empty()) {
		arpDataIn.read(currWord);
		// The first transaction contains all the necessary information
		if (wordCount==0) {
			meta.srcMac  		= currWord.data( 95,  48);
			meta.ethType 		= currWord.data(111,  96);
			meta.hwType 		= currWord.data(127, 112);
			meta.protoType 		= currWord.data(143, 128);
			meta.hwLen 			= currWord.data(151, 144);
			meta.protoLen 		= currWord.data(159, 152);
			opCode 				= currWord.data(175, 160);
			meta.hwAddrSrc 		= currWord.data(223, 176);
			meta.protoAddrSrc 	= currWord.data(255, 224);
			protoAddrDst 		= currWord.data(335, 304);

			if ((opCode == REQUEST) && (protoAddrDst == myIpAddress))
			  	arpReplyOut.write(meta);
			else if ((opCode == REPLY) && (protoAddrDst == myIpAddress))
				arpTableInsertOut.write(arpTableEntry(meta.hwAddrSrc, meta.protoAddrSrc, true));
		}

		if (currWord.last)
			wordCount = 0;
		else
			wordCount++;
	}
}

/** @ingroup    arp_server
 *
 * @brief      Sends both ARP replay or ARP request packets 
 *
 * @param      arpReplyIn          Metadata information to generate an ARP replay
 * @param      arpRequestIn  	   IP address to generate an ARP request
 * @param      arpDataOut          APR packet out at Ethernet level
 * @param      myMacAddress        My MAC address
 * @param      myIpAddress         My IP address
 * @param      gatewayIP           My gateway ip
 * @param      networkMask         Network mask
 */
void arp_pkg_sender(
		stream<arpReplyMeta>&     arpReplyIn,
        stream<ap_uint<32> >&     arpRequestIn,
        stream<axiWord>&          arpDataOut,
        ap_uint<48>&			  myMacAddress,
        ap_uint<32>&              myIpAddress,
        ap_uint<32>&              gatewayIP,
        ap_uint<32>&              networkMask) {

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	enum arpSendStateType {ARP_IDLE, ARP_REPLY, ARP_SENTRQ};
  	static arpSendStateType aps_fsmState = ARP_IDLE;
  
  	static arpReplyMeta		replyMeta;
  	static ap_uint<32>		inputIP;
  
  	axiWord sendWord;
  	ap_uint<32>             auxQueryIP;
  	switch (aps_fsmState) {
   		case ARP_IDLE:
	    	if (!arpReplyIn.empty()){
	    	  arpReplyIn.read(replyMeta);
	    	  aps_fsmState = ARP_REPLY;
	    	}
	    	else if (!arpRequestIn.empty()){
	    	  arpRequestIn.read(inputIP);
	    	  aps_fsmState = ARP_SENTRQ;
	    	}
    		break;
    	// SEND REQUEST
  		case ARP_SENTRQ:

  			// Check whether the IP is within the FPGA subnetwork.
  			// IP within subnet, use IP
  			// IP outside subnet use gatewayIP
            if((inputIP & networkMask) == (myIpAddress & networkMask))
                auxQueryIP = inputIP;
            else
                auxQueryIP = gatewayIP;

			sendWord.data(47, 0)  	= BROADCAST_MAC;
			sendWord.data(95, 48) 	= myMacAddress;			// Source MAC
			sendWord.data(111, 96) 	= 0x0608;				// Ethertype

			sendWord.data(127, 112) = 0x0100;				// Hardware type
			sendWord.data(143, 128) = 0x0008;				// Protocol type IP Address
			sendWord.data(151, 144) = 6;					// HW Address Length
			sendWord.data(159, 152) = 4;					// Protocol Address Length
			sendWord.data(175, 160) = REQUEST;
			sendWord.data(223, 176) = myMacAddress;
			sendWord.data(255, 224) = myIpAddress;			// MY_IP_ADDR;
			sendWord.data(303, 256) = 0;					// Sought-after MAC pt.1
			sendWord.data(335, 304) = auxQueryIP;
			sendWord.data(383, 336) = 0;					// Sought-after MAC pt.1
			sendWord.data(511, 384) = 0x454546464F43;		// padding

			sendWord.keep = 0x0FFFFFFFFFFFFFFF;				// 60-Byte packet
			sendWord.last = 1;
			aps_fsmState = ARP_IDLE;

			arpDataOut.write(sendWord);
			break;
		//SEND REPLAY 
  		case ARP_REPLY:

			sendWord.data(47, 0)  	= replyMeta.srcMac;
			sendWord.data(95, 48) 	= myMacAddress;				// Source MAC
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

/** @ingroup    arp_server
 *
 * @brief      Update and query ARP table
 *
 * @param      arpTableInsertIn    Insert a new element in the table
 * @param      macIpEncode_req     Request to get the associated MAC address of a given IP address
 * @param      macIpEncode_rsp     Response to an IP association  
 * @param      arpRequestOut  	   Generate an ARP request
 * @param      arpTable            ARP table accessible from AXI4-Lite as well
 * @param      myIpAddress         My IP address
 * @param      gatewayIP           My gateway IP
 * @param      networkMask         Network mask
 */
void arp_table( 
		stream<arpTableEntry>& 		arpTableInsertIn,
        stream<ap_uint<32> >& 		macIpEncode_req,
        stream<arpTableEntry>& 		macIpEncode_rsp,
        stream<ap_uint<32> >& 		arpRequestOut,
        arpTableEntry				arpTable[256],
        ap_uint<32>&                myIpAddress,
        ap_uint<32>&                gatewayIP,
        ap_uint<32>&                networkMask){

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	#pragma HLS DEPENDENCE variable=arpTable inter false

	ap_uint<32>			query_ip;
	ap_uint<32>         auxIP;
	arpTableEntry		currEntry;


	if (!arpTableInsertIn.empty()) {
		arpTableInsertIn.read(currEntry);
		arpTable[currEntry.get_id()] = currEntry;
	}
	else if (!macIpEncode_req.empty()) {
		macIpEncode_req.read(query_ip);

        /*Check whether the current IP belongs to the FPGA subnet, otherwise use gateway's IP*/
        if((query_ip & networkMask) == (myIpAddress & networkMask))
            auxIP = query_ip;
        else
            auxIP = gatewayIP;

		currEntry = arpTable[auxIP(31,24)];
		// If the entry is not valid generate an ARP request
		if (!currEntry.valid) {
			arpRequestOut.write(query_ip);	// send ARP request
		}
		// Send response to an IP to MAC association
		macIpEncode_rsp.write(currEntry);
	}

}

/**
 * @brief      Generate an ARP discovery for all the IP in the subnet
 *
 * @param      macIpEncodeIn      Inbound request to associate an IP to a MAC
 * @param      macIpEncode_rsp_i  Internal response to an IP association
 * @param      macIpEncode_rsp_o  Outbound response to an IP association
 * @param      macIpEncodeOut     Internal request to associate an IP to a MAC
 * @param      myIpAddress        My ip address
 */
void genARPDiscovery (
		stream<ap_uint<32> >& 		macIpEncodeIn, 
		stream<arpTableEntry>&    	macIpEncode_rsp_i,
		stream<arpTableReply>&    	macIpEncode_rsp_o,
		stream<ap_uint<32> >& 		macIpEncodeOut,
		ap_uint< 1>& 				arp_scan,
		ap_uint<32>& 				myIpAddress){

#pragma HLS PIPELINE II=1
#pragma INLINE off

	enum gia_states {SETUP, WAIT_TIME, GEN_IP, FWD, WAIT_RESPONSE};
#if SCANNING	
	static gia_states gia_fsm_state = WAIT_TIME;
#else
	static gia_states gia_fsm_state = FWD;
#endif

#pragma HLS RESET variable=gia_fsm_state


	static ap_uint<32>	time_counter = 0;
	static ap_uint<8> 	ip_lsb = 0;
	static ap_uint< 1>	arp_scan_1d = 0;
	ap_uint<32>			ip_aux;
	arpTableEntry 		macEnc_i;
	ap_uint<1>			checkArpScan = 0;

	switch (gia_fsm_state){
		// Wait for a period of time until starting with the ARP Request generation
		case WAIT_TIME:	
			if (time_counter == MAX_COUNT)
				gia_fsm_state = GEN_IP;
			time_counter++;
			break;
		// Generate IP addresses in the same subnetwork as my IP address and go to wait state
		case GEN_IP:
			ip_aux =  (ip_lsb,myIpAddress(23,0));
			macIpEncodeOut.write(ip_aux);			// Support for Gratuitous ARP
            ip_lsb++;
            gia_fsm_state = WAIT_RESPONSE;
            break;
        // Wait for ARP response
        case WAIT_RESPONSE:
            if (!macIpEncode_rsp_i.empty()){
                macIpEncode_rsp_i.read(macEnc_i);
                // The ARP ends when overflow happens
                if (ip_lsb == 0){
                	gia_fsm_state = FWD;
                }
				else
                	gia_fsm_state = GEN_IP;
            }
            break;
        // Forward incoming request and outgoing replays appropriately                   
		case FWD: 
			if (!macIpEncodeIn.empty())
				macIpEncodeOut.write(macIpEncodeIn.read());
			else
				checkArpScan = 1;
			
			if (!macIpEncode_rsp_i.empty())
				macIpEncode_rsp_o.write(macIpEncode_rsp_i.read());
			else
				checkArpScan = 1;

			if ((checkArpScan == 1) && (arp_scan_1d == 0) && (arp_scan == 1)){
				arp_scan = 0;
				gia_fsm_state = SETUP;
			}
			break;
		// Clear variables
		case SETUP:
			ip_lsb 	      = 0;
			gia_fsm_state = GEN_IP;
			break;
	}
	arp_scan_1d = arp_scan;

}


/** @ingroup    arp_server
 *
 * @brief      Implements the basic functionality of ARP
 * 			   Send REQUEST, process REPLAY and keep a table
 * 			   where IP and MAC are associated
 *
 * @param      arpDataIn        Inbound ARP packets
 * @param      macIpEncode_req  Request to associate an IP to a MAC
 * @param      arpDataOut       Outbound ARP packets
 * @param      macIpEncode_rsp  Response to the IP to a MAC association
 * @param      arpTable         ARP table accessible from AXI4-Lite as well
 * @param      myMacAddress     My MAC address
 * @param      myIpAddress      My IP address
 * @param      gatewayIP        My gateway IP
 * @param      networkMask      network mask
 */
void arp_server(  
		stream<axiWord>&          		arpDataIn,
		stream<ap_uint<32> >&     		macIpEncode_req,
		stream<axiWord>&          		arpDataOut,
		stream<arpTableReply>&    		macIpEncode_rsp,
		arpTableEntry					arpTable[256],
		ap_uint< 1>& 					arp_scan,
		ap_uint<48>& 					myMacAddress,
		ap_uint<32>& 					myIpAddress,
		ap_uint<32>&                    gatewayIP,
		ap_uint<32>&                    networkMask)	{


#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

#pragma HLS INTERFACE ap_none register port=myMacAddress
#pragma HLS INTERFACE ap_none register port=myIpAddress
#pragma HLS INTERFACE ap_none register port=gatewayIP
#pragma HLS INTERFACE ap_none register port=networkMask


#pragma HLS INTERFACE axis register both port=arpDataIn
#pragma HLS INTERFACE axis register both port=arpDataOut
#pragma HLS INTERFACE axis register both port=macIpEncode_req
#pragma HLS INTERFACE axis register both port=macIpEncode_rsp
#pragma HLS INTERFACE s_axilite port=arp_scan bundle=s_axilite
#pragma HLS INTERFACE s_axilite port=arpTable bundle=s_axilite
#pragma HLS BIND_STORAGE variable=arpTable type=ram_t2p
#pragma HLS DISAGGREGATE variable=arpTable

	static stream<arpReplyMeta>     arpReplyFifo("arpReplyFifo");
	#pragma HLS STREAM variable=arpReplyFifo depth=4
	  
	static stream<ap_uint<32> >   arpRequestFifo("arpRequestFifo");
	#pragma HLS STREAM variable=arpRequestFifo depth=4

	static stream<arpTableEntry>    arpTableInsertFifo("arpTableInsertFifo");
	#pragma HLS STREAM variable=arpTableInsertFifo depth=4

	static stream<ap_uint<32> >     		macIpEncode_i("macIpEncode_i");
	#pragma HLS STREAM variable=macIpEncode_i depth=4

	static stream<arpTableEntry> 		macIpEncode_rsp_i("macIpEncode_rsp_i");
	#pragma HLS STREAM variable=macIpEncode_rsp_i depth=4

	genARPDiscovery (
		macIpEncode_req,
		macIpEncode_rsp_i,
		macIpEncode_rsp,
		macIpEncode_i,
		arp_scan,
		myIpAddress);

  	arp_pkg_receiver(
  		arpDataIn, 
  		arpReplyFifo, 
  		arpTableInsertFifo, 
  		myIpAddress);
  
  	arp_pkg_sender(
  		arpReplyFifo, 
  		arpRequestFifo, 
  		arpDataOut, 
  		myMacAddress, 
  		myIpAddress,
  		gatewayIP,
        networkMask);

  	arp_table(
  		arpTableInsertFifo, 
  		macIpEncode_i,
  		macIpEncode_rsp_i,
  		arpRequestFifo,
  		arpTable,
  		myIpAddress,
  		gatewayIP,
        networkMask);
}
