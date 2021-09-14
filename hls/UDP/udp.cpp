/**********
Copyright (c) 2021, Xilinx, Inc.
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
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#include "udp.hpp"


void TableHandler (
    stream<udpMetadata>     &MetaIn,
    stream<ap_uint<16> >    &idIn,
    stream<tableResponse>   &DropMeta,
    stream<udpMetadata>     &MetaOut,
    socket_table            SocketTable[NUMBER_SOCKETS],
    ap_uint<16>             &numberSockets,
    ap_uint<32>             &myIpAddress) {
#pragma HLS INLINE off
#pragma HLS pipeline II=1
#pragma HLS ARRAY_PARTITION variable=SocketTable complete dim=1
#pragma HLS DISAGGREGATE variable=SocketTable

    udpMetadata     currRxMeta;
    ap_uint<16>  currId;
    socket_table cS;

    if (!MetaIn.empty()){
        MetaIn.read(currRxMeta);
        ap_int<17> index = -1;
        for (unsigned int i = 0; i < NUMBER_SOCKETS; i++) {
            if ((SocketTable[i].theirIP   == currRxMeta.theirIP)   &&
                (SocketTable[i].theirPort == currRxMeta.theirPort) &&
                (SocketTable[i].myPort    == currRxMeta.myPort)) {
                index = i;
                break;
            }
        }
        tableResponse currResp;
        currResp.drop = ((currRxMeta.myIP == byteSwap<32>(myIpAddress)) && (index != -1)) ? 0 : 1;
        currResp.id = index(15,0);
        currResp.user = userMetadata(byteSwap<32>(myIpAddress), currRxMeta.theirIP, currRxMeta.myPort, currRxMeta.theirPort);
        DropMeta.write(currResp);
    }

    if(!idIn.empty()){
        idIn.read(currId);
        cS = SocketTable[currId];
        MetaOut.write(udpMetadata(cS.theirIP,byteSwap<32>(myIpAddress),cS.theirPort,cS.myPort,cS.valid));
    }

    numberSockets = NUMBER_SOCKETS;
}



/**
 * @brief      This function parses the incoming packet at IP level, gets its metadata (IP src, IP dst, Port src and Port dst).
 *             It sends packet metatada for evaluation, socket being listen to?
 *             It also shave-off the IP and UDP headers from the packet and send the remaining to rxEngPacketDropper
 *
 * @param      DataIn   IP packet
 * @param      MetaOut  Packet metadata
 * @param      DataOut  UDP payload
 */
void udpRxEngine (
    stream<axiWord>         &DataIn,
    stream<udpMetadata>     &MetaOut,
    stream<axiWordi>        &DataOut){
#pragma HLS INLINE off
#pragma HLS pipeline II=1

    enum ureStateEnum {HEADER, CONSUME, EXTRAWORD};
    static ureStateEnum ure_state = HEADER;
    static axiWordi     prevWord;

    ap_uint<32>         ip_dst;
    ap_uint<32>         ip_src;
    ap_uint<16>         port_src;
    ap_uint<16>         port_dst;
    ap_uint<16>         udp_lenght;
    ap_uint<16>         udp_checksum;
    ap_uint<16>         ip_totalLen;
    ap_uint< 4>         ip_headerlen;
    axiWord             currWord;
    axiWordi            sendWord = axiWordi(0,0,0);
    axiWordi            auxWord = axiWordi(0,0,0);

    switch (ure_state){
        case HEADER:
            if (!DataIn.empty()){
                DataIn.read(currWord);
                /*Parse IP Header*/
                ip_headerlen    = currWord.data( 3, 0);                 // Read IP header len
                ip_totalLen     = byteSwap<16>( currWord.data( 31, 16));    // Read IP total len
                ip_src          = byteSwap<32>( currWord.data(127, 96));
                ip_dst          = byteSwap<32>( currWord.data(159,128));
                /*Parse UDP Header*/
                port_src        = byteSwap<16>(currWord.data(175,160));
                port_dst        = byteSwap<16>(currWord.data(191,176));
                udp_lenght      = byteSwap<16>(currWord.data(207,192));
                udp_checksum    = byteSwap<16>(currWord.data(223,208));

                /*Create next previous word*/
                auxWord.data(287, 0)  = currWord.data(511, 224);
                auxWord.keep( 35, 0)  = currWord.keep( 63,  28);
                auxWord.last          = currWord.last;

                MetaOut.write(udpMetadata(ip_src, ip_dst, port_src, port_dst, 1)); // TODO Maybe check if payload
                if (currWord.last)
                    DataOut.write(auxWord);
                else 
                    ure_state = CONSUME;

                prevWord              = auxWord;
            }
            break;
        case CONSUME:
            if (!DataIn.empty()){
                DataIn.read(currWord);

                /*Create current word*/
                sendWord.data(287,  0)  = prevWord.data(287,  0);
                sendWord.keep( 35,  0)  = prevWord.keep( 35,  0);
                sendWord.data(511,288)  = currWord.data(223,  0);
                sendWord.keep( 63, 36)  = currWord.keep( 27,  0);

                /*Create next previous word*/
                prevWord.data(287 , 0)  = currWord.data(511, 224);
                prevWord.keep( 35 , 0)  = currWord.keep( 63,  28);
                prevWord.last           = currWord.last;

                if (currWord.last){
                    if (currWord.keep.bit(28))
                        ure_state = EXTRAWORD;
                    else {
                        sendWord.last = 1;
                        ure_state = HEADER;
                    }
                }
                DataOut.write(sendWord);
            }
            break;
        case EXTRAWORD:
            sendWord.data(287,  0)  = prevWord.data(287,  0);
            sendWord.keep( 35,  0)  = prevWord.keep( 35,  0);
            sendWord.last            = 1;
            DataOut.write(sendWord);
            ure_state = HEADER;
            break;
    }
}

/**
 * @brief      This module reads the metadata and forward the payload depending on the socket status.
 *             If the payload is forwarded, the id is included in tdest
 *
 * @param      repdDataIn   Input Payload
 * @param      repdMetaIn   Input Metadata, id and drop decision
 * @param      repdDataOut  Output Payload
 */
void rxEngPacketDropper (
    stream<axiWordi>        &repdDataIn,
    stream<tableResponse>   &repdMetaIn,
    stream<axiWordUdp>      &repdDataOut
    ){

#pragma HLS INLINE off
#pragma HLS pipeline II=1

    enum repdStateEnum {GET_RESPONSE, READ};
    static repdStateEnum repd_state = GET_RESPONSE;

    static tableResponse   response;
    axiWordi        currWord;
    axiWordUdp      sendWord;

    switch (repd_state){
        case GET_RESPONSE:
            if (!repdMetaIn.empty()){
                repdMetaIn.read(response);
                repd_state = READ;
            }
            break;
        case READ:
            if (!repdDataIn.empty()){
                repdDataIn.read(currWord);
                sendWord.data = currWord.data;
                sendWord.keep = currWord.keep;
                sendWord.dest = response.id;
                sendWord.last = currWord.last;
                sendWord.user = (response.user.myIP, response.user.theirIP, response.user.myPort, response.user.theirPort);
                if (!response.drop)
                    repdDataOut.write(sendWord);
                if (currWord.last)
                    repd_state = GET_RESPONSE;
            }
            break;
    }
}


void appGetMetaData (
    stream<axiWordUdp>      &DataIn,
    stream<axiWordi>        &DataOut,
    stream<ap_uint<16> >    &idOut,
    stream<ap_uint<16> >    &ploadLenOut){
#pragma HLS INLINE off
#pragma HLS pipeline II=1

    enum agmdStateEnum {GET_METADATA, FORWARD};
    static agmdStateEnum agmd_state = GET_METADATA;
    static ap_uint<16>  lenCount;
    
    axiWordUdp currWord;
    axiWordi sendWord;

    switch(agmd_state){
        case GET_METADATA:
            if (!DataIn.empty() && !DataOut.full()){
                DataIn.read(currWord);
                idOut.write(currWord.dest);
                sendWord = axiWordi(currWord.data, currWord.keep, currWord.last);
                DataOut.write(sendWord);
                
                lenCount = 64;

                if (currWord.last) {
                    ploadLenOut.write(keep2len(currWord.keep));
                }
                else
                    agmd_state = FORWARD;
            }
            break;
        case FORWARD:
            if (!DataIn.empty() && !DataOut.full()){
                DataIn.read(currWord);
                sendWord = axiWordi(currWord.data, currWord.keep, currWord.last);
                DataOut.write(sendWord);
                if (currWord.last) {
                    ploadLenOut.write((lenCount + keep2len(currWord.keep)));
                    agmd_state = GET_METADATA;
                }
                else
                    lenCount +=64;
            }
            break;
    }

}

void udpTxEngine (
    stream<axiWordi>        &DataIn,
    stream<udpMetadata>     &MetaIn,
    stream<ap_uint<16> >    &ploadLenIn,
    stream<axiWord>         &DataOut){
#pragma HLS INLINE off
#pragma HLS pipeline II=1

    enum uteStateEnum {GET_METADATA, PKTHEADER, FORWARD, EXTRAWORD, DROP};
    static uteStateEnum ute_state = GET_METADATA;
    
    static axiWord      prevWord;
    static udpMetadata  currMetaData;
    static ap_uint<16>  ip_len;
    static ap_uint<16>  udp_len;
    
    axiWordi    currWord;
    axiWord     sendWord;
    ap_uint<16> currLen;

    sendWord.data = 0;
    sendWord.keep = 0;
    sendWord.last = 0;

    switch (ute_state) {
        case GET_METADATA:
            if (!MetaIn.empty() && !ploadLenIn.empty()){
                MetaIn.read(currMetaData);
                ploadLenIn.read(currLen);

                ip_len  = currLen + 20 + 8;
                udp_len = currLen + 8;
                ute_state = (currMetaData.valid) ? PKTHEADER : DROP;
            }
            break;
        case PKTHEADER:
            if (!DataIn.empty()){
                DataIn.read(currWord);
                // Create the IP header
                sendWord.data(  7,  0) = 0x45;                                  // Version && IHL
                sendWord.data( 15,  8) = 0;                                     // TOS
                sendWord.data( 31, 16) = byteSwap<16>(ip_len);                  //length
                sendWord.data( 47, 32) = 0;                                     // Identification
                sendWord.data( 50, 48) = 0;                                     //Flags
                sendWord.data( 63, 51) = 0x0;                                   //Fragment Offset
                sendWord.data( 71, 64) = 0x40;                                  // TTL
                sendWord.data( 79, 72) = 0x11;                                  // Protocol = UDP
                sendWord.data( 95, 80) = 0;                                     // IP header checksum   
                sendWord.data(127, 96) = byteSwap<32>(currMetaData.myIP);       // Source IP
                sendWord.data(159,128) = byteSwap<32>(currMetaData.theirIP);    // Destination IP
                // Create the UDP header
                sendWord.data(175,160) = byteSwap<16>(currMetaData.myPort);     // Source Port
                sendWord.data(191,176) = byteSwap<16>(currMetaData.theirPort);  // Destination Port
                sendWord.data(207,192) = byteSwap<16>(udp_len);                 // UDP Length
                sendWord.data(223,208) = 0x0;                                   // CheckSum
                sendWord.keep( 27,  0) = 0xFFFFFFF;                             // Assign 1 to all positions of IP and UDP header

                // Insert Payload to the packet
                sendWord.data(511,224)  = currWord.data(287,  0);
                sendWord.keep( 63, 28)  = currWord.keep( 35,  0);
                // Create next previous word
                prevWord.data(223,  0) = currWord.data(511,288);
                prevWord.keep( 27,  0) = currWord.keep( 63, 36);

                sendWord.last = 0;

                if (currWord.last){
                    if (currWord.keep.bit(36))     // If this bit is enable we need one more cycle to complete the packet
                        ute_state = EXTRAWORD;
                    else{
                        sendWord.last = 1;
                        ute_state = GET_METADATA;
                    }
                }
                else {
                    ute_state = FORWARD;
                }

                DataOut.write(sendWord);
            }
            break;
        case FORWARD:
            if (!DataIn.empty()){
                DataIn.read(currWord);

                // Continue inserting payload to the packet
                sendWord.data(223,  0)  = prevWord.data(223,  0);
                sendWord.keep( 27,  0)  = prevWord.keep( 27,  0);
                sendWord.data(511,224)  = currWord.data(287,  0);
                sendWord.keep( 63, 28)  = currWord.keep( 35,  0);

                // Create next previous word
                prevWord.data(223,  0) = currWord.data(511,288);
                prevWord.keep( 27,  0) = currWord.keep( 63, 36);

                sendWord.last = 0;
                if (currWord.last){
                    if (currWord.keep.bit(36))     // If this bit is enable we need one more cycle to complete the packet
                        ute_state = EXTRAWORD;
                    else{
                        sendWord.last = 1;
                        ute_state = GET_METADATA;
                    }
                }
                DataOut.write(sendWord);
            }        
            break;
        case EXTRAWORD:
            // Insert last piece of payload to the packet
            sendWord.data(223,  0)  = prevWord.data(223,  0);
            sendWord.keep( 27,  0)  = prevWord.keep( 27,  0);
            sendWord.last = 1;
            DataOut.write(sendWord);
            ute_state = GET_METADATA;
            break;
        case DROP:
            if (!DataIn.empty()){
                DataIn.read(currWord);
                if (currWord.last)
                    ute_state = GET_METADATA;
            }
            break;
    }

}


void udp(
    // At IP level
    stream<axiWord>         &rxUdpDataIn,
    stream<axiWord>         &txUdpDataOut,
    // Application Data
    stream<axiWordUdp>      &DataOutApp,  
    stream<axiWordUdp>      &DataInApp,
       
    //IP Address Input
    ap_uint<32>             &myIpAddress,

    socket_table            SocketTable[NUMBER_SOCKETS],
    ap_uint<16>             &numberSockets ) {
#pragma HLS DATAFLOW


#pragma HLS INTERFACE axis register both port=rxUdpDataIn
#pragma HLS INTERFACE axis register both port=txUdpDataOut
#pragma HLS INTERFACE axis register both port=DataOutApp
#pragma HLS INTERFACE axis register both port=DataInApp

#pragma HLS INTERFACE ap_none register port=myIpAddress
#pragma HLS INTERFACE s_axilite port=SocketTable bundle=s_axilite
#pragma HLS INTERFACE s_axilite port=numberSockets bundle=s_axilite
#pragma HLS INTERFACE ap_ctrl_none port=return


    // Internal streams are of a different type, since Vitis HLS does not support
    // standard ap_axiu for internal channels
    static stream<axiWordi>     ureDataPayload("ureDataPayload");
    #pragma HLS STREAM variable=ureDataPayload depth=256

    static stream<axiWordi>     agmdDataOut("agmdDataOut");
    #pragma HLS STREAM variable=agmdDataOut depth=256

    static stream<udpMetadata>     ureMetaData("ureMetaData");
    #pragma HLS STREAM variable=ureMetaData depth=32

    static stream<tableResponse>     rthDropFifo("rthDropFifo");
    #pragma HLS STREAM variable=rthDropFifo depth=32

    static stream<ap_uint<16> >     agmdIdOut("agmdIdOut");
    #pragma HLS STREAM variable=agmdIdOut depth=256

    static stream<ap_uint<16> >     agmdpayloadLenOut("agmdpayloadLenOut");
    #pragma HLS STREAM variable=agmdpayloadLenOut depth=256


    static stream<udpMetadata>     txthMetaData("txthMetaData");
    #pragma HLS STREAM variable=txthMetaData depth=32


    udpRxEngine (
        rxUdpDataIn,
        ureMetaData,
        ureDataPayload);

    TableHandler (
        ureMetaData,
        agmdIdOut,
        rthDropFifo,
        txthMetaData,
        SocketTable,
        numberSockets,
        myIpAddress);

    rxEngPacketDropper (
        ureDataPayload,
        rthDropFifo,
        DataOutApp);

    appGetMetaData (
        DataInApp,
        agmdDataOut,
        agmdIdOut,
        agmdpayloadLenOut);

    udpTxEngine (
        agmdDataOut,
        txthMetaData,
        agmdpayloadLenOut,
        txUdpDataOut);

}
