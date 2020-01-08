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

#include "echo_server_application.hpp"

void open_port(	
		stream<ap_uint<16> >& 		listenPortReq, 
		stream<listenPortStatus>&	listenPortRes
		)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	enum esa_openPort_states {OPEN_PORT, WAIT_RESPONSE, IDLE};
	static esa_openPort_states openPort_fsm = OPEN_PORT;
	#pragma HLS RESET variable=openPort_fsm

	listenPortStatus 		listen_rsp ;

	static ap_uint<16>		listen_port;

	switch (openPort_fsm){
		/* Open/Listen on Port at start-up */
		case OPEN_PORT:
			listen_port = 15000;
			listenPortReq.write(listen_port);
			//cout << "Request to listen to port: " << dec << listen_port << " has been issued." << endl;
			openPort_fsm = WAIT_RESPONSE;
			break;
		
		/* Check if listening on Port was successful, otherwise try again*/
		case WAIT_RESPONSE :
			if (!listenPortRes.empty()) {
				listenPortRes.read(listen_rsp);

				//cout << "Port: " << dec << listen_rsp.port_number << " has been opened successfully " << ((listen_rsp.open_successfully) ? "Yes." : "No.");
				//cout << "\twrong port number " << ((listen_rsp.wrong_port_number) ? "Yes." : "No.");
				//cout << "\tthe port is already open " << ((listen_rsp.already_open) ? "Yes." : "No.") << endl;
				if (listen_rsp.port_number == listen_port){
					if (listen_rsp.open_successfully || listen_rsp.already_open) {
						openPort_fsm = IDLE;
					}
					else {
						openPort_fsm = OPEN_PORT; // If the port was not opened successfully try again
					}
				}
			}
			break;
		case IDLE:
			/* Stay here for ever */
			openPort_fsm = IDLE;
		break;	

	}
}

void dummy(	
		stream<ipTuple>& 			txApp_openConnection, 
		stream<openStatus>& 		txApp_openConnStatus,
		stream<ap_uint<16> >& 		closeConnection)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	openStatus newConn;
	ipTuple tuple;

	// Dummy code should never be executed, this is necessary because every streams has to be written/read
	if (!txApp_openConnStatus.empty()) {
		txApp_openConnStatus.read(newConn);
		tuple.ip_address 	= 0xC0A80008;  // 192.168.0.8
		tuple.ip_port 		= 13330;
		txApp_openConnection.write(tuple);
		if (newConn.success) {
			closeConnection.write(newConn.sessionID);
			//closePort.write(tuple.ip_port);
		}
		//cout << "Connection: " << dec << newConn.sessionID << " successfully opened: " << (newConn.success ? "Yes" : "No") << endl;
	}

}

void replyData(	
		stream<es_metaData>& 		transferMetaData,
		stream<axiWord>& 			DataIn,
		stream<appTxRsp>& 			txAppDataReqStatus,
		stream<appTxMeta>& 			txAppDataReqMeta, 
		stream<axiWord>& 			DataOut)
{
#pragma HLS INLINE off
#pragma HLS PIPELINE II=1

	enum esrd_states {READ_META , WAIT_RESPONSE, FORWARD_DATA, WAIT_CYCLES};
	static esrd_states 		rd_fsm_state = READ_META;
	static ap_uint<8>		cycle_counter;

	static es_metaData 			metaData;
	appTxRsp 					write_request_response;
	axiWord 					currWord;
	static int response_counter = 0; // deleteme

	switch (rd_fsm_state) {
		case READ_META:
			if (!transferMetaData.empty()) {
				transferMetaData.read(metaData);
				txAppDataReqMeta.write(appTxMeta(metaData.sessionID, metaData.length));
				rd_fsm_state = WAIT_RESPONSE;
			}
			break;
		case WAIT_RESPONSE:
			if (!txAppDataReqStatus.empty()){
				txAppDataReqStatus.read(write_request_response);
				//cout << "Response for transfer " << setw(5) << dec << response_counter << " length : " << dec << write_request_response.length;
				//cout << "\tremaining space: " << write_request_response.remaining_space << "\terror: " <<  write_request_response.error;
				//cout << "\ttime: "  << simCycleCounter << endl << endl;

				if (write_request_response.error != 0){
					cycle_counter 	= 0;
					rd_fsm_state 	= WAIT_CYCLES;
					//cout << endl << endl;
				}
				else{
					response_counter++;
					rd_fsm_state 	= FORWARD_DATA;
				}
			}
			break;	
		case FORWARD_DATA:
			if (!DataIn.empty()) {
				DataIn.read(currWord);
				DataOut.write(currWord);
				if (currWord.last) {
					rd_fsm_state = READ_META;
				}
			}
			break;
		case WAIT_CYCLES:
			cycle_counter++;
			if (cycle_counter == 100){
				txAppDataReqMeta.write(appTxMeta(metaData.sessionID, metaData.length));	// issue writing command again and wait for response
				rd_fsm_state = WAIT_RESPONSE;
			}
			break;	
	}
}


void consumeRxData(
		stream<txApp_client_status>& 	txAppNewClientNoty,
		stream<ap_uint<16> >& 			rxApp_readRequest_RspID,
		stream<appNotification>& 		rxAppNotification,
		stream<axiWord>& 				DataIn,
		stream<es_metaData>& 			transferMetaData,
		stream<axiWord>& 				dataFifo,
		stream<appReadRequest>& 		rxApp_readRequest)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	// Reads new data from memory and writes it into fifo
	// Read & write metadata only once per package
	enum crd_states {GET_DATA_NOTY , WAIT_SESSION_ID, FORWARD_DATA};
	static crd_states fsm_state = GET_DATA_NOTY;
	static appNotification 		notification;
	es_metaData					metaData;

	ap_uint<16> sessionID;
	axiWord currWord;
	txApp_client_status rx_client_notification;

	static int noty_num = 0; // deleteme

	if (!txAppNewClientNoty.empty()){
		txAppNewClientNoty.read(rx_client_notification);
		//cout << "replyData session ID: " << dec << rx_client_notification.sessionID;
		//cout << "\t\tBuffer size: " << (rx_client_notification.buffersize+1)*65536;
		//cout << "\t\t max transfer size: " << rx_client_notification.max_transfer_size;
		//cout << "\t\tTCP_NODELAY: " << (rx_client_notification.tcp_nodelay ? "Yes" : "No");
		//cout << "\t\tBuffer empty: " << (rx_client_notification.buffer_empty ? "Yes" : "No") << endl;
	}



	switch (fsm_state) {
		/* Receive rxAppNotification, about new data which is available	*/
		case GET_DATA_NOTY:
			if (!rxAppNotification.empty()) {
				rxAppNotification.read(notification);
				//cout << "Notification [" << setw(4) << noty_num << "] ID: " << dec << notification.sessionID << "\tIP: ";
				//cout << ((notification.ipAddress >> 24) & 0xff) << "." << ((notification.ipAddress >> 16) & 0xff);
				//cout << "." << ((notification.ipAddress >> 8) & 0xff) << "." << (notification.ipAddress  & 0xff);
				//cout << "\t\tdst port: " << notification.dstPort;
				//cout << "\t\tlength: " << notification.length;
				//cout << "\t\tclosed: " << (notification.closed ? "Yes" : "No") << endl;
				if (notification.length != 0) {
					rxApp_readRequest.write(appReadRequest(notification.sessionID, notification.length));
					fsm_state = WAIT_SESSION_ID;
				}
			}
			break;	
		case WAIT_SESSION_ID:
			if (!rxApp_readRequest_RspID.empty()) {
				rxApp_readRequest_RspID.read(sessionID);

				metaData.sessionID = sessionID;
				metaData.length = notification.length;

				//cout << "Got a notification [" << setw(4) << noty_num  << "] of ID: " << dec << sessionID << endl << endl;

				transferMetaData.write(metaData);
				noty_num++;
				fsm_state = FORWARD_DATA;
			}
			break;
		case FORWARD_DATA:
			if (!DataIn.empty()) {
				DataIn.read(currWord);
				dataFifo.write(currWord);
				if (currWord.last) {
					fsm_state = GET_DATA_NOTY;
				}
			}
			break;
		}

}

void sinker(
		stream<txApp_client_status>& 	txAppNewClientNoty,
		stream<ap_uint<16> >& 			rxApp_readRequest_RspID,
		stream<appNotification>& 		rxAppNotification,
		stream<axiWord>& 				DataIn,
		stream<es_metaData>& 			transferMetaData,
		stream<axiWord>& 				dataFifo,
		stream<appReadRequest>& 		rxApp_readRequest)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	// Reads new data from memory and writes it into fifo
	// Read & write metadata only once per package
	enum s_states {GET_DATA_NOTY, WAIT_READ_RESPONSE, SINK_DATA};
	static s_states fsm_state = GET_DATA_NOTY;
	static appNotification 		notification;

	ap_uint<16> 				sessionID;
	axiWord 					currWord;
	txApp_client_status 		rx_client_notification;

	static int 					noty_num = 0; // deleteme

	if (!txAppNewClientNoty.empty()){
		txAppNewClientNoty.read(rx_client_notification);
		//cout << "replyData session ID: " << dec << rx_client_notification.sessionID;
		//cout << "\t\tBuffer size: " << (rx_client_notification.buffersize+1)*65536;
		//cout << "\t\t max transfer size: " << rx_client_notification.max_transfer_size;
		//cout << "\t\tTCP_NODELAY: " << (rx_client_notification.tcp_nodelay ? "Yes" : "No");
		//cout << "\t\tBuffer empty: " << (rx_client_notification.buffer_empty ? "Yes" : "No") << endl;
	}



	switch (fsm_state) {
		/* Receive rxAppNotification, about new data which is available	*/
		case GET_DATA_NOTY:
			/*if (!rxAppNotification.empty()) {
				rxAppNotification.read(notification);
				cout << "Notification [" << setw(4) << noty_num << "] ID: " << dec << notification.sessionID << "\tIP: ";
				cout << ((notification.ipAddress >> 24) & 0xff) << "." << ((notification.ipAddress >> 16) & 0xff);
				cout << "." << ((notification.ipAddress >> 8) & 0xff) << "." << (notification.ipAddress  & 0xff);
				cout << "\t\tdst port: " << notification.dstPort;
				cout << "\t\tlength: " << notification.length;
				cout << "\t\tclosed: " << (notification.closed ? "Yes" : "No") << endl;
				//if (notification.length != 0) {
				//	rxApp_readRequest.write(appReadRequest(notification.sessionID, notification.length));
				//	fsm_state = WAIT_READ_RESPONSE;
				//}
			}*/
			break;	
		case WAIT_READ_RESPONSE:
			if (!rxApp_readRequest_RspID.empty()) {
				rxApp_readRequest_RspID.read(sessionID);
				//cout << "Got a notification [" << setw(4) << noty_num  << "] of ID: " << dec << sessionID << endl << endl;
				noty_num++;
				fsm_state = SINK_DATA;
				fsm_state = GET_DATA_NOTY;
			}
			break;			
		case SINK_DATA:
			if (!DataIn.empty()) {
				DataIn.read(currWord);

				if (currWord.last) {
					fsm_state = GET_DATA_NOTY;
				}
			}
			break;
		}

}

/** @ingroup    kvs_server
 *
 * @brief      Wrapper for the rest of the function
 *
 * @param      listenPortReq  				Notify the application about the port to listen to
 * @param      listenPortRes  				Notification from the TOE to the app about the listen port request
 * @param      rxAppNotification      		Notify the rx application
 * @param      rxApp_readRequest      		Request data from the memory
 * @param      rxApp_readRequest_RspID      The receive meta data
 * @param      rxData_to_rxApp              The receive data
 * @param      txApp_openConnection         The open connection
 * @param      txApp_openConnStatus          The open con status
 * @param      closeConnection        		The close connection
 * @param      txAppDataReqMeta             The transmit meta data
 * @param      txAppData_to_TOE             The transmit data
 * @param      txAppDataReqStatus           The transmit status
 */
void echo_server_application(	
			stream<ap_uint<16> >& 			listenPortReq, 
			stream<listenPortStatus>&		listenPortRes,
			stream<appNotification>& 		rxAppNotification, 
			stream<appReadRequest>& 		rxApp_readRequest,
			stream<ap_uint<16> >& 			rxApp_readRequest_RspID, 
			stream<axiWord>& 				rxData_to_rxApp,
			stream<ipTuple>& 				txApp_openConnection, 
			stream<openStatus>& 			txApp_openConnStatus,
			stream<ap_uint<16> >& 			closeConnection,
			stream<appTxMeta>& 				txAppDataReqMeta,
			stream<axiWord> & 				txAppData_to_TOE,
			stream<appTxRsp>& 				txAppDataReqStatus,
			stream<txApp_client_status>& 	txAppNewClientNoty)
{
//#pragma HLS INTERFACE ap_ctrl_none register port=return

#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return

#pragma HLS INTERFACE axis register both port=listenPortReq name=m_ListenPortRequest
#pragma HLS INTERFACE axis register both port=listenPortRes name=s_ListenPortResponse
#pragma HLS INTERFACE axis register both port=rxAppNotification name=s_RxAppNoty
#pragma HLS INTERFACE axis register both port=rxApp_readRequest name=m_App2RxEngRequestData
#pragma HLS INTERFACE axis register both port=rxApp_readRequest_RspID name=s_App2RxEngResponseID
#pragma HLS INTERFACE axis register both port=rxData_to_rxApp name=s_RxRequestedData
#pragma HLS INTERFACE axis register both port=txApp_openConnection name=m_OpenConnRequest
#pragma HLS INTERFACE axis register both port=txApp_openConnStatus name=s_OpenConnResponse
#pragma HLS INTERFACE axis register both port=closeConnection name=m_CloseConnRequest
#pragma HLS INTERFACE axis register both port=txAppDataReqMeta name=m_TxDataRequest
#pragma HLS INTERFACE axis register both port=txAppData_to_TOE name=m_TxPayload
#pragma HLS INTERFACE axis register both port=txAppDataReqStatus name=s_TxDataResponse
#pragma HLS INTERFACE axis register both port=txAppNewClientNoty name=s_NewClientNoty

#pragma HLS DATA_PACK variable=listenPortRes
#pragma HLS DATA_PACK variable=rxAppNotification
#pragma HLS DATA_PACK variable=rxApp_readRequest
#pragma HLS DATA_PACK variable=txApp_openConnection
#pragma HLS DATA_PACK variable=txApp_openConnStatus
#pragma HLS DATA_PACK variable=txAppDataReqMeta
#pragma HLS DATA_PACK variable=txAppDataReqStatus
#pragma HLS DATA_PACK variable=txAppNewClientNoty	


	static stream<es_metaData>		transferMetaData("transferMetaData");
#pragma HLS stream variable=transferMetaData depth=64
#pragma HLS DATA_PACK variable=transferMetaData

	static stream<axiWord>			esa_dataFifo("esa_dataFifo");
#pragma HLS stream variable=esa_dataFifo depth=512
#pragma HLS DATA_PACK variable=esa_dataFifo


	open_port(
		listenPortReq, 
		listenPortRes);

	replyData(
		transferMetaData, 
		esa_dataFifo,
		txAppDataReqStatus,
		txAppDataReqMeta, 
		txAppData_to_TOE);
	
	consumeRxData(
	//sinker(
		txAppNewClientNoty,
		rxApp_readRequest_RspID, 
		rxAppNotification,
		rxData_to_rxApp,
		transferMetaData, 
		esa_dataFifo,
		rxApp_readRequest);

	dummy(
		txApp_openConnection, 
		txApp_openConnStatus, 
		closeConnection);

}
