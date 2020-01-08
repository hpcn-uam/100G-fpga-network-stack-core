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

#include "toe.hpp"

#include "session_lookup_controller/session_lookup_controller.hpp"
#include "state_table/state_table.hpp"
#include "rx_sar_table/rx_sar_table.hpp"
#include "tx_sar_table/tx_sar_table.hpp"
#include "retransmit_timer/retransmit_timer.hpp"
#include "probe_timer/probe_timer.hpp"
#include "close_timer/close_timer.hpp"
#include "event_engine/event_engine.hpp"
#include "ack_delay/ack_delay.hpp"
#include "port_table/port_table.hpp"
#include "rx_engine/rx_engine.hpp"
#include "tx_engine/tx_engine.hpp"
#include "rx_app_stream_if/rx_app_stream_if.hpp"
#include "tx_app_interface/tx_app_interface.hpp"
#include "memory_access/memory_access.hpp"
#include "statistics/statistics.hpp"

/** @ingroup timer
 *
 */
void timerCloseMerger(stream<ap_uint<16> >& in1, stream<ap_uint<16> >& in2, stream<ap_uint<16> >& out)
{
#pragma HLS PIPELINE II=1

	ap_uint<16> sessionID;

	if (!in1.empty()) {
		in1.read(sessionID);
		out.write(sessionID);
	}
	else if (!in2.empty()) {
		in2.read(sessionID);
		out.write(sessionID);
	}
}

/** @ingroup tcp_module
 * Generic stream merger function
 */
template <typename T>
void stream_merger (stream<T>& in1, stream<T>& in2, stream<T>& out) {
#pragma HLS PIPELINE II=1

	if (!in1.empty()) {
		out.write(in1.read());
	}
	else if (!in2.empty()) {
		out.write(in2.read());
	}
}

/** @defgroup timer Timers
 *  @ingroup tcp_module
 *  @param[in]		rxEng2timer_clearRetransmitTimer
 *  @param[in]		txEng2timer_setRetransmitTimer
 *  @param[in]		txEng2timer_setProbeTimer
 *  @param[in]		rxEng2timer_setCloseTimer
 *  @param[out]		timer2stateTable_releaseState
 *  @param[out]		timer2eventEng_setEvent
 *  @param[out]		rtTimer2rxApp_notification
 */
void timerWrapper(	stream<rxRetransmitTimerUpdate>&	rxEng2timer_clearRetransmitTimer,
					stream<txRetransmitTimerSet>&		txEng2timer_setRetransmitTimer,
					stream<ap_uint<16> >&				rxEng2timer_clearProbeTimer,
					stream<ap_uint<16> >&				txEng2timer_setProbeTimer,
					stream<ap_uint<16> >&				rxEng2timer_setCloseTimer,
					stream<ap_uint<16> >&				timer2stateTable_releaseState,
					stream<event>&						timer2eventEng_setEvent,
					stream<appNotification>&			rtTimer2rxApp_notification,
					stream<openStatus>&					rtTimer2txApp_notification)
{
#pragma HLS DATAFLOW
#pragma HLS INLINE off
	//#pragma HLS PIPELINE II=1
	
	static stream<ap_uint<16> > closeTimer2stateTable_releaseState("closeTimer2stateTable_releaseState");
	#pragma HLS STREAM variable=closeTimer2stateTable_releaseState		depth=4

	static stream<ap_uint<16> > rtTimer2stateTable_releaseState("rtTimer2stateTable_releaseState");
	#pragma HLS STREAM variable=rtTimer2stateTable_releaseState			depth=4

	static stream<event> rtTimer2eventEng_setEvent("rtTimer2eventEng_setEvent");
	#pragma HLS STREAM variable=rtTimer2eventEng_setEvent		depth=4
	#pragma HLS DATA_PACK variable=rtTimer2eventEng_setEvent

	static stream<event> probeTimer2eventEng_setEvent("probeTimer2eventEng_setEvent");
	#pragma HLS STREAM variable=probeTimer2eventEng_setEvent	depth=4
	#pragma HLS DATA_PACK variable=probeTimer2eventEng_setEvent

	// Merge Events, Order: rtTimer has to be before probeTimer
	stream_merger(
				rtTimer2eventEng_setEvent, 
				probeTimer2eventEng_setEvent, 
				timer2eventEng_setEvent);

	retransmit_timer(	
				rxEng2timer_clearRetransmitTimer,
				txEng2timer_setRetransmitTimer,
				rtTimer2eventEng_setEvent,
				rtTimer2stateTable_releaseState,
				rtTimer2rxApp_notification,
				rtTimer2txApp_notification);

	probe_timer(
				rxEng2timer_clearProbeTimer,
				txEng2timer_setProbeTimer,
				probeTimer2eventEng_setEvent);
	
	close_timer(
				rxEng2timer_setCloseTimer,
				closeTimer2stateTable_releaseState);

	stream_merger(		
				closeTimer2stateTable_releaseState,
				rtTimer2stateTable_releaseState,
				timer2stateTable_releaseState);
}


void rxAppWrapper(	stream<appReadRequest>&			appRxDataReq,
					stream<rxSarAppd>&				rxSar2rxApp_upd_rsp,
					stream<appNotification>&		rxEng2rxApp_notification,
					stream<appNotification>&		timer2rxApp_notification,
					stream<ap_uint<16> >&			appRxDataRspIDsession,
					stream<rxSarAppd>&				rxApp2rxSar_upd_req,
#if (!RX_DDR_BYPASS)
					stream<mmCmd>&					rxBufferReadCmd,
					stream<axiWord>& 				rxBufferReadData,
					stream<axiWord>& 				rxDataRsp,
#endif
					stream<appNotification>&		appNotification)
{
	#pragma HLS INLINE
	#pragma HLS PIPELINE II=1

	 // RX Application Stream Interface
#if (!RX_DDR_BYPASS)

	static stream<cmd_internal>			rxAppStreamIf2memAccessBreakdown("rxAppStreamIf2memAccessBreakdown");
	#pragma HLS STREAM variable=rxAppStreamIf2memAccessBreakdown	depth=16
	#pragma HLS DATA_PACK variable=rxAppStreamIf2memAccessBreakdown

	static stream<memDoubleAccess>		rxAppDoubleAccess("rxAppDoubleAccess");
	#pragma HLS STREAM variable=rxAppDoubleAccess					depth=16
	#pragma HLS DATA_PACK variable=rxAppDoubleAccess

	rx_app_stream_if(
					appRxDataReq, 
					rxSar2rxApp_upd_rsp,
					appRxDataRspIDsession,
					rxAppStreamIf2memAccessBreakdown,
					rxApp2rxSar_upd_req);

	app_ReadMemAccessBreakdown(
					rxAppStreamIf2memAccessBreakdown,
					rxBufferReadCmd,
					rxAppDoubleAccess);

	app_MemDataRead_aligner(
					rxBufferReadData, 
					rxAppDoubleAccess,
					rxDataRsp);
#else
	rx_app_stream_if(
					appRxDataReq,
					rxSar2rxApp_upd_rsp,
					appRxDataRspIDsession,
					rxApp2rxSar_upd_req);

#endif

	stream_merger(
					rxEng2rxApp_notification,
					timer2rxApp_notification,
					appNotification);
}

/** @defgroup tcp_module TCP Module
 *  @defgroup app_if Application Interface
 *  @ingroup tcp_module
 *  @image top_module.png
 *  @param[in]		ipRxData							: Incoming packets from the interface (IP Layer)
 *  @param[in]		rxBufferWriteStatus					: Response of the data mover write request
 *  @param[out]		rxBufferWriteCmd					: Data mover command to write data to the memory
 *  @param[out]		rxBufferReadCmd 					: Data mover command to read data from the memory
 *  @param[in]		txBufferWriteStatus 				: Response of the data mover read request
 *  @param[in]		rxBufferReadData					: Data from the packet payload to the application
 *  @param[in]		txBufferReadData					: Data from the application to the packet payload
 *  @param[out]		ipTxData							: Outgoing packets to the interface (IP Layer)
 *  @param[out]		txBufferWriteCmd
 *  @param[out]		txBufferReadCmd
 *  @param[out]		rxBufferWriteData
 *  @param[out]		txBufferWriteData
 *  @param[in]		sessionLookup_rsp					: three-tuple to ID reply
 *  @param[in]		sessionUpdate_rsp 					: three-tuple insertion/delete response
 *  @param[out]		sessionLookup_req					: three-tuple to ID request
 *  @param[out]		sessionUpdate_req					: three-tuple insertion/delete request
 *  @param[in]		listenPortRequest
 *  @param[in]		rxApp_readRequest
 *  @param[in]		openConnReq
 *  @param[in]		closeConnReq
 *  @param[in]		txDataReqMeta
 *  @param[in]		txApp_Data2send 					: Data coming from the application which have to be sent.
 *  @param[out]		listenPortResponse
 *  @param[out]		rxAppNotification
 *  @param[out]		rxApp_readRequest_RspID
 *  @param[out]		rxDataRsp
 *  @param[out]		openConnRsp
 *  @param[out]		txAppDataRsp
 *  @param[in]		myIpAddress							: FPGA IP address
 *  @param[out]		regSessionCount						: Number of connections
 *  @param[out]		tx_pseudo_packet_to_checksum		: TX pseudo TCP packet
 *  @param[in]		tx_pseudo_packet_res_checksum		: TX TCP checksum
 *  @param[out]		rxEng_pseudo_packet_to_checksum		: RX pseudo TCP packet
 *  @param[in]		rxEng_pseudo_packet_res_checksum	: RX TCP checksum
 */
void toe(	
			// Data & Memory Interface
			stream<axiWord>&						ipRxData,
#if (!RX_DDR_BYPASS)
			stream<mmStatus>&						rxBufferWriteStatus,
			stream<mmCmd>&							rxBufferWriteCmd,
			stream<mmCmd>&							rxBufferReadCmd,
			stream<axiWord>&						rxBufferReadData,
			stream<axiWord>&						rxBufferWriteData,
#endif
			stream<mmStatus>&						txBufferWriteStatus,
			stream<axiWord>&						txBufferReadData,
			stream<axiWord>&						ipTxData,		
			stream<mmCmd>&							txBufferWriteCmd,
			stream<mmCmd>&							txBufferReadCmd,
			stream<axiWord>&						txBufferWriteData,
			// SmartCam Interface
			stream<rtlSessionLookupReply>&			sessionLookup_rsp,
			stream<rtlSessionUpdateReply>&			sessionUpdate_rsp,
			stream<rtlSessionLookupRequest>&		sessionLookup_req,
			stream<rtlSessionUpdateRequest>&		sessionUpdate_req,
			// Application Interface
			stream<ap_uint<16> >&					listenPortRequest,
			// This is disabled for the time being, due to complexity concerns
			stream<appReadRequest>&					rxApp_readRequest,
			stream<ipTuple>&						openConnReq,
			stream<ap_uint<16> >&					closeConnReq,
			stream<appTxMeta>&					   	txDataReqMeta,
			stream<axiWord>&						txApp_Data2send, 

			stream<listenPortStatus>&				listenPortResponse, 				
			stream<appNotification>&				rxAppNotification,
			stream<txApp_client_status>& 			rxEng2txAppNewClientNoty,
			stream<ap_uint<16> >&					rxApp_readRequest_RspID,
			stream<axiWord>&						rxDataRsp,							
			stream<openStatus>&						openConnRsp,
			stream<appTxRsp>&						txAppDataRsp,

#if (STATISTICS_MODULE)
		   	statsRegs& 								stat_regs,			
#endif	

			//IP Address Input
			ap_uint<32>&							myIpAddress,
			//statistic
			ap_uint<16>&							regSessionCount,
			stream<axiWord>&						tx_pseudo_packet_to_checksum,	
			stream<ap_uint<16> >&					tx_pseudo_packet_res_checksum,
			stream<axiWord>&						rxEng_pseudo_packet_to_checksum,
			stream<ap_uint<16> >&					rxEng_pseudo_packet_res_checksum)
{
#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return

/*
 * PRAGMAs
 */
// AXI4-Stream
	// PACKET
#pragma HLS INTERFACE axis register both port=ipRxData name=s_axis_tcp_data
#pragma HLS INTERFACE axis register both port=ipTxData name=m_axis_tcp_data
	// CHECKSUM
#pragma HLS INTERFACE axis register both port=tx_pseudo_packet_to_checksum name=m_axis_tx_pseudo_packet
#pragma HLS INTERFACE axis register both port=rxEng_pseudo_packet_to_checksum name=m_axis_rx_pseudo_packet
#pragma HLS INTERFACE axis register both port=rxEng_pseudo_packet_res_checksum name=s_axis_rx_pseudo_packet_checksum
#pragma HLS INTERFACE axis register both port=tx_pseudo_packet_res_checksum name=s_axis_tx_pseudo_packet_checksum
	// MEMORY
#pragma HLS INTERFACE axis register both port=txBufferReadData name=s_axis_tx_MM2S
#pragma HLS INTERFACE axis register both port=txBufferWriteData name=m_axis_tx_S2MM
#pragma HLS INTERFACE axis register both port=txBufferWriteStatus name=s_axis_txwrite_sts
#pragma HLS INTERFACE axis register both port=txBufferWriteCmd name=m_axis_txwrite_cmd
#pragma HLS INTERFACE axis register both port=txBufferReadCmd name=m_axis_txread_cmd
#if (!RX_DDR_BYPASS)
#pragma HLS INTERFACE axis register both port=rxBufferReadData name=s_axis_rx_MM2S
#pragma HLS INTERFACE axis register both port=rxBufferWriteData name=m_axis_rx_S2MM
#pragma HLS INTERFACE axis register both port=rxBufferWriteStatus name=s_axis_rxwrite_sts
#pragma HLS INTERFACE axis register both port=rxBufferWriteCmd name=m_axis_rxwrite_cmd
#pragma HLS INTERFACE axis register both port=rxBufferReadCmd name=m_axis_rxread_cmd	
#pragma HLS DATA_PACK variable=rxBufferWriteStatus
#pragma HLS DATA_PACK variable=rxBufferWriteCmd
#pragma HLS DATA_PACK variable=rxBufferReadCmd
#endif

	// APLICATION
#pragma HLS INTERFACE axis register both port=listenPortResponse name=m_ListenPortResponse
#pragma HLS INTERFACE axis register both port=listenPortRequest name=s_ListenPortRequest 
#pragma HLS INTERFACE axis register both port=rxAppNotification name=m_RxAppNoty
#pragma HLS INTERFACE axis register both port=rxApp_readRequest name=s_App2RxEngRequestData 
#pragma HLS INTERFACE axis register both port=rxEng2txAppNewClientNoty name=m_NewClientNoty
#pragma HLS INTERFACE axis register both port=rxApp_readRequest_RspID name=m_App2RxEngResponseID
#pragma HLS INTERFACE axis register both port=rxDataRsp name=m_RxRequestedData
#pragma HLS INTERFACE axis register both port=openConnReq name=s_OpenConnRequest
#pragma HLS INTERFACE axis register both port=openConnRsp name=m_OpenConnResponse 
#pragma HLS INTERFACE axis register both port=closeConnReq name=s_CloseConnRequest 
#pragma HLS INTERFACE axis register both port=txDataReqMeta name=s_TxDataRequest
#pragma HLS INTERFACE axis register both port=txApp_Data2send name=s_TxPayload 
#pragma HLS INTERFACE axis register both port=txAppDataRsp name=m_TxDataResponse 


	// SmartCam Interface
#pragma HLS INTERFACE axis register both port=sessionLookup_rsp name=s_axis_session_lup_rsp 
#pragma HLS INTERFACE axis register both port=sessionUpdate_rsp name=s_axis_session_upd_rsp
#pragma HLS INTERFACE axis register both port=sessionLookup_req name=m_axis_session_lup_req
#pragma HLS INTERFACE axis register both port=sessionUpdate_req name=m_axis_session_upd_req 


#pragma HLS DATA_PACK variable=txBufferWriteCmd
#pragma HLS DATA_PACK variable=txBufferReadCmd
#pragma HLS DATA_PACK variable=txBufferWriteStatus
#pragma HLS DATA_PACK variable=sessionLookup_rsp
#pragma HLS DATA_PACK variable=sessionUpdate_rsp
#pragma HLS DATA_PACK variable=sessionLookup_req
#pragma HLS DATA_PACK variable=sessionUpdate_req
#pragma HLS DATA_PACK variable=rxEng2txAppNewClientNoty
#pragma HLS DATA_PACK variable=rxAppNotification
#pragma HLS DATA_PACK variable=rxApp_readRequest
#pragma HLS DATA_PACK variable=openConnReq
#pragma HLS DATA_PACK variable=openConnRsp
#pragma HLS DATA_PACK variable=txDataReqMeta
#pragma HLS DATA_PACK variable=txAppDataRsp
#pragma HLS DATA_PACK variable=listenPortResponse

#pragma HLS INTERFACE ap_stable register port=myIpAddress name=myIpAddress
#pragma HLS INTERFACE ap_none register port=regSessionCount

	/*
	 * FIFOs
	 */
	// Session Lookup
	static stream<sessionLookupQuery>		rxEng2sLookup_req("rxEng2sLookup_req");
	#pragma HLS STREAM variable=rxEng2sLookup_req			depth=4
	#pragma HLS DATA_PACK variable=rxEng2sLookup_req

	static stream<sessionLookupReply>		sLookup2rxEng_rsp("sLookup2rxEng_rsp");
	#pragma HLS STREAM variable=sLookup2rxEng_rsp			depth=4
	#pragma HLS DATA_PACK variable=sLookup2rxEng_rsp

	static stream<threeTuple>				txApp2sLookup_req("txApp2sLookup_req");
	#pragma HLS STREAM variable=txApp2sLookup_req			depth=4
	#pragma HLS DATA_PACK variable=txApp2sLookup_req

	static stream<sessionLookupReply>		sLookup2txApp_rsp("sLookup2txApp_rsp");
	#pragma HLS STREAM variable=sLookup2txApp_rsp			depth=4
	#pragma HLS DATA_PACK variable=sLookup2txApp_rsp

	static stream<ap_uint<16> >				txEng2sLookup_rev_req("txEng2sLookup_rev_req");
	#pragma HLS STREAM variable=txEng2sLookup_rev_req		depth=32

	static stream<fourTuple>				sLookup2txEng_rev_rsp("sLookup2txEng_rev_rsp");
	#pragma HLS STREAM variable=sLookup2txEng_rev_rsp		depth=4
	#pragma HLS DATA_PACK variable=sLookup2txEng_rev_rsp

	// State Table
	static stream<stateQuery>			rxEng2stateTable_upd_req("rxEng2stateTable_upd_req");
	#pragma HLS STREAM variable=rxEng2stateTable_upd_req			depth=2
	#pragma HLS DATA_PACK variable=rxEng2stateTable_upd_req

	static stream<sessionState>			stateTable2rxEng_upd_rsp("stateTable2rxEng_upd_rsp");
	#pragma HLS STREAM variable=stateTable2rxEng_upd_rsp			depth=4
	#pragma HLS DATA_PACK variable=stateTable2rxEng_upd_rsp

	static stream<stateQuery>			txApp2stateTable_upd_req("txApp2stateTable_upd_req");
	#pragma HLS STREAM variable=txApp2stateTable_upd_req			depth=2
	#pragma HLS DATA_PACK variable=txApp2stateTable_upd_req

	static stream<sessionState>			stateTable2txApp_upd_rsp("stateTable2txApp_upd_rsp");
	#pragma HLS STREAM variable=stateTable2txApp_upd_rsp			depth=2
	#pragma HLS DATA_PACK variable=stateTable2txApp_upd_rsp

	static stream<ap_uint<16> >			txApp2stateTable_req("txApp2stateTable_req");
	#pragma HLS STREAM variable=txApp2stateTable_req				depth=2

	static stream<sessionState>			stateTable2txApp_rsp("stateTable2txApp_rsp");
	#pragma HLS STREAM variable=stateTable2txApp_rsp				depth=2
	#pragma HLS DATA_PACK variable=stateTable2txApp_rsp

	static stream<ap_uint<16> >			stateTable2sLookup_releaseSession("stateTable2sLookup_releaseSession");
	#pragma HLS STREAM variable=stateTable2sLookup_releaseSession	depth=2

	// RX Sar Table
	static stream<rxSarRecvd>			rxEng2rxSar_upd_req("rxEng2rxSar_upd_req");
	#pragma HLS STREAM variable=rxEng2rxSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=rxEng2rxSar_upd_req

	static stream<rxSarEntry>			rxSar2rxEng_upd_rsp("rxSar2rxEng_upd_rsp");
	#pragma HLS STREAM variable=rxSar2rxEng_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=rxSar2rxEng_upd_rsp

	static stream<rxSarAppd>			rxApp2rxSar_upd_req("rxApp2rxSar_upd_req");
	#pragma HLS STREAM variable=rxApp2rxSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=rxApp2rxSar_upd_req

	static stream<rxSarAppd>			rxSar2rxApp_upd_rsp("rxSar2rxApp_upd_rsp");
	#pragma HLS STREAM variable=rxSar2rxApp_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=rxSar2rxApp_upd_rsp

	static stream<ap_uint<16> >			txEng2rxSar_req("txEng2rxSar_req");
	#pragma HLS STREAM variable=txEng2rxSar_req			depth=2

	static stream<rxSarEntry_rsp>		rxSar2txEng_rsp("rxSar2txEng_rsp");
	#pragma HLS STREAM variable=rxSar2txEng_rsp			depth=2
	#pragma HLS DATA_PACK variable=rxSar2txEng_rsp

	// TX Sar Table
	static stream<txTxSarQuery>			txEng2txSar_upd_req("txEng2txSar_upd_req");
	#pragma HLS STREAM variable=txEng2txSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=txEng2txSar_upd_req
	
	static stream<txTxSarReply>			txSar2txEng_upd_rsp("txSar2txEng_upd_rsp");
	#pragma HLS STREAM variable=txSar2txEng_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=txSar2txEng_upd_rsp

	static stream<rxTxSarQuery>			rxEng2txSar_upd_req("rxEng2txSar_upd_req");
	#pragma HLS STREAM variable=rxEng2txSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=rxEng2txSar_upd_req
	
	static stream<rxTxSarReply>			txSar2rxEng_upd_rsp("txSar2rxEng_upd_rsp");
	#pragma HLS STREAM variable=txSar2rxEng_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=txSar2rxEng_upd_rsp

	static stream<txSarAckPush>			txSar2txApp_ack_push("txSar2txApp_ack_push");
	#pragma HLS STREAM variable=txSar2txApp_ack_push	depth=2
	#pragma HLS DATA_PACK variable=txSar2txApp_ack_push

	static stream<txAppTxSarPush>		txApp2txSar_push("txApp2txSar_push");
	#pragma HLS STREAM variable=txApp2txSar_push		depth=2
	#pragma HLS DATA_PACK variable=txApp2txSar_push

	// Retransmit Timer
	static stream<rxRetransmitTimerUpdate>		rxEng2timer_clearRetransmitTimer("rxEng2timer_clearRetransmitTimer");
	#pragma HLS STREAM variable=rxEng2timer_clearRetransmitTimer depth=2
	#pragma HLS DATA_PACK variable=rxEng2timer_clearRetransmitTimer
	
	static stream<txRetransmitTimerSet>			txEng2timer_setRetransmitTimer("txEng2timer_setRetransmitTimer");
	#pragma HLS STREAM variable=txEng2timer_setRetransmitTimer depth=2
	#pragma HLS DATA_PACK variable=txEng2timer_setRetransmitTimer
	// Probe Timer
	static stream<ap_uint<16> >					rxEng2timer_clearProbeTimer("rxEng2timer_clearProbeTimer");
	#pragma HLS STREAM variable=rxEng2timer_clearProbeTimer depth=2

	static stream<ap_uint<16> >					txEng2timer_setProbeTimer("txEng2timer_setProbeTimer");
	#pragma HLS STREAM variable=txEng2timer_setProbeTimer depth=2
	// Close Timer
	static stream<ap_uint<16> >					rxEng2timer_setCloseTimer("rxEng2timer_setCloseTimer");
	#pragma HLS STREAM variable=rxEng2timer_setCloseTimer depth=2
	// Timer Session release Fifos
	static stream<ap_uint<16> >					timer2stateTable_releaseState("timer2stateTable_releaseState");
	#pragma HLS STREAM variable=timer2stateTable_releaseState			depth=2

	// Event Engine
	static stream<extendedEvent>			rxEng2eventEng_setEvent("rxEng2eventEng_setEvent");
	#pragma HLS STREAM variable=rxEng2eventEng_setEvent				depth=512
	#pragma HLS DATA_PACK variable=rxEng2eventEng_setEvent
	
	static stream<event>					txApp2eventEng_setEvent("txApp2eventEng_setEvent");
	#pragma HLS STREAM variable=txApp2eventEng_setEvent			depth=4
	#pragma HLS DATA_PACK variable=txApp2eventEng_setEvent

	static stream<event>					timer2eventEng_setEvent("timer2eventEng_setEvent");
	#pragma HLS STREAM variable=timer2eventEng_setEvent			depth=4
	#pragma HLS DATA_PACK variable=timer2eventEng_setEvent

	static stream<extendedEvent>			eventEng2ackDelay_event("eventEng2ackDelay_event");
	#pragma HLS STREAM variable=eventEng2ackDelay_event				depth=4
	#pragma HLS DATA_PACK variable=eventEng2ackDelay_event

	static stream<extendedEvent>			eventEng2txEng_event("eventEng2txEng_event");
	#pragma HLS STREAM variable=eventEng2txEng_event				depth=16
	#pragma HLS DATA_PACK variable=eventEng2txEng_event

	// Application Interface
	static stream<openStatus>				conEstablishedFifo("conEstablishedFifo");
	#pragma HLS STREAM variable=conEstablishedFifo depth=4
	#pragma HLS DATA_PACK variable=conEstablishedFifo

	static stream<appNotification> 			rxEng2rxApp_notification("rxEng2rxApp_notification");
	#pragma HLS STREAM variable=rxEng2rxApp_notification		depth=4
	#pragma HLS DATA_PACK variable=rxEng2rxApp_notification

	static stream<appNotification>			timer2rxApp_notification("timer2rxApp_notification");
	#pragma HLS STREAM variable=timer2rxApp_notification		depth=4
	#pragma HLS DATA_PACK variable=timer2rxApp_notification

	static stream<openStatus>				timer2txApp_notification("timer2txApp_notification");
	#pragma HLS STREAM variable=timer2txApp_notification		depth=4
	#pragma HLS DATA_PACK variable=timer2txApp_notification

	// Port Table
	static stream<ap_uint<16> >				rxEng2portTable_req("rxEng2portTable_req");
	#pragma HLS STREAM variable=rxEng2portTable_req			depth=8

	static stream<bool>						portTable2rxEng_rsp("portTable2rxEng_rsp");
	#pragma HLS STREAM variable=portTable2rxEng_rsp			depth=32

	static stream<ap_uint<16> >				portTable2txApp_free_port("portTable2txApp_free_port");
	#pragma HLS STREAM variable=portTable2txApp_free_port			depth=4

	static stream<ap_uint<16> >				sLookup2portTable_releasePort("sLookup2portTable_releasePort");
	#pragma HLS STREAM variable=sLookup2portTable_releasePort		depth=4

	static stream<ap_uint<1> > ackDelayFifoReadCount("ackDelayFifoReadCount");
	#pragma HLS STREAM variable=ackDelayFifoReadCount		depth=2

	static stream<ap_uint<1> > ackDelayFifoWriteCount("ackDelayFifoWriteCount");
	#pragma HLS STREAM variable=ackDelayFifoWriteCount		depth=2

	static stream<ap_uint<1> > txEngFifoReadCount("txEngFifoReadCount");
	#pragma HLS STREAM variable=txEngFifoReadCount depth=2

#if (TCP_NODELAY)	
   	static stream<axiWord>                 txApp2txEng2PseudoHeader("txApp2txEng2PseudoHeader");
   	#pragma HLS STREAM variable=txApp2txEng2PseudoHeader   depth=512
   	#pragma HLS DATA_PACK variable=txApp2txEng2PseudoHeader

	static stream<axiWord>                 txApp2ExtMemory("txApp2ExtMemory");
   	#pragma HLS STREAM variable=txApp2ExtMemory   depth=512
   	#pragma HLS DATA_PACK variable=txApp2ExtMemory
#endif

#if (STATISTICS_MODULE)
	static stream<rxStatsUpdate>  rxEngStatsUpdate("rxEngStatsUpdate");
	#pragma HLS STREAM variable=rxEngStatsUpdate   depth=8
	#pragma HLS DATA_PACK variable=rxEngStatsUpdate

	static stream<txStatsUpdate>  txEngStatsUpdate("txEngStatsUpdate");
	#pragma HLS STREAM variable=txEngStatsUpdate   depth=8
	#pragma HLS DATA_PACK variable=txEngStatsUpdate

	#pragma HLS INTERFACE s_axilite port=stat_regs bundle=toe_stats

#endif
	/*
	 * Data Structures
	 */
	// Session Lookup Controller
	session_lookup_controller(	
					rxEng2sLookup_req,
					sLookup2rxEng_rsp,
					stateTable2sLookup_releaseSession,
					sLookup2portTable_releasePort,
					txApp2sLookup_req,
					sLookup2txApp_rsp,
					txEng2sLookup_rev_req,
					sLookup2txEng_rev_rsp,
					sessionLookup_req,
					sessionLookup_rsp,
					sessionUpdate_req,
					sessionUpdate_rsp,
					regSessionCount,
					myIpAddress);
	// State Table
	state_table(	rxEng2stateTable_upd_req,
					txApp2stateTable_upd_req,
					txApp2stateTable_req,
					timer2stateTable_releaseState,
					stateTable2rxEng_upd_rsp,
					stateTable2txApp_upd_rsp,
					stateTable2txApp_rsp,
					stateTable2sLookup_releaseSession);
	// RX Sar Table
	rx_sar_table(	rxEng2rxSar_upd_req,
					rxApp2rxSar_upd_req,
					txEng2rxSar_req,
					rxSar2rxEng_upd_rsp,
					rxSar2rxApp_upd_rsp,
					rxSar2txEng_rsp);

	// TX Sar Table
	tx_sar_table(	rxEng2txSar_upd_req,
					txEng2txSar_upd_req,
					txApp2txSar_push,
					txSar2rxEng_upd_rsp,
					txSar2txEng_upd_rsp,
					txSar2txApp_ack_push);
	// Port Table
	port_table(		rxEng2portTable_req,
					listenPortRequest,
					sLookup2portTable_releasePort,
					portTable2rxEng_rsp,
					listenPortResponse,
					portTable2txApp_free_port);

	// Timers
	timerWrapper(	rxEng2timer_clearRetransmitTimer,
					txEng2timer_setRetransmitTimer,
					rxEng2timer_clearProbeTimer,
					txEng2timer_setProbeTimer,
					rxEng2timer_setCloseTimer,
					timer2stateTable_releaseState,
					timer2eventEng_setEvent,
					timer2rxApp_notification,
					timer2txApp_notification);

	event_engine(   txApp2eventEng_setEvent, 
					rxEng2eventEng_setEvent, 
					timer2eventEng_setEvent, 
					eventEng2ackDelay_event,
					ackDelayFifoReadCount, 
					ackDelayFifoWriteCount, 
					txEngFifoReadCount);

	ack_delay(      eventEng2ackDelay_event, 
					eventEng2txEng_event, 
					ackDelayFifoReadCount, 
					ackDelayFifoWriteCount);
	/*
	 * Engines
	 */
	// RX Engine
	rx_engine(		ipRxData,
					sLookup2rxEng_rsp,
					stateTable2rxEng_upd_rsp,
					portTable2rxEng_rsp,
					rxSar2rxEng_upd_rsp,
					txSar2rxEng_upd_rsp,
#if !(RX_DDR_BYPASS)
					rxBufferWriteStatus,
					rxBufferWriteCmd,
					rxBufferWriteData,
#else					
					rxDataRsp,
#endif
					rxEng2sLookup_req,
					rxEng2stateTable_upd_req,
					rxEng2portTable_req,
					rxEng2rxSar_upd_req,
					rxEng2txSar_upd_req,
					rxEng2timer_clearRetransmitTimer,
					rxEng2timer_clearProbeTimer,
					rxEng2timer_setCloseTimer,
					conEstablishedFifo, //remove this
					rxEng2eventEng_setEvent,
					rxEng2rxApp_notification,
					rxEng2txAppNewClientNoty,
#if (STATISTICS_MODULE)
					rxEngStatsUpdate,
#endif						
					rxEng_pseudo_packet_to_checksum,
					rxEng_pseudo_packet_res_checksum);
	// TX Engine
	tx_engine(		eventEng2txEng_event,
					rxSar2txEng_rsp,
					txSar2txEng_upd_rsp,
					txBufferReadData,
#if (TCP_NODELAY)
            		txApp2txEng2PseudoHeader,
#endif
#if (STATISTICS_MODULE)
					txEngStatsUpdate,
#endif	            		
					sLookup2txEng_rev_rsp,
					txEng2rxSar_req,
					txEng2txSar_upd_req,
					txEng2timer_setRetransmitTimer,
					txEng2timer_setProbeTimer,
					txBufferReadCmd,
					txEng2sLookup_rev_req,
					ipTxData,
					txEngFifoReadCount,
					tx_pseudo_packet_to_checksum,
					tx_pseudo_packet_res_checksum);

	/*
	 * Application Interfaces
	 */
	 rxAppWrapper(	rxApp_readRequest,
			 	 	rxSar2rxApp_upd_rsp,
			 	 	rxEng2rxApp_notification,
			 	 	timer2rxApp_notification,
			 	 	rxApp_readRequest_RspID,
			 	 	rxApp2rxSar_upd_req,
#if !(RX_DDR_BYPASS)
			 	 	rxBufferReadCmd,
			 	 	rxBufferReadData,
#endif
			 	 	rxAppNotification);


#if (TCP_NODELAY)
	 DataBroadcast(
					txApp_Data2send,
					txApp2ExtMemory,
					txApp2txEng2PseudoHeader);
#endif

	tx_app_interface(
					txDataReqMeta,
#if (TCP_NODELAY)				
					txApp2ExtMemory,
#else
					txApp_Data2send,
#endif					
					stateTable2txApp_rsp,
					txSar2txApp_ack_push,
					txBufferWriteStatus,

					openConnReq,
					closeConnReq,
					sLookup2txApp_rsp,
					portTable2txApp_free_port,
					stateTable2txApp_upd_rsp,					
					conEstablishedFifo,
					
					txAppDataRsp,
					txApp2stateTable_req,
					txBufferWriteCmd,
					txBufferWriteData,
					txApp2txSar_push,
					openConnRsp,
					txApp2sLookup_req,
					txApp2stateTable_upd_req,
					txApp2eventEng_setEvent,
					timer2txApp_notification,
					myIpAddress);

#if (STATISTICS_MODULE)
	toeStatistics (
				    rxEngStatsUpdate,
				    txEngStatsUpdate,
				   	stat_regs);
#endif	


}

