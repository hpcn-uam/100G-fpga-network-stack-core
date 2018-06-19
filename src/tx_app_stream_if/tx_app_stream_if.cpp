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

#include "tx_app_stream_if.hpp"

using namespace hls;

/** @ingroup tx_app_stream_if
 *  Reads the request from the application and loads the necessary metadata,
 *  the FSM decides if the packet is written to the TX buffer or discarded.
 */
void tasi_metaLoader(	stream<appTxMeta>&				appTxDataReqMetaData,
						stream<sessionState>&			stateTable2txApp_rsp,
						stream<txAppTxSarReply>&		txSar2txApp_upd_rsp,
						stream<appTxRsp>&				appTxDataRsp,
						stream<ap_uint<16> >&			txApp2stateTable_req,
						stream<txAppTxSarQuery>&		txApp2txSar_upd_req,
						stream<pkgPushMeta>&			tasi_writeToBufFifo,
						stream<event>&					txAppStream2eventEng_setEvent)
{
#pragma HLS pipeline II=1

	enum tai_states {READ_REQUEST, READ_META};
	static tai_states tai_state = READ_REQUEST;

	static appTxMeta 		tasi_writeMeta;
	txAppTxSarReply 		writeSar;
	ap_uint<16> 			maxWriteLength;
	ap_uint<16> 			usedLength;
	ap_uint<16> 			usableWindow;

	sessionState state;

	// FSM requests metadata, decides if packet goes to buffer or not
	switch(tai_state) {
		case READ_REQUEST:
			if (!appTxDataReqMetaData.empty()) {
				appTxDataReqMetaData.read(tasi_writeMeta); 								// Read sessionID
				txApp2stateTable_req.write(tasi_writeMeta.sessionID); 					// Get session state
				txApp2txSar_upd_req.write(txAppTxSarQuery(tasi_writeMeta.sessionID));	// Get Ack pointer
				tai_state = READ_META;
			}
			break;
		case READ_META:
			if (!txSar2txApp_upd_rsp.empty() && !stateTable2txApp_rsp.empty()) {
				stateTable2txApp_rsp.read(state);
				txSar2txApp_upd_rsp.read(writeSar);
				maxWriteLength = (writeSar.ackd - writeSar.mempt) - 1;
#if (TCP_NODELAY)
				usedLength 		= ((ap_uint<16>) writeSar.mempt - writeSar.ackd);
				if (writeSar.min_window > usedLength) {
					usableWindow = writeSar.min_window - usedLength;
				}
				else {
					usableWindow 	= 0;
				}
#endif
				//std::cout << "READ_META writeSar.min_window " << std::dec << writeSar.min_window << "\tusedLength " << usedLength << "\tusable window "  << usableWindow << std::endl;
				if (state != ESTABLISHED) {
					tasi_writeToBufFifo.write(pkgPushMeta(true));	// drop data
					appTxDataRsp.write(appTxRsp(tasi_writeMeta.length, maxWriteLength, ERROR_NOCONNECTION)); // Notify app about fail
				}
#if (!TCP_NODELAY)
				else if(tasi_writeMeta.length > maxWriteLength) {
#else
				else if(tasi_writeMeta.length > maxWriteLength || usableWindow < tasi_writeMeta.length) {
#endif
					// Notify app about fail
					appTxDataRsp.write(appTxRsp(tasi_writeMeta.length, maxWriteLength, ERROR_NOSPACE));
				}	
				else {
					// TODO there seems some redundancy
					tasi_writeToBufFifo.write(pkgPushMeta(tasi_writeMeta.sessionID, writeSar.mempt, tasi_writeMeta.length));
					appTxDataRsp.write(appTxRsp(tasi_writeMeta.length, maxWriteLength, NO_ERROR));
					txAppStream2eventEng_setEvent.write(event(TX, tasi_writeMeta.sessionID, writeSar.mempt, tasi_writeMeta.length));
					txApp2txSar_upd_req.write(txAppTxSarQuery(tasi_writeMeta.sessionID, writeSar.mempt+tasi_writeMeta.length));
				}
				tai_state = READ_REQUEST;
			}
			break;
	} //switch
}


/** @ingroup tx_app_stream_if
 *  In case the @tasi_metaLoader decides to write the packet to the memory,
 *  it forward the data and it generates a command to the memory the command may produce
 *  buffer overflow, but the next module has to deal with it. Otherwise the data is discarded
 *  and no command is generated.
 */
void tasi_pkg_dropper (											// FIXME Unnecessary the application is intelligent enough to no send data
		stream<pkgPushMeta>&			tasi_writeToBufFifo,
		stream<axiWord>& 				tasi_dataIn,
		stream<axiWord>& 				tasi_dataOut,
		stream<mmCmd>&					txBufferWriteCmd){

#pragma HLS pipeline II=1
#pragma HLS INLINE off

	static pkgPushMeta 	tasi_pushMeta;
	static bool 		reading_data = false;
	ap_uint<32> 		pkgAddr;
	axiWord				currWord;

	// TODO: The app gets a notification if can write or not, probably this modules produces a stall 

	if (!tasi_writeToBufFifo.empty() && !reading_data){
		tasi_writeToBufFifo.read(tasi_pushMeta);
		//std::cout << "PACKAGE DROPPER waiting for data DROP " << (tasi_pushMeta.drop ? "YES": " NO")<< std::endl;

		if (!tasi_pushMeta.drop){
			pkgAddr(31, 30) = 0x01;
			pkgAddr(29, 16) = tasi_pushMeta.sessionID(13, 0);
			pkgAddr(15, 0)  = tasi_pushMeta.address;

			txBufferWriteCmd.write(mmCmd(pkgAddr, tasi_pushMeta.length));
		}

		reading_data = true;

	}
	else if(!tasi_dataIn.empty() && reading_data){
		tasi_dataIn.read(currWord);
		if (!tasi_pushMeta.drop){
			tasi_dataOut.write(currWord);
		}

		if (currWord.last){
			reading_data = false;
		}
	}

}

/** @ingroup tx_app_stream_if
 *  This application interface is used to transmit data streams of established connections.
 *  The application sends the Session-ID on through @p writeMetaDataIn and the data stream
 *  on @p writeDataIn. The interface checks then the state of the connection and loads the
 *  application pointer into the memory. It then writes the data into the memory. The application
 *  is notified through @p writeReturnStatusOut if the write to the buffer succeeded. In case
 *  of success the length of the write is returned, otherwise -1;
 *  @param[in]		appTxDataReqMetaData
 *  @param[in]		appTxDataReq
 *  @param[in]		stateTable2txApp_rsp
 *  @param[in]		txSar2txApp_upd_rsp
 *  @param[out]		appTxDataRsp
 *  @param[out]		txApp2stateTable_req
 *  @param[out]		txApp2txSar_upd_req
 *  @param[out]		txBufferWriteCmd
 *  @param[out]		txBufferWriteData
 *  @param[out]		txAppStream2eventEng_setEvent
 */
void tx_app_stream_if(	stream<appTxMeta>&				appTxDataReqMetaData,
						stream<axiWord>&				appTxDataReq,
						stream<sessionState>&			stateTable2txApp_rsp,
						stream<txAppTxSarReply>&		txSar2txApp_upd_rsp, //TODO rename
						stream<appTxRsp>&				appTxDataRsp,
						stream<ap_uint<16> >&			txApp2stateTable_req,
						stream<txAppTxSarQuery>&		txApp2txSar_upd_req, //TODO rename
						stream<mmCmd>&					txBufferWriteCmd,
						stream<axiWord>&				txBufferWriteData,
#if (TCP_NODELAY)
						stream<axiWord>&				txApp2txEng_data_stream,
#endif
						stream<event>&					txAppStream2eventEng_setEvent)
{
#pragma HLS INLINE

	static stream<pkgPushMeta> tasi_writeToBufFifo("tasi_writeToBufFifo");
	#pragma HLS stream variable=tasi_writeToBufFifo depth=32
	#pragma HLS DATA_PACK variable=tasi_writeToBufFifo

	static stream<axiWord> tasi_app_data_accepted("tasi_app_data_accepted");
	#pragma HLS DATA_PACK variable=tasi_app_data_accepted

	static stream<mmCmd> tasi_command_accepted("tasi_command_accepted");
	#pragma HLS DATA_PACK variable=tasi_command_accepted

	tasi_metaLoader(	
			appTxDataReqMetaData,
			stateTable2txApp_rsp,
			txSar2txApp_upd_rsp,
			appTxDataRsp,
			txApp2stateTable_req,
			txApp2txSar_upd_req,
			tasi_writeToBufFifo,
			txAppStream2eventEng_setEvent);

	tasi_pkg_dropper (
			tasi_writeToBufFifo,
			appTxDataReq,
			tasi_app_data_accepted,
			tasi_command_accepted);


	tx_Data_to_Memory(
			tasi_app_data_accepted,
			tasi_command_accepted,		// Command with potential overflow
			txBufferWriteCmd,			// Command without overflow
#if (TCP_NODELAY)					
			txApp2txEng_data_stream,
#endif		
			txBufferWriteData	
		);

}
