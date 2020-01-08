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
#include "../toe.hpp"
#include "dummy_memory.hpp"
#include "../session_lookup_controller/session_lookup_controller.hpp"
#include <map>
#include <string>
#include "pcap2stream.hpp"
#include "../common_utilities/common_utilities.hpp"
#include "../../echo_replay/echo_server_application.hpp"
#include "../../iperf2_tcp/iperf_client.hpp"
#include <iomanip>

#define ECHO_REPLAY 0

#define totalSimCycles 50000

using namespace std;

unsigned int	simCycleCounter		= 0;


void compute_pseudo_tcp_checksum(	
									stream<axiWord>&			dataIn,
									stream<ap_uint<16> >&		pseudo_tcp_checksum,
									ap_uint<1>					source)
{

	static ap_uint<1> 	compute_checksum[2] = {0 , 0};
	static ap_uint<16> 	word_sum[32][2]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	ap_uint<17> 		ip_sums_L1[16];
	ap_uint<18> 		ip_sums_L2[8];
	ap_uint<19> 		ip_sums_L3[4];
	ap_uint<20> 		ip_sums_L4[2];
	ap_uint<21> 		ip_sums_L5;
	ap_uint<16> 		tmp;
	ap_uint<17> 		tmp1;
	ap_uint<17> 		tmp2;
	ap_uint<17> 		final_sum_r; 							// real add
	ap_uint<17> 		final_sum_o; 							// overflowed add
	ap_uint<16> 		res_checksum;
	axiWord 			currWord;

	if (!dataIn.empty() && !compute_checksum[source]){
		dataIn.read(currWord);

		first_level_sum : for (int i=0 ; i < 32 ; i++ ){
			if (currWord.keep.bit((i*2)+1))
				tmp(7,0) 	= currWord.data((((i*2)+1)*8)+7,((i*2)+1)*8);
			else
				tmp(7,0) 	= 0;	

			if (currWord.keep.bit(i*2))
				tmp(15,8) 	= currWord.data(((i*2)*8)+7,(i*2)*8);
			else 
				tmp(15,8) 	= 0;

			tmp1 		= word_sum[i][source] + tmp;
			tmp2 		= word_sum[i][source] + tmp + 1;
			if (tmp1.bit(16)) 				// one's complement adder
				word_sum[i][source] = tmp2(15,0);
			else
				word_sum[i][source] = tmp1(15,0);
		}

		if(currWord.last){
			compute_checksum[source] = 1;
		}
	}
	else if(compute_checksum[source]) {
		//adder tree
		second_level_sum : for (int i = 0; i < 16; i++) {
			ip_sums_L1[i]= word_sum[i*2][source] + word_sum[i*2+1][source];
			word_sum[i*2][source]   = 0; // clear adder variable
			word_sum[i*2+1][source] = 0;
		}
		//adder tree L2
		third_level_sum : for (int i = 0; i < 8; i++) {
			ip_sums_L2[i] = ip_sums_L1[i*2+1] + ip_sums_L1[i*2];
		}
		//adder tree L3
		fourth_level_sum : for (int i = 0; i < 4; i++) {
			ip_sums_L3[i] = ip_sums_L2[i*2+1] + ip_sums_L2[i*2];
		}

		ip_sums_L4[0] = ip_sums_L3[1] + ip_sums_L3[0];
		ip_sums_L4[1] = ip_sums_L3[3] + ip_sums_L3[2];
		ip_sums_L5 = ip_sums_L4[1] + ip_sums_L4[0];

		final_sum_r = ip_sums_L5.range(15,0) + ip_sums_L5.range(20,16);
		final_sum_o = ip_sums_L5.range(15,0) + ip_sums_L5.range(20,16) + 1;

		if (final_sum_r.bit(16))
			res_checksum = ~(final_sum_o.range(15,0));
		else
			res_checksum = ~(final_sum_r.range(15,0));

		compute_checksum[source] = 0;
		pseudo_tcp_checksum.write(res_checksum);
	}
}


void rxApp_sim (
	stream<ipTuple>& 			openConnection,
	stream<openStatus>& 		openConStatus,
	stream<ap_uint<16> >& 		closeConnection,
	stream<ap_uint<16> >& 		data_session_ID, 
	stream<axiWord>& 			memory_to_rx_app_data,
	stream<appNotification>& 	notifications,
	stream<appReadRequest>& 	rxapp_2_mem_read_request,
	ap_uint<14>					connection_to_establish,
	ap_uint<32>					remote_server_address_0,
	ap_uint<32>					remote_server_address_1,
	ap_uint<32>					remote_server_address_2,
	ap_uint<32>					remote_server_address_3,
	bool 						test_RX
	){

	enum rxApp_states {INIT_CON, WAIT_CON, READ_DATA,IDLE};
	enum rxApp_read_data {WAIT_DATA, CONSUME_DATA};
	static rxApp_states rx_app_fsm = INIT_CON;
	static rxApp_read_data rx_read_data_fsm = WAIT_DATA;

	static ap_uint<14> 	concurrent_sessions = 0;
	static ap_uint<16> 	current_sessionID;
	ipTuple 			openTuple;
	openStatus 			connection_status;
	appNotification 	rx_app_notification;
	axiWord 			currWord;
	static int 			packet=0;
	static int 			transaction=0;

	if (test_RX){			// Only do things if client is enable
		switch (rx_app_fsm) {
			case INIT_CON:
				if (concurrent_sessions < connection_to_establish){
					
					switch (concurrent_sessions(1,0)){
						case 0:
							openTuple.ip_address    = remote_server_address_0;
							break;
						case 1:
							openTuple.ip_address    = remote_server_address_1;
							break;
						case 2:
							openTuple.ip_address    = remote_server_address_2;
							break;
						case 3:
							openTuple.ip_address    = remote_server_address_3;
							break;
					}

					openTuple.ip_port 		= 5001;
					openConnection.write(openTuple);

					//cout << "RX app request to establish a new connection socket " << dec << ((openTuple.ip_address) >> 24 & 0xFF) <<".";
					//cout << ((openTuple.ip_address >> 16) & 0xFF) << "." << ((openTuple.ip_address >> 8) & 0xFF) << ".";
					//cout << (openTuple.ip_address & 0xFF) << ":" << openTuple.ip_port;
					//cout << endl;
					concurrent_sessions++;

				}
				if (concurrent_sessions == connection_to_establish) {
					concurrent_sessions = 0;
					rx_app_fsm = WAIT_CON;
				}
				break;
			case WAIT_CON:
				if (concurrent_sessions < connection_to_establish){
					if (!openConStatus.empty()){
						openConStatus.read(connection_status);
						if (connection_status.success){
							//cout << "Connection [" << dec << connection_status.sessionID << "] successfully opened." << endl;
						}
						else {
							//cout << "Connection [" << dec << connection_status.sessionID << "] could not be opened." << endl;
						}
						concurrent_sessions++;
						if (concurrent_sessions == connection_to_establish) {
							rx_app_fsm = READ_DATA;
						}
					}
				}
				break;
			case READ_DATA: 

				if (!notifications.empty()){
					notifications.read(rx_app_notification);		// TOE notifies that there are data available in the RX buffer
					if (rx_app_notification.length != 0){
						rxapp_2_mem_read_request.write(appReadRequest(rx_app_notification.sessionID, rx_app_notification.length));	
					}
				}

				break;
		}

		switch(rx_read_data_fsm){
			case WAIT_DATA:
				if (!data_session_ID.empty()){
					data_session_ID.read(current_sessionID);
					//cout << "RX Data from memory to app ID[" << dec << current_sessionID <<"]" << endl;
					rx_read_data_fsm = CONSUME_DATA;
				}
				break;
			case CONSUME_DATA:
				if(!memory_to_rx_app_data.empty()){
					memory_to_rx_app_data.read(currWord);
					//cout << "RX APP [" << dec << packet << "][" << dec << transaction++ << "]";
					//cout << "\tData " << hex << currWord.data << "\tkeep " << currWord.keep << "\tlast " << currWord.last << endl;
					if (currWord.last){
						packet ++;
						transaction = 0;
						rx_read_data_fsm = WAIT_DATA;
					}
				}
				break;				
		}
	}
	else {
		if (!notifications.empty()){
			notifications.read(rx_app_notification);
			//cout << "RX notification sessionID: " << dec << rx_app_notification.sessionID;
			//cout << "\tlength: " << rx_app_notification.length << "\t" << "\tipAddress: " << hex <<  rx_app_notification.ipAddress;
			//cout << "\tdst port: " << dec << rx_app_notification.dstPort << "\tclosed: " << rx_app_notification.closed << endl;
		}			
	}
}

void txApp_sim(
	stream<ap_uint<16> >& 			rxApp2portTable_listen_req,		// notify which ports are open to listen
	stream<txApp_client_status>& 	rxEng2txApp_client_notification,
	stream<listenPortStatus>& 		rxApp2portTable_listen_rsp,		// TOE response to listen to new port
	stream<appTxMeta>& 				txApp_write_request,			// ask for write
	stream<appTxRsp>&				txApp_data_write_response,		// It tells if the data was accepted by TOE
	stream<axiWord>& 				txApp_write_Data,				// data to write
	ap_uint<32>						bytes_to_send,					// amount of bytes to send
	bool 							test_TX
	){

	enum tx_data_states {WAIT_CLIENT, SEND_COMMAND, WAIT_RESPONSE, SEND_DATA, IDLE, WAIT_CYCLES};
	static listenPortStatus 		listen_rsp;
	static bool				 		listenDone = false;
	static ap_uint<1> 	listenFsm 	= 0;
	static tx_data_states fsm_state = WAIT_CLIENT;
	
	static txApp_client_status	rx_client_notification;
	static int 					bytes2send = (int)bytes_to_send;
	static ap_uint<32> 			bytes_sent = 0;
	static int 					segment_num = 0;
	static ap_uint<32> 			tmp=0;
	static int					cycle_counter = 0;
	static int 					data_transfer_num = 0;
	static int					write_request_length_r;
	
	appTxMeta 					write_request;
	axiWord 					sendWord=axiWord(0,0,0);
	appTxRsp 					write_request_response;
	int 						transfer;

	if (test_TX){
		if (!listenDone) { 				// Open Port 15000
			switch (listenFsm) {
			case 0:
				rxApp2portTable_listen_req.write(15000);
				listenFsm++;
				break;
			case 1:
				if (!rxApp2portTable_listen_rsp.empty()) {
					rxApp2portTable_listen_rsp.read(listen_rsp);
					listenDone = listen_rsp.open_successfully;
					cout << "Port 15000 opened " << dec << listenDone << endl;
					listenFsm++;
				}
				break;
			}
		}

		switch (fsm_state){
			case WAIT_CLIENT:
				// WAIT for a client
				if (!rxEng2txApp_client_notification.empty()){
					rxEng2txApp_client_notification.read(rx_client_notification);
					cout << "Client session ID: " << dec << rx_client_notification.sessionID;
					cout << "\t\tBuffer size: " << (rx_client_notification.buffersize+1)*65536;
					cout << "\t\t max transfer size: " << rx_client_notification.max_transfer_size;
					cout << "\t\tTCP_NODELAY: " << (rx_client_notification.tcp_nodelay ? "Yes" : "No");
					cout << "\t\tBuffer empty: " << (rx_client_notification.buffer_empty ? "Yes" : "No") << endl;
					segment_num = 0;
					fsm_state = SEND_COMMAND;

				}
				break;
			case SEND_COMMAND:
				write_request.sessionID = rx_client_notification.sessionID;
	
				fsm_state = WAIT_RESPONSE;
				if (rx_client_notification.tcp_nodelay){
					if (bytes2send > rx_client_notification.max_transfer_size){
						write_request.length 	= rx_client_notification.max_transfer_size;
						bytes_sent				= rx_client_notification.max_transfer_size;
					}
					else if (bytes2send > 0){
						write_request.length 	= bytes2send;
						bytes_sent				= bytes2send;
					}
					else {
						fsm_state = IDLE;
					}
				}
				else {
					write_request.length 	= bytes2send;
					bytes_sent				= bytes2send;
				}

				write_request_length_r = write_request.length ;

				if (fsm_state == WAIT_RESPONSE) {
					txApp_write_request.write(write_request);
					cout << "A TX data transfer nº " << dec << setw(3) << data_transfer_num <<" has  been programmed ID: " << dec << rx_client_notification.sessionID;
					cout << "\tlength: " << write_request.length  << "\tSegment [" << setw(4) << segment_num << "," << setw(4) << segment_num + write_request.length;
					cout << "]\ttime: " << simCycleCounter << endl;
				}

				break;
			case WAIT_RESPONSE:	
				if (!txApp_data_write_response.empty()){
					txApp_data_write_response.read(write_request_response);
					cout << "Response for transfer " << dec << setw(3) << data_transfer_num << " length : " << dec << write_request_response.length;
					cout << "\tremaining space: " << write_request_response.remaining_space << "\terror: " <<  write_request_response.error;
					cout << "\ttime: "  << simCycleCounter << endl;

					if (write_request_response.error != 0){
						cycle_counter 	= 0;
						fsm_state 		= WAIT_CYCLES;
					}
					else{
						bytes2send 				-= rx_client_notification.max_transfer_size;
						fsm_state = SEND_DATA;
						data_transfer_num++;
						segment_num += write_request_length_r;
					}
				}
				break;
			case SEND_DATA:
				for (int i=0 ; i < 64 ; i++){
					if (i < bytes_sent){
						sendWord.data((i*8)+7,i*8) = tmp(((i%2)*8)+7,(i%2)*8);
						sendWord.keep.bit(i)=1;
						transfer = i;
						if ((i%2) !=0 )
							tmp++;
					}
					else {
						sendWord.data((i*8)+7,i*8) = 0;	
						sendWord.keep.bit(i)=0;
						sendWord.last = 1;
					}
				}
				bytes_sent -= 64;
				if (sendWord.last){
					if (rx_client_notification.tcp_nodelay){
						fsm_state = SEND_COMMAND;
					}
					else {
						fsm_state = IDLE;
					}
				}

				txApp_write_Data.write(sendWord);

				break;
			case IDLE:
				fsm_state = WAIT_CLIENT;
				break;
			case WAIT_CYCLES:
				cycle_counter++;
				if (cycle_counter==100){
					cycle_counter = 0;
					fsm_state = SEND_COMMAND;
				}
			break;	
		}
	}
}

void sessionLookupStub(
		stream<rtlSessionLookupRequest>& 	lup_req, 
		stream<rtlSessionLookupReply>& 		lup_rsp,
		stream<rtlSessionUpdateRequest>& 	upd_req, 
		stream<rtlSessionUpdateReply>& 		upd_rsp) {
						//stream<ap_uint<14> >& new_id, stream<ap_uint<14> >& fin_id)
	static map<threeTuple, ap_uint<14> > lookupTable;

	rtlSessionLookupRequest request;
	rtlSessionUpdateRequest update;

	map<threeTuple, ap_uint<14> >::const_iterator findPos;

	if (!lup_req.empty()) {
		lup_req.read(request);
		//cout << "TABLE lookup req " << (!request.source ? "RX" : "TX_APP") << "\t";
		//cout << hex  << byteSwap32(request.key.theirIp) << ":" << dec << byteSwap16(request.key.theirPort) << ":" << byteSwap16(request.key.myPort);
		findPos = lookupTable.find(request.key);
		if (findPos != lookupTable.end()){ //hit
			lup_rsp.write(rtlSessionLookupReply(true, findPos->second, request.source));
			//cout << "\tHIT! ID: " << dec <<findPos->second; 
		}
		else {
			lup_rsp.write(rtlSessionLookupReply(false, request.source));
			//cout << "\tNO HIT! "; 
		}

		//cout << "\ttime: " << simCycleCounter << endl;
	}

	if (!upd_req.empty()) {	//TODO what if element does not exist
		upd_req.read(update);
		//cout << "TABLE update " << (!update.source ? "RX" : "TX_APP") << " " <<(!update.op ? "INSERT" : "DELETE") << " ID: " << dec << update.value;
		//cout << "\t" << hex  << byteSwap32(update.key.theirIp) << ":"<< dec << byteSwap16(update.key.theirPort) << ":" << byteSwap16(update.key.myPort);
		if (update.op == INSERT) {	//Is there a check if it already exists?
			// Read free id
			//new_id.read(update.value);
			lookupTable[update.key] = update.value;
			upd_rsp.write(rtlSessionUpdateReply(update.value, INSERT, update.source));

		}
		else {	// DELETE
			//fin_id.write(update.value);
			lookupTable.erase(update.key);
			upd_rsp.write(rtlSessionUpdateReply(update.value, DELETE, update.source));
		}
		//cout << "\ttime: " << simCycleCounter << endl;
	}
}

// Use Dummy Memory
void simulateRx(
				dummyMemory* 		memory, 
				stream<mmCmd>& 		WriteCmdFifo,  
				stream<mmStatus>& 	WriteStatusFifo, 
				stream<mmCmd>& 		ReadCmdFifo,
				stream<axiWord>& 	BufferIn, 
				stream<axiWord>& 	BufferOut) {

	mmCmd cmd;
	mmStatus status;
	axiWord inWord = axiWord(0, 0, 0);
	axiWord outWord = axiWord(0, 0, 0);
	static uint32_t rxMemCounter = 0;
	static uint32_t rxMemCounterRd = 0;

	static bool stx_write = false;
	static bool stx_read = false;

	static bool stx_readCmd = false;
	static ap_uint<16> wrBufferWriteCounter = 0;
	static ap_uint<16> wrBufferReadCounter = 0;
	static int write_command_number = 0;
	static int read_command_number = 0;
	static int write_packet_number = 0;
	static int write_transaction_number = 0;
	static int read_packet_number = 0;
	static int read_transaction_number = 0;

	ap_uint<WINDOW_BITS+1> address_comparator;	

	if (!WriteCmdFifo.empty() && !stx_write) {
		WriteCmdFifo.read(cmd);
		memory->setWriteCmd(cmd);
		wrBufferWriteCounter = cmd.bbt;
		stx_write = true;
		//cout << "Rx WRITE command [" << setw(3) << write_command_number++<< "] address: " << hex << cmd.saddr << "\tlength: " << dec << cmd.bbt << "\ttime: " << simCycleCounter << endl;
		address_comparator = cmd.saddr + cmd.bbt;
		if (address_comparator > BUFFER_SIZE){
			cout << endl << endl << "Rx WRITE ERROR memory write overflow!!!!!!! Trying to read from " << hex << address_comparator << endl << endl ;
		}
	}
	else if (!BufferIn.empty() && stx_write) {
		BufferIn.read(inWord);

		//cout << "Rx WRITE [" << dec << setw(3) << write_packet_number << "][" << setw(2) << write_transaction_number++ << "]";
		//cout <<"\tdata: " << setw(130) << hex << inWord.data << "\tkeep: " << setw(18) << inWord.keep; 
		//cout << "\tlast: " << inWord.last << "\ttime: " << dec << simCycleCounter << endl;

		memory->writeWord(inWord);
		if (wrBufferWriteCounter < (ETH_INTERFACE_WIDTH/8) + 1) {
			stx_write = false;
			write_packet_number++;
			write_transaction_number = 0;
			status.okay = 1;
			WriteStatusFifo.write(status);
		}
		else
			wrBufferWriteCounter -= (ETH_INTERFACE_WIDTH/8);
	}
	if (!ReadCmdFifo.empty() && !stx_read) {
		ReadCmdFifo.read(cmd);
		memory->setReadCmd(cmd);
		wrBufferReadCounter = cmd.bbt;
		stx_read = true;
		//cout << "Rx  READ command [" << setw(3) << read_command_number++<< "] address: " << hex << cmd.saddr << "\tlength: " << dec << cmd.bbt << "\ttime: " << simCycleCounter << endl;
		address_comparator = cmd.saddr + cmd.bbt;
		if (address_comparator > BUFFER_SIZE){
			cout << endl << endl << "Rx READ ERROR memory read overflow!!!!!!! Trying to read from " << hex << address_comparator << endl << endl ;
		}
	}
	else if(stx_read) {
		memory->readWord(outWord);
		BufferOut.write(outWord);

		//cout << "Rx READ [" << dec << setw(3) << read_packet_number << "][" << setw(2) << read_transaction_number++ << "]";
		//cout <<"\tdata: " << setw(130) << hex << outWord.data << "\tkeep: " << setw(18) << outWord.keep; 
		//cout << "\tlast: " << outWord.last << "\ttime: " << dec << simCycleCounter << endl;

		if (wrBufferReadCounter < (ETH_INTERFACE_WIDTH/8)+1) {
			stx_read = false;
			read_packet_number++;
			read_transaction_number = 0;
		}
		else
			wrBufferReadCounter -= (ETH_INTERFACE_WIDTH/8);
	}
}


void simulateTx(
		dummyMemory* 		memory, 
		stream<mmCmd>& 		WriteCmdFifo, 
		stream<mmStatus>& 	WriteStatusFifo, 
		stream<mmCmd>& 		ReadCmdFifo,
		stream<axiWord>& 	BufferIn, 
		stream<axiWord>& 	BufferOut) {

	mmCmd cmd;
	mmStatus status;
	axiWord inWord;
	axiWord outWord;
	static int write_command_number = 0;

	static bool stx_write 	= false;
	static bool stx_read 	= false;
	static bool stx_readCmd = false;

	static int app2mem_word=0;
	static int mem2tx_word=0;
	ap_uint<WINDOW_BITS+1> address_comparator;	

	if (!WriteCmdFifo.empty() && !stx_write) {
		WriteCmdFifo.read(cmd);
		memory->setWriteCmd(cmd);
		stx_write = true;
		//cout << endl << "Tx WRITE command [" << setw(3) << write_command_number++ << "] address: " << hex << cmd.saddr << "\tlength: " << dec << cmd.bbt << "\ttime: " << simCycleCounter << endl;
		address_comparator = cmd.saddr + cmd.bbt;
		if (address_comparator > BUFFER_SIZE){
			cout << endl << endl << "Tx WRITE ERROR memory write overflow!!!!!!! Trying to read from " << hex << address_comparator << endl << endl ;
		}
	}
	else if (!BufferIn.empty() && stx_write) {
		BufferIn.read(inWord);
		memory->writeWord(inWord);
		//cout << "Tx WRITE app2mem[" << dec << setw(3) << app2mem_word++ << "] data: " << hex << setw(130) << inWord.data << "\tkeep: " << hex << setw(18) << inWord.keep << "\tlast: " << inWord.last << "\ttime: " << dec << simCycleCounter << endl;
		if (inWord.last) {
			//fake_txBuffer.write(inWord); // RT hack
			stx_write = false;
			status.okay = 1;
			WriteStatusFifo.write(status);
			app2mem_word = 0;
		}
	}
	if (!ReadCmdFifo.empty() && !stx_read) {
		ReadCmdFifo.read(cmd);
		memory->setReadCmd(cmd);
		stx_read = true;
		//cout << endl << "Tx READ command address: " << hex << cmd.saddr << "\tlength: " << dec << cmd.bbt << "\ttime: " << simCycleCounter << endl << endl;
		address_comparator = cmd.saddr + cmd.bbt;
		if (address_comparator > BUFFER_SIZE){
			cout << endl << endl << "Tx READ ERROR memory read overflow!!!!!!! Trying to read from " << hex << address_comparator << endl << endl ;
		}
	}
	else if(stx_read) {
		memory->readWord(outWord);
		BufferOut.write(outWord);
		if (outWord.last)
			stx_read = false;
		//cout << "Tx  READ app2mem[" << dec << setw(3) << mem2tx_word++ << "] data: " << hex << setw(130) << outWord.data << "\tkeep: " << setw(18) << outWord.keep << "\tlast: " << outWord.last << "\ttime: " << dec << simCycleCounter << endl;
	}
}


int main(int argc, char **argv) {

  	stream<axiWord>						ipRxData("ipRxData");

  	stream<mmStatus>					rxBufferWriteStatus("rxBufferWriteStatus");
	stream<mmStatus>					txBufferWriteStatus("txBufferWriteStatus");
	stream<axiWord>						rxBufferReadData("rxBufferReadData");
	stream<axiWord>						txBufferReadData("txBufferReadData");
	stream<axiWord>						ipTxData("ipTxData");
	stream<mmCmd>						rxBufferWriteCmd("rxBufferWriteCmd");
	stream<mmCmd>						rxBufferReadCmd("rxBufferReadCmd");
	stream<mmCmd>						txBufferWriteCmd("txBufferWriteCmd");
	stream<mmCmd>						txBufferReadCmd("txBufferReadCmd");
	stream<axiWord>						rxBufferWriteData("rxBufferWriteData");
	stream<axiWord>						txBufferWriteData("txBufferWriteData");
	stream<rtlSessionLookupReply>		sessionLookup_rsp("sessionLookup_rsp");
	stream<rtlSessionUpdateReply>		sessionUpdate_rsp("sessionUpdate_rsp");
	stream<rtlSessionLookupRequest>		sessionLookup_req("sessionLookup_req");
	stream<rtlSessionUpdateRequest>		sessionUpdate_req("sessionUpdate_req");
	stream<ap_uint<16> >				rxApp2portTable_listen_req("rxApp2portTable_listen_req");
	stream<appReadRequest>				rxApp_request_memory_data("rxApp_request_memory_data");
	stream<ipTuple>						openConnReq("openConnReq");
	stream<ap_uint<16> >				closeConnReq("closeConnReq");
	stream<appTxMeta>				    txApp_write_request("txApp_write_request");
	stream<axiWord>						txApp_write_Data("txApp_write_Data");
	stream<listenPortStatus>			rxApp2portTable_listen_rsp("rxApp2portTable_listen_rsp");
	stream<appNotification>				rxAppNotification("rxAppNotification");
	stream<ap_uint<16> >				rxDataRspIDsession("rxDataRspIDsession");
	stream<axiWord>						rxData_to_rxApp("rxData_to_rxApp");
	stream<openStatus>					openConnRsp("openConnRsp");
	stream<appTxRsp>					txApp_data_write_response("txApp_data_write_response");
	ap_uint<16>							regSessionCount;
	ap_uint<32>							myIP_address=0x0500A8C0;
	stream<axiWord> 					rxDataOut("rxDataOut");						// This stream contains the data output from the Rx App I/F
  	stream<axiWord>						tx_pseudo_packet_to_checksum("tx_pseudo_packet_to_checksum");
	stream<ap_uint<16> >				tx_pseudo_packet_res_checksum("tx_pseudo_packet_res_checksum");
  	stream<axiWord>						rxEng_pseudo_packet_to_checksum("rxEng_pseudo_packet_to_checksum");
	stream<ap_uint<16> >				rxEng_pseudo_packet_res_checksum("rxEng_pseudo_packet_res_checksum");
	stream<txApp_client_status> 		rxEng2txApp_client_notification("rxEng2txApp_client_notification");

   	
	statsRegs 							stat_registers;

   	bool                     			readEnable = false;
	ap_uint<16>             			userID = 0;
	ap_uint<64>             			txBytes;
	ap_uint<54>             			txPackets;
	ap_uint<54>             			txRetransmissions;
	ap_uint<64>             			rxBytes;
	ap_uint<54>             			rxPackets;
	ap_uint<32>             			connectionRTT;


	dummyMemory rxMemory;
	dummyMemory txMemory;
	
	axiWord currOutWord;
	int 	packet=0;
	int		transaction=0;

	simCycleCounter		= 0;

	/* This variable indicates if the RX path is to be tested, thus it remains true in case of Rx only or bidirectional testing
	* It means TOE working as a client.
	*/
	/* This variable indicates if the TX path is to be tested, thus it remains true in case of Rx only or bidirectional testing
	* It means TOE working as a server.
	*/
	bool			testRxPath;	 
	bool			testTxPath;
	bool			firsts_packets_pace 	= true;
	bool			seconds_packets_pace 	= false;
	bool 			feed_sim;
	
	int 			mode=-1;


	stream<axiWord> 					tx_iperf_data("tx_iperf_data");						
  	stream<ap_uint<16> > 				tx_iperf_MetaData("tx_iperf_MetaData");
	stream<ap_int<17> > 				tx_iperf_Status("tx_iperf_Status");


	ap_uint<32> 						iperf_ipAddress0 		= 0xC0A80008;
	ap_uint<32> 						iperf_ipAddress1 		= 0xC0A80009;
	ap_uint<32> 						iperf_ipAddress2 		= 0xC0A8000A;
	ap_uint<32> 						iperf_ipAddress3 		= 0xC0A8000B;

	iperf_regs  						iperf_settings;

	iperf_settings.dualModeEn 								= 0;
	iperf_settings.transfer_size 							= 12543;
	iperf_settings.useTimer 								= 1;
	iperf_settings.runTime 									= 25000;
	iperf_settings.numConnections							= 1;
	iperf_settings.ipDestination 							= iperf_ipAddress0;
	iperf_settings.packet_mss 								= 1460;
	iperf_settings.dstPort 									= 5001;
	
	char 								*input_file;
	char 								*output_file;
	bool 								end_of_data=false;
	int 								packet_in_counter = 0;

	if (argc < 4) {
		cerr << "[ERROR] missing arguments " __FILE__  << " <MODE 0: RX 1: TX><INPUT_PCAP_FILE> <OUTPUT_PCAP_FILE> " << endl;;
		cerr << "If mode is TX, and optional argument can be provided, and it is the amount of bytes to transfer" << endl;
		return -1;
	}


	mode = atoi(argv[1]);
	if (mode==0){
		testRxPath = true;
		testTxPath = false;
	}
	else if(mode==1){
		testRxPath = false;
		testTxPath = true;

		if (argc == 5){
			iperf_settings.transfer_size = atoi(argv[4]);		// get the amount of bytes to send from argument
		}
	}
	else{
		cerr << "[ERROR] bad mode, it only can be 0 or 1" << endl;
		return -1;
	}

	cout << "WINDOW_BITS " << dec << WINDOW_BITS << "\tMAX_SESSIONS " << MAX_SESSIONS << "\tBUFFER_SIZE " << BUFFER_SIZE << endl;

	
	input_file 	= argv[2];
	output_file = argv[3];

	do  {


		if (testTxPath){
			if (simCycleCounter == 200){
				feed_sim = true;
			}
			else if (simCycleCounter > 1500) {
				if ((simCycleCounter+1) % 80) {
					feed_sim = true;
				}
				else {
					feed_sim = false;
				}
			}
			else {
				feed_sim = false;
			}

			
			//if ((((((simCycleCounter+1) % 80) ==0) && firsts_packets_pace) ||
			//	((((simCycleCounter+1) % 60) ==0) && seconds_packets_pace)) && (end_of_data==false)) {

			if (feed_sim && (end_of_data==false)) {	
				pcap2stream_step(input_file, false ,end_of_data, ipRxData);
				//cout << "Packet ["<< setw(3) << dec << packet_in_counter++ << "] goes in \ttime: " << simCycleCounter << endl;
			}
		}

		if (testRxPath){
			if (simCycleCounter > 200){
				firsts_packets_pace = false;
			}

			if (simCycleCounter > 400){
				seconds_packets_pace = true;
			}

			if ((((((simCycleCounter+1) % 80) ==0) && firsts_packets_pace) ||
				((((simCycleCounter+1) % 50) ==0) && seconds_packets_pace)) && (end_of_data==false)) {
				pcap2stream_step(input_file, false ,end_of_data, ipRxData);
				//cout << "Packet ["<< setw(3) << dec << packet_in_counter++ << "] goes in \ttime: " << simCycleCounter << endl;
			}

		}

		if (simCycleCounter == 10){
			iperf_settings.runExperiment = 1;
		}
		else {
			iperf_settings.runExperiment = 0;
		}

		if (simCycleCounter == totalSimCycles-5){
			readEnable = true;
		}

		stat_registers.readEnable 	= readEnable;
		stat_registers.userID 		= userID;

		toe(
			ipRxData,
#if (!RX_DDR_BYPASS)				
			rxBufferWriteStatus, 
			rxBufferWriteCmd,
			rxBufferReadCmd, 
			rxBufferReadData, 
			rxBufferWriteData, 
#endif				
			txBufferWriteStatus, 
			txBufferReadData, 
			ipTxData, 
			txBufferWriteCmd, 
			txBufferReadCmd, 
			txBufferWriteData, 

			sessionLookup_rsp, 
			sessionUpdate_rsp,
			sessionLookup_req, 
			sessionUpdate_req, 

			rxApp2portTable_listen_req, 

			rxApp_request_memory_data, 
			openConnReq, 
			closeConnReq, 
			txApp_write_request, 		
			txApp_write_Data,			

			rxApp2portTable_listen_rsp, 
			rxAppNotification,
			rxEng2txApp_client_notification,
			rxDataRspIDsession, 
			rxData_to_rxApp, 		
			openConnRsp, 
			txApp_data_write_response, 	

#ifdef STATISTICS_MODULE
			stat_registers,
#endif	

			myIP_address, 						// 192.168.0.5
			regSessionCount,
			tx_pseudo_packet_to_checksum,
			tx_pseudo_packet_res_checksum,
			rxEng_pseudo_packet_to_checksum,
			rxEng_pseudo_packet_res_checksum);


#if (!ECHO_REPLAY)

        iperf2_client(  
        	rxApp2portTable_listen_req,
            rxApp2portTable_listen_rsp,
            rxAppNotification,
            rxApp_request_memory_data,
            rxDataRspIDsession,
            rxDataOut,
            openConnReq,
            openConnRsp,
            closeConnReq,

            txApp_write_request,
            txApp_data_write_response,
            txApp_write_Data,
            rxEng2txApp_client_notification,

            iperf_settings);

/*		
		rxApp_sim (
			openConnReq,
			openConnRsp,
			closeConnReq,
			rxDataRspIDsession, 
			rxData_to_rxApp,
			rxAppNotification,
			rxApp_request_memory_data,
			iperf_num_useConn,
			iperf_ipAddress0,
			iperf_ipAddress1,
			iperf_ipAddress2,
			iperf_ipAddress3,
			testRxPath
		);

		txApp_sim(
			rxApp2portTable_listen_req,
			rxEng2txApp_client_notification,
			rxApp2portTable_listen_rsp,
			txApp_write_request,
			txApp_data_write_response,
			txApp_write_Data,
			bytes_to_send,
			testTxPath
		);
*/
#else
		echo_server_application(	
			rxApp2portTable_listen_req, 
			rxApp2portTable_listen_rsp,
			rxAppNotification, 
			rxApp_request_memory_data,
			rxDataRspIDsession, 
			rxData_to_rxApp,
			openConnReq, 
			openConnRsp,
			closeConnReq,
			txApp_write_request,
			txApp_write_Data,
			txApp_data_write_response,
			rxEng2txApp_client_notification);
#endif		
		simulateRx(
			&rxMemory, 
			rxBufferWriteCmd, 
			rxBufferWriteStatus, 
			rxBufferReadCmd, 
			rxBufferWriteData, 
			rxBufferReadData);

		simulateTx(
			&txMemory, 
			txBufferWriteCmd, 
			txBufferWriteStatus, 
			txBufferReadCmd, 
			txBufferWriteData, 
			txBufferReadData);
	   	
	   	sessionLookupStub(
	   		sessionLookup_req, 
	   		sessionLookup_rsp,
	   		sessionUpdate_req, 
	   		sessionUpdate_rsp);

	   	compute_pseudo_tcp_checksum(	
			tx_pseudo_packet_to_checksum,
			tx_pseudo_packet_res_checksum,
			0);

	   	compute_pseudo_tcp_checksum(	
			rxEng_pseudo_packet_to_checksum,
			rxEng_pseudo_packet_res_checksum,
			1);


		stream2pcap(output_file, false, true, ipTxData, false);

	} while (simCycleCounter++ < totalSimCycles);

	stream2pcap(output_file,false,true,ipTxData,true);

	cout << "regSessionCount " << dec << regSessionCount << endl; 

#if (STATISTICS_MODULE)
    cout << "  ------- Statistics ------- " << dec << endl;
	cout << "           txBytes -> " << txBytes << endl; 
	cout << "         txPackets -> " << txPackets << endl; 
	cout << " txRetransmissions -> " << txRetransmissions << endl; 
	cout << "           rxBytes -> " << rxBytes << endl; 
	cout << "         rxPackets -> " << rxPackets << endl; 
	cout << "     connectionRTT -> " << connectionRTT << endl; 
#endif	

	packet=0;
	transaction=0;
		
	return 0;

}
