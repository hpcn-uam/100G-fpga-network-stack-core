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
	static bool 		bypass_ddr=false;
	bool 				bypass_ddr_tmp=false;


#if (TCP_NODELAY)
	if (!txEng_isDDRbypass.empty() && !reading_payload && !extra_word && !bypass_ddr){
		txEng_isDDRbypass.read(bypass_ddr_tmp);
		//cout << "bypass_ddr: " << (bypass_ddr_tmp ? "Yes": "No" ) << endl;
		bypass_ddr 		= bypass_ddr_tmp;
	}
	else 
#endif	

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
	else if(!mem_payload_unaligned.empty()&& reading_payload && !align_words && !bypass_ddr){
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
	else if(!mem_payload_unaligned.empty()&& !reading_payload && align_words && !bypass_ddr){
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
	else if (extra_word && !bypass_ddr){
		extra_word = false;
		align_words = false;
		//cout << "Extra : " << hex << prevWord.data << "\tkeep: " << prevWord.keep << "\tlast: " << dec << prevWord.last << endl;
		tx_payload_aligned.write(prevWord);
	}
#if (TCP_NODELAY)
	else if (!txApp2txEng_data_stream.empty() && bypass_ddr){
		txApp2txEng_data_stream.read(currWord);
		tx_payload_aligned.write(currWord);
		if (currWord.last){
			bypass_ddr 		= false;
			reading_payload = false;
			align_words 	= false;
		}
	}
#endif	
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


/**
 * This module reads a command and determines whether two memory access are needed or not.
 * In base of that issues one or two memory write command(s), and the data is aligned properly, the realignment
 * if is necessary is done in the second access.
 *
 * @param      rxMemWrDataIn   Packet payload if any
 * @param      rxMemWrCmdIn    Internal command to write data into the memory. It does not take into account buffer overflow
 * @param      rxMemWrCmdOut   Command to the data mover. It takes into account buffer overflow. Two write commands when buffer overflows
 * @param      rxMemWrDataOut  Data to memory. If the buffer overflows, the second part of the data has to be realigned
 * @param      doubleAccess    If two memory writes are needed this flag is set, is not is cleared
 */
void Rx_Data_to_Memory(
					stream<axiWord>& 				rxMemWrDataIn,
					stream<mmCmd>&					rxMemWrCmdIn,
					stream<mmCmd>&					rxMemWrCmdOut,
					stream<axiWord>&				rxMemWrDataOut,
					stream<ap_uint<1> >&			doubleAccess)
{
#pragma HLS pipeline II=1
#pragma HLS INLINE off

	axiWord 			currWord;
	axiWord				sendWord;
	static axiWord		prevWord;
	mmCmd 				input_command;
	ap_uint<32>			saddr;
	ap_uint<17> 		buffer_overflow;
	mmCmd 				first_command;
	mmCmd 				second_command;

	static ap_uint<6>	byte_offset;
	static ap_uint<10>	number_of_words_to_send;
	static ap_uint<64> 	keep_first_trans;
	static ap_uint<10>	count_word_sent=1;

	static bool rxWrBreakDown 	= false;
	static bool reading_data 	= false;
	static bool second_write 	= false;
	static bool extra_word	 	= false;

	if (!rxMemWrCmdIn.empty() && !reading_data && !extra_word){
		rxMemWrCmdIn.read(input_command);

		saddr 				= input_command.saddr;
		buffer_overflow 	= input_command.saddr.range(15, 0) + input_command.bbt; // Compute the address of the last byte to write
		reading_data 		= true;
		if (buffer_overflow.bit(16)){	// The remaining buffer space is not enough. An address overflow has to be done
			// TODO: I think is enough with putting a last and the correct keep to determine the end of the first transaction. TEST IT!
			first_command.bbt 	= 65536 - input_command.bbt;	// Compute how much bytes are needed in the first transaction
			first_command.saddr = input_command.saddr;
			rxWrBreakDown = true;
			byte_offset 		= first_command.bbt.range(5,0);	// Determines the position of the MSB in the last word

			if (byte_offset != 0){ 								// Determines how many transaction are in the first memory access
				number_of_words_to_send = first_command.bbt.range(15,6) + 1;
			}
			else {
				number_of_words_to_send = first_command.bbt.range(15,6);
			}
			keep_first_trans 	= len2Keep(byte_offset);			// Get the keep of the last transaction of the first memory offset
			count_word_sent 	= 1;
		}
		else{
			first_command = input_command;
		}

		doubleAccess.write(rxWrBreakDown);					// Notify if there are two memory access
		rxMemWrCmdOut.write(first_command); 				// issue the first command
	}
	else if(!rxMemWrDataIn.empty() && reading_data && !second_write && !extra_word){
		rxMemWrDataIn.read(currWord);

		if (rxWrBreakDown){
			if (count_word_sent == number_of_words_to_send){
				currWord.keep 	= keep_first_trans;
				currWord.last 	= 1;
				second_command.saddr(31,16) 	= input_command.saddr(31,16);
				second_command.saddr(15,0) 		= 0;										// point to the beginning of the buffer
				second_command.bbt 				= input_command.bbt - first_command.bbt;	// Recompute the bytes to transfer in the second memory access
				rxMemWrCmdOut.write(second_command);										// Issue the second command
				second_write 	= true;
			}
			count_word_sent++;
			prevWord = currWord;
			rxMemWrDataOut.write(currWord);
		}
		else{												// There is not double memory access
			if (currWord.last){
				reading_data = false;
			}
			rxMemWrDataOut.write(currWord);
		}
	}
	else if(!rxMemWrDataIn.empty() && reading_data && second_write && !extra_word){
		rxMemWrDataIn.read(currWord);
		rx_align_two_64bytes_words (currWord, &prevWord, byte_offset, &sendWord);

		if (currWord.last){
//			if (!sendWord.last){		// If the sendWord has not have a last it is means that a extra transaction is needed
			if (currWord.keep.bit(64-byte_offset) && byte_offset!=0){ // TODO try to improve dependence
				extra_word = true;
			}
			reading_data = false;
			second_write = false;
		}
//		prevWord = currWord;
		rxMemWrDataOut.write(sendWord);
	}
	else if (extra_word) {
		extra_word = false;
		//prevWord.last = 1;
		rxMemWrDataOut.write(prevWord);
	}

}


/**
 * This module reads a command and determines whether two memory access are needed or not.
 * In base of that issues one or two memory write command(s), and the data is aligned properly, the realignment
 * if is necessary is done in the second access.
 *
 * @param      txMemWrDataIn   Packet payload if any
 * @param      txMemWrCmdIn    Internal command to write data into the memory. It does not take into account buffer overflow
 * @param      txMemWrCmdOut   Command to the data mover. It takes into account buffer overflow. Two write commands when buffer overflows
 * @param      rxMemWrDataOut  Data to memory. If the buffer overflows, the second part of the data has to be realigned
  */
void tx_Data_to_Memory(
					stream<axiWord>& 				txMemWrDataIn,
					stream<mmCmd>&					txMemWrCmdIn,
					stream<mmCmd>&					txMemWrCmdOut,
#if (TCP_NODELAY)					
					stream<axiWord>&				txApp2txEng_data_stream,
#endif
					stream<axiWord>&				txMemWrDataOut)
{
#pragma HLS pipeline II=1
#pragma HLS INLINE off

	axiWord 			currWord;
	axiWord				sendWord;
	static axiWord		prevWord;
	mmCmd 				input_command;
	ap_uint<32>			saddr;
	ap_uint<17> 		buffer_overflow;
	mmCmd 				first_command;
	mmCmd 				second_command;

	static ap_uint<6>	byte_offset;
	static ap_uint<10>	number_of_words_to_send;
	static ap_uint<64> 	keep_first_trans;
	static ap_uint<10>	count_word_sent=1;

	static bool rxWrBreakDown 	= false;
	static bool reading_data 	= false;
	static bool second_write 	= false;
	static bool extra_word	 	= false;

	if (!txMemWrCmdIn.empty() && !reading_data && !extra_word){
		txMemWrCmdIn.read(input_command);

		saddr 				= input_command.saddr;
		buffer_overflow 	= input_command.saddr.range(15, 0) + input_command.bbt; // Compute the address of the last byte to write
		reading_data 		= true;
		if (buffer_overflow.bit(16)){	// The remaining buffer space is not enough. An address overflow has to be done
			// TODO: I think is enough with putting a last and the correct keep to determine the end of the first transaction. TEST IT!
			first_command.bbt 	= 65536 - input_command.bbt;	// Compute how much bytes are needed in the first transaction
			first_command.saddr = input_command.saddr;
			rxWrBreakDown = true;
			byte_offset 		= first_command.bbt.range(5,0);	// Determines the position of the MSB in the last word

			if (byte_offset != 0){ 								// Determines how many transaction are in the first memory access
				number_of_words_to_send = first_command.bbt.range(15,6) + 1;
			}
			else {
				number_of_words_to_send = first_command.bbt.range(15,6);
			}
			keep_first_trans 	= len2Keep(byte_offset);			// Get the keep of the last transaction of the first memory offset
			count_word_sent 	= 1;
		}
		else{
			first_command = input_command;
		}

		txMemWrCmdOut.write(first_command); 				// issue the first command
	}
	else if(!txMemWrDataIn.empty() && reading_data && !second_write && !extra_word){
		txMemWrDataIn.read(currWord);
#if (TCP_NODELAY)
			txApp2txEng_data_stream.write(currWord);
#endif
		if (rxWrBreakDown){
			if (count_word_sent == number_of_words_to_send){
				currWord.keep 	= keep_first_trans;
				currWord.last 	= 1;
				second_command.saddr(31,16) 	= input_command.saddr(31,16);
				second_command.saddr(15,0) 		= 0;										// point to the beginning of the buffer
				second_command.bbt 				= input_command.bbt - first_command.bbt;	// Recompute the bytes to transfer in the second memory access
				txMemWrCmdOut.write(second_command);										// Issue the second command
				second_write 	= true;
			}
			count_word_sent++;
			prevWord = currWord;
			txMemWrDataOut.write(currWord);
		}
		else{												// There is not double memory access
			if (currWord.last){
				reading_data = false;
			}
			txMemWrDataOut.write(currWord);
		}
	}
	else if(!txMemWrDataIn.empty() && reading_data && second_write && !extra_word){
		txMemWrDataIn.read(currWord);
#if (TCP_NODELAY)
			txApp2txEng_data_stream.write(currWord);
#endif		
		rx_align_two_64bytes_words (currWord, &prevWord, byte_offset, &sendWord);

		if (currWord.last){
			if (currWord.keep.bit(64-byte_offset) && byte_offset!=0){ // TODO try to improve dependence
				extra_word = true;
			}
			reading_data = false;
			second_write = false;
		}
		txMemWrDataOut.write(sendWord);
	}
	else if (extra_word) {
		extra_word = false;
		txMemWrDataOut.write(prevWord);
	}

}
