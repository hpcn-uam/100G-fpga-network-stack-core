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

#include "memory_access.hpp"

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
					stream<axiWord>& 			DtaInNoAlig,
					stream<memDoubleAccess>& 	mem_two_access,
#if (TCP_NODELAY)
					stream<bool>&				txEng_isDDRbypass,
					stream<axiWord>&			txAppDataIn,
#endif					
					stream<axiWord>& 			DataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1
	
	enum tmra_states {READ_ACCESS , FWD_NO_BREAKDOWN , FWD_BREAKDOWN_0, FWD_BREAKDOWN_1, FWD_EXTRA

#if (TCP_NODELAY)
		, NO_USE_DDR , READ_BYPASS
#endif
	};
#if (TCP_NODELAY)	
	const tmra_states INITIAL_STATE = READ_BYPASS;
#else	
	const tmra_states INITIAL_STATE = READ_ACCESS;
#endif
	static tmra_states tmra_fsm_state = INITIAL_STATE;

	static ap_uint<6>	byte_offset;

	static axiWord 		prevWord;
	
	memDoubleAccess 	mem_double_access;
	axiWord 			currWord;
	axiWord 			sendWord;

	bool 				bypass_ddr_i;


	switch (tmra_fsm_state){
#if (TCP_NODELAY)	
		case READ_BYPASS: 
			if (!txEng_isDDRbypass.empty()){
				txEng_isDDRbypass.read(bypass_ddr_i);
				cout << endl << "Reading bypass port" ;
				if (bypass_ddr_i){
					cout << " it says bypass memory" ;	
					tmra_fsm_state = NO_USE_DDR;
				}
				else {
					cout << " it says use memory" ;	
					tmra_fsm_state = READ_ACCESS;
				}
				cout << endl << endl;
			}
			break;
#endif				
		case READ_ACCESS:
			if (!mem_two_access.empty()){
				mem_two_access.read(mem_double_access);

				byte_offset			= mem_double_access.offset;
				if (mem_double_access.double_access){
					cout << " DOUBLE ACCESS" ;	
					tmra_fsm_state = FWD_BREAKDOWN_0;
				}
				else {
					tmra_fsm_state = FWD_NO_BREAKDOWN;
				}
			}
			break;
		case FWD_NO_BREAKDOWN:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				DataOut.write(currWord);

				if (currWord.last){
					tmra_fsm_state = INITIAL_STATE;
				}
			}
			break;
		case FWD_BREAKDOWN_0:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);

				if (currWord.last){
					if (byte_offset == 0){			// This mean that the last has all byte valid
						DataOut.write(currWord);
					}
					tmra_fsm_state = FWD_BREAKDOWN_1;
				} 
				else {
					DataOut.write(currWord);
				}
				prevWord = currWord;
			}
			break;
		case FWD_BREAKDOWN_1:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				tx_align_two_64bytes_words(currWord,prevWord,byte_offset,sendWord);

				if (currWord.last){
					if (sendWord.last){
						tmra_fsm_state = INITIAL_STATE;
					}
					else {
						tmra_fsm_state = FWD_EXTRA;
					}
				}

				DataOut.write(sendWord);
				prevWord = currWord;
			}
			break;
		case FWD_EXTRA:
			tx_align_two_64bytes_words(axiWord(0,0,1),prevWord,byte_offset,sendWord);
			//sendWord.last = 1;
			DataOut.write(sendWord);
			tmra_fsm_state = INITIAL_STATE;
	
			break;
#if (TCP_NODELAY)			
		case NO_USE_DDR			:
			if (!txAppDataIn.empty()){
				txAppDataIn.read(currWord);
				DataOut.write(currWord);

				if (currWord.last){
					tmra_fsm_state = READ_BYPASS;
				}
				
			}
			break;
#endif			
	}

}


/** @
 *  It gets data from the memory which can be split into two burst.
 *  The main drawback is aligned the end of the first burst with the beginning
 *  of the second burst, because, it implies a 64 to 1 multiplexer.
 */


void app_MemDataRead_aligner(
					stream<axiWord>& 			DtaInNoAlig,
					stream<memDoubleAccess>& 	mem_two_access,
					stream<axiWord>& 			DataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1	
	
	enum amdra_fsm_states {READ_ACCESS , FWD_NO_BREAKDOWN , FWD_BREAKDOWN_0, FWD_BREAKDOWN_1, FWD_EXTRA};
	static amdra_fsm_states amdra_state = READ_ACCESS;

	static ap_uint<6>	byte_offset;

	static axiWord 		prevWord;
	
	memDoubleAccess 	mem_double_access;
	axiWord 			currWord;
	axiWord 			sendWord;

	switch (amdra_state){
		case READ_ACCESS:
			if (!mem_two_access.empty()){
				mem_two_access.read(mem_double_access);

				byte_offset			= mem_double_access.offset;
				if (mem_double_access.double_access){
					amdra_state = FWD_BREAKDOWN_0;
				}
				else {
					amdra_state = FWD_NO_BREAKDOWN;
				}
			}
			break;
		case FWD_NO_BREAKDOWN:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				DataOut.write(currWord);

				if (currWord.last){
					amdra_state = READ_ACCESS;
				}
			}
			break;
		case FWD_BREAKDOWN_0:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);

				if (currWord.last){
					if (byte_offset == 0){			// This mean that the last has all byte valid
						DataOut.write(currWord);
					}
					amdra_state = FWD_BREAKDOWN_1;
				} 
				else {
					DataOut.write(currWord);
				}
				prevWord = currWord;
			}
			break;
		case FWD_BREAKDOWN_1:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				tx_align_two_64bytes_words(currWord,prevWord,byte_offset,sendWord);

				if (currWord.last){
					if (sendWord.last){
						amdra_state = READ_ACCESS;
					}
					else {
						amdra_state = FWD_EXTRA;
					}
				}

				DataOut.write(sendWord);
				prevWord = currWord;
			}
			break;
		case FWD_EXTRA:
			tx_align_two_64bytes_words(axiWord(0,0,1),prevWord,byte_offset,sendWord);
			//sendWord.last = 1;
			DataOut.write(sendWord);
			amdra_state = READ_ACCESS;
			break;
	}

}

void Rx_Data_to_Memory(
					stream<axiWord>& 				DataIn,
					stream<mmCmd>&					CmdIn,
					stream<mmCmd>&					CmdOut,
					stream<axiWord>&				DataOut,
					stream<ap_uint<1> >&			doubleAccess)

{

#pragma HLS pipeline II=1
#pragma HLS INLINE off

	enum data2mem_fsm {WAIT_CMD, FWD_NO_BREAKDOWN, FWD_BREAKDOWN_0, FWD_BREAKDOWN_1, FWD_EXTRA};
	static data2mem_fsm data2mem_state = WAIT_CMD;

	static ap_uint<6>		byte_offset;
	static ap_uint<10>		number_of_words_to_send;
	static ap_uint<10>		count_word_sent=1;
	static mmCmd 			input_command;
	static ap_uint<23> 		bytes_first_command;
	static mmCmd 			command_i;

	bool 				rxWrBreakDown;
	ap_uint<17> 		buffer_overflow;

	axiWord 			currWord;
	axiWord 			sendWord;
	static axiWord 			prevWord;

	switch (data2mem_state) {
		case WAIT_CMD :
			if (!CmdIn.empty() && !CmdOut.full() && !doubleAccess.full()){

				CmdIn.read(input_command);

				buffer_overflow 	= input_command.saddr(15, 0) + input_command.bbt; // Compute the address of the last byte to write
				
				if (buffer_overflow.bit(16)){	// The remaining buffer space is not enough. An address overflow has to be done
					command_i.bbt 		= 65536 - input_command.bbt;	// Compute how much bytes are needed in the first transaction
					command_i.saddr 	= input_command.saddr;
					byte_offset 		= command_i.bbt.range(5,0);	// Determines the position of the MSB in the last word
					bytes_first_command = command_i.bbt;

					if (byte_offset != 0){ 								// Determines how many transaction are in the first memory access
						number_of_words_to_send = command_i.bbt.range(15,6) + 1;
					}
					else {
						number_of_words_to_send = command_i.bbt.range(15,6);
					}
					count_word_sent 	= 1;
					rxWrBreakDown 		= true;
					//data2mem_state = FWD_BREAKDOWN_0;
				}
				else {
					rxWrBreakDown 		= false;
					command_i 			= input_command;
				}
				
				data2mem_state 	= FWD_NO_BREAKDOWN;

				doubleAccess.write(rxWrBreakDown);					// Notify if there are two memory access
				CmdOut.write(command_i); 					// issue the first command

			}
			break;
		case FWD_NO_BREAKDOWN :
			if (!DataIn.empty() /*&& !DataOut.full()*/){
 				DataIn.read(currWord);
 				sendWord.data = currWord.data;
 				sendWord.keep = currWord.keep;
 				sendWord.last = currWord.last;

				if (currWord.last){
					data2mem_state = WAIT_CMD;
				}
				DataOut.write(sendWord);
			}
			break;
		case FWD_BREAKDOWN_0 :
			if (!DataIn.empty() /*&& !DataOut.full()*/) {
				DataIn.read(currWord);
				if (count_word_sent == number_of_words_to_send){
					sendWord.data = currWord.data;
					sendWord.keep 	= len2Keep(byte_offset);						// Get the keep of the last transaction of the first memory offset;
					sendWord.last 	= 1;
					command_i.saddr(31,16) 	= input_command.saddr(31,16);
					command_i.saddr(15,0) 		= 0;										// point to the beginning of the buffer
					command_i.bbt 				= input_command.bbt - bytes_first_command;	// Recompute the bytes to transfer in the second memory access
					CmdOut.write(command_i);										// Issue the second command
					data2mem_state = FWD_BREAKDOWN_1;
				}
				else
					sendWord = currWord;
				
				count_word_sent++;
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case FWD_BREAKDOWN_1 :
			if (!DataIn.empty() /*&& !DataOut.full()*/) {
				DataIn.read(currWord);
				rx_align_two_64bytes_words (currWord, prevWord, byte_offset, sendWord);

				if (currWord.last){
					if (currWord.keep.bit(64-byte_offset) && byte_offset!=0){ // TODO try to improve dependence
						data2mem_state = FWD_EXTRA;
					}
					else {
						data2mem_state = WAIT_CMD;
					}
				}
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case FWD_EXTRA :
//			if (!DataOut.full()){
				sendWord.data = prevWord.data;
				sendWord.keep = prevWord.keep;
				sendWord.last = 1;
				DataOut.write(sendWord);
				data2mem_state = WAIT_CMD;
//			}
			break;
	}

}


/**
 * This module reads a command and determines whether two memory access are needed or not.
 * In base of that issues one or two memory write command(s), and the data is aligned properly, the realignment
 * if is necessary is done in the second access.
 *
 * @param      DataIn   Packet payload if any
 * @param      CmdIn    Internal command to write data into the memory. It does not take into account buffer overflow
 * @param      CmdOut   Command to the data mover. It takes into account buffer overflow. Two write commands when buffer overflows
 * @param      DataOut  Data to memory. If the buffer overflows, the second part of the data has to be realigned
  */
void tx_Data_to_Memory(
					stream<axiWord>& 				DataIn,
					stream<mmCmd>&					CmdIn,
					stream<mmCmd>&					CmdOut,
#if (TCP_NODELAY)					
					stream<axiWord>&				data2app,
#endif
					stream<axiWord>&				DataOut)
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

	if (!CmdIn.empty() && !reading_data && !extra_word){
		CmdIn.read(input_command);

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

		CmdOut.write(first_command); 				// issue the first command
	}
	else if(!DataIn.empty() && reading_data && !second_write && !extra_word){
		DataIn.read(currWord);
#if (TCP_NODELAY)
			data2app.write(currWord);
#endif
		if (rxWrBreakDown){
			if (count_word_sent == number_of_words_to_send){
				currWord.keep 	= keep_first_trans;
				currWord.last 	= 1;
				second_command.saddr(31,16) 	= input_command.saddr(31,16);
				second_command.saddr(15,0) 		= 0;										// point to the beginning of the buffer
				second_command.bbt 				= input_command.bbt - first_command.bbt;	// Recompute the bytes to transfer in the second memory access
				CmdOut.write(second_command);										// Issue the second command
				second_write 	= true;
			}
			count_word_sent++;
			prevWord = currWord;
			DataOut.write(currWord);
		}
		else{												// There is not double memory access
			if (currWord.last){
				reading_data = false;
			}
			DataOut.write(currWord);
		}
	}
	else if(!DataIn.empty() && reading_data && second_write && !extra_word){
		DataIn.read(currWord);
#if (TCP_NODELAY)
			data2app.write(currWord);
#endif		
		rx_align_two_64bytes_words (currWord, prevWord, byte_offset, sendWord);

		if (currWord.last){
			if (currWord.keep.bit(64-byte_offset) && byte_offset!=0){ // TODO try to improve dependence
				extra_word = true;
			}
			reading_data = false;
			second_write = false;
		}
		DataOut.write(sendWord);
	}
	else if (extra_word) {
		extra_word = false;
		DataOut.write(prevWord);
	}

}
