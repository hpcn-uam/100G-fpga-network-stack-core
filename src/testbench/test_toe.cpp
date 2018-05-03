/************************************************
Copyright (c) 2016, Xilinx, Inc.
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
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2015 Xilinx, Inc.
************************************************/
#include "../toe.hpp"
#include "dummy_memory.hpp"
#include "../session_lookup_controller/session_lookup_controller.hpp"
#include <map>
#include <string>
#include "pcap2stream.hpp"

//#define totalSimCycles 2500000
#define totalSimCycles 1000

using namespace std;

uint32_t cycleCounter;
unsigned int	simCycleCounter		= 0;


void rx_compute_pseudo_tcp_checksum(	
									stream<axiWord>&			dataIn,
									stream<ap_uint<16> >&		pseudo_tcp_checksum)
{

	axiWord 				currWord;
	static ap_uint<1> 		compute_checksum=0;

	static ap_uint<16> word_sum[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	ap_uint<16> tmp;
	ap_uint<17> tmp1;
	ap_uint<17> tmp2;

	static ap_uint<17> ip_sums_L1[16];
	static ap_uint<18> ip_sums_L2[8];
	static ap_uint<19> ip_sums_L3[4] = {0, 0, 0, 0};
	static ap_uint<20> ip_sums_L4[2];
	static ap_uint<21> ip_sums_L5;
	ap_uint<17> final_sum_r; 							// real add
	ap_uint<17> final_sum_o; 							// overflowed add
	ap_uint<16> res_checksum;

	if (!dataIn.empty() && !compute_checksum){
		dataIn.read(currWord);

		first_level_sum : for (int i=0 ; i < 32 ; i++ ){
			tmp(7,0) 	= currWord.data((((i*2)+1)*8)+7,((i*2)+1)*8);
			tmp(15,8) 	= currWord.data(((i*2)*8)+7,(i*2)*8);
			tmp1 		= word_sum[i] + tmp;
			tmp2 		= word_sum[i] + tmp + 1;
			if (tmp1.bit(16)) 				// one's complement adder
				word_sum[i] = tmp2(15,0);
			else
				word_sum[i] = tmp1(15,0);
		}

		if(currWord.last){
			compute_checksum = 1;
		}
	}
	else if(compute_checksum) {
		//adder tree
		second_level_sum : for (int i = 0; i < 16; i++) {
			ip_sums_L1[i] = word_sum[i*2] + word_sum[i*2+1];
			word_sum[i*2]   = 0; // clear adder variable
			word_sum[i*2+1] = 0;
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

		compute_checksum = 0;
		pseudo_tcp_checksum.write(res_checksum);
	}
}


void tx_compute_pseudo_tcp_checksum(	
									stream<axiWord>&			dataIn,
									stream<ap_uint<16> >&		pseudo_tcp_checksum)
{

	axiWord 				currWord;
	static ap_uint<1> 		compute_checksum=0;

	static ap_uint<16> word_sum[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	ap_uint<16> tmp;
	ap_uint<17> tmp1;
	ap_uint<17> tmp2;

	static ap_uint<17> ip_sums_L1[16];
	static ap_uint<18> ip_sums_L2[8];
	static ap_uint<19> ip_sums_L3[4] = {0, 0, 0, 0};
	static ap_uint<20> ip_sums_L4[2];
	static ap_uint<21> ip_sums_L5;
	ap_uint<17> final_sum_r; 							// real add
	ap_uint<17> final_sum_o; 							// overflowed add
	ap_uint<16> res_checksum;

	if (!dataIn.empty() && !compute_checksum){
		dataIn.read(currWord);

		first_level_sum : for (int i=0 ; i < 32 ; i++ ){
			tmp(7,0) 	= currWord.data((((i*2)+1)*8)+7,((i*2)+1)*8);
			tmp(15,8) 	= currWord.data(((i*2)*8)+7,(i*2)*8);
			tmp1 		= word_sum[i] + tmp;
			tmp2 		= word_sum[i] + tmp + 1;
			if (tmp1.bit(16)) 				// one's complement adder
				word_sum[i] = tmp2(15,0);
			else
				word_sum[i] = tmp1(15,0);
		}

		if(currWord.last){
			compute_checksum = 1;
		}
	}
	else if(compute_checksum) {
		//adder tree
		second_level_sum : for (int i = 0; i < 16; i++) {
			ip_sums_L1[i] = word_sum[i*2] + word_sum[i*2+1];
			word_sum[i*2]   = 0; // clear adder variable
			word_sum[i*2+1] = 0;
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

		compute_checksum = 0;
		pseudo_tcp_checksum.write(res_checksum);
	}
}


void sessionLookupStub(
		stream<rtlSessionLookupRequest>& 	lup_req, 
		stream<rtlSessionLookupReply>& 		lup_rsp,
		stream<rtlSessionUpdateRequest>& 	upd_req, 
		stream<rtlSessionUpdateReply>& 		upd_rsp) {
						//stream<ap_uint<14> >& new_id, stream<ap_uint<14> >& fin_id)
	static map<fourTupleInternal, ap_uint<14> > lookupTable;

	rtlSessionLookupRequest request;
	rtlSessionUpdateRequest update;

	map<fourTupleInternal, ap_uint<14> >::const_iterator findPos;

	if (!lup_req.empty()) {
		lup_req.read(request);
		findPos = lookupTable.find(request.key);
		if (findPos != lookupTable.end()) //hit
			lup_rsp.write(rtlSessionLookupReply(true, findPos->second, request.source));
		else
			lup_rsp.write(rtlSessionLookupReply(false, request.source));
	}

	if (!upd_req.empty()) {	//TODO what if element does not exist
		upd_req.read(update);
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

	if (!WriteCmdFifo.empty() && !stx_write) {
		WriteCmdFifo.read(cmd);
		memory->setWriteCmd(cmd);
		wrBufferWriteCounter = cmd.bbt;
		stx_write = true;
	}
	else if (!BufferIn.empty() && stx_write) {
		BufferIn.read(inWord);
		//cerr << dec << rxMemCounter << " - " << hex << inWord.data << " " << inWord.keep << " " << inWord.last << endl;
		//rxMemCounter++;;
		memory->writeWord(inWord);
		if (wrBufferWriteCounter < 9) {
			//fake_txBuffer.write(inWord); // RT hack
			stx_write = false;
			status.okay = 1;
			WriteStatusFifo.write(status);
		}
		else
			wrBufferWriteCounter -= 8;
	}
	if (!ReadCmdFifo.empty() && !stx_read) {
		ReadCmdFifo.read(cmd);
		memory->setReadCmd(cmd);
		wrBufferReadCounter = cmd.bbt;
		stx_read = true;
	}
	else if(stx_read) {
		memory->readWord(outWord);
		BufferOut.write(outWord);
		//cerr << dec << rxMemCounterRd << " - " << hex << outWord.data << " " << outWord.keep << " " << outWord.last << endl;
		rxMemCounterRd++;;
		if (wrBufferReadCounter < 9)
			stx_read = false;
		else
			wrBufferReadCounter -= 8;
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

	static bool stx_write = false;
	static bool stx_read = false;

	static bool stx_readCmd = false;

	if (!WriteCmdFifo.empty() && !stx_write) {
		WriteCmdFifo.read(cmd);
		cout << "WR: " << dec << cycleCounter << hex << " - " << cmd.saddr << " - " << cmd.bbt << endl;
		memory->setWriteCmd(cmd);
		stx_write = true;
	}
	else if (!BufferIn.empty() && stx_write) {
		BufferIn.read(inWord);
		cout << "Data: " << dec << cycleCounter << hex << inWord.data << " - " << inWord.keep << " - " << inWord.last << endl;
		memory->writeWord(inWord);
		if (inWord.last) {
			//fake_txBuffer.write(inWord); // RT hack
			stx_write = false;
			status.okay = 1;
			WriteStatusFifo.write(status);
		}
	}
	if (!ReadCmdFifo.empty() && !stx_read) {
		ReadCmdFifo.read(cmd);
		cout << "RD: " << cmd.saddr << " - " << cmd.bbt << endl;
		memory->setReadCmd(cmd);
		stx_read = true;
	}
	else if(stx_read) {
		memory->readWord(outWord);
		cout << inWord.data << " " << inWord.last << " - ";
		BufferOut.write(outWord);
		if (outWord.last)
			stx_read = false;
	}
}

void iperf(	
			stream<ap_uint<16> >& 		listenPort, 
			stream<bool>& 				listenPortStatus,
			// This is disabled for the time being, because it adds complexity/potential issues
			//stream<ap_uint<16> >& closePort,
			stream<appNotification>& 	notifications, 
			stream<appReadRequest>& 	readRequest,
			stream<ap_uint<16> >& 		rxMetaData, 
			stream<axiWord>& 			rxData, 
			stream<axiWord>& 			rxDataOut,
			stream<ipTuple>& 			openConnection, 
			stream<openStatus>& 		openConStatus,
			stream<ap_uint<16> >& 		closeConnection, 
			vector<ap_uint<16> >& 		txSessionIDs) {

	static bool listenDone = false;
	static bool runningExperiment = false;
	static ap_uint<1> listenFsm = 0;

	openStatus newConStatus;
	appNotification notification;
	ipTuple tuple;
	openStatus tempStatus;

	enum consumeFsmStateType {WAIT_PKG, CONSUME, HEADER_2, HEADER_3};
	static consumeFsmStateType  serverFsmState = WAIT_PKG;
	ap_uint<16> sessionID;
	axiWord currWord;
	currWord.last = 0;
	static bool dualTest = false;
	static ap_uint<32> mAmount = 0;
	
	axiWord transmitWord;

	// Open Port 5001
	if (!listenDone) {
		switch (listenFsm) {
		case 0:
			listenPort.write(5001);
			listenFsm++;
			break;
		case 1:
			if (!listenPortStatus.empty()) {
				listenPortStatus.read(listenDone);
				cout << "Listen port Done " << (listenDone ? "Yes" : "No") << endl;
				listenFsm++;
			}
			break;
		}
	}

	// In case we are connecting back
	if (!openConStatus.empty()) {
		openConStatus.read(tempStatus);
		cout << "tempStatus.ID " << dec << tempStatus.sessionID <<"tempStatus.success: " << tempStatus.success << endl;
		if(tempStatus.success){
			txSessionIDs.push_back(tempStatus.sessionID);
		}
	}

	if (!notifications.empty())	{
		notifications.read(notification);
		cout << "Notification length: " << dec << notification.length << endl;

		if (notification.length != 0)
			readRequest.write(appReadRequest(notification.sessionID, notification.length));
		else // closed
			runningExperiment = false;
	}


	switch (serverFsmState)	{
		case WAIT_PKG:
			if (!rxMetaData.empty() && !rxData.empty())	{
				cout << "WAIT_PKG time:" << dec << simCycleCounter << endl;
				rxMetaData.read(sessionID);
				rxData.read(currWord);
				rxDataOut.write(currWord);
				if (!runningExperiment) {
					if (currWord.data(31, 0) == 0x00000080) // Dual test
						dualTest = true;
					else
						dualTest = false;

					runningExperiment = true;
					serverFsmState = HEADER_2;
				}
				else
					serverFsmState = CONSUME;
			}
			break;
		case HEADER_2:
			if (!rxData.empty()) {
				cout << "HEADER_2 time:" << dec << simCycleCounter << endl;
				rxData.read(currWord);
				rxDataOut.write(currWord);
				if (dualTest) {
					tuple.ip_address = 0x0800A8C0;
					tuple.ip_port = currWord.data(31, 16);
					openConnection.write(tuple);
				}
				serverFsmState = HEADER_3;
			}
			break;
		case HEADER_3:
			if (!rxData.empty()) {
				cout << "HEADER_3 time:" << dec << simCycleCounter << endl;
				rxData.read(currWord);
				rxDataOut.write(currWord);
				mAmount = currWord.data(63, 32);
				serverFsmState = CONSUME;
			}
			break;
		case CONSUME:
			if (!rxData.empty()) {
				cout << "CONSUME time:" << dec << simCycleCounter << endl;
				rxData.read(currWord);
				rxDataOut.write(currWord);
			}
			break;
	}
	if (currWord.last == 1)
		serverFsmState = WAIT_PKG;
}

string decodeApUint64(ap_uint<64> inputNumber) {
	string 				outputString	= "0000000000000000";
	unsigned short int			tempValue 		= 16;
	static const char* const	lut 			= "0123456789ABCDEF";
	for (int i = 15;i>=0;--i) {
	tempValue = 0;
	for (unsigned short int k = 0;k<4;++k) {
		if (inputNumber.bit((i+1)*4-k-1) == 1)
			tempValue += static_cast <unsigned short int>(pow(2.0, 3-k));
		}
		outputString[15-i] = lut[tempValue];
	}
	return outputString;
}

string decodeApUint8(ap_uint<8> inputNumber) {
	string 						outputString	= "00";
	unsigned short int			tempValue 		= 16;
	static const char* const	lut 			= "0123456789ABCDEF";
	for (int i = 1;i>=0;--i) {
	tempValue = 0;
	for (unsigned short int k = 0;k<4;++k) {
		if (inputNumber.bit((i+1)*4-k-1) == 1)
			tempValue += static_cast <unsigned short int>(pow(2.0, 3-k));
		}
		outputString[1-i] = lut[tempValue];
	}
	return outputString;
}

ap_uint<64> encodeApUint64(string dataString){
	ap_uint<64> tempOutput = 0;
	unsigned short int	tempValue = 16;
	static const char* const	lut = "0123456789ABCDEF";

	for (unsigned short int i = 0; i<dataString.size();++i) {
		for (unsigned short int j = 0;j<16;++j) {
			if (lut[j] == dataString[i]) {
				tempValue = j;
				break;
			}
		}
		if (tempValue != 16) {
			for (short int k = 3;k>=0;--k) {
				if (tempValue >= pow(2.0, k)) {
					tempOutput.bit(63-(4*i+(3-k))) = 1;
					tempValue -= static_cast <unsigned short int>(pow(2.0, k));
				}
			}
		}
	}
	return tempOutput;
}

ap_uint<8> encodeApUint8(string keepString){
	ap_uint<8> tempOutput = 0;
	unsigned short int	tempValue = 16;
	static const char* const	lut = "0123456789ABCDEF";

	for (unsigned short int i = 0; i<2;++i) {
		for (unsigned short int j = 0;j<16;++j) {
			if (lut[j] == keepString[i]) {
				tempValue = j;
				break;
			}
		}
		if (tempValue != 16) {
			for (short int k = 3;k>=0;--k) {
				if (tempValue >= pow(2.0, k)) {
					tempOutput.bit(7-(4*i+(3-k))) = 1;
					tempValue -= static_cast <unsigned short int>(pow(2.0, k));
				}
			}
		}
	}
	return tempOutput;
}

ap_uint<16> checksumComputation(deque<axiWord>	pseudoHeader) {
	ap_uint<32> tcpChecksum = 0;
	for (uint8_t i=0;i<pseudoHeader.size();++i) {
		ap_uint<64> tempInput = (pseudoHeader[i].data.range(7, 0), pseudoHeader[i].data.range(15, 8), pseudoHeader[i].data.range(23, 16), pseudoHeader[i].data.range(31, 24), pseudoHeader[i].data.range(39, 32), pseudoHeader[i].data.range(47, 40), pseudoHeader[i].data.range(55, 48), pseudoHeader[i].data.range(63, 56));
		//cerr << hex << tempInput << " " << pseudoHeader[i].data << endl;
		tcpChecksum = ((((tcpChecksum + tempInput.range(63, 48)) + tempInput.range(47, 32)) + tempInput.range(31, 16)) + tempInput.range(15, 0));
		tcpChecksum = (tcpChecksum & 0xFFFF) + (tcpChecksum >> 16);
		tcpChecksum = (tcpChecksum & 0xFFFF) + (tcpChecksum >> 16);
	}
//	tcpChecksum = tcpChecksum.range(15, 0) + tcpChecksum.range(19, 16);
	tcpChecksum = ~tcpChecksum;			// Reverse the bits of the result
	return tcpChecksum.range(15, 0);	// and write it into the output
}


//// This version does not work for TCP segments that are too long... overflow happens
//ap_uint<16> checksumComputation(deque<axiWord>	pseudoHeader) {
//	ap_uint<20> tcpChecksum = 0;
//	for (uint8_t i=0;i<pseudoHeader.size();++i) {
//		ap_uint<64> tempInput = (pseudoHeader[i].data.range(7, 0), pseudoHeader[i].data.range(15, 8), pseudoHeader[i].data.range(23, 16), pseudoHeader[i].data.range(31, 24), pseudoHeader[i].data.range(39, 32), pseudoHeader[i].data.range(47, 40), pseudoHeader[i].data.range(55, 48), pseudoHeader[i].data.range(63, 56));
//		//cerr << hex << tempInput << " " << pseudoHeader[i].data << endl;
//		tcpChecksum = ((((tcpChecksum + tempInput.range(63, 48)) + tempInput.range(47, 32)) + tempInput.range(31, 16)) + tempInput.range(15, 0));
//	}
//	tcpChecksum = tcpChecksum.range(15, 0) + tcpChecksum.range(19, 16);
//	tcpChecksum = ~tcpChecksum;			// Reverse the bits of the result
//	return tcpChecksum.range(15, 0);	// and write it into the output
//}

ap_uint<16> recalculateChecksum(deque<axiWord> inputPacketizer) {
	ap_uint<16> newChecksum = 0;
	// Create the pseudo-header
	ap_uint<16> tcpLength 					= (inputPacketizer[0].data.range(23, 16), inputPacketizer[0].data.range(31, 24)) - 20;
	inputPacketizer[0].data					= (inputPacketizer[2].data.range(31, 0), inputPacketizer[1].data.range(63, 32));
	inputPacketizer[1].data.range(15, 0)	= 0x0600;
	inputPacketizer[1].data.range(31, 16)	= (tcpLength.range(7, 0), tcpLength(15, 8));
	inputPacketizer[4].data.range(47, 32)	= 0x0;
	//ap_uint<32> temp	= (tcpLength, 0x0600);
	inputPacketizer[1].data.range(63, 32) 	= inputPacketizer[2].data.range(63, 32);
	for (uint8_t i=2;i<inputPacketizer.size() -1;++i)
		inputPacketizer[i]= inputPacketizer[i+1];
	inputPacketizer.pop_back();
	//for (uint8_t i=0;i<inputPacketizer.size();++i)
	//	cerr << hex << inputPacketizer[i].data << " " << inputPacketizer[i].keep << endl;
	//newChecksum = checksumComputation(inputPacketizer); 		// Calculate the checksum
	//return newChecksum;
	return checksumComputation(inputPacketizer);
}

short int injectAckNumber(deque<axiWord> &inputPacketizer, map<fourTuple, ap_uint<32> > &sessionList) {
	fourTuple newTuple = fourTuple(inputPacketizer[1].data.range(63, 32), inputPacketizer[2].data.range(31, 0), inputPacketizer[2].data.range(47, 32), inputPacketizer[2].data.range(63, 48));
	//cerr << hex << inputPacketizer[4].data << endl;
	if (inputPacketizer[4].data.bit(9))	{													// If this packet is a SYN there's no need to inject anything
		//cerr << " Open Tuple: " << hex << inputPacketizer[1].data.range(63, 32) << " - " << inputPacketizer[2].data.range(31, 0) << " - " << inputPacketizer[2].data.range(47, 32) << " - " << inputPacketizer[2].data.range(63, 48) << endl;
		if (sessionList.find(newTuple) != sessionList.end()) {
			cerr << "WARNING: Trying to open an existing session! - " << simCycleCounter << endl;
			return -1;
		}
		else {
			sessionList[newTuple] = 0;
			cerr << "INFO: Opened new session - " << simCycleCounter << endl;
			return 0;
		}
	}
	else {																					// if it is any other packet
		if (sessionList.find(newTuple) != sessionList.end()) {
			inputPacketizer[3].data.range(63, 32) 	= sessionList[newTuple];				// inject the oldest acknowldgement number in the ack number deque
			//cerr << hex << "Input: " << inputPacketizer[3].data.range(63, 32) << endl;
			ap_uint<16> tempChecksum = recalculateChecksum(inputPacketizer);
			//inputPacketizer[4].data.range(47, 32) 	= recalculateChecksum(inputPacketizer);	// and recalculate the checksum
			inputPacketizer[4].data.range(47, 32) = (tempChecksum.range(7, 0), tempChecksum(15, 8));
			//inputPacketizer[4].data.range(47, 32) 	= 0xB13D;
			//for (uint8_t i=0;i<inputPacketizer.size();++i)
			//	cerr << hex << inputPacketizer[i].data << endl;
			return 1;
		}
		else {
			cerr << "WARNING: Trying to send data to a non-existing session!" << endl;
			return -1;
		}
	}
}

bool parseOutputPacket(deque<axiWord> &outputPacketizer, map<fourTuple, ap_uint<32> > &sessionList, deque<axiWord> &inputPacketizer) {		// Looks for an ACK packet in the output stream and when found if stores the ackNumber from that packet into
// the seqNumbers deque and clears the deque containing the output packet.
	bool returnValue = false;
	bool finPacket = false;
	static int pOpacketCounter = 0;
	static ap_uint<32> oldSeqNumber = 0;
	if (outputPacketizer[4].data.bit(9) && !outputPacketizer[4].data.bit(12)) { // Check if this is a SYN packet and if so reply with a SYN-ACK
		inputPacketizer.push_back(outputPacketizer[0]);
		ap_uint<32> ipBuffer = outputPacketizer[1].data.range(63, 32);
		outputPacketizer[1].data.range(63, 32) = outputPacketizer[2].data.range(31, 0);
		inputPacketizer.push_back(outputPacketizer[1]);
		outputPacketizer[2].data.range(31, 0) = ipBuffer;
		ap_uint<16> portBuffer = outputPacketizer[2].data.range(47, 32);
		outputPacketizer[2].data.range(47, 32) = outputPacketizer[2].data.range(63, 48);
		outputPacketizer[2].data.range(63, 48) = portBuffer;
		inputPacketizer.push_back(outputPacketizer[2]);
		ap_uint<32> reversedSeqNumber = (outputPacketizer[3].data.range(7,0), outputPacketizer[3].data.range(15, 8), outputPacketizer[3].data.range(23, 16), outputPacketizer[3].data.range(31, 24)) + 1;
		reversedSeqNumber = (reversedSeqNumber.range(7, 0), reversedSeqNumber.range(15, 8), reversedSeqNumber.range(23, 16), reversedSeqNumber.range(31, 24));
		outputPacketizer[3].data.range(31, 0) = outputPacketizer[3].data.range(63, 32);
		outputPacketizer[3].data.range(63, 32) = reversedSeqNumber;
		inputPacketizer.push_back(outputPacketizer[3]);
		outputPacketizer[4].data.bit(12) = 1;												// Set the ACK bit
		ap_uint<16> tempChecksum = recalculateChecksum(outputPacketizer);
		outputPacketizer[4].data.range(47, 32) = (tempChecksum.range(7, 0), tempChecksum(15, 8));
		inputPacketizer.push_back(outputPacketizer[4]);
		/*cerr << hex << outputPacketizer[0].data << endl;
		cerr << hex << outputPacketizer[1].data << endl;
		cerr << hex << outputPacketizer[2].data << endl;
		cerr << hex << outputPacketizer[3].data << endl;
		cerr << hex << outputPacketizer[4].data << endl;*/
	}
	else if (outputPacketizer[4].data.bit(8) && !outputPacketizer[4].data.bit(12)) 	// If the FIN bit is set but without the ACK bit being set at the same time
		sessionList.erase(fourTuple(outputPacketizer[1].data.range(63, 32), outputPacketizer[2].data.range(31, 0), outputPacketizer[2].data.range(47, 32), outputPacketizer[2].data.range(63, 48))); // Erase the tuple for this session from the map
	else if (outputPacketizer[4].data.bit(12)) { // If the ACK bit is set
		uint16_t packetLength = byteSwap16(outputPacketizer[0].data.range(31, 16));
		ap_uint<32> reversedSeqNumber = (outputPacketizer[3].data.range(7,0), outputPacketizer[3].data.range(15, 8), outputPacketizer[3].data.range(23, 16), outputPacketizer[3].data.range(31, 24));
		if (outputPacketizer[4].data.bit(9) || outputPacketizer[4].data.bit(8))
			reversedSeqNumber++;
		if (packetLength >= 40) {
			packetLength -= 40;
			reversedSeqNumber += packetLength;
		}
		reversedSeqNumber = (reversedSeqNumber.range(7, 0), reversedSeqNumber.range(15, 8), reversedSeqNumber.range(23, 16), reversedSeqNumber.range(31, 24));
		fourTuple packetTuple = fourTuple(outputPacketizer[2].data.range(31, 0), outputPacketizer[1].data.range(63, 32), outputPacketizer[2].data.range(63, 48), outputPacketizer[2].data.range(47, 32));
		sessionList[packetTuple] = reversedSeqNumber;
		returnValue = true;
		if (outputPacketizer[4].data.bit(8)) {	// This might be a FIN segment at the same time. In this case erase the session from the list
			uint8_t itemsErased = sessionList.erase(fourTuple(outputPacketizer[2].data.range(31, 0), outputPacketizer[1].data.range(63, 32), outputPacketizer[2].data.range(63, 48), outputPacketizer[2].data.range(47, 32))); // Erase the tuple for this session from the map
			finPacket = true;
			//cerr << "Close Tuple: " << hex << outputPacketizer[2].data.range(31, 0) << " - " << outputPacketizer[1].data.range(63, 32) << " - " << inputPacketizer[2].data.range(63, 48) << " - " << outputPacketizer[2].data.range(47, 32) << endl;
			if (itemsErased != 1)
				cerr << "WARNING: Received FIN segment for a non-existing session - " << simCycleCounter << endl;
			else
				cerr << "INFO: Session closed successfully - " << simCycleCounter << endl;
		}
		// Check if the ACK packet also constains data. If it does generate an ACK for. Look into the IP header length for this.
		if (packetLength > 0 || finPacket == true) { // 20B of IP Header & 20B of TCP Header since we never generate options
			finPacket = false;
			outputPacketizer[0].data.range(31, 16) = 0x2800;
			inputPacketizer.push_back(outputPacketizer[0]);
			ap_uint<32> ipBuffer = outputPacketizer[1].data.range(63, 32);
			outputPacketizer[1].data.range(63, 32) = outputPacketizer[2].data.range(31, 0);
			inputPacketizer.push_back(outputPacketizer[1]);
			outputPacketizer[2].data.range(31, 0) = ipBuffer;
			ap_uint<16> portBuffer = outputPacketizer[2].data.range(47, 32);
			outputPacketizer[2].data.range(47, 32) = outputPacketizer[2].data.range(63, 48);
			outputPacketizer[2].data.range(63, 48) = portBuffer;
			inputPacketizer.push_back(outputPacketizer[2]);
			//ap_uint<32> seqNumber = outputPacketizer[3].data.range(31, 0);
			outputPacketizer[3].data.range(31, 0) = outputPacketizer[3].data.range(63, 32);
			outputPacketizer[3].data.range(63, 32) = reversedSeqNumber;
			//cerr << hex << outputPacketizer[3].data.range(63, 32) << " - " << reversedSeqNumber << endl;
			inputPacketizer.push_back(outputPacketizer[3]);
			outputPacketizer[4].data.bit(12) = 1;												// Set the ACK bit
			outputPacketizer[4].data.bit(8) = 0;												// Unset the FIN bit
			ap_uint<16> tempChecksum = recalculateChecksum(outputPacketizer);
			outputPacketizer[4].data.range(47, 32) = (tempChecksum.range(7, 0), tempChecksum(15, 8));
			outputPacketizer[4].keep = 0x3F;
			outputPacketizer[4].last = 1;
			inputPacketizer.push_back(outputPacketizer[4]);
			/*cerr << std::hex << outputPacketizer[0].data << " - " << outputPacketizer[0].keep << " - " << outputPacketizer[0].last << endl;
			cerr << std::hex << outputPacketizer[1].data << " - " << outputPacketizer[1].keep << " - " << outputPacketizer[1].last << endl;
			cerr << std::hex << outputPacketizer[2].data << " - " << outputPacketizer[2].keep << " - " << outputPacketizer[2].last << endl;
			cerr << std::hex << outputPacketizer[3].data << " - " << outputPacketizer[3].keep << " - " << outputPacketizer[3].last << endl;
			cerr << std::hex << outputPacketizer[4].data << " - " << outputPacketizer[4].keep << " - " << outputPacketizer[4].last << endl;*/
			if (oldSeqNumber != reversedSeqNumber) {
				pOpacketCounter++;
				//cerr << "ACK cnt: " << dec << pOpacketCounter << hex << " - " << outputPacketizer[3].data.range(63, 32) << endl;
			}
			oldSeqNumber = reversedSeqNumber;
		}
		//cerr << hex << "Output: " << outputPacketizer[3].data.range(31, 0) << endl;
	}
	outputPacketizer.clear();
	return returnValue;
}

void flushInputPacketizer(deque<axiWord> &inputPacketizer, stream<axiWord> &ipRxData, map<fourTuple, ap_uint<32> > &sessionList) {
	if (inputPacketizer.size() != 0) {
		injectAckNumber(inputPacketizer, sessionList);
		uint8_t inputPacketizerSize = inputPacketizer.size();
		for (uint8_t i=0;i<inputPacketizerSize;++i) {
			axiWord temp = inputPacketizer.front();
			ipRxData.write(temp);
			inputPacketizer.pop_front();
		}
	}
}

vector<string> parseLine(string stringBuffer) {
	vector<string> tempBuffer;
	bool found = false;

	while (stringBuffer.find(" ") != string::npos)	// Search for spaces delimiting the different data words
	{
		string temp = stringBuffer.substr(0, stringBuffer.find(" "));							// Split the the string
		stringBuffer = stringBuffer.substr(stringBuffer.find(" ")+1, stringBuffer.length());	// into two
		tempBuffer.push_back(temp);		// and store the new part into the vector. Continue searching until no more spaces are found.
	}
	tempBuffer.push_back(stringBuffer);	// and finally push the final part of the string into the vector when no more spaces are present.
	return tempBuffer;
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
//	stream<rtlSessionUpdateRequest>		sessionUpdate_req;
	stream<ap_uint<16> >				listenPortReq("listenPortReq");
	stream<appReadRequest>				rxDataReq("rxDataReq");
	stream<ipTuple>						openConnReq("openConnReq");
	stream<ap_uint<16> >				closeConnReq("closeConnReq");
	stream<appTxMeta>				    txDataReqMeta("txDataReqMeta");
	stream<axiWord>						txDataReq("txDataReq");
	stream<bool>						listenPortRsp("listenPortRsp");
	stream<appNotification>				notification("notification");
	stream<ap_uint<16> >				rxDataRspMeta("rxDataRspMeta");
	stream<axiWord>						rxDataRsp("rxDataRsp");
	stream<openStatus>					openConnRsp("openConnRsp");
	stream<appTxRsp>					txDataRsp("txDataRsp");
	ap_uint<16>							regSessionCount;
	ap_uint<16>							relSessionCount;
	axiWord								ipTxDataOut_Data;
	axiWord								ipRxData_Data;
	axiWord								ipTxDataIn_Data;
	stream<axiWord> 					rxDataOut("rxDataOut");						// This stream contains the data output from the Rx App I/F
	axiWord								rxDataOut_Data;								// This variable is where the data read from the stream above is temporarily stored before output
  	
  	stream<axiWord>						tx_pseudo_packet_to_checksum("tx_pseudo_packet_to_checksum");
	stream<ap_uint<16> >				tx_pseudo_packet_res_checksum("tx_pseudo_packet_res_checksum");
  	stream<axiWord>						rxEng_pseudo_packet_to_checksum("rxEng_pseudo_packet_to_checksum");
	stream<ap_uint<16> >				rxEng_pseudo_packet_res_checksum("rxEng_pseudo_packet_res_checksum");

	dummyMemory rxMemory;
	dummyMemory txMemory;
	
	map<fourTuple, ap_uint<32> > sessionList;

	deque<axiWord>	inputPacketizer;
	deque<axiWord>	outputPacketizer;						// This deque collects the output data word until a whole packet is accumulated.

	axiWord currOutWord;
	int 	packet=0;
	int		transaction=0;

	ifstream	rxInputFile;
	ifstream	txInputFile;
	ofstream	rxOutput;
	ofstream	txOutput;
	ifstream	rxGold;
	ifstream	txGold;
	unsigned int	skipCounter 		= 0;
	cycleCounter = 0;
	simCycleCounter		= 0;
	unsigned int	myCounter		= 0;
	bool		idleCycle		= 0;
	string		dataString, keepString;
	vector<string> 	stringVector;
	vector<string>	txStringVector;
	bool 		firstWordFlag		= true;
	int 		rxGoldCompare		= 0;
	int			txGoldCompare		= 0; // not used yet
	int			returnValue		= 0;
	uint16_t	txWordCounter 		= 0;
	uint16_t	rxPayloadCounter	= 0; 	// This variable counts the number of packets output during the simulation on the Rx side
	uint16_t	txPacketCounter		= 0;	// This variable countrs the number of packet send from the Tx side (total number of packets, all kinds, from all sessions).
	bool		testRxPath		= true;	// This variable indicates if the Rx path is to be tested, thus it remains true in case of Rx only or bidirectional testing
	bool		testTxPath		= true;	// Same but for the Tx path.
	vector<ap_uint<16> > txSessionIDs;		// This vector holds the session IDs of the session to which data are transmitted.
	uint16_t	currentTxSessionID	= 0;	// Holds the session ID which was last used to send data into the Tx path.

	//char 		mode 		= *argv[1];



	if (argc < 3) {
		cout << "[ERROR] missing arguments " __FILE__  << " <INPUT_PCAP_FILE> <OUTPUT_PCAP_FILE> " << endl;
		return -1;
	}



	if (testTxPath == true) { 										// If the Tx Path will be tested then open a session for testing.
		for (uint8_t i=0;i<noOfTxSessions;++i) {
			ipTuple newTuple = {0x0800A8C0, 5002}; 				// IP address and port to open
			openConnReq.write(newTuple); 							// Write into TOE Tx I/F queue
		}
	}


	do  {

		if (simCycleCounter == 10 || simCycleCounter == 20){
			pcap2stream_step(argv[1], false ,ipRxData);
		}

		toe(
			ipRxData,
#if (!RX_DDR_BYPASS)				
			rxBufferWriteStatus, 
			rxBufferWriteCmd,
			rxBufferReadCmd, 
#endif				
			txBufferWriteStatus, 
			rxBufferReadData, 
			txBufferReadData, 
			ipTxData, 
			txBufferWriteCmd, 
			txBufferReadCmd, 
			rxBufferWriteData, 
			txBufferWriteData, 

			sessionLookup_rsp, 
			sessionUpdate_rsp,
			sessionLookup_req, 
			sessionUpdate_req, 

			listenPortReq, 
			rxDataReq, 
			openConnReq, 
			closeConnReq, 
			txDataReqMeta, 
			txDataReq,

			listenPortRsp, 
			notification, 
			rxDataRspMeta, 
			rxDataRsp, 
			openConnRsp, 
			txDataRsp, 

			0x0500A8C0, 						// 192.168.0.5
			regSessionCount,
			tx_pseudo_packet_to_checksum,
			tx_pseudo_packet_res_checksum,
			rxEng_pseudo_packet_to_checksum,
			rxEng_pseudo_packet_res_checksum);


		iperf(
			listenPortReq, 
			listenPortRsp, 
			notification, 
			rxDataReq,
			rxDataRspMeta, 
			rxDataRsp, 
			rxDataOut, 								// TODO read this
			openConnReq, 
			openConnRsp,
			closeConnReq, 
			txSessionIDs);							// TODO use this

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

	   	rx_compute_pseudo_tcp_checksum(	
			tx_pseudo_packet_to_checksum,
			tx_pseudo_packet_res_checksum);

	   	tx_compute_pseudo_tcp_checksum(	
			rxEng_pseudo_packet_to_checksum,
			rxEng_pseudo_packet_res_checksum);

		//while (!ipTxData.empty()){
		//	ipTxData.read(currOutWord);
		//	cout << "Output packet [" << dec << packet << "][" << dec << transaction++ << "] time[" << simCycleCounter << "]";
		//	cout	<< "\tData " << hex << currOutWord.data << "\tkeep " << currOutWord.keep << dec <<" (" << keep_to_length(currOutWord.keep) << ")\tlast " << currOutWord.last << endl;
		//	if (currOutWord.last){
		//		packet ++;
		//		transaction = 0;
		//	}
		//}

	} while (simCycleCounter++ < totalSimCycles);


	stream2pcap(argv[2],false,true,ipTxData);

	cout << "regSessionCount " << dec << regSessionCount << endl; 
	/*
	packet=0;
	transaction=0;

	while (!rxDataOut.empty()){
		rxDataOut.read(currOutWord);
		cout << "Rx data out [" << dec << packet << "] [" << dec << transaction++ << "]";
		cout	<< "\tData " << hex << currOutWord.data << "\tkeep " << currOutWord.keep << "\tlast " << currOutWord.last << endl;
		if (currOutWord.last){
			packet ++;
			transaction = 0;
		}
	}
	*/
	return 0;

}
