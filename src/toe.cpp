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
#include "memory_read/memory_read.hpp"


ap_uint<16> byteSwap16(ap_uint<16> inputVector) {
	return (inputVector.range(7,0), inputVector(15, 8));
}

ap_uint<32> byteSwap32(ap_uint<32> inputVector) {
	return (inputVector.range(7,0), inputVector(15, 8), inputVector.range(23,16), inputVector(31, 24));
}


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
	#pragma HLS stream variable=closeTimer2stateTable_releaseState		depth=2

	static stream<ap_uint<16> > rtTimer2stateTable_releaseState("rtTimer2stateTable_releaseState");
	#pragma HLS stream variable=rtTimer2stateTable_releaseState			depth=2

	static stream<event> rtTimer2eventEng_setEvent("rtTimer2eventEng_setEvent");
	#pragma HLS stream variable=rtTimer2eventEng_setEvent		depth=2

	static stream<event> probeTimer2eventEng_setEvent("probeTimer2eventEng_setEvent");
	#pragma HLS stream variable=probeTimer2eventEng_setEvent	depth=2

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


#if (RX_DDR_BYPASS)
void rxAppMemDataRead(	stream<ap_uint<1> >&	rxBufferReadCmd,
						stream<axiWord>&		rxBufferReadData,
						stream<axiWord>&		rxDataRsp)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	static ap_uint<1> ramdr_fsmState = 0;
	static bool 	  reading_data;
	axiWord currWord;

	if (!rxBufferReadCmd.empty() && !reading_data){			// FIXME this is completely useless. Why wait to read?
		rxBufferReadCmd.read();
		reading_data = true;
	}
	else if (!rxBufferReadData.empty() && reading_data) {
		rxBufferReadData.read(currWord);
		rxDataRsp.write(currWord);
		if (currWord.last){
			reading_data = false;
		}
	}
}
#endif



void rxAppWrapper(	stream<appReadRequest>&			appRxDataReq,
					stream<rxSarAppd>&				rxSar2rxApp_upd_rsp,
					stream<appNotification>&		rxEng2rxApp_notification,
					stream<appNotification>&		timer2rxApp_notification,
					stream<ap_uint<16> >&			appRxDataRspMetadata,
					stream<rxSarAppd>&				rxApp2rxSar_upd_req,
#if (!RX_DDR_BYPASS)
					stream<mmCmd>&					rxBufferReadCmd,
#endif
					stream<appNotification>&		appNotification,
					stream<axiWord> 				&rxBufferReadData,
					stream<axiWord> 				&rxDataRsp)
{
	#pragma HLS INLINE
	#pragma HLS PIPELINE II=1

	 // RX Application Stream Interface
#if (!RX_DDR_BYPASS)

	static stream<mmCmd>			rxAppStreamIf2memAccessBreakdown("rxAppStreamIf2memAccessBreakdown");
	#pragma HLS stream variable=rxAppStreamIf2memAccessBreakdown	depth=16

	static stream<memDoubleAccess>		rxAppDoubleAccess("rxAppDoubleAccess");
	#pragma HLS stream variable=rxAppDoubleAccess					depth=16

	rx_app_stream_if(
					appRxDataReq, 
					rxSar2rxApp_upd_rsp,
					appRxDataRspMetadata,
					rxApp2rxSar_upd_req,
					rxAppStreamIf2memAccessBreakdown);

	app_ReadMemAccessBreakdown(
					rxAppStreamIf2memAccessBreakdown,
					rxBufferReadCmd,
					rxAppDoubleAccess);

	app_MemDataRead_aligner(
					rxBufferReadData, 
					rxAppDoubleAccess,
					rxDataRsp);
#else

	static stream<ap_uint<1> >		rxBufferReadCmd("rxBufferReadCmd");
	#pragma HLS stream variable=rxBufferReadCmd					depth=4

	rx_app_stream_if(
					appRxDataReq,
					rxSar2rxApp_upd_rsp,
					appRxDataRspMetadata,
					rxApp2rxSar_upd_req,
					rxBufferReadCmd);

	rxAppMemDataRead(
					rxBufferReadCmd,
					rxBufferReadData,
					rxDataRsp);
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
 *  @param[in]		ipRxData				: Incoming packets from the interface (IP Layer)
 *  @param[in]		rxBufferWriteStatus		: Response of the data mover write request
 *  @param[out]		rxBufferWriteCmd		: Data mover command to write data to the memory
 *  @param[out]		rxBufferReadCmd 		: Data mover command to read data from the memory
 *  @param[in]		txBufferWriteStatus 	: Response of the data mover read request
 *  @param[in]		rxBufferReadData		: Data from the packet payload to the application
 *  @param[in]		txBufferReadData		: Data from the application to the packet payload
 *  @param[out]		ipTxData				: Outgoing packets to the interface (IP Layer)
 *  @param[out]		txBufferWriteCmd
 *  @param[out]		txBufferReadCmd
 *  @param[out]		rxBufferWriteData
 *  @param[out]		txBufferWriteData
 *  @param[in]		sessionLookup_rsp		: four-tuple to ID reply
 *  @param[in]		sessionUpdate_rsp 		: four-tuple insertion/delete response
 *  @param[out]		sessionLookup_req		: four-tuple to ID request
 *  @param[out]		sessionUpdate_req		: four-tuple insertion/delete request
 *  @param[in]		rxApp2portTable_listen_req
 *  @param[in]		rxDataReq
 *  @param[in]		openConnReq
 *  @param[in]		closeConnReq
 *  @param[in]		txDataReqMeta
 *  @param[in]		txDataReq
 *  @param[out]		portTable2rxApp_listen_rsp
 *  @param[out]		notification
 *  @param[out]		rxDataRspMeta
 *  @param[out]		rxDataRsp
 *  @param[out]		openConnRsp
 *  @param[out]		txDataRsp
 *  @param[in]		myIpAddress							: FPGA IP address
 *  @param[out]		regSessionCount						: Number of connections
 *  @param[out]		tx_pseudo_packet_to_checksum		: TX pseudo TCP packet
 *  @param[in]		tx_pseudo_packet_res_checksum		: TX TCP checksum
 *  @param[out]		rxEng_pseudo_packet_to_checksum		: RX pseudo TCP packet
 *  @param[in]		rxEng_pseudo_packet_res_checksum	: RX TCP checksum
 */
void toe(	// Data & Memory Interface
			stream<axiWord>&						ipRxData,
#if (!RX_DDR_BYPASS)
			stream<mmStatus>&						rxBufferWriteStatus,
			stream<mmCmd>&							rxBufferWriteCmd,
			stream<mmCmd>&							rxBufferReadCmd,
#endif
			stream<mmStatus>&						txBufferWriteStatus,
			stream<axiWord>&						rxBufferReadData,
			stream<axiWord>&						txBufferReadData,
			stream<axiWord>&						ipTxData,
			stream<mmCmd>&							txBufferWriteCmd,
			stream<mmCmd>&							txBufferReadCmd,
			stream<axiWord>&						rxBufferWriteData,
			stream<axiWord>&						txBufferWriteData,
			// SmartCam Interface
			stream<rtlSessionLookupReply>&			sessionLookup_rsp,
			stream<rtlSessionUpdateReply>&			sessionUpdate_rsp,
			stream<rtlSessionLookupRequest>&		sessionLookup_req,
			stream<rtlSessionUpdateRequest>&		sessionUpdate_req,
			// Application Interface
			stream<ap_uint<16> >&					rxApp2portTable_listen_req,
			// This is disabled for the time being, due to complexity concerns
			stream<appReadRequest>&					rxDataReq,
			stream<ipTuple>&						openConnReq,
			stream<ap_uint<16> >&					closeConnReq,
			stream<appTxMeta>&					   	txDataReqMeta,
			stream<axiWord>&						txDataReq,

			stream<bool>&							portTable2rxApp_listen_rsp,
			stream<appNotification>&				notification,
			stream<ap_uint<16> >&					rxDataRspMeta,
			stream<axiWord>&						rxDataRsp,
			stream<openStatus>&						openConnRsp,
			stream<appTxRsp>&						txDataRsp,

			//IP Address Input
			ap_uint<32>								myIpAddress,
			//statistic
			ap_uint<16>&							regSessionCount,
			stream<axiWord>&						tx_pseudo_packet_to_checksum,
			stream<ap_uint<16> >&					tx_pseudo_packet_res_checksum,
			stream<axiWord>&						rxEng_pseudo_packet_to_checksum,
			stream<ap_uint<16> >&					rxEng_pseudo_packet_res_checksum)
{
#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return
//#pragma HLS PIPELINE II=1
//#pragma HLS INLINE off

/*
 * PRAGMAs
 */
// Data & Memory interface
#pragma HLS INTERFACE axis off port=ipRxData name=s_axis_tcp_data
#pragma HLS INTERFACE axis off port=ipTxData name=m_axis_tcp_data
#pragma HLS INTERFACE axis off port=tx_pseudo_packet_to_checksum name=m_axis_tx_pseudo_packet
#pragma HLS INTERFACE axis off port=tx_pseudo_packet_res_checksum name=s_axis_tx_pseudo_packet_checksum
#pragma HLS INTERFACE axis off port=rxEng_pseudo_packet_to_checksum name=m_axis_rx_pseudo_packet
#pragma HLS INTERFACE axis off port=rxEng_pseudo_packet_res_checksum name=s_axis_rx_pseudo_packet_checksum

#pragma HLS INTERFACE axis off port=rxBufferWriteData name=m_axis_rxwrite_data
#pragma HLS INTERFACE axis off port=rxBufferReadData name=s_axis_rxread_data

#pragma HLS INTERFACE axis off port=txBufferWriteData name=m_axis_txwrite_data
#pragma HLS INTERFACE axis off port=txBufferReadData name=s_axis_txread_data

#if (!RX_DDR_BYPASS)
#pragma HLS INTERFACE axis off port=rxBufferWriteStatus name=s_axis_rxwrite_sts
#pragma HLS INTERFACE axis off port=rxBufferWriteCmd name=m_axis_rxwrite_cmd
#pragma HLS DATA_PACK variable=rxBufferWriteStatus
#pragma HLS DATA_PACK variable=rxBufferWriteCmd
#pragma HLS INTERFACE axis off port=rxBufferReadCmd name=m_axis_rxread_cmd	
#pragma HLS DATA_PACK variable=rxBufferReadCmd
#endif


#pragma HLS INTERFACE axis off port=txBufferWriteCmd name=m_axis_txwrite_cmd
#pragma HLS INTERFACE axis off port=txBufferReadCmd name=m_axis_txread_cmd
#pragma HLS DATA_PACK variable=txBufferWriteCmd
#pragma HLS DATA_PACK variable=txBufferReadCmd

// Data mover interface


#pragma HLS INTERFACE axis off port=txBufferWriteStatus name=s_axis_txwrite_sts
#pragma HLS DATA_PACK variable=txBufferWriteStatus

// SmartCam Interface
#pragma HLS INTERFACE axis off port=sessionLookup_rsp name=s_axis_session_lup_rsp 
#pragma HLS INTERFACE axis off port=sessionUpdate_rsp name=s_axis_session_upd_rsp
#pragma HLS INTERFACE axis off port=sessionLookup_req name=m_axis_session_lup_req
#pragma HLS INTERFACE axis off port=sessionUpdate_req name=m_axis_session_upd_req 

#pragma HLS DATA_PACK variable=sessionLookup_rsp
#pragma HLS DATA_PACK variable=sessionUpdate_rsp
#pragma HLS DATA_PACK variable=sessionLookup_req
#pragma HLS DATA_PACK variable=sessionUpdate_req

// Application Interface
#pragma HLS INTERFACE axis off port=portTable2rxApp_listen_rsp name=m_axis_listen_port_rsp
#pragma HLS INTERFACE axis off port=rxApp2portTable_listen_req name=s_axis_listen_port_req 
//#pragma HLS resource core=AXI4Stream variable=appClosePortIn metadata="-bus_bundle s_axis_close_port"

#pragma HLS INTERFACE axis off port=notification name=m_axis_notification
#pragma HLS INTERFACE axis off port=rxDataReq name=s_axis_rx_data_req 

#pragma HLS INTERFACE axis off port=rxDataRspMeta name=m_axis_rx_data_rsp_metadata
#pragma HLS INTERFACE axis off port=rxDataRsp name=m_axis_rx_data_rsp 

#pragma HLS INTERFACE axis off port=openConnReq name=s_axis_open_conn_req
#pragma HLS INTERFACE axis off port=openConnRsp name=m_axis_open_conn_rsp 
#pragma HLS INTERFACE axis off port=closeConnReq name=s_axis_close_conn_req 

#pragma HLS INTERFACE axis off port=txDataReqMeta name=s_axis_tx_data_req_metadata
#pragma HLS INTERFACE axis off port=txDataReq name=s_axis_tx_data_req 
#pragma HLS INTERFACE axis off port=txDataRsp name=m_axis_tx_data_rsp 
#pragma HLS DATA_PACK variable=notification
#pragma HLS DATA_PACK variable=rxDataReq
#pragma HLS DATA_PACK variable=openConnReq
#pragma HLS DATA_PACK variable=openConnRsp
#pragma HLS DATA_PACK variable=txDataReqMeta
#pragma HLS DATA_PACK variable=txDataRsp

#pragma HLS INTERFACE ap_none register port=myIpAddress
#pragma HLS INTERFACE ap_vld port=regSessionCount

	/*
	 * FIFOs
	 */
	// Session Lookup
	static stream<sessionLookupQuery>		rxEng2sLookup_req("rxEng2sLookup_req");
	#pragma HLS stream variable=rxEng2sLookup_req			depth=4
	#pragma HLS DATA_PACK variable=rxEng2sLookup_req

	static stream<sessionLookupReply>		sLookup2rxEng_rsp("sLookup2rxEng_rsp");
	#pragma HLS stream variable=sLookup2rxEng_rsp			depth=4
	#pragma HLS DATA_PACK variable=sLookup2rxEng_rsp

	static stream<fourTuple>				txApp2sLookup_req("txApp2sLookup_req");
	#pragma HLS stream variable=txApp2sLookup_req			depth=4
	#pragma HLS DATA_PACK variable=txApp2sLookup_req

	static stream<sessionLookupReply>		sLookup2txApp_rsp("sLookup2txApp_rsp");
	#pragma HLS stream variable=sLookup2txApp_rsp			depth=4
	#pragma HLS DATA_PACK variable=sLookup2txApp_rsp

	static stream<ap_uint<16> >				txEng2sLookup_rev_req("txEng2sLookup_rev_req");
	#pragma HLS stream variable=txEng2sLookup_rev_req		depth=4

	static stream<fourTuple>				sLookup2txEng_rev_rsp("sLookup2txEng_rev_rsp");
	#pragma HLS stream variable=sLookup2txEng_rev_rsp		depth=4
	#pragma HLS DATA_PACK variable=sLookup2txEng_rev_rsp

	// State Table
	static stream<stateQuery>			rxEng2stateTable_upd_req("rxEng2stateTable_upd_req");
	#pragma HLS stream variable=rxEng2stateTable_upd_req			depth=2
	#pragma HLS DATA_PACK variable=rxEng2stateTable_upd_req

	static stream<sessionState>			stateTable2rxEng_upd_rsp("stateTable2rxEng_upd_rsp");
	#pragma HLS stream variable=stateTable2rxEng_upd_rsp			depth=2

	static stream<stateQuery>			txApp2stateTable_upd_req("txApp2stateTable_upd_req");
	#pragma HLS stream variable=txApp2stateTable_upd_req			depth=2
	#pragma HLS DATA_PACK variable=txApp2stateTable_upd_req

	static stream<sessionState>			stateTable2txApp_upd_rsp("stateTable2txApp_upd_rsp");
	#pragma HLS stream variable=stateTable2txApp_upd_rsp			depth=2

	static stream<ap_uint<16> >			txApp2stateTable_req("txApp2stateTable_req");
	#pragma HLS stream variable=txApp2stateTable_req				depth=2
	//#pragma HLS DATA_PACK variable=txApp2stateTable_req

	static stream<sessionState>			stateTable2txApp_rsp("stateTable2txApp_rsp");
	#pragma HLS stream variable=stateTable2txApp_rsp				depth=2

	static stream<ap_uint<16> >			stateTable2sLookup_releaseSession("stateTable2sLookup_releaseSession");
	#pragma HLS stream variable=stateTable2sLookup_releaseSession	depth=2

	// RX Sar Table
	static stream<rxSarRecvd>			rxEng2rxSar_upd_req("rxEng2rxSar_upd_req");
	#pragma HLS stream variable=rxEng2rxSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=rxEng2rxSar_upd_req

	static stream<rxSarEntry>			rxSar2rxEng_upd_rsp("rxSar2rxEng_upd_rsp");
	#pragma HLS stream variable=rxSar2rxEng_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=rxSar2rxEng_upd_rsp

	static stream<rxSarAppd>			rxApp2rxSar_upd_req("rxApp2rxSar_upd_req");
	#pragma HLS stream variable=rxApp2rxSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=rxApp2rxSar_upd_req

	static stream<rxSarAppd>			rxSar2rxApp_upd_rsp("rxSar2rxApp_upd_rsp");
	#pragma HLS stream variable=rxSar2rxApp_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=rxSar2rxApp_upd_rsp

	static stream<ap_uint<16> >			txEng2rxSar_req("txEng2rxSar_req");
	#pragma HLS stream variable=txEng2rxSar_req			depth=2

	static stream<rxSarEntry>		    rxSar2txEng_rsp("rxSar2txEng_rsp");
	#pragma HLS stream variable=rxSar2txEng_rsp			depth=2
	#pragma HLS DATA_PACK variable=rxSar2txEng_rsp

	// TX Sar Table
	static stream<txTxSarQuery>			txEng2txSar_upd_req("txEng2txSar_upd_req");
	#pragma HLS stream variable=txEng2txSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=txEng2txSar_upd_req
	
	static stream<txTxSarReply>			txSar2txEng_upd_rsp("txSar2txEng_upd_rsp");
	#pragma HLS stream variable=txSar2txEng_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=txSar2txEng_upd_rsp

	//static stream<txAppTxSarQuery>		txApp2txSar_upd_req("txApp2txSar_upd_req");
	//static stream<txAppTxSarReply>		txSar2txApp_upd_rsp("txSar2txApp_upd_rsp");
	static stream<rxTxSarQuery>			rxEng2txSar_upd_req("rxEng2txSar_upd_req");
	#pragma HLS stream variable=rxEng2txSar_upd_req		depth=2
	#pragma HLS DATA_PACK variable=rxEng2txSar_upd_req
	
	static stream<rxTxSarReply>			txSar2rxEng_upd_rsp("txSar2rxEng_upd_rsp");
	#pragma HLS stream variable=txSar2rxEng_upd_rsp		depth=2
	#pragma HLS DATA_PACK variable=txSar2rxEng_upd_rsp

	static stream<txSarAckPush>			txSar2txApp_ack_push("txSar2txApp_ack_push");
	#pragma HLS stream variable=txSar2txApp_ack_push	depth=2
	#pragma HLS DATA_PACK variable=txSar2txApp_ack_push

	static stream<txAppTxSarPush>		txApp2txSar_push("txApp2txSar_push");
	#pragma HLS stream variable=txApp2txSar_push		depth=2
	#pragma HLS DATA_PACK variable=txApp2txSar_push
	//#pragma HLS stream variable=txApp2txSar_upd_req		depth=2
	//#pragma HLS stream variable=txSar2txApp_upd_rsp		depth=2
	//#pragma HLS DATA_PACK variable=txApp2txSar_upd_req
	//#pragma HLS DATA_PACK variable=txSar2txApp_upd_rsp

	// Retransmit Timer
	static stream<rxRetransmitTimerUpdate>		rxEng2timer_clearRetransmitTimer("rxEng2timer_clearRetransmitTimer");
	#pragma HLS stream variable=rxEng2timer_clearRetransmitTimer depth=2
	#pragma HLS DATA_PACK variable=rxEng2timer_clearRetransmitTimer
	
	static stream<txRetransmitTimerSet>			txEng2timer_setRetransmitTimer("txEng2timer_setRetransmitTimer");
	#pragma HLS stream variable=txEng2timer_setRetransmitTimer depth=2
	#pragma HLS DATA_PACK variable=txEng2timer_setRetransmitTimer
	// Probe Timer
	static stream<ap_uint<16> >					rxEng2timer_clearProbeTimer("rxEng2timer_clearProbeTimer");

	static stream<ap_uint<16> >					txEng2timer_setProbeTimer("txEng2timer_setProbeTimer");
	#pragma HLS stream variable=txEng2timer_setProbeTimer depth=2
	// Close Timer
	static stream<ap_uint<16> >					rxEng2timer_setCloseTimer("rxEng2timer_setCloseTimer");
	#pragma HLS stream variable=rxEng2timer_setCloseTimer depth=2
	// Timer Session release Fifos
	static stream<ap_uint<16> >					timer2stateTable_releaseState("timer2stateTable_releaseState");
	#pragma HLS stream variable=timer2stateTable_releaseState			depth=2

	// Event Engine
	static stream<extendedEvent>			rxEng2eventEng_setEvent("rxEng2eventEng_setEvent");
	#pragma HLS stream variable=rxEng2eventEng_setEvent				depth=512
	#pragma HLS DATA_PACK variable=rxEng2eventEng_setEvent
	
	static stream<event>					txApp2eventEng_setEvent("txApp2eventEng_setEvent");
	#pragma HLS stream variable=txApp2eventEng_setEvent			depth=4
	#pragma HLS DATA_PACK variable=txApp2eventEng_setEvent

	//static stream<event>					appStreamEventFifo("appStreamEventFifo");
	//static stream<event>					retransmitEventFifo("retransmitEventFifo");
	static stream<event>					timer2eventEng_setEvent("timer2eventEng_setEvent");
	#pragma HLS stream variable=timer2eventEng_setEvent			depth=4 //TODO maybe reduce to 2, there should be no evil cycle
	#pragma HLS DATA_PACK variable=timer2eventEng_setEvent

	static stream<extendedEvent>			eventEng2ackDelay_event("eventEng2ackDelay_event");
	#pragma HLS stream variable=eventEng2ackDelay_event				depth=4
	#pragma HLS DATA_PACK variable=eventEng2ackDelay_event

	static stream<extendedEvent>			eventEng2txEng_event("eventEng2txEng_event");
	#pragma HLS stream variable=eventEng2txEng_event				depth=16
	#pragma HLS DATA_PACK variable=eventEng2txEng_event

	// Application Interface
	static stream<openStatus>				conEstablishedFifo("conEstablishedFifo");
	#pragma HLS stream variable=conEstablishedFifo depth=4
	#pragma HLS DATA_PACK variable=conEstablishedFifo

	static stream<appNotification> 			rxEng2rxApp_notification("rxEng2rxApp_notification");
	#pragma HLS stream variable=rxEng2rxApp_notification		depth=4
	#pragma HLS DATA_PACK variable=rxEng2rxApp_notification

	static stream<appNotification>			timer2rxApp_notification("timer2rxApp_notification");
	#pragma HLS stream variable=timer2rxApp_notification		depth=4
	#pragma HLS DATA_PACK variable=timer2rxApp_notification

	static stream<openStatus>				timer2txApp_notification("timer2txApp_notification");
	#pragma HLS stream variable=timer2txApp_notification		depth=4
	#pragma HLS DATA_PACK variable=timer2txApp_notification

	// Port Table
	static stream<ap_uint<16> >				rxEng2portTable_check_req("rxEng2portTable_check_req");
	#pragma HLS stream variable=rxEng2portTable_check_req			depth=4

	static stream<bool>						portTable2rxEng_check_rsp("portTable2rxEng_check_rsp");
	#pragma HLS stream variable=portTable2rxEng_check_rsp			depth=4

	//static stream<ap_uint<1> >				txApp2portTable_port_req("txApp2portTable_port_req");
	static stream<ap_uint<16> >				portTable2txApp_port_rsp("portTable2txApp_port_rsp");
	#pragma HLS stream variable=portTable2txApp_port_rsp			depth=4

	static stream<ap_uint<16> >				sLookup2portTable_releasePort("sLookup2portTable_releasePort");
	#pragma HLS stream variable=sLookup2portTable_releasePort		depth=4

	//#pragma HLS stream variable=txApp2portTable_port_req			depth=4

   	static stream<axiWord>                 txApp2txEng_data_stream("txApp2txEng_data_stream");
   	#pragma HLS stream variable=txApp2txEng_data_stream   depth=2048

	static stream<ap_uint<1> > ackDelayFifoReadCount("ackDelayFifoReadCount");
	#pragma HLS stream variable=ackDelayFifoReadCount		depth=2

	static stream<ap_uint<1> > ackDelayFifoWriteCount("ackDelayFifoWriteCount");
	#pragma HLS stream variable=ackDelayFifoWriteCount		depth=2

	static stream<ap_uint<1> > txEngFifoReadCount("txEngFifoReadCount");
	#pragma HLS stream variable=txEngFifoReadCount		depth=2
	
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
					regSessionCount);
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
					//txApp2txSar_upd_req,
					txEng2txSar_upd_req,
					txApp2txSar_push,
					txSar2rxEng_upd_rsp,
					//txSar2txApp_upd_rsp,
					txSar2txEng_upd_rsp,
					txSar2txApp_ack_push);
	// Port Table
	port_table(		rxEng2portTable_check_req,
					rxApp2portTable_listen_req,
					//txApp2portTable_port_req,
					sLookup2portTable_releasePort,
					portTable2rxEng_check_rsp,
					portTable2rxApp_listen_rsp,
					portTable2txApp_port_rsp);

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
					portTable2rxEng_check_rsp,
					rxSar2rxEng_upd_rsp,
					txSar2rxEng_upd_rsp,
#if !(RX_DDR_BYPASS)
					rxBufferWriteStatus,
					rxBufferWriteCmd,
#endif
					rxBufferWriteData,
					rxEng2sLookup_req,
					rxEng2stateTable_upd_req,
					rxEng2portTable_check_req,
					rxEng2rxSar_upd_req,
					rxEng2txSar_upd_req,
					rxEng2timer_clearRetransmitTimer,
					rxEng2timer_clearProbeTimer,
					rxEng2timer_setCloseTimer,
					conEstablishedFifo, //remove this
					rxEng2eventEng_setEvent,
					rxEng2rxApp_notification,
					rxEng_pseudo_packet_to_checksum,
					rxEng_pseudo_packet_res_checksum);
	// TX Engine
	tx_engine(		eventEng2txEng_event,
					rxSar2txEng_rsp,
					txSar2txEng_upd_rsp,
					txBufferReadData,
#if (TCP_NODELAY)
            		txApp2txEng_data_stream,
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
	 rxAppWrapper(	rxDataReq,
			 	 	rxSar2rxApp_upd_rsp,
			 	 	rxEng2rxApp_notification,
			 	 	timer2rxApp_notification,
			 	 	rxDataRspMeta,
			 	 	rxApp2rxSar_upd_req,
#if !(RX_DDR_BYPASS)
			 	 	rxBufferReadCmd,
#endif
			 	 	notification,
			 	 	rxBufferReadData,
					rxDataRsp);

	tx_app_interface(
					txDataReqMeta,
					txDataReq,
					stateTable2txApp_rsp,
					//txSar2txApp_upd_rsp,
					txSar2txApp_ack_push,
					txBufferWriteStatus,
					openConnReq,
					closeConnReq,
					sLookup2txApp_rsp,
					portTable2txApp_port_rsp,
					stateTable2txApp_upd_rsp,
					conEstablishedFifo,
					txDataRsp,
					txApp2stateTable_req,
					//txApp2txSar_upd_req,
					txBufferWriteCmd,
					txBufferWriteData,
#if (TCP_NODELAY)
                  	txApp2txEng_data_stream,
#endif
					txApp2txSar_push,
					openConnRsp,
					txApp2sLookup_req,
					//txApp2portTable_port_req,
					txApp2stateTable_upd_req,
					txApp2eventEng_setEvent,
					timer2txApp_notification,
					myIpAddress);

}

