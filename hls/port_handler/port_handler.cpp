/************************************************
BSD 3-Clause License

Copyright (c) 2019, HPCN Group, UAM Spain (hpcn-uam.es)
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

#include "port_handler.hpp"

template<unsigned int bitWidth>
ap_uint<bitWidth> byteSwap(ap_uint<bitWidth> inputVector) {
    ap_uint<bitWidth> aux = 0;
    for (unsigned int m = 0 ; m < bitWidth ; m+= 8)
        aux(bitWidth-m-1,bitWidth-m-8) = inputVector(m+7,m);
    return aux;
}




/**
 * @brief      Enable the tuser signal when the source port is in the proper range.
 *             At this point the packets are at ip level
 *
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            -    
 * |Version|  IHL  |Type of Service|          Total Length         |     0- 31  |    
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |    
 * |         Identification        |Flags|      Fragment Offset    |    32- 63  |    
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |    
 * |  Time to Live |    Protocol   |         Header Checksum       |    64- 95   \   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+              IP 
 * |                       Source Address                          |    96-127   /   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |    
 * |                    Destination Address                        |   128-159  |    
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |    
 * |          Source Port          |        Destination Port       |   160-191  |        
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            -        
 * |                        Sequence Number                        |   192-223  |        
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |        
 * |                     Acknowledgment Number                     |   224-255   \       
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+              TCP    
 * | Offset|  Res. |     Flags     |             Window            |   256-287   /       
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |        
 * |            Checksum           |         Urgent Pointer        |   288-319  |        
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            |        
 * |                    Options                    |    Padding    |   320-351  |        
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            -        
 *
 * @param      dataIn        The data in
 * @param      dataOut       The data out
 * @param      portRangeMin  The port range minimum
 * @param      portRangeMax  The port range maximum
 */

void port_handler(
            stream<axiWord>&            dataIn,
            stream<axiWordOut>&         dataOut,
            ap_uint<16>&                portRangeMin,
            ap_uint<16>&                portRangeMax) {

#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS pipeline II=1

#pragma HLS INTERFACE axis register both port=dataIn name=s_axis
#pragma HLS INTERFACE axis register both port=dataOut name=m_axis


#pragma HLS INTERFACE s_axilite port=portRangeMin bundle=portRange
#pragma HLS INTERFACE s_axilite port=portRangeMax bundle=portRange

    enum ph_states {FIRST_WORD, REMAINING_WORDS};
    static ph_states ph_fsm_state = FIRST_WORD;
    static ap_uint<1>  destination;
    axiWord     currWord;
    axiWordOut  sendWord;
    ap_uint<16> srcPort;

    switch (ph_fsm_state){
        case FIRST_WORD :
            if (!dataIn.empty()){
                dataIn.read(currWord);
                srcPort = byteSwap<16>(currWord.data(175,160)); // Get destination port
                destination = ((srcPort >= portRangeMin) && (srcPort <= portRangeMax)) ? 1 : 0;  // Check if the port falls into the range
                //std::cout << "Port " << std::dec << srcPort << " dst " << destination << std::endl;
                // Create the output word 
                sendWord.data = currWord.data;
                sendWord.keep = currWord.keep;
                sendWord.last = currWord.last;
                sendWord.dest = destination;
                dataOut.write(sendWord);
                if (!currWord.last)
                    ph_fsm_state = REMAINING_WORDS;
            }
            break;
        case REMAINING_WORDS :
            if (!dataIn.empty()){
                dataIn.read(currWord);
                // Create the output word 
                sendWord.data = currWord.data;
                sendWord.keep = currWord.keep;
                sendWord.last = currWord.last;
                sendWord.dest = destination;
                dataOut.write(sendWord);
                // in the last transaction go back to the first state
                if (currWord.last)
                    ph_fsm_state = FIRST_WORD;
            }
            break;
        default :
            ph_fsm_state = FIRST_WORD;
            break;
    }

}
