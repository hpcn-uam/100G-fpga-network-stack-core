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

#include "icmp_server.hpp"


ap_uint<16> computeCheckSum20B(ap_uint<160> ipHeader){
#pragma HLS INLINE
    ap_uint<16> checksumL0[10]={0,0,0,0,0,0,0,0,0,0};
    ap_uint<17> checksumL1[5] = {0,0,0,0,0};
    ap_uint<18> checksumL2[2] = {0,0};
    ap_uint<20> checksumL3;
    ap_uint<17> checksumL4_r;
    ap_uint<17> checksumL4_o;
    ap_uint<16> checksum;
    
    assign2array: for (int m = 0; m < 10 ; m++){
#pragma HLS UNROLL
        checksumL0[m] = ipHeader((m*16)+15,m*16);
    }

    sumElementsL1 : for (int m = 0; m < 10 ; m=m+2){
#pragma HLS UNROLL
        checksumL1[m/2] = checksumL0[m] + checksumL0[m+1];
    }

    checksumL2[0] = checksumL1[0] + checksumL1[1];
    checksumL2[1] = checksumL1[2] + checksumL1[3];
    checksumL3 = checksumL2[0] + checksumL2[1] + checksumL1[4];

    checksumL4_r = checksumL3(15, 0) + checksumL3(19,16);
    checksumL4_o = checksumL3(15, 0) + checksumL3(19,16) + 1;
    
    if (checksumL4_r.bit(16))
        checksum   = ~checksumL4_o(15,0);
    else 
        checksum   = ~checksumL4_r(15,0);

    return checksum;
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


/** @ingroup icmp_server
 *  Main function
 *  @param[in]      dataIn
 *  @param[out]     dataOut
 */
void icmp_server(
            stream<axiWord>&            dataIn,
            ap_uint<32>&                myIpAddress,
            stream<axiWord>&            dataOut) {

#pragma HLS INTERFACE ap_ctrl_none port=return


#pragma HLS INTERFACE axis register both port=dataIn name=s_axis_icmp
#pragma HLS INTERFACE axis register both port=dataOut name=m_axis_icmp
#pragma HLS INTERFACE ap_stable register port=myIpAddress name=myIpAddress

#pragma HLS pipeline II=1

    enum    aiStates {READ_PACKET, EVALUATE_CONDITIONS, SEND_FIRST_WORD, FORWARD, DROP_PACKET};
    static aiStates aiFSMState = READ_PACKET;
    
    static axiWord         prevWord;
    static ap_uint< 32>    ipDestination;
    static ap_uint< 17>    icmpChecksum;
    static ap_uint<  8>    icmpType;
    static ap_uint<  8>    icmpCode;

    static ap_uint< 16>    auxInchecksum_r;
    ap_uint< 17>    icmpChecksumTmp;
    ap_uint< 16>    auxInchecksum;
    axiWord         currWord;
    ap_uint<160>    auxIPheader;

    switch(aiFSMState){
        case READ_PACKET:
            if (!dataIn.empty()){               // Packet at IP level. No IP options are taken into account
                dataIn.read(currWord);
                for (int m = 0 ; m < 20 ; m++){ // Arrange IP header
                    #pragma HLS UNROLL
                    auxIPheader(159-m*8,152-m*8) = currWord.data((m*8)+7,m*8);
                }
                ipDestination           = currWord.data(159,128);                   // Get destination IP to be verified
                icmpType    = currWord.data(167,160);
                icmpCode    = currWord.data(175,168);

                icmpChecksum = (currWord.data(183,176),currWord.data(191,184)) + 0x0800;
                auxInchecksum_r = computeCheckSum20B(auxIPheader);

                aiFSMState = EVALUATE_CONDITIONS;

                prevWord   = currWord;
            }
            break;

        case EVALUATE_CONDITIONS:
            if ((ipDestination == myIpAddress) && (icmpType == ECHO_REQUEST && (icmpCode == 0)) && auxInchecksum_r == 0){
                aiFSMState = SEND_FIRST_WORD;
            }
            else {
                if (!currWord.last){
                    aiFSMState = DROP_PACKET;
                }
                else{
                    aiFSMState = READ_PACKET;
                }
            }
            break;
                
        case SEND_FIRST_WORD:

            currWord = prevWord;

            currWord.data( 71, 64) = 128;                                   // IP time to live
            currWord.data( 95, 72) = ICMP_PROTOCOL;                         // IP protocol
            currWord.data( 95, 80) = 0;                                     // clear IP checksum, it is inserted later
            currWord.data(127, 96) = prevWord.data(159,128);                
            currWord.data(159,128) = prevWord.data(127, 96);                // swap IPs

            for (int m = 0 ; m < 20 ; m++){ // Arrange IP header
                auxIPheader(159-m*8,152-m*8) = currWord.data((m*8)+7,m*8);
            }

            icmpChecksumTmp = icmpChecksum(15,0) + icmpChecksum.bit(16);
            currWord.data(167,160) = ECHO_REPLY;
            currWord.data(191,176) = (icmpChecksumTmp(7,0),icmpChecksumTmp(15,8));     // Insert ICMP checksum

            if (prevWord.last)
                aiFSMState = READ_PACKET;
            else
                aiFSMState = FORWARD;

            dataOut.write(currWord);
            break;
        case DROP_PACKET:
            if (!dataIn.empty()){
                dataIn.read(currWord);

                if (currWord.last)
                    aiFSMState = READ_PACKET;
            }
            break;
        case FORWARD:
            if (!dataIn.empty()){
                dataIn.read(currWord);
                dataOut.write(currWord);
                
                if (currWord.last)
                    aiFSMState = READ_PACKET;
            }
            break;
    }

}
