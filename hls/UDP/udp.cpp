/**********
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


void rxTableHandler (
    stream<udpMetadata>     &rxMetaIn,
    socket_table            SocketTable[NUMBER_SOCKETS],
    stream<tableResponse>   &rxDropMeta,
    ap_uint<16>             &numberSockets,
    ap_uint<32>             &myIpAddress) {
#pragma HLS INLINE off
#pragma HLS pipeline II=1

#pragma HLS ARRAY_PARTITION variable=SocketTable complete dim=1


    udpMetadata     currRxMeta;


    if (!rxMetaIn.empty()){
        rxMetaIn.read(currRxMeta);
        ap_int<17> index = -1;
        for (unsigned int i = 0; i < NUMBER_SOCKETS; i++) {
		//#pragma HLS UNROLL
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
        rxDropMeta.write(currResp);
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
    stream<axiWord>         &DataOut){
#pragma HLS INLINE off
#pragma HLS pipeline II=1

    enum ureStateEnum {HEADER, CONSUME, EXTRAWORD};
    static ureStateEnum ure_state = HEADER;
    static axiWord      prevWord;


    ap_uint<4>          ip_headerlen;
    ap_uint<16>         ip_totalLen;
    ap_uint<32>         ip_dst;
    ap_uint<32>         ip_src;
    ap_uint<32>         port_src;
    ap_uint<32>         port_dst;
    ap_uint<32>         udp_lenght;

    axiWord             currWord;
    axiWord             sendWord = axiWord();
    axiWord             auxWord = axiWord();

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

                /*Create next previous word*/
                auxWord.data(303 , 0)  = currWord.data(511, 208);
                auxWord.keep( 37 , 0)  = currWord.keep( 63,  26);
                auxWord.last           = currWord.last;


                MetaOut.write(udpMetadata(ip_src, ip_dst, port_src, port_dst)); // TODO Maybe check if payload
                if (currWord.last)
                    DataOut.write(auxWord);
                else 
                    ure_state = CONSUME;

                prevWord        = auxWord;
            }
            break;
        case CONSUME:
            if (!DataIn.empty()){
                DataIn.read(currWord);

                /*Create current word*/
                sendWord.data(303 ,  0)  = prevWord.data(303, 0);
                sendWord.keep( 37 ,  0)  = prevWord.keep( 37, 0);
                sendWord.data(511 ,304)  = currWord.data(207, 0);
                sendWord.keep( 63 , 38)  = currWord.keep( 25, 0);

                /*Create next previous word*/
                auxWord.data(303 , 0)  = currWord.data(511, 208);
                auxWord.keep( 37 , 0)  = currWord.keep( 63,  26);
                auxWord.last           = currWord.last;


                if (currWord.last){
                    if (currWord.keep.bit(26))
                        ure_state = EXTRAWORD;
                    else {
                        sendWord.last = 1;
                        ure_state = HEADER;
                    }
                }
                DataOut.write(sendWord);

                prevWord        = auxWord;
            }
            break;
        case EXTRAWORD:
            sendWord.data(303 ,  0)  = prevWord.data(303, 0);
            sendWord.data( 37 ,  0)  = prevWord.keep( 37, 0);
            sendWord.last            = 1;
            DataOut.write(sendWord);
            ure_state = HEADER;
            break;
    }
}

/**
 * @brief      This module reads the metadata and forward the payload depending on the socket status.
 *             If the payload is forwarded, the id is included in both tuser and tdest
 *
 * @param      repdDataIn   Input Payload
 * @param      repdMetaIn   Input Metadata, id and drop decision
 * @param      repdDataOut  Output Payload
 */
void rxEngPacketDropper (
    stream<axiWord>         &repdDataIn,
    stream<tableResponse>   &repdMetaIn,
    stream<axiWordUdp>      &repdDataOut
    ){

#pragma HLS INLINE off
#pragma HLS pipeline II=1

    enum repdStateEnum {GET_RESPONSE, READ};
    static repdStateEnum repd_state = GET_RESPONSE;

    static tableResponse   currResponse;
    axiWord         currWord;
    axiWordUdp      sendWord;

    switch (repd_state){
        case GET_RESPONSE:
            if (!repdMetaIn.empty()){
                repdMetaIn.read(currResponse);
                repd_state = READ;
            }
            break;
        case READ:
            if (!repdDataIn.empty()){
                repdDataIn.read(currWord);
                sendWord = axiWordUdp(currWord.data,currWord.keep, currResponse.id, currWord.last);
                if (currResponse.drop)
                    repdDataOut.write(sendWord);
                if (currWord.last)
                    repd_state = GET_RESPONSE;
            }
            break;
    }
}


void udp(
    // At IP level
    stream<axiWord>         &rxUdpDataIn,
    stream<axiWord>         &txUdpDataOut,
    // Appplication Data
    stream<axiWordUdp>      &rxDataOutApp,  
    stream<axiWordUdp>      &txDataInApp,
       
    //IP Address Input
    ap_uint<32>             &myIpAddress,

    socket_table            SocketTableRx[NUMBER_SOCKETS],
    socket_table            SocketTableTx[NUMBER_SOCKETS],
    ap_uint<16>             &numberSockets ) {

#pragma HLS DATAFLOW


#pragma HLS INTERFACE axis register both port=rxUdpDataIn name=rxUdpDataIn
#pragma HLS INTERFACE axis register both port=txUdpDataOut name=txUdpDataOut
#pragma HLS INTERFACE axis register both port=rxDataOutApp name=rxDataOutApp
#pragma HLS INTERFACE axis register both port=txDataInApp name=txDataInApp

#pragma HLS INTERFACE ap_stable register port=myIpAddress name=myIpAddress
#pragma HLS INTERFACE s_axilite port=SocketTableRx bundle=s_axilite
#pragma HLS INTERFACE s_axilite port=SocketTableTx bundle=s_axilite
#pragma HLS INTERFACE s_axilite port=numberSockets bundle=s_axilite
#pragma HLS INTERFACE ap_ctrl_none port=return


    static stream<axiWord>     ureDataPayload("ureDataPayload");
    #pragma HLS STREAM variable=ureDataPayload depth=512
    #pragma HLS DATA_PACK variable=ureDataPayload

    static stream<udpMetadata>     ureMetaData("ureMetaData");
    #pragma HLS STREAM variable=ureMetaData depth=32
    #pragma HLS DATA_PACK variable=ureMetaData

    static stream<tableResponse>     rthDropFifo("rthDropFifo");
    #pragma HLS STREAM variable=rthDropFifo depth=32
    #pragma HLS DATA_PACK variable=rthDropFifo


    udpRxEngine (
        rxUdpDataIn,
        ureMetaData,
        ureDataPayload);


    rxTableHandler (
        ureMetaData,
        SocketTableRx,
        rthDropFifo,
        numberSockets,
        myIpAddress);


    rxEngPacketDropper (
        ureDataPayload,
        rthDropFifo,
        rxDataOutApp);

}
