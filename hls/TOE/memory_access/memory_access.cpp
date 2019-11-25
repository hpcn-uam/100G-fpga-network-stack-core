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

#include "memory_access.hpp"

using namespace hls;
using namespace std;


void app_ReadMemAccessBreakdown(
					stream<cmd_internal>& 		inputMemAccess, 
					stream<mmCmd>& 				outputMemAccess, 
					stream<memDoubleAccess>& 	memAccessBreakdown) {

#pragma HLS pipeline II=1
#pragma HLS INLINE off
	static bool txEngBreakdown = false;
	static cmd_internal txEngTempCmd;
	static ap_uint<16> 	txEngBreakTemp = 0;
	//static ap_uint<16> 	txPktCounter = 0;
	mmCmd 				tempCmd;
	memDoubleAccess 	double_access=memDoubleAccess(false,0);

	if (txEngBreakdown == false) {
		if (!inputMemAccess.empty()) {
			inputMemAccess.read(txEngTempCmd);
			tempCmd = mmCmd(txEngTempCmd.addr, txEngTempCmd.length);

			if (txEngTempCmd.next_addr.bit(WINDOW_BITS)) {	// Check for overflow
				txEngBreakTemp = BUFFER_SIZE - txEngTempCmd.addr;		// Compute remaining space in the buffer before overflow
				tempCmd = mmCmd(txEngTempCmd.addr, txEngBreakTemp);

				txEngBreakdown = true;
				double_access.double_access = true;
				double_access.offset 		= txEngBreakTemp(5,0);	// Offset of MSB byte valid in the last transaction of the first burst
			}
			outputMemAccess.write(tempCmd);
			memAccessBreakdown.write(double_access);
			//txPktCounter++;
			//cerr << dec << "MemCmd: " << cycleCounter << " - " << txPktCounter << " - " << hex << " - " << tempCmd.saddr << " - " << tempCmd.length << endl;
		}
	}
	else if (txEngBreakdown == true) {
		txEngTempCmd.addr.range(WINDOW_BITS-1, 0) = 0;	// Clear the least significant bits of the address
		outputMemAccess.write(mmCmd(txEngTempCmd.addr, txEngTempCmd.length - txEngBreakTemp));
		txEngBreakdown = false;
		//cerr << dec << "MemCmd: " << cycleCounter << " - " << hex << " - " << txEngTempCmd.saddr << " - " << txEngTempCmd.length - txEngBreakTemp << endl;
	}
}

void tx_ReadMemAccessBreakdown(
					stream<cmd_internal>& 		inputMemAccess, 
					stream<mmCmd>& 				outputMemAccess, 
					stream<memDoubleAccess>& 	memAccessBreakdown) {

#pragma HLS pipeline II=1
#pragma HLS INLINE off
	static bool txEngBreakdown = false;
	static cmd_internal txEngTempCmd;
	static ap_uint<16> 	txEngBreakTemp = 0;
	//static ap_uint<16> 	txPktCounter = 0;
	mmCmd 				tempCmd;
	memDoubleAccess 	double_access=memDoubleAccess(false,0);

	if (txEngBreakdown == false) {
		if (!inputMemAccess.empty()) {
			inputMemAccess.read(txEngTempCmd);
			tempCmd = mmCmd(txEngTempCmd.addr, txEngTempCmd.length);

			if (txEngTempCmd.next_addr.bit(WINDOW_BITS)) {	// Check for overflow
				txEngBreakTemp = BUFFER_SIZE - txEngTempCmd.addr;		// Compute remaining space in the buffer before overflow
				tempCmd = mmCmd(txEngTempCmd.addr, txEngBreakTemp);

				txEngBreakdown = true;
				double_access.double_access = true;
				double_access.offset 		= txEngBreakTemp(5,0);	// Offset of MSB byte valid in the last transaction of the first burst
			}
			outputMemAccess.write(tempCmd);
			memAccessBreakdown.write(double_access);
			//txPktCounter++;
			//cerr << dec << "MemCmd: " << cycleCounter << " - " << txPktCounter << " - " << hex << " - " << tempCmd.saddr << " - " << tempCmd.length << endl;
		}
	}
	else if (txEngBreakdown == true) {
		txEngTempCmd.addr.range(WINDOW_BITS-1, 0) = 0;	// Clear the least significant bits of the address
		outputMemAccess.write(mmCmd(txEngTempCmd.addr, txEngTempCmd.length - txEngBreakTemp));
		txEngBreakdown = false;
		//cerr << dec << "MemCmd: " << cycleCounter << " - " << hex << " - " << txEngTempCmd.saddr << " - " << txEngTempCmd.length - txEngBreakTemp << endl;
	}
}



/* This function is in charge of reading the data coming from the memory and sending to the application.
 * The application request blocks of L bytes, but, since the buffer is implemented  as a circular buffer, 
 * the overflow arises. That means that a requested block may come as two blocks (beats), once in a while.
 * Because the memory has to be read twice to fulfil the L bytes. In that cases the function will be notified
 * by @MemoryDoubleAccess that the requested data is composed of two input beats. 
 * The main problem is that the alignment between the end of the first beat with the start
 * of the second beat is different to the rest of alignment in the second beat. The following image shows an 
 * example.
 *   
 *   ------------------------------------------------------------------
 * 0 ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * 1 ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * .
 * .
 * . ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * . ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * M                                          ||||||||||||||||||||||||| 	last=1  wordM
 *   ------------------------------------------------------------------
 *  
 *  Second BEAT
 *  
 *   ------------------------------------------------------------------
 * 0 ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * 1 ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * .
 * .
 * . ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * . ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
 * N                   |||||||||||||||||||||||||||||||||||||||||||||||| 	last=1
 *   ------------------------------------------------------------------
 *
 * If you notice, the first transaction of second beat must be placed in the MSB of the output word.
 * Whereas the remaining bytes of the first transaction of the second beat must be placed in the LSB of following output word
 * To clarify the following image shows the output word after merge, lets denote the offset as 'k' expressed in bits, wordM the last
 * transaction of the first beat and wordS_0 the first transaction of the second beat.
 * 
 * 
 *   ------------------------------------------------------------------
 *   |           wordS_0(511-k,0)             |      wordM(k-1,0)     |	
 *   ------------------------------------------------------------------
 * Now the wordS_0 has (512-k)-bit to output, then, the following word must have the following pattern
 * 
 *   ------------------------------------------------------------------
 *   |           wordS_1(511-k,0)             | wordS_0(511-1,512-k)  |	
 *   ------------------------------------------------------------------
 *   
 * As you can see the multiplexation is different and must be treated carefully. 
 * 
 * The same concept is applied to tx_MemDataRead_aligner
 * 
 *  
 *
 */

void app_MemDataRead_aligner(
					stream<axiWord>& 			DtaInNoAlig,
					stream<memDoubleAccess>& 	MemoryDoubleAccess,
					stream<axiWord>& 			DataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1	
	
	enum amdra_fsm_states {READ_ACCESS , NO_BREAKDOWN , BREAKDOWN_BLOCK_0, BREAKDOWN_ALIGNED, 
						   FIRST_MERGE, BREAKDOWN_BLOCK_1, EXTRA_DATA};
	static amdra_fsm_states amdra_state = READ_ACCESS;

	static ap_uint<6>	offset_block0;
	static ap_uint<6>	offset_block1;

	static axiWord 		prevWord;
	
	memDoubleAccess 	mem_double_access;
	axiWord 			currWord;
	axiWord 			sendWord;
	bool 				write_word = true;

	switch (amdra_state){
		case READ_ACCESS:
			if (!MemoryDoubleAccess.empty()){
				MemoryDoubleAccess.read(mem_double_access);
				offset_block0	= mem_double_access.offset;
				offset_block1	= 64 - mem_double_access.offset;

				if (mem_double_access.double_access){
					amdra_state = BREAKDOWN_BLOCK_0;
				}
				else {
					amdra_state = NO_BREAKDOWN;
				}
			}
			break;
		case NO_BREAKDOWN:
		case BREAKDOWN_ALIGNED:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				DataOut.write(currWord);

				if (currWord.last){
					amdra_state = READ_ACCESS;
				}
			}
			break;
		case BREAKDOWN_BLOCK_0:
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);

				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.last = 0;
				if (currWord.last){
					//cout << "BYTE OFFSET0 " << dec << offset_block0 << "BYTE OFFSET1 " << offset_block1 << endl << endl;
					if (offset_block0 == 0){				// The current word has to be written because every bit is useful
						amdra_state = BREAKDOWN_ALIGNED;
					}
					else {									// This mean that the last not has all byte valid
						write_word = false;
						amdra_state = FIRST_MERGE;
					}
				} 

				//cout << "BREAKDOWN_BLOCK_0 current data: " << setw(130) << hex << currWord.data << "\tkeep: " << setw(18) << currWord.keep; 
				//cout << "\tlast: " << currWord.last << endl;

				if (write_word){
					DataOut.write(sendWord);
					//cout << "BREAKDOWN_BLOCK_0 sent    data: " << setw(130) << hex << sendWord.data << "\tkeep: " << setw(18) << sendWord.keep; 
					//cout << "\tlast: " << sendWord.last << endl << endl;
				}
				prevWord = currWord;
			}
			break;
		case FIRST_MERGE :	// Always the input word is unaligned otherwise there is a bug
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				//AlingWordFromMemoryStageOne(currWord,prevWord,offset_block0,sendWord);
				align_words_from_memory(currWord,prevWord,offset_block0,sendWord);
				sendWord.last = 0;
				
				if (currWord.last){
					if (currWord.keep.bit(offset_block1)){
						amdra_state = EXTRA_DATA;
					}
					else{
						sendWord.last = 1;
						amdra_state = READ_ACCESS;
					}
				}
				else {
					amdra_state = BREAKDOWN_BLOCK_1;
				}
				//cout << "FIRST_MERGE      current data: " << setw(130) << hex << currWord.data << "\tkeep: " << setw(18) << currWord.keep; 
				//cout << "\tlast: " << currWord.last << endl;
				//cout << "FIRST_MERGE      sent    data: " << setw(130) << hex << sendWord.data << "\tkeep: " << setw(18) << sendWord.keep; 
				//cout << "\tlast: " << sendWord.last << endl<< endl;
				prevWord = currWord;
				DataOut.write(sendWord);
			}	
		break;
		case BREAKDOWN_BLOCK_1: // Always the input word is unaligned otherwise there is a bug
			if (!DtaInNoAlig.empty()){
				DtaInNoAlig.read(currWord);
				align_words_to_memory(currWord,prevWord,offset_block1,sendWord);

				sendWord.last = 0;
				if (currWord.last){
					if (currWord.keep.bit(offset_block1)){
						amdra_state = EXTRA_DATA;
					}
					else {
						sendWord.last = 1;
						amdra_state = READ_ACCESS;
					}
				}
				//cout << "BREAKDOWN_BLOCK_1 current data: " << setw(130) << hex << currWord.data << "\tkeep: " << setw(18) << currWord.keep; 
				//cout << "\tlast: " << currWord.last << endl;
				//cout << "BREAKDOWN_BLOCK_1 send    data: " << setw(130) << hex << sendWord.data << "\tkeep: " << setw(18) << sendWord.keep; 
				//cout << "\tlast: " << sendWord.last << endl << endl;
				
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case EXTRA_DATA:
			align_words_to_memory(axiWord(0,0,1),prevWord,offset_block1,sendWord);
			sendWord.last = 1;
			DataOut.write(sendWord);
			//cout << "EXTRA_DATA               data: " << setw(130) << hex << sendWord.data << "\tkeep: " << setw(18) << sendWord.keep; 
			//cout << "\tlast: " << sendWord.last << endl;
			amdra_state = READ_ACCESS;
			break;
	}

}

/** @ingroup tx_engine
 *  See app_MemDataRead_aligner description
 */

void tx_MemDataRead_aligner(
					stream<axiWord>& 			DtaInNoAlig,
					stream<memDoubleAccess>& 	MemoryDoubleAccess,
#if (TCP_NODELAY)
					stream<bool>&				txEng_isDDRbypass,
					stream<axiWord>&			txAppDataIn,
#endif					
					stream<axiWord>& 			DataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1
	
	enum tmra_states {READ_ACCESS , NO_BREAKDOWN , BREAKDOWN_BLOCK_0, BREAKDOWN_ALIGNED, 
		              FIRST_MERGE, BREAKDOWN_BLOCK_1, EXTRA_DATA
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

	static ap_uint<6>	offset_block0;
	static ap_uint<6>	offset_block1;

	static axiWord 		prevWord;
	
	memDoubleAccess 	mem_double_access;
	axiWord 			currWord;
	axiWord 			sendWord;

	bool 				bypass_ddr_i;
	bool 				write_word = true;

	switch (tmra_fsm_state){
#if (TCP_NODELAY)	
		case READ_BYPASS: 
			if (!txEng_isDDRbypass.empty()){
				txEng_isDDRbypass.read(bypass_ddr_i);
				if (bypass_ddr_i){
					tmra_fsm_state = NO_USE_DDR;
				}
				else {
					tmra_fsm_state = READ_ACCESS;
				}
			}
			break;
		case NO_USE_DDR			:
			if (!txAppDataIn.empty() && !DataOut.full()){
				txAppDataIn.read(currWord);
				DataOut.write(currWord);
				if (currWord.last){
					tmra_fsm_state = READ_BYPASS;
				}
			}
			break;
#endif			
		case READ_ACCESS:
			if (!MemoryDoubleAccess.empty()){
				MemoryDoubleAccess.read(mem_double_access);
				offset_block0	= mem_double_access.offset;
				offset_block1	= 64 - mem_double_access.offset;
				
				if (mem_double_access.double_access){
					tmra_fsm_state = BREAKDOWN_BLOCK_0;
				}
				else {
					tmra_fsm_state = NO_BREAKDOWN;
				}
			}
			break;
		case NO_BREAKDOWN:
		case BREAKDOWN_ALIGNED:
			if (!DtaInNoAlig.empty() && !DataOut.full()){
				DtaInNoAlig.read(currWord);
				DataOut.write(currWord);

				if (currWord.last){
					tmra_fsm_state = INITIAL_STATE;
				}
			}
			break;
		case BREAKDOWN_BLOCK_0:
			if (!DtaInNoAlig.empty() && !DataOut.full()){
				DtaInNoAlig.read(currWord);

				sendWord.data = currWord.data;
				sendWord.keep = currWord.keep;
				sendWord.last = 0;
				if (currWord.last){
					if (offset_block0 == 0){				// The current word has to be written because every bit is useful
						tmra_fsm_state = BREAKDOWN_ALIGNED;
					}
					else {									// This mean that the last not has all byte valid
						write_word = false;
						tmra_fsm_state = FIRST_MERGE;
					}
				} 
				if (write_word){
					DataOut.write(sendWord);
				}

				prevWord = currWord;
			}
			break;
		case FIRST_MERGE :	// Always the input word is unaligned otherwise there is a bug
			if (!DtaInNoAlig.empty() && !DataOut.full()){
				DtaInNoAlig.read(currWord);
				align_words_from_memory(currWord,prevWord,offset_block0,sendWord);
				sendWord.last = 0;

				if (currWord.last){
					if (currWord.keep.bit(offset_block1)){
						tmra_fsm_state = EXTRA_DATA;
					}
					else{
						sendWord.last = 1;
						tmra_fsm_state = INITIAL_STATE;
					}
				}
				else {
					tmra_fsm_state = BREAKDOWN_BLOCK_1;
				}
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case BREAKDOWN_BLOCK_1: // Always the input word is unaligned otherwise there is a bug
			if (!DtaInNoAlig.empty() && !DataOut.full()){
				DtaInNoAlig.read(currWord);
				align_words_to_memory(currWord,prevWord,offset_block1,sendWord);

				sendWord.last = 0;
				if (currWord.last){
					if (currWord.keep.bit(offset_block1)){
						tmra_fsm_state = EXTRA_DATA;
					}
					else{
						sendWord.last = 1;
						tmra_fsm_state = INITIAL_STATE;
					}
				}

				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case EXTRA_DATA:
			if (!DataOut.full()) {
				align_words_to_memory(axiWord(0,0,1),prevWord,offset_block1,sendWord);
				sendWord.last = 1;

				DataOut.write(sendWord);
				tmra_fsm_state = INITIAL_STATE;
			}
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

	bool 					rxWrBreakDown;
	ap_uint<WINDOW_BITS+1> 	buffer_overflow;

	axiWord 			currWord;
	axiWord 			sendWord;
	static axiWord 			prevWord;

	switch (data2mem_state) {
		case WAIT_CMD :
			if (!CmdIn.empty()){

				CmdIn.read(input_command);

				buffer_overflow 	= input_command.saddr(WINDOW_BITS-1, 0) + input_command.bbt; 	// Compute the address of the last byte to write
				
				if (buffer_overflow.bit(WINDOW_BITS)){											// The remaining buffer space is not enough. An address overflow has to be done
					command_i.bbt 		= BUFFER_SIZE - input_command.saddr(WINDOW_BITS-1,0);			// Compute how much bytes are needed in the first transaction
					command_i.saddr 	= input_command.saddr;
					byte_offset 		= command_i.bbt.range(5,0);	// Determines the position of the MSB in the last word
					bytes_first_command = command_i.bbt;

					if (byte_offset != 0){ 								// Determines how many transaction are in the first memory access
						number_of_words_to_send = command_i.bbt.range(WINDOW_BITS-1,6) + 1;
					}
					else {
						number_of_words_to_send = command_i.bbt.range(WINDOW_BITS-1,6);
					}
					count_word_sent 	= 1;
					rxWrBreakDown 		= true;
					//cout << "BREAKDOWN byte_offset: " << dec << byte_offset;
					//cout << "\tcommand address " << hex << command_i.saddr << "\tbtt " << dec << command_i.bbt << endl; 
					data2mem_state = FWD_BREAKDOWN_0;
				}
				else {
					rxWrBreakDown 		= false;
					command_i 			= input_command;
					data2mem_state 	= FWD_NO_BREAKDOWN;
				}
				
				doubleAccess.write(rxWrBreakDown);					// Notify if there are two memory access
				CmdOut.write(command_i); 					// issue the first command

			}
			break;
		case FWD_NO_BREAKDOWN :
			if (!DataIn.empty()){
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
			if (!DataIn.empty()) {
				DataIn.read(currWord);
				if (count_word_sent == number_of_words_to_send){
					sendWord.data   = currWord.data;
					sendWord.keep 	= len2Keep(byte_offset);						// Get the keep of the last transaction of the first memory offset;
					sendWord.last 	= 1;
					command_i.saddr(31,WINDOW_BITS) 	= input_command.saddr(31,WINDOW_BITS);
					command_i.saddr(WINDOW_BITS-1,0) 		= 0;										// point to the beginning of the buffer
					command_i.bbt 				= input_command.bbt - bytes_first_command;	// Recompute the bytes to transfer in the second memory access
					CmdOut.write(command_i);										// Issue the second command
					
					if (currWord.last){					// The second part of the memory write has less than 64 bytes
						//cout << "CORNER CASE!!!!\tbtt: " << dec << command_i.bbt << endl;
						data2mem_state = FWD_EXTRA;
					}
					else {
						data2mem_state = FWD_BREAKDOWN_1;
					}
				}
				else
					sendWord = currWord;
				
				count_word_sent++;
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case FWD_BREAKDOWN_1 :
			if (!DataIn.empty()) {
				DataIn.read(currWord);
				align_words_to_memory (currWord, prevWord, byte_offset, sendWord);

				sendWord.last = 0;
				if (currWord.last){
					if (currWord.keep.bit(byte_offset) && byte_offset!=0){
						data2mem_state = FWD_EXTRA;
					}
					else {
						sendWord.last = 1;
						data2mem_state = WAIT_CMD;
					}
				}
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case FWD_EXTRA :
			align_words_to_memory (axiWord(0,0,0), prevWord, byte_offset, sendWord);
			sendWord.last = 1;
			DataOut.write(sendWord);
			data2mem_state = WAIT_CMD;
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
					stream<axiWord>&				DataOut)

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
	static ap_uint<64> 		keep_last_word;

	bool 					rxWrBreakDown;
	ap_uint<WINDOW_BITS+1> 	buffer_overflow;

	axiWord 				currWord;
	axiWord 				sendWord;
	static axiWord 			prevWord;
	static int				transaction = 0; // deleteme

	switch (data2mem_state) {
		case WAIT_CMD :
			if (!CmdIn.empty()){

				CmdIn.read(input_command);

				buffer_overflow 	= input_command.saddr(WINDOW_BITS-1, 0) + input_command.bbt; // Compute the address of the last byte to write
				
				if (buffer_overflow.bit(WINDOW_BITS)){	// The remaining buffer space is not enough. An address overflow has to be done
					command_i.bbt 		= BUFFER_SIZE - input_command.saddr(WINDOW_BITS-1,0);	// Compute how much bytes are needed in the first transaction
					//cout << "COMMAND BREAKDOWN input_command.bbt " << dec << input_command.bbt << "\tcommand_i.bbt " << command_i.bbt << endl;
					command_i.saddr 	= input_command.saddr;
					byte_offset 		= command_i.bbt.range(5,0);	// Determines the position of the MSB in the last word
					bytes_first_command = command_i.bbt;
					keep_last_word 	    = len2Keep(byte_offset);								// Get the keep of the last transaction of the first memory offset;

					if (byte_offset != 0){ 								// Determines how many transaction are in the first memory access
						number_of_words_to_send = command_i.bbt.range(WINDOW_BITS-1,6) + 1;
					}
					else {
						number_of_words_to_send = command_i.bbt.range(WINDOW_BITS-1,6);
					}
					count_word_sent 	= 1;
					rxWrBreakDown 		= true;
					data2mem_state = FWD_BREAKDOWN_0;
					transaction = 0; // deleteme
				}
				else {
					rxWrBreakDown 		= false;
					command_i 			= input_command;
					data2mem_state 	= FWD_NO_BREAKDOWN;
				}
				
				CmdOut.write(command_i); 					// issue the first command

			}
			break;
		case FWD_NO_BREAKDOWN :
			if (!DataIn.empty()){
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
			if (!DataIn.empty()){
				DataIn.read(currWord);
				sendWord.last 	= 0;
				if (count_word_sent == number_of_words_to_send){
					sendWord.data = currWord.data;
					sendWord.keep 	= keep_last_word;
					sendWord.last 	= 1;
					command_i.saddr(31,WINDOW_BITS) 	= input_command.saddr(31,WINDOW_BITS);
					command_i.saddr(WINDOW_BITS-1,0) 		= 0;										// point to the beginning of the buffer
					command_i.bbt 				= input_command.bbt - bytes_first_command;	// Recompute the bytes to transfer in the second memory access
					CmdOut.write(command_i);												// Issue the second command
					if (currWord.last) {													// The second part of the memory write has less than 64 bytes
						data2mem_state = FWD_EXTRA;
					} 
					else {
						data2mem_state = FWD_BREAKDOWN_1;
					}
				}
				else
					sendWord = currWord;

				//cout << "TX APP to mem FWD_BREAKDOWN_0[ " << dec << setw(2)  << transaction++ <<  " ] DATA: " << setw(130) << hex <<  sendWord.data;
				//cout << "\tkeep " << setw(30) << sendWord.keep  << "\tlast " << sendWord.last << endl;

				count_word_sent++;
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case FWD_BREAKDOWN_1 :
			if (!DataIn.empty()){
				DataIn.read(currWord);
				align_words_to_memory (currWord, prevWord, byte_offset, sendWord);

				sendWord.last = 0;
				if (currWord.last){
					if (currWord.keep.bit(byte_offset) && byte_offset!=0){
						data2mem_state = FWD_EXTRA;
						//cout << "JUMPING TO EXTRA " << endl;
					}
					else {
						data2mem_state = WAIT_CMD;
						sendWord.last = 1;
					}
				}

				//cout << "TX APP to mem FWD_BREAKDOWN_1[ " << dec << setw(2)  << transaction++ <<  " ] DATA: " << setw(130) << hex <<  sendWord.data;
				//cout << "\tkeep " << setw(30) << sendWord.keep  << "\tlast " << sendWord.last << endl;
				prevWord = currWord;
				DataOut.write(sendWord);
			}
			break;
		case FWD_EXTRA :
			align_words_to_memory (axiWord(0,0,0), prevWord, byte_offset, sendWord);
			sendWord.last = 1;
			//cout << "TX APP to mem FWD_EXTRA[ " << dec << setw(2)  << transaction++ <<  " ] DATA: " << setw(130) << hex <<  sendWord.data;
			//cout << "\tkeep " << setw(30) << sendWord.keep  << "\tlast " << sendWord.last << endl;
			DataOut.write(sendWord);
			data2mem_state = WAIT_CMD;
			break;
	}

}
