/************************************************
BSD 3-Clause License

Copyright (c) 2019, HPCN Group, UAM Spain (hpcn-uam.es)
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

#include "user_abstraction.hpp"


void manageActiveConnections ( 
                    stream<ipTuple>&                openConnection, 
                    stream<openStatus>&             openConStatus,
                    stream<ap_uint<16> >&           closeConnection,
                    stream<txApp_client_status>&    txAppNewClientNoty,
                    stream<ap_uint<16> >&           userID,
                    stream<ap_uint<16> >&           toeID,
                    userRegs&                       settings_regs)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum oc_states {WAIT_PULSE, ISSUE_REQUEST, WAIT_RESPONSE, CLOSE_CNN};
    static oc_states oc_fsm_state = WAIT_PULSE;
    static ap_uint< 1>  openConn_r = 1;
    static ap_uint< 1>  closeConn_r = 1;
    static ap_uint<16>  translateTable[MAX_SESSIONS];
//#pragma HLS DEPENDENCE variable=translateTable inter false
#pragma HLS RESOURCE variable=translateTable core=RAM_2P_BRAM
    
    openStatus              requestStatus;
    txApp_client_status     rx_client_notification;
    ap_uint<16>             currUserID;
    settings_regs.maxConnections = MAX_SESSIONS;

    switch (oc_fsm_state) {
        case WAIT_PULSE :
            if (!openConn_r && settings_regs.openConn)
                oc_fsm_state = ISSUE_REQUEST;
            else if (!closeConn_r && settings_regs.closeConn)
                oc_fsm_state = CLOSE_CNN;
            else if (!txAppNewClientNoty.empty()){
                txAppNewClientNoty.read(rx_client_notification);
                translateTable[rx_client_notification.sessionID] = rx_client_notification.sessionID;
            }
            else if (!userID.empty()){
                userID.read(currUserID);
                toeID.write(translateTable[currUserID]);
            }
            break;
        case ISSUE_REQUEST :
            openConnection.write(ipTuple(settings_regs.dstIP,settings_regs.dstPort));
            oc_fsm_state = WAIT_RESPONSE;
            break;
        case WAIT_RESPONSE :
            if (!openConStatus.empty()){
                openConStatus.read(requestStatus);
                if (requestStatus.success){
                    translateTable[settings_regs.connID] = requestStatus.sessionID;
                }
                oc_fsm_state = WAIT_PULSE;
            }
            break;
        case CLOSE_CNN:
            closeConnection.write(translateTable[settings_regs.connID]);
            oc_fsm_state = WAIT_PULSE;
            break;   
        default :
            oc_fsm_state = WAIT_PULSE;
            break;
    }
    openConn_r = settings_regs.openConn;
    closeConn_r = settings_regs.closeConn;

}
void managePasiveConnections(    
                stream<ap_uint<16> >&           listenPort, 
                stream<listenPortStatus>&       listenPortRes)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum op_states {OPEN_PORT, WAIT_RESPONSE, IDLE};
    static op_states op_fsm_state = OPEN_PORT;
#pragma HLS RESET variable=op_fsm_state

    enum consumeFsmStateType {GET_DATA_NOTY , WAIT_SESSION_ID, CONSUME};
    static consumeFsmStateType  serverFsmState = GET_DATA_NOTY;
    static ap_uint<16>      listen_port;
    
    listenPortStatus        listen_rsp;
    appNotification         notification;
    axiWord                 currWord;

    static ap_uint<16>      connectionID=0;

    switch (op_fsm_state){
        case OPEN_PORT:
            listen_port = 5001 + connectionID;
            listenPort.write(listen_port);              // Open port 5001
            //std::cout << "Request to listen to port: " << std::dec << listen_port << " has been issued." << std::endl;
            op_fsm_state = WAIT_RESPONSE;
            break;
        case WAIT_RESPONSE:
            if (!listenPortRes.empty()){
                listenPortRes.read(listen_rsp);
                if (listen_rsp.port_number == listen_port){
                    if (listen_rsp.open_successfully || listen_rsp.already_open) {
                        if (connectionID == MAX_SESSIONS-1){
                            op_fsm_state = IDLE;
                        }
                        else{
                            connectionID++;
                            op_fsm_state = OPEN_PORT;
                        }
                    }
                    else {
                        op_fsm_state = OPEN_PORT; // If the port was not opened successfully try again
                    }
                }
            }
            break;
        case IDLE:  // Stay here forever
            op_fsm_state = IDLE;
            break;                      
    }

}

void consumeTOEtoUser(  /*TOE interface */              
                        stream<appNotification>&        toe2appNotification, 
                        stream<appReadRequest>&         app2toeReadRequest,
                        stream<ap_uint<16> >&           toe2appIDsession, 
                        stream<axiWord>&                toe2RxData,
                        /*User interface*/
                        stream<axiWordUser>&            rxShell2Usr) {

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

        enum ctuStates {GET_DATA_NOTY, CONSUME_ID, CONSUME_DATA};
        static ctuStates ctuFSMstate = GET_DATA_NOTY;
        appNotification         notification;
        static ap_uint<16>      connID;
        axiWord                 currWord;
        
        switch (ctuFSMstate) {
        case GET_DATA_NOTY:
            if (!toe2appNotification.empty()){
                toe2appNotification.read(notification);

                if (notification.length != 0) {
                    app2toeReadRequest.write(appReadRequest(notification.sessionID, notification.length));
                }
                ctuFSMstate = CONSUME_ID;
            }
            break;
        case CONSUME_ID:
            if (!toe2appIDsession.empty()){
                toe2appIDsession.read(connID);
                ctuFSMstate = CONSUME_DATA;
            }
        case CONSUME_DATA:
            if (!toe2RxData.empty()) {
                toe2RxData.read(currWord);
                if (currWord.last) {
                    ctuFSMstate = GET_DATA_NOTY;
                }
                rxShell2Usr.write(axiWordUser(currWord.data,currWord.keep,currWord.last,connID));
            }
            break;
    }
}

void countSegmentBytes (
                        stream<axiWordUser>&            txUsr2Shell,
                        stream<axiWord>&                txUsrData_i,
                        stream<ap_uint<16> >&           userID,
                        stream<txMessageMetaData>&      usrMsgMetaData){
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off


    static ap_uint<11>  byteCounter = 0;
    static ap_uint<16>  connID;
    static bool         issueRequest = true;
    ap_uint<1>          last = 0;
    axiWordUser         currWord;

    if(!txUsr2Shell.empty()){
        txUsr2Shell.read(currWord);
        if (issueRequest){
            userID.write(currWord.user);
            connID  = currWord.user;
            issueRequest  = false;
        }
        if ((byteCounter >= (MAXIMUM_SEGMENT_SIZE-64) && ~currWord.last) || currWord.last){
            ap_uint<11> aux_sum = byteCounter + keep2len(currWord.keep);
            cout << "countSegmentBytes " << dec << aux_sum << endl;
            usrMsgMetaData.write(txMessageMetaData(aux_sum,connID));
            last = 1;
            byteCounter   = 0;
            issueRequest  = true;
        }
        else{
            byteCounter += 64;
        }

        txUsrData_i.write(axiWord(currWord.data,currWord.keep,last)); 
    }
}

void Abstraction2TOE (
                    stream<axiWord>&                txUsrData_i,
                    stream<txMessageMetaData>&      usrMsgMetaData,
                    stream<ap_uint<16> >&           toeID,
                    stream<appTxMeta>&              app2TOEReqMetaData,
                    stream<appTxRsp>&               TOE2appReqMetaDataRsp,
                    stream<axiWord>&                app2TOEData){
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum a2tStates {WAIT_USER_DATA, WAIT_TOE_RESPONSE, FORWARD_DATA, RETRY_SPACE};
    static a2tStates a2tFSMState = WAIT_USER_DATA;
    static txMessageMetaData userMsg;
    appTxRsp        toeSpaceResponse;
    axiWord         currWord;
    ap_uint<16>     currTOEUser;

    switch (a2tFSMState){
        case WAIT_USER_DATA:
            if (!usrMsgMetaData.empty() && !toeID.empty()){
                usrMsgMetaData.read(userMsg);
                toeID.read(currTOEUser);
                app2TOEReqMetaData.write(appTxMeta(currTOEUser,userMsg.bytes));
                a2tFSMState = WAIT_TOE_RESPONSE;
                cout << "Abstraction2TOE " << dec << userMsg.bytes << endl;
            }
            break;
        case WAIT_TOE_RESPONSE:
            if (!TOE2appReqMetaDataRsp.empty()){
                TOE2appReqMetaDataRsp.read(toeSpaceResponse);
                if(toeSpaceResponse.error==NO_ERROR)
                    a2tFSMState = FORWARD_DATA;
                else
                    a2tFSMState = RETRY_SPACE;
            }
            break;
        case FORWARD_DATA:
            if (!txUsrData_i.empty()){
                txUsrData_i.read(currWord);
                app2TOEData.write(currWord);
                if(currWord.last)
                    a2tFSMState = WAIT_USER_DATA;
            }
            break;
        case RETRY_SPACE:
            app2TOEReqMetaData.write(appTxMeta(userMsg.connID,userMsg.bytes));
            a2tFSMState = WAIT_TOE_RESPONSE;
            break;
        default:
            break;
    }
}

/**
 * @brief      Wrapper for user_abstraction. This piece of code abstract the complex interface
 *             between the user side and the TOE. The interface to the user are two AXI4-Stream
 *             one for receive and other for transmit stream. Each AXI4-Stream has a TDEST which 
 *             indicates the ID of the connection. 
 *
 * @param      listenPortReq            Request to open a port
 * @param      listenPortRes            Port open response
 * @param      rxAppNotification        Notification about new data
 * @param      rxApp_readRequest        Request data from the TOE
 * @param      rxApp_readRequest_RspID  Response ID for data requested
 * @param      rxData_to_rxApp          Data coming from the TOE
 * @param      txApp_openConnection     Open socket request
 * @param      txApp_openConnStatus     Open socket response
 * @param      closeConnection          Close a connection
 * @param      txAppDataReqMeta         Request to send data
 * @param      txAppDataReqStatus       Response for transmission request
 * @param      txAppData_to_TOE         Data from the app to TOE
 * @param      txAppNewClientNoty       Notification of new client
 * @param      rxShell2Usr              Receive data from the shell to the User
 * @param      txUsr2Shell              Transmit data from the user to the shell
 */

void user_abstraction( 
                    /*Interface with TOE*/
                    stream<ap_uint<16> >&           listenPortReq, 
                    stream<listenPortStatus>&       listenPortRes,
                    stream<appNotification>&        rxAppNotification, //*
                    stream<appReadRequest>&         rxApp_readRequest, //*
                    stream<ap_uint<16> >&           rxApp_readRequest_RspID, //*
                    stream<axiWord>&                rxData_to_rxApp, //*
                    stream<ipTuple>&                txApp_openConnection, 
                    stream<openStatus>&             txApp_openConnStatus,
                    stream<ap_uint<16> >&           closeConnection,
                    stream<appTxMeta>&              txAppDataReqMeta, //*
                    stream<appTxRsp>&               txAppDataReqStatus, //*
                    stream<axiWord>&                txAppData_to_TOE, //*
                    stream<txApp_client_status>&    txAppNewClientNoty, 
                    /* Interface with user */

                    stream<axiWordUser>&            rxShell2Usr,        //*
                    stream<axiWordUser>&            txUsr2Shell,        //*

                    /* AXI4-Lite registers */
                    userRegs&                       settings_regs){
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
#pragma HLS INTERFACE axis register both port=txAppDataReqStatus name=s_TxDataResponse
#pragma HLS INTERFACE axis register both port=txAppData_to_TOE name=m_TxPayload
#pragma HLS INTERFACE axis register both port=txAppNewClientNoty name=s_NewClientNoty
#pragma HLS INTERFACE axis register both port=rxShell2Usr
#pragma HLS INTERFACE axis register both port=txUsr2Shell

#pragma HLS DATA_PACK variable=listenPortRes
#pragma HLS DATA_PACK variable=rxAppNotification
#pragma HLS DATA_PACK variable=rxApp_readRequest
#pragma HLS DATA_PACK variable=txApp_openConnection
#pragma HLS DATA_PACK variable=txApp_openConnStatus
#pragma HLS DATA_PACK variable=txAppDataReqMeta
#pragma HLS DATA_PACK variable=txAppDataReqStatus
#pragma HLS DATA_PACK variable=txAppNewClientNoty

#pragma HLS INTERFACE s_axilite port=settings_regs bundle=settings

    static stream<axiWord> txUsrData_i("txUsrData_i");
    #pragma HLS STREAM variable=txUsrData_i depth=512
    #pragma HLS DATA_PACK variable=txUsrData_i
    
    static stream<ap_uint<16> > userID_i("userID_i");
    #pragma HLS STREAM variable=userID_i depth=16

    static stream<ap_uint<16> > toeID_i("toeID_i");
    #pragma HLS STREAM variable=toeID_i depth=16    

    static stream<txMessageMetaData> usrMsgMetaData_i("usrMsgMetaData_i");
    #pragma HLS STREAM variable=usrMsgMetaData_i depth=16
    #pragma HLS DATA_PACK variable=usrMsgMetaData_i

    consumeTOEtoUser(  /*TOE interface */              
                        rxAppNotification, 
                        rxApp_readRequest,
                        rxApp_readRequest_RspID, 
                        rxData_to_rxApp,
                        /*User interface*/
                        rxShell2Usr);

    countSegmentBytes (
                        txUsr2Shell,
                        txUsrData_i,
                        userID_i,
                        usrMsgMetaData_i);

    Abstraction2TOE (
                        txUsrData_i,
                        usrMsgMetaData_i,
                        toeID_i,
                        txAppDataReqMeta,
                        txAppDataReqStatus,
                        txAppData_to_TOE);

    managePasiveConnections(    
                        listenPortReq, 
                        listenPortRes);

    manageActiveConnections ( 
                        txApp_openConnection, 
                        txApp_openConnStatus,
                        closeConnection,
                        txAppNewClientNoty,
                        userID_i,
                        toeID_i,
                        settings_regs);
}
