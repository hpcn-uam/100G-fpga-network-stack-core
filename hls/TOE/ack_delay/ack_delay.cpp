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

#include "ack_delay.hpp"

using namespace hls;

void ack_delay(	stream<extendedEvent>&	eventEng2ackDelay_event,
				stream<extendedEvent>&	eventEng2txEng_event,
				stream<ap_uint<1> >&	ackDelayFifoReadCount,
				stream<ap_uint<1> >&	ackDelayFifoWriteCount)
{
#pragma HLS INLINE off
#pragma HLS PIPELINE II=1

	static ap_uint<14> ack_table[MAX_SESSIONS];
	#pragma HLS RESOURCE variable=ack_table core=RAM_2P_BRAM
	#pragma HLS DEPENDENCE variable=ack_table inter false
	static ap_uint<16>	ad_pointer = 0;
	extendedEvent ev;

	if (!eventEng2ackDelay_event.empty()) {
		eventEng2ackDelay_event.read(ev);
		ackDelayFifoReadCount.write(1);
		// Check if there is a delayed ACK
		if (ev.type == ACK && ack_table[ev.sessionID] == 0) {
			ack_table[ev.sessionID] = TIME_64us;
		}
		else {
			// Assumption no SYN/RST
			ack_table[ev.sessionID] = 0;
			eventEng2txEng_event.write(ev);
			ackDelayFifoWriteCount.write(1);
		}
	}
	else {
		if (ack_table[ad_pointer] > 0) {
			if (ack_table[ad_pointer] == 1) {
				eventEng2txEng_event.write(event(ACK, ad_pointer));
				ackDelayFifoWriteCount.write(1);
			}
			// Decrease value
			ack_table[ad_pointer] -= 1;
		}
		ad_pointer++;
		if (ad_pointer == MAX_SESSIONS) {
			ad_pointer = 0;
		}
	}
}
