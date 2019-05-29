/************************************************
Copyright (c) 2019, HPCN Group, UAM Spain.
All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>
************************************************/

#include "port_handler.hpp"

template<unsigned int bitWidth>
ap_uint<bitWidth> byteSwap(ap_uint<bitWidth> inputVector) {
    ap_uint<bitWidth> aux = 0;
    for (unsigned int m = 0 ; m < bitWidth ; m+= 8)
        aux(bitWidth-m-1,bitWidth-m-8) = inputVector(m+7,m);
    return aux;
}


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
    ap_uint<16> dstPort;

    switch (ph_fsm_state){
        case FIRST_WORD :
            if (!dataIn.empty()){
                dataIn.read(currWord);
                dstPort = byteSwap<16>(currWord.data(191,176)); // Get destination port
                destination = (portRangeMin <= dstPort && dstPort <=portRangeMax) ? 1 : 0;  // Check if the port falls into the range
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
