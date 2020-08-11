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
#include "../TOE/testbench/pcap2stream.hpp"

template<int D>
ap_uint<D> byteSwap(ap_uint<D> inputVector) {
    
    ap_uint<D> aux = 0;

    for (unsigned int i =0; i < D ; i +=8){
        aux(i+7 , i) = inputVector(D-1-i, D-8-i);
    }

    return aux;
}

arpTableEntry                   arpTable[256];

void initTable(void){
    for (unsigned int m=0 ; m < 255 ; m++){
        arpTable[m] = arpTableEntry(0,0,0);
    }
}

int main(void){
    stream<axiWord>                 arpDataIn("arpDataIn");
    stream<axiWord>                 arpDataOut("arpDataOut");
    stream<axiWord>                 arpDataOut2pcap("arpDataOut");
    stream<ap_uint<32> >            macIpEncode_req("arpDataOut2pcap");
    stream<arpTableReply>           macIpEncode_rsp("macIpEncode_rsp");
    ap_uint< 1>                     arp_scan= 0;
    ap_uint<48>                     myMacAddress = byteSwap<48>(0x000A35029DE5);
    ap_uint<32>                     myIpAddress  = byteSwap<32>(0xC0A80005);
    ap_uint<32>                     gatewayIP    = byteSwap<32>(0xC0A80001);
    ap_uint<32>                     networkMask  = byteSwap<32>(0xFFFFFF00);
    char                            *outputPcap = (char *) "outputARP.pcap";

    // Clear ARP table
    initTable();

    for (unsigned int i = 0; i < 100000 ; i++){
        arp_server(  
            arpDataIn,
            macIpEncode_req,
            arpDataOut,
            macIpEncode_rsp,
            arpTable,
            arp_scan,
            myMacAddress,
            myIpAddress,
            gatewayIP,
            networkMask);
        if (i == 2353)
            arp_scan = 1;
    }

    unsigned int outpkt= 0;
    while(!arpDataOut.empty()){
        axiWord currWord;
        arpDataOut.read(currWord);
        arpDataOut2pcap.write(currWord);
        if (currWord.last)
            outpkt++;
    }

    std::cout << "There were a total of " << outpkt << " output ARP packets" << std::endl;
    /*Store ARP out*/
    stream2pcap(outputPcap,true,false,arpDataOut2pcap,true);

    return 0;
}