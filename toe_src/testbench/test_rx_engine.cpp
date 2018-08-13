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
#include "../rx_engine/rx_engine.hpp"
#include <iostream>
#include "pcap2stream.hpp"


using namespace hls;
using namespace std;

void simSlookup(stream<sessionLookupQuery>&	req, stream<sessionLookupReply>& rsp)
{
	if (!req.empty())
	{
		req.read();
		rsp.write(sessionLookupReply(1, true));
	}
}

void simPortTable(stream<ap_uint<16> >& req, stream<bool>& rsp)
{
	if (!req.empty())
	{
		req.read();
		rsp.write(true);
	}
}

void simStateTable(stream<stateQuery>& req, stream<sessionState>& rsp)
{
	static sessionState currentState = CLOSED;
	stateQuery query;
	if (!req.empty())
	{
		req.read(query);
		if (query.write)
		{
			currentState = query.state;
		}
		else
		{
			rsp.write(currentState);
		}
	}

}

void simRxSar(stream<rxSarRecvd>& req, stream<rxSarEntry>& rsp)
{
	static rxSarEntry currRxEntry;
	rxSarRecvd query;
	if (!req.empty())
	{
		req.read(query);
		if (query.write)
		{
			currRxEntry.recvd = query.recvd;
		}
		else
		{
			rsp.write(currRxEntry);
		}
	}

}

void simTxSar(stream<rxTxSarQuery>& req, stream<rxTxSarReply>& rsp)
{
	static rxTxSarReply currTxEntry;
	rxTxSarQuery query;
	if (!req.empty())
	{
		req.read(query);
		if (query.write)
		{
			//currTxEntry.prevAck = query.ackd;
			currTxEntry.nextByte= query.ackd;
			//currTxEntry.cong_window = query.recv_window;
		}
		else
		{
			rsp.write(currTxEntry);
		}
	}

}

int main(int argc, char** argv)
{

	stream<axiWord>						ipRxData;
	stream<sessionLookupReply>			sLookup2rxEng_rsp;
	stream<sessionState>				stateTable2rxEng_upd_rsp("stateTable2rxEng_upd_rsp");
	stream<bool>						portTable2rxEng_rsp("portTable2rxEng_rsp");
	stream<rxSarEntry>					rxSar2rxEng_upd_rsp;
	stream<rxTxSarReply>				txSar2rxEng_upd_rsp;
	stream<mmStatus>					rxBufferWriteStatus;
	stream<axiWord>						rxBufferWriteData;
	stream<sessionLookupQuery>			rxEng2sLookup_req;
	stream<stateQuery>					rxEng2stateTable_upd_req("rxEng2stateTable_upd_req");
	stream<ap_uint<16> >				rxEng2portTable_req("rxEng2portTable_req");
	stream<rxSarRecvd>					rxEng2rxSar_upd_req;
	stream<rxTxSarQuery>				rxEng2txSar_upd_req;
	stream<rxRetransmitTimerUpdate>		rxEng2timer_clearRetransmitTimer;
	stream<ap_uint<16> >				rxEng2timer_clearProbeTimer;
	stream<ap_uint<16> >				rxEng2timer_setCloseTimer;
	stream<openStatus>					openConStatusOut; //TODO remove
	stream<extendedEvent>				rxEng2eventEng_setEvent("rxEng2eventEng_setEvent");
	stream<mmCmd>						rxBufferWriteCmd;
	stream<appNotification>				rxEng2rxApp_notification;

	int 								count = 0;

	std::ofstream outputFile;


	axiWord outData;

	if (argc < 2) {
		cout << "[ERROR] missing arguments." << endl;
		return -1;
	}
	

	uint16_t strbTemp;
	uint64_t dataTemp;
	uint16_t lastTemp;

	/* Read pcap file and convert to stream */
	pcap2stream_no_eth(argv[1], ipRxData);


	while(!ipRxData.empty()) {
		cout << "Loop " << dec << count++ << endl;
		rx_engine(	ipRxData,
					sLookup2rxEng_rsp,
					stateTable2rxEng_upd_rsp,
					portTable2rxEng_rsp,
					rxSar2rxEng_upd_rsp,
					txSar2rxEng_upd_rsp,
#if !(RX_DDR_BYPASS)
					rxBufferWriteStatus,
					rxBufferWriteCmd,
#endif
					rxBufferWriteData,
					rxEng2sLookup_req,
					rxEng2stateTable_upd_req,
					rxEng2portTable_req,
					rxEng2rxSar_upd_req,
					rxEng2txSar_upd_req,
					rxEng2timer_clearRetransmitTimer,
					rxEng2timer_clearProbeTimer,
					rxEng2timer_setCloseTimer,
					openConStatusOut, //TODO remove
					rxEng2eventEng_setEvent,
					rxEng2rxApp_notification);

		simPortTable(rxEng2portTable_req, portTable2rxEng_rsp);
		simSlookup(rxEng2sLookup_req, sLookup2rxEng_rsp);
		simStateTable(rxEng2stateTable_upd_req, stateTable2rxEng_upd_rsp);
		simRxSar(rxEng2rxSar_upd_req, rxSar2rxEng_upd_rsp);
		simTxSar(rxEng2txSar_upd_req, txSar2rxEng_upd_rsp);
	}

	count = 0;

	while (count < 500) {

		rx_engine(	ipRxData,
					sLookup2rxEng_rsp,
					stateTable2rxEng_upd_rsp,
					portTable2rxEng_rsp,
					rxSar2rxEng_upd_rsp,
					txSar2rxEng_upd_rsp,
#if (!RX_DDR_BYPASS)
					rxBufferWriteStatus,
					rxBufferWriteCmd,
#endif
					rxBufferWriteData,
					rxEng2sLookup_req,
					rxEng2stateTable_upd_req,
					rxEng2portTable_req,
					rxEng2rxSar_upd_req,
					rxEng2txSar_upd_req,
					rxEng2timer_clearRetransmitTimer,
					rxEng2timer_clearProbeTimer,
					rxEng2timer_setCloseTimer,
					openConStatusOut, //TODO remove
					rxEng2eventEng_setEvent,
					rxEng2rxApp_notification);

		simPortTable(rxEng2portTable_req, portTable2rxEng_rsp);
		simSlookup(rxEng2sLookup_req, sLookup2rxEng_rsp);
		simStateTable(rxEng2stateTable_upd_req, stateTable2rxEng_upd_rsp);
		simRxSar(rxEng2rxSar_upd_req, rxSar2rxEng_upd_rsp);
		simTxSar(rxEng2txSar_upd_req, txSar2rxEng_upd_rsp);
		count++;
	}


#if (!RX_DDR_BYPASS)
	while (!rxBufferWriteCmd.empty()) {
		int count_cmd=0;
		mmCmd cmd;
		rxBufferWriteCmd.read(cmd);
		cout << "CMD: [" << dec << count_cmd++ << "]\tAddress " << hex << cmd.saddr << "\t BTT: " << dec << cmd.bbt << endl;
	}
	while (!rxBufferWriteStatus.empty()) {
		int count_status=0;
		mmStatus status;
		rxBufferWriteStatus.read(status);
		cout << "Status: [" << dec << count_status++ << "]" << endl;
	}
#endif

	while (!rxBufferWriteData.empty()) {
		int pkt=0, word=0;
		axiWord buff_data;
		rxBufferWriteData.read(buff_data);
		cout << "Word: [" << dec << pkt << "] word [" << word++ << "]\tdata: " << buff_data.data << endl;

		if (buff_data.last){
			pkt++;
			word=0;
		}
	}

	while (!rxEng2eventEng_setEvent.empty()) {
		extendedEvent ev;
		rxEng2eventEng_setEvent.read(ev);
		cout << "Event ID: "<< ev.sessionID << "\t Event type: " << ev.type << endl;
	}

	while (!rxEng2timer_clearRetransmitTimer.empty()){
		rxRetransmitTimerUpdate rt_upd;
		rxEng2timer_clearRetransmitTimer.read(rt_upd);
		cout << "Retransmission timing update ID: " << dec << rt_upd.sessionID << "\tstop: " << rt_upd.stop<< endl;

	}

	while (!rxEng2timer_clearProbeTimer.empty()){
		ap_uint<16>  clr_timer;
		rxEng2timer_clearProbeTimer.read(clr_timer);
		cout << "Clear probe timer ID: " << clr_timer << endl;
	}

	while (!rxEng2timer_setCloseTimer.empty()) {
		ap_uint<16> set_timer;
		rxEng2timer_setCloseTimer.read(set_timer);
		cout << "Set close timer ID: " << set_timer << endl;
	}

/*
	while (!(rxBufferWriteData.empty())) {
		rxBufferWriteData.read(outData);
		outputFile << std::hex << std::noshowbase;
		outputFile << std::setfill('0');
		outputFile << std::setw(8) << ((uint32_t) outData.data(63, 32));
		outputFile << std::setw(8) << ((uint32_t) outData.data(31, 0));
		outputFile << " " << std::setw(2) << ((uint32_t) outData.keep) << " ";
		outputFile << std::setw(1) << ((uint32_t) outData.last);
		outputFile << std::endl;
	}

*/

	return 0;
}
