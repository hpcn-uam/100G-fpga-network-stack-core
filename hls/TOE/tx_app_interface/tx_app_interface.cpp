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

#include "tx_app_interface.hpp"

using namespace hls;

void txEventMerger(	stream<event>&	txApp2eventEng_mergeEvent,
					stream<event>&	txAppStream2event_mergeEvent,
#if (TCP_NODELAY)
					stream<event>&	tasi_txEventCacheFifo,
#endif
					stream<event>&	txApp_merged_event)
{
#pragma HLS PIPELINE II=1

	event ev;
	// Merge Events
	if (!txApp2eventEng_mergeEvent.empty()) {
		txApp_merged_event.write(txApp2eventEng_mergeEvent.read());
	}
	else if (!txAppStream2event_mergeEvent.empty()) {
		txAppStream2event_mergeEvent.read(ev);
		txApp_merged_event.write(ev);
#if (TCP_NODELAY)
		if (ev.type == TX) {
			tasi_txEventCacheFifo.write(ev);
		}
#endif
	}
}

void txAppStatusHandler(stream<mmStatus>&				txBufferWriteStatus,
						stream<event>&					tasi_eventCacheFifo,
#if (!TCP_NODELAY)
						stream<event>&					txAppStream2eventEng_setEvent,
#endif
						stream<txAppTxSarPush>&			txApp2txSar_app_push)
{
#pragma HLS pipeline II=1

	enum tash_fsm_status {READ_EV, READ_STATUS_1, READ_STATUS_2};
	static tash_fsm_status tash_state=READ_EV;

	static event event_i;

	mmStatus status;
	ap_uint<WINDOW_BITS+1> tempLength;

	switch (tash_state){
		case READ_EV:
			if (!tasi_eventCacheFifo.empty()){
				tasi_eventCacheFifo.read(event_i);
				if (event_i.type == TX){
					tash_state = READ_STATUS_1;
				}
#if (!TCP_NODELAY)
				else {
					txAppStream2eventEng_setEvent.write(event_i);
				}
#endif
			}
			break;
		case READ_STATUS_1:
			if (!txBufferWriteStatus.empty()){
				txBufferWriteStatus.read(status);
				
				tempLength = event_i.address + event_i.length;

				if (status.okay){
					if (tempLength.bit(WINDOW_BITS)){
						tash_state =  READ_STATUS_2;
					}
					else {
						txApp2txSar_app_push.write(txAppTxSarPush(event_i.sessionID, event_i.address+event_i.length)); // App pointer update, pointer is released
						tash_state =  READ_EV;
#if (!TCP_NODELAY)
						txAppStream2eventEng_setEvent.write(event_i);
#endif
					}
				}

			}
			break;
		case READ_STATUS_2:
			if (!txBufferWriteStatus.empty()){
				txBufferWriteStatus.read(status);

				if (status.okay){
					txApp2txSar_app_push.write(txAppTxSarPush(event_i.sessionID, event_i.address+event_i.length)); // App pointer update, pointer is released
#if (!TCP_NODELAY)
					txAppStream2eventEng_setEvent.write(event_i);
#endif
					tash_state =  READ_EV;
				}

			}
			break;
		default :
			tash_state=READ_EV;
			break;			
	}
}

void tx_app_table(	stream<txSarAckPush>&		txSar2txApp_ack_push,
					stream<txAppTxSarQuery>&	txApp_upd_req,
					stream<txAppTxSarReply>&	txApp_upd_rsp)
{
#pragma HLS PIPELINE II=1

	static txAppTableEntry app_table[MAX_SESSIONS];
	#pragma HLS RESOURCE variable=app_table core=RAM_T2P_BRAM

	txSarAckPush	ackPush;
	txAppTxSarQuery txAppUpdate;

	if (!txSar2txApp_ack_push.empty()) {
		txSar2txApp_ack_push.read(ackPush);
		if (ackPush.init) {
			// At init this is actually not_ackd
			app_table[ackPush.sessionID].ackd = ackPush.ackd-1;
			app_table[ackPush.sessionID].mempt = ackPush.ackd;
			std::cout << "tx_app_table  .ackd " << std::hex << app_table[ackPush.sessionID].ackd << "\t.mempt " << app_table[ackPush.sessionID].mempt << std::endl;
#if (TCP_NODELAY)
			app_table[ackPush.sessionID].min_window = ackPush.min_window;
#endif			
		}
		else {
			app_table[ackPush.sessionID].ackd = ackPush.ackd;
#if (TCP_NODELAY)
			app_table[ackPush.sessionID].min_window = ackPush.min_window;
#endif			
		}
	}
	else if (!txApp_upd_req.empty()) {
		txApp_upd_req.read(txAppUpdate);
		// Write
		if(txAppUpdate.write) {
			app_table[txAppUpdate.sessionID].mempt = txAppUpdate.mempt;
		}
		else { // Read
		
#if !(TCP_NODELAY)
			txApp_upd_rsp.write(txAppTxSarReply(txAppUpdate.sessionID, app_table[txAppUpdate.sessionID].ackd, app_table[txAppUpdate.sessionID].mempt));
#else
			txApp_upd_rsp.write(txAppTxSarReply(txAppUpdate.sessionID, app_table[txAppUpdate.sessionID].ackd, app_table[txAppUpdate.sessionID].mempt, app_table[txAppUpdate.sessionID].min_window));
#endif
		}
	}

}

void tx_app_interface(	
					stream<appTxMeta>&			 	appTxDataReqMetadata,
					stream<axiWord>&				appTxDataReq,
					stream<sessionState>&			stateTable2txApp_rsp,
					stream<txSarAckPush>&			txSar2txApp_ack_push,
					stream<mmStatus>&				txBufferWriteStatus,

					stream<ipTuple>&				appOpenConnReq,
					stream<ap_uint<16> >&			appCloseConnReq,
					stream<sessionLookupReply>&		sLookup2txApp_rsp,
					stream<ap_uint<16> >&			portTable2txApp_port_rsp,
					stream<sessionState>&			stateTable2txApp_upd_rsp,
					stream<openStatus>&				conEstablishedFifo,

					stream<appTxRsp>&				appTxDataRsp,
					stream<ap_uint<16> >&			txApp2stateTable_req,
					stream<mmCmd>&					txBufferWriteCmd,
					stream<axiWord>&				txBufferWriteData,
					stream<txAppTxSarPush>&			txApp2txSar_push,

					stream<openStatus>&				appOpenConnRsp,
					stream<threeTuple>&				txApp2sLookup_req,
					//stream<ap_uint<1> >&			txApp2portTable_port_req,
					stream<stateQuery>&				txApp2stateTable_upd_req,
					stream<event>&					txApp2eventEng_setEvent,
					stream<openStatus>&				rtTimer2txApp_notification,
					ap_uint<32>&					myIpAddress)
{
//#pragma HLS DATAFLOW
	#pragma HLS INLINE

	// Fifos
	static stream<event> txApp2eventEng_mergeEvent("txApp2eventEng_mergeEvent");
	static stream<event> txAppStream2event_mergeEvent("txAppStream2event_mergeEvent");
	#pragma HLS stream variable=txApp2eventEng_mergeEvent		depth=2
	#pragma HLS stream variable=txAppStream2event_mergeEvent	depth=2
	#pragma HLS DATA_PACK variable=txApp2eventEng_mergeEvent
	#pragma HLS DATA_PACK variable=txAppStream2event_mergeEvent

	static stream<event> txApp_eventCacheFifo("txApp_eventCacheFifo");
	static stream<event> txApp_txEventCache("txApp_txEventCache");
	#pragma HLS stream variable=txApp_eventCacheFifo	depth=2
	#pragma HLS DATA_PACK variable=txApp_eventCacheFifo
	#pragma HLS stream variable=txApp_txEventCache	depth=64
	#pragma HLS DATA_PACK variable=txApp_txEventCache

	static stream<txAppTxSarQuery>		txApp2txSar_upd_req("txApp2txSar_upd_req");
	static stream<txAppTxSarReply>		txSar2txApp_upd_rsp("txSar2txApp_upd_rsp");
	#pragma HLS stream variable=txApp2txSar_upd_req		depth=2
	#pragma HLS stream variable=txSar2txApp_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=txApp2txSar_upd_req
	#pragma HLS DATA_PACK variable=txSar2txApp_upd_rsp

	// Before merging, check status for TX
	//txAppEvSplitter(txAppStream2event_mergeEvent, tasi_txSplit2mergeFifo, txApp_txEventCache);
	//txAppStatusHandler(txBufferWriteStatus, txApp_txEventCache, txApp2txSar_push);
	// Merge Events
	txEventMerger(	txApp2eventEng_mergeEvent,
					txAppStream2event_mergeEvent,
#if (TCP_NODELAY)
					txApp_txEventCache,
					txApp2eventEng_setEvent);
#else
					txApp_eventCacheFifo);
#endif
	
	txAppStatusHandler(	txBufferWriteStatus,
#if (TCP_NODELAY)
						txApp_txEventCache,
#else
						txApp_eventCacheFifo,
						txApp2eventEng_setEvent,
#endif
						txApp2txSar_push);

	// TX application Stream Interface
	tx_app_stream_if(	appTxDataReqMetadata,
						appTxDataReq,
						stateTable2txApp_rsp,
						txSar2txApp_upd_rsp,
						appTxDataRsp,
						txApp2stateTable_req,
						txApp2txSar_upd_req,
						txBufferWriteCmd,
						txBufferWriteData,
						txAppStream2event_mergeEvent);

	// TX Application Interface
	tx_app_if(	appOpenConnReq,
				appCloseConnReq,
				sLookup2txApp_rsp,
				portTable2txApp_port_rsp,
				stateTable2txApp_upd_rsp,
				conEstablishedFifo,
				appOpenConnRsp,
				txApp2sLookup_req,
				//txApp2portTable_port_req,
				txApp2stateTable_upd_req,
				txApp2eventEng_mergeEvent,
				rtTimer2txApp_notification,
				myIpAddress);

	// TX App Meta Table
	tx_app_table(	txSar2txApp_ack_push,
					txApp2txSar_upd_req,
					txSar2txApp_upd_rsp);
}
