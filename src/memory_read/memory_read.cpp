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

#include "memory_read.hpp"

using namespace hls;
using namespace std;


void app_ReadMemAccessBreakdown(
					stream<mmCmd>& 				inputMemAccess, 
					stream<mmCmd>& 				outputMemAccess, 
					stream<memDoubleAccess>& 	memAccessBreakdown) {

#pragma HLS pipeline II=1
#pragma HLS INLINE off
	static bool 		appBreakdown = false;
	static mmCmd 		appTempCmd;
	static ap_uint<16> 	appBreakTemp = 0;
	//static ap_uint<16> 	txPktCounter = 0;
	memDoubleAccess 	double_access=memDoubleAccess(false,0);

	if (appBreakdown == false) {
		if (!inputMemAccess.empty() && !outputMemAccess.full()) {
			appTempCmd = inputMemAccess.read();
			mmCmd tempCmd = appTempCmd;
			if ((appTempCmd.saddr.range(15, 0) + appTempCmd.bbt) > 65536) {
				appBreakTemp = 65536 - appTempCmd.saddr;
				tempCmd = mmCmd(appTempCmd.saddr, appBreakTemp);
				appBreakdown = true;

				double_access.double_access = true;
				double_access.offset 		= appBreakTemp(5,0);	// Offset of MSB byte valid in the last transaction of the first burst
			}
			outputMemAccess.write(tempCmd);
			memAccessBreakdown.write(double_access);
			//txPktCounter++;
			//std::cerr << std::dec << "MemCmd: " << cycleCounter << " - " << txPktCounter << " - " << std::hex << " - " << tempCmd.saddr << " - " << tempCmd.bbt << std::endl;
		}
	}
	else if (appBreakdown == true) {
		if (!outputMemAccess.full()) {
			appTempCmd.saddr.range(15, 0) = 0;
			//std::cerr << std::dec << "MemCmd: " << cycleCounter << " - " << std::hex << " - " << appTempCmd.saddr << " - " << appTempCmd.bbt - appBreakTemp << std::endl;
			outputMemAccess.write(mmCmd(appTempCmd.saddr, appTempCmd.bbt - appBreakTemp));
			appBreakdown = false;
		}
	}
}

void tx_ReadMemAccessBreakdown(
					stream<mmCmd>& 				inputMemAccess, 
					stream<mmCmd>& 				outputMemAccess, 
					stream<memDoubleAccess>& 	memAccessBreakdown) {

#pragma HLS pipeline II=1
#pragma HLS INLINE off
	static bool txEngBreakdown = false;
	static mmCmd txEngTempCmd;
	static ap_uint<16> 	txEngBreakTemp = 0;
	//static ap_uint<16> 	txPktCounter = 0;
	memDoubleAccess 	double_access=memDoubleAccess(false,0);

	if (txEngBreakdown == false) {
		if (!inputMemAccess.empty() && !outputMemAccess.full()) {
			txEngTempCmd = inputMemAccess.read();
			mmCmd tempCmd = txEngTempCmd;
			if ((txEngTempCmd.saddr.range(15, 0) + txEngTempCmd.bbt) > 65536) {
				txEngBreakTemp = 65536 - txEngTempCmd.saddr;
				tempCmd = mmCmd(txEngTempCmd.saddr, txEngBreakTemp);
				txEngBreakdown = true;

				double_access.double_access = true;
				double_access.offset 		= txEngBreakTemp(5,0);	// Offset of MSB byte valid in the last transaction of the first burst
			}
			outputMemAccess.write(tempCmd);
			memAccessBreakdown.write(double_access);
			//txPktCounter++;
			//std::cerr << std::dec << "MemCmd: " << cycleCounter << " - " << txPktCounter << " - " << std::hex << " - " << tempCmd.saddr << " - " << tempCmd.bbt << std::endl;
		}
	}
	else if (txEngBreakdown == true) {
		if (!outputMemAccess.full()) {
			txEngTempCmd.saddr.range(15, 0) = 0;
			//std::cerr << std::dec << "MemCmd: " << cycleCounter << " - " << std::hex << " - " << txEngTempCmd.saddr << " - " << txEngTempCmd.bbt - txEngBreakTemp << std::endl;
			outputMemAccess.write(mmCmd(txEngTempCmd.saddr, txEngTempCmd.bbt - txEngBreakTemp));
			txEngBreakdown = false;
		}
	}
}



/** @ingroup tx_engine
 *  It gets data from the memory which can be split into two burst.
 *  The main drawback is aligned the end of the first burst with the beginning
 *  of the second burst, because, it implies a 64 to 1 multiplexer.
 */


void tx_MemDataRead_aligner(
					stream<axiWord>& 			mem_payload_unaligned,
					stream<memDoubleAccess>& 	mem_two_access,
				
#if (TCP_NODELAY)
					stream<bool>&				txEng_isDDRbypass,
					stream<axiWord>&			txApp2txEng_data_stream,
#endif
					stream<axiWord>& 			tx_payload_aligned)
{
#pragma HLS INLINE off
#pragma HLS LATENCY max=1

	static ap_uint<6>	byte_offset;
	static bool			breakdownAccess=false;
	static bool			reading_payload=false;
	static bool			align_words=false;
	static bool			extra_word=false;
	memDoubleAccess 	mem_double_access;
	axiWord 			currWord;
	axiWord 			sendWord;
	static axiWord 		prevWord;
	axiWord 			next_previous_word;

	if (!mem_payload_unaligned.empty() && !mem_two_access.empty() && !reading_payload && !align_words && !extra_word){		// New data is available 
		mem_two_access.read(mem_double_access);

		breakdownAccess		= mem_double_access.double_access;
		byte_offset			= mem_double_access.offset;
		//cout << "byte offset: " << dec << byte_offset << endl;
		mem_payload_unaligned.read(currWord);

		if (currWord.last){
			if (breakdownAccess){
				align_words	= true;
			}
			else{
				align_words	= false;
				//cout << "Payload aligner 0: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
				tx_payload_aligned.write(currWord);
			}
			reading_payload = false;
		}
		else{
			reading_payload = true;
			//cout << "Payload aligner 1: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
			tx_payload_aligned.write(currWord);
		}

		prevWord = currWord;

	}
	else if(!mem_payload_unaligned.empty()&& reading_payload && !align_words){
		mem_payload_unaligned.read(currWord);
		if (currWord.last){
			if (breakdownAccess){
				align_words	= true;
			}
			else{
				align_words	= false;
				//cout << "Payload aligner 2: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
				tx_payload_aligned.write(currWord);
			}
			reading_payload = false;
		}
		else{
			//cout << "Payload aligner 3: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
			tx_payload_aligned.write(currWord);
		}
		prevWord = currWord;
	}
	else if(!mem_payload_unaligned.empty()&& !reading_payload && align_words){
		mem_payload_unaligned.read(currWord);
		
		tx_align_two_64bytes_words(currWord,prevWord,byte_offset,&sendWord,&next_previous_word);

		if (currWord.last){
			if(currWord.keep.bit(64-byte_offset) && byte_offset != 0){
				extra_word = true;
			}
			else {
				align_words = false;
			}
		}
		//cout << "Double access : " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
		tx_payload_aligned.write(sendWord);
		
		prevWord = next_previous_word;
	}
	else if (extra_word){
		extra_word = false;
		align_words = false;
		//cout << "Extra : " << hex << prevWord.data << "\tkeep: " << prevWord.keep << "\tlast: " << dec << prevWord.last << endl;
		tx_payload_aligned.write(prevWord);
	}

}


/** @
 *  It gets data from the memory which can be split into two burst.
 *  The main drawback is aligned the end of the first burst with the beginning
 *  of the second burst, because, it implies a 64 to 1 multiplexer.
 */


void app_MemDataRead_aligner(
					stream<axiWord>& 			mem_payload_unaligned,
					stream<memDoubleAccess>& 	mem_two_access,
					stream<axiWord>& 			app_data_aligned)
{
#pragma HLS INLINE off
#pragma HLS LATENCY max=1

	static ap_uint<6>	byte_offset;
	static bool			breakdownAccess=false;
	static bool			reading_payload=false;
	static bool			align_words=false;
	static bool			extra_word=false;
	memDoubleAccess 	mem_double_access;
	axiWord 			currWord;
	axiWord 			sendWord;
	static axiWord 		prevWord;
	axiWord 			next_previous_word;

	if (!mem_payload_unaligned.empty() && !mem_two_access.empty() && !reading_payload && !align_words && !extra_word){		// New data is available 
		mem_two_access.read(mem_double_access);

		breakdownAccess		= mem_double_access.double_access;
		byte_offset			= mem_double_access.offset;
		//cout << "byte offset: " << dec << byte_offset << endl;
		mem_payload_unaligned.read(currWord);

		if (currWord.last){
			if (breakdownAccess){
				align_words	= true;
			}
			else{
				align_words	= false;
				//cout << "Payload aligner 0: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
				app_data_aligned.write(currWord);
			}
			reading_payload = false;
		}
		else{
			reading_payload = true;
			//cout << "Payload aligner 1: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
			app_data_aligned.write(currWord);
		}

		prevWord = currWord;

	}
	else if(!mem_payload_unaligned.empty()&& reading_payload && !align_words){
		mem_payload_unaligned.read(currWord);
		if (currWord.last){
			if (breakdownAccess){
				align_words	= true;
			}
			else{
				align_words	= false;
				//cout << "Payload aligner 2: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
				app_data_aligned.write(currWord);
			}
			reading_payload = false;
		}
		else{
			//cout << "Payload aligner 3: " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
			app_data_aligned.write(currWord);
		}
		prevWord = currWord;
	}
	else if(!mem_payload_unaligned.empty()&& !reading_payload && align_words){
		mem_payload_unaligned.read(currWord);
		
		tx_align_two_64bytes_words(currWord,prevWord,byte_offset,&sendWord,&next_previous_word);

		if (currWord.last){
			if(currWord.keep.bit(64-byte_offset) && byte_offset != 0){
				extra_word = true;
			}
			else {
				align_words = false;
			}
		}
		//cout << "Double access : " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
		app_data_aligned.write(sendWord);
		
		prevWord = next_previous_word;
	}
	else if (extra_word){
		extra_word = false;
		align_words = false;
		//cout << "Extra : " << hex << prevWord.data << "\tkeep: " << prevWord.keep << "\tlast: " << dec << prevWord.last << endl;
		app_data_aligned.write(prevWord);
	}

}