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

#include "iperf_client.hpp"

void client_r(    
                stream<ipTuple>&            openConnection, 
                stream<openStatus>&         openConStatus,
                stream<ap_uint<16> >&       closeConnection,
                stream<appTxMeta>&          txMetaData,
                stream<appTxRsp>&           txStatus,
                stream<axiWord>&            txData,
                stream<ap_uint<64> >&       stopWatchStart,
                stream<bool >&              stopWatchStop,
                iperf_regs&                 settings_regs)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum iperfFsmStateType {WAIT_USER_START, INIT_CON, WAIT_CON, REQ_WRITE_FIRST, INIT_RUN, COMPUTE_NECESSARY_SPACE,
                            SPACE_RESPONSE, SPACE_RESPONSE_1, SEND_PACKET, REQUEST_SPACE, WAIT_TIME, CLOSE_CONN};
    static iperfFsmStateType    iperfFsmState = WAIT_USER_START;
    static ap_uint<16>          experimentID[MAX_SESSIONS];

    #pragma HLS RESOURCE variable=experimentID core=RAM_2P_LUTRAM
    #pragma HLS DEPENDENCE variable=experimentID inter false

    static ap_uint<14>          sessionIt       = 0;

    static ap_uint<32>          bytes_already_sent;
    static ap_uint< 6>          bytes_last_word;
    static ap_uint<10>          transactions;
    static ap_uint<10>          wordSentCount;
    static ap_uint<16>          transaction_length;
    static ap_uint<16>          waitCounter;
    

    static ap_uint<32>          transfer_size_r;
    static ap_uint<16>          packet_mss_r;
    static ap_uint<14>          numConnections_r;
    static ap_uint<1>           runExperiment_r = 1;
    static ap_uint<16>          dstPort_r;
    static ap_uint<32>          ipDestination_r;
    static ap_uint<1>           useTimer_r;

    static ap_uint<1>           stopWatchEnd     = 0;

    ipTuple                     openTuple;
    openStatus                  status;
    appTxMeta                   meta_i;
    ap_uint<32>                 remaining_bytes_to_send;
    appTxRsp                    spaceReplay;

    axiWord                     currWord;

    settings_regs.maxConnections = MAX_SESSIONS;

    /*
     * CLIENT FSM
     */

    switch (iperfFsmState) {
        /* Wait until user star the client. Once the starting is detect all settings are registered*/
        case WAIT_USER_START:
            
            sessionIt   = 0;
            if (settings_regs.runExperiment && !runExperiment_r) {    // Rising edge 
                std::cout << "runExperiment set " << std::endl;  
                if ((settings_regs.numConnections > 0) && (settings_regs.numConnections <= MAX_SESSIONS)) {  // Start experiment only if the user specifies a valid number of connection
                    numConnections_r    = settings_regs.numConnections;
                    transfer_size_r     = settings_regs.transfer_size;
                    useTimer_r          = settings_regs.useTimer;
                    ipDestination_r     = settings_regs.ipDestination;
                    dstPort_r           = settings_regs.dstPort;
                    packet_mss_r        = settings_regs.packet_mss;       // Register input variables
                    if (settings_regs.useTimer) {
                        stopWatchStart.write(settings_regs.runTime);        // Start stopwatch
                    }
                    iperfFsmState       = INIT_CON;
                }
            }
            break;
        case INIT_CON:
           // Open as many connection as the user wants
            openTuple.ip_address = ipDestination_r;            // Conform the socket
            openTuple.ip_port    = dstPort_r + sessionIt;      // For the time being all the connections are done to the same machine and a incremental port number     
            openConnection.write(openTuple);

            cout << "IPERF request to establish a new connection socket " << dec << ((openTuple.ip_address) >> 24 & 0xFF) <<".";
            cout << ((openTuple.ip_address >> 16) & 0xFF) << "." << ((openTuple.ip_address >> 8) & 0xFF) << ".";
            cout << (openTuple.ip_address & 0xFF) << ":" << openTuple.ip_port << endl;

            sessionIt++;
            if (sessionIt == numConnections_r) {
                sessionIt = 0;
                iperfFsmState = WAIT_CON;
                if (useTimer_r) {               // If the timer is going to be used start timer
                    std::cout << "IPERF2 Init timer at " << std::dec << simCycleCounter << std::endl;
                }
            }
            break;
        case WAIT_CON:

            if (!openConStatus.empty()) {
                openConStatus.read(status);
                if (status.success) {
                    experimentID[sessionIt] = status.sessionID;
                    std::cout << "Connection successfully opened." << std::endl;
                }
                else {
                    std::cout << "Connection could not be opened." << std::endl;
                }
                sessionIt++;
                if (sessionIt == numConnections_r) { //maybe move outside
                    sessionIt = 0;
                    iperfFsmState = REQ_WRITE_FIRST;
                }

                cout << " read status" << endl;
            }

            cout << endl;

            break;


        /* Request space and send the first packet per every connection */    
        case REQ_WRITE_FIRST:

            meta_i.sessionID = experimentID[sessionIt];
            if (useTimer_r) {
                //meta_i.length    = packet_mss_r;
                meta_i.length    = 64;
            }
            else {
                meta_i.length    = 24;
            }
            txMetaData.write(meta_i);
            iperfFsmState = INIT_RUN;
            break; 

        case INIT_RUN:

            if (!txStatus.empty()){
                txStatus.read(spaceReplay);

                if (spaceReplay.error==0){
                    //std::cout << "Response for transfer " /*<< std::setw(5)*/ << std::dec << sessionIt << " length : " << spaceReplay.length;
                    //std::cout << "\tremaining space: " << spaceReplay.remaining_space << "\terror: " <<  spaceReplay.error << std::endl;
                    
                    if (useTimer_r) {
                        currWord.data( 63,  0) = 0x3736353400000000;
                        currWord.data( 79, 64) = 0x3938;
                        currWord.data(511, 80) = 0;
                        currWord.keep          = 0xFFFFFFFFFFFFFFFF;
                        currWord.last          = 1;
                    }
                    else {
                        if (settings_regs.dualModeEn) {
                            currWord.data( 63,  0) = 0x0100000001000080;            //run now
                        }
                        else {
                            currWord.data( 63,  0) = 0x0100000000000000;
                        }
                        currWord.data(127, 64) = 0x0000000089130000;
                        currWord.data(159,128) = 0x39383736;
                        currWord.data(191,160) = byteSwap32(transfer_size_r);      // transfer size

                        currWord.data(511,192) = 0;
                        currWord.keep = 0xffffff;
                        currWord.last = 1;
                    }    
                    
                    sessionIt++;
                    
                    if (sessionIt == numConnections_r) {
                        sessionIt = 0;
                        bytes_already_sent = 0;
                        iperfFsmState = COMPUTE_NECESSARY_SPACE;
                    }
                    else {
                        iperfFsmState = REQ_WRITE_FIRST;
                    }
                    txData.write(currWord);
                }
                else {
                    iperfFsmState = REQ_WRITE_FIRST;
                }

            }
            break;

        case COMPUTE_NECESSARY_SPACE:   
            meta_i.sessionID = experimentID[sessionIt];

            if (useTimer_r) {                   // If the timer is being used send the maximum packet
                if (stopWatchEnd) {             // When time is over finish
                    remaining_bytes_to_send = 0;
                }
                else {
                    remaining_bytes_to_send = packet_mss_r;
                }
            }
            else {
                remaining_bytes_to_send = transfer_size_r - bytes_already_sent;
            }

            //std::cout << "COMPUTE_NECESSARY_SPACE remaining_bytes_to_send:  " << std::dec << remaining_bytes_to_send;

            if (remaining_bytes_to_send > 0){
                if (remaining_bytes_to_send >= packet_mss_r){           // Check if we can send a packet
                    
                    bytes_last_word     = packet_mss_r(5,0);            // How many bytes are necessary for the last transaction
                    if (packet_mss_r(5,0) == 0){                        // compute how many transactions are necessary
                        transactions        =  packet_mss_r(15,6);  
                    }
                    else {
                        transactions        =  packet_mss_r(15,6) + 1;  
                    }
                    meta_i.length    = packet_mss_r;
                }
                else {
                    
                    bytes_last_word     = remaining_bytes_to_send(5,0);            // How many bytes are necessary for the last transaction
                    if (remaining_bytes_to_send(5,0) == 0){                        // compute how many transactions are necessary
                        transactions        =  remaining_bytes_to_send(15,6);  
                    }
                    else {
                        transactions        =  remaining_bytes_to_send(15,6) + 1;  
                    }
                    meta_i.length    = remaining_bytes_to_send;
                }
                sessionIt     = 0;
                txMetaData.write(meta_i);
                iperfFsmState = SPACE_RESPONSE;
            }
            else {
                sessionIt     = 0;
                iperfFsmState = CLOSE_CONN;
            }


            //std::cout << "\ttransactions: " << transactions << "\tbytes_last_word: " << bytes_last_word << std::endl;

            transaction_length = meta_i.length;
            wordSentCount = 1;

            break; 

        case SPACE_RESPONSE:    
            if (!txStatus.empty()){
                txStatus.read(spaceReplay);

                //std::cout << "SPACE_RESPONSE Response for transfer " /*<< std::setw(5)*/ << std::dec << sessionIt << " length : " << spaceReplay.length;
                //std::cout << "\tremaining space: " << spaceReplay.remaining_space << "\terror: " <<  spaceReplay.error << std::endl << std::endl;
                
                if (spaceReplay.error==0){
                    sessionIt++;
                    iperfFsmState = SEND_PACKET;
                    bytes_already_sent = bytes_already_sent + transaction_length;
                }
                else {
                    waitCounter = 0;
                    iperfFsmState = WAIT_TIME;
                }
            }
            break;

        case SEND_PACKET:

            currWord.data(63 ,  0) = 0x6d61752d6e637068;
            currWord.data(127, 64) = 0x202020202073652e;
            currWord.data(191,128) = 0x2e736d6574737973;
            currWord.data(255,192) = 0x2068632e7a687465;
            currWord.data(319,256) = 0x3736353433323130;
            currWord.data(383,320) = 0x3736353433323130;
            currWord.data(447,384) = 0x3736353433323130;
            currWord.data(511,448) = 0x3736353433323130;    // Dummy data


            if (wordSentCount == transactions){
                currWord.keep = len2Keep(bytes_last_word);
                currWord.last = 1;            
                if (sessionIt == numConnections_r){
                    iperfFsmState = COMPUTE_NECESSARY_SPACE;
                }
                else {
                    iperfFsmState = REQUEST_SPACE;
                }
            }
            else {
                currWord.keep = 0xFFFFFFFFFFFFFFFF;
                currWord.last = 0;
            }
            
            wordSentCount++;

            txData.write(currWord);
            break;
 
        case REQUEST_SPACE:
            meta_i.sessionID = experimentID[sessionIt];
            meta_i.length    = transaction_length;
            txMetaData.write(meta_i);
            iperfFsmState = SPACE_RESPONSE_1;
            break;

        case SPACE_RESPONSE_1:    
            if (!txStatus.empty()){
                txStatus.read(spaceReplay);

                //std::cout << "SPACE_RESPONSE_1 Response for transfer " /*<< std::setw(5)*/ << std::dec << sessionIt << " length : " << spaceReplay.length;
                //std::cout << "\tremaining space: " << spaceReplay.remaining_space << "\terror: " <<  spaceReplay.error << std::endl << std::endl;
                
                if (spaceReplay.error==0){
                    sessionIt++;
                    iperfFsmState = SEND_PACKET;
                    bytes_already_sent = bytes_already_sent + transaction_length;
                }
                else {
                    iperfFsmState = REQUEST_SPACE;
                }
            }
            break;            

        case WAIT_TIME:
            
            if (waitCounter == 1)
                iperfFsmState = COMPUTE_NECESSARY_SPACE;

            waitCounter++;

            break;    
        case CLOSE_CONN:
            //std::cout << std::endl << std::endl << "CLOSE_CONN closing connection" << std::endl << std::endl;
            closeConnection.write(experimentID[sessionIt]);
            sessionIt++;
            if (sessionIt == numConnections_r){
                iperfFsmState = WAIT_USER_START;
            }

            break;    
    }

    runExperiment_r = settings_regs.runExperiment;                // Register run Experiment


    if(!stopWatchStop.empty()){
        stopWatchStop.read();
        stopWatchEnd = 1;
    }

}

void client(    
                stream<ipTuple>&            openConnection, 
                stream<openStatus>&         openConStatus,
                stream<ap_uint<16> >&       closeConnection,
                stream<appTxMeta>&          txMetaData,
                stream<appTxRsp>&           txStatus,
                stream<axiWord>&            txData,
                stream<ap_uint<64> >&       stopWatchStart,
                stream<bool >&              stopWatchStop,
                iperf_regs&                 settings_regs)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum iperfFsmStateType {WAIT_USER_START, INIT_CON, WAIT_CON, REQ_WRITE_FIRST, INIT_RUN, COMPUTE_NECESSARY_SPACE,
                            SPACE_RESPONSE, SEND_PACKET, REQUEST_SPACE, CLOSE_CONN, CLOSE_CONN1, ERROR_OPENING_CONNECTION};

    static iperfFsmStateType    iperfFsmState = WAIT_USER_START;
    static ap_uint<64>          last_transfer_keep;
    static ap_uint<32>          bytes_already_sent;
    static ap_uint<32>          transfer_size_r;
    static ap_uint<32>          ipDestination_r;
    static ap_uint<16>          experimentID[MAX_SESSIONS];
    static ap_uint<16>          packet_mss_r;
    static ap_uint<16>          dstPort_r;
    static ap_uint<16>          transaction_length;
    static ap_uint<16>          waitCounter = 0;
    static ap_uint<14>          numConnections_r;
    static ap_uint<14>          sessionIt       = 0;
    static ap_uint<10>          transactions;
    static ap_uint<10>          wordSentCount;
    static ap_uint< 6>          bytes_last_word;
    static ap_uint< 1>          runExperiment_r = 1;
    static ap_uint< 1>          useTimer_r;
    static ap_uint< 1>          errorOpeningConnection = 0;
    static ap_uint< 1>          stopWatchEnd     = 0;

    #pragma HLS DEPENDENCE variable=last_transfer_keep inter false


    ipTuple                     openTuple;
    openStatus                  status;
    appTxMeta                   meta_i;
    ap_uint<32>                 remaining_bytes_to_send;
    appTxRsp                    spaceReplay;

    axiWord                     currWord;

    settings_regs.maxConnections = MAX_SESSIONS;
    settings_regs.currentState   = iperfFsmState;
    settings_regs.errorOpenningConnection   = errorOpeningConnection;

    /*
     * CLIENT FSM
     */

    switch (iperfFsmState) {
        /* Wait until user star the client. Once the starting is detect all settings are registered*/
        case WAIT_USER_START:
            
            if (settings_regs.runExperiment && !runExperiment_r) {    // Rising edge 
                std::cout << "runExperiment set " << std::endl;  
                if ((settings_regs.numConnections > 0) && (settings_regs.numConnections <= MAX_SESSIONS)) {  // Start experiment only if the user specifies a valid number of connection
                    numConnections_r    = settings_regs.numConnections;
                    transfer_size_r     = settings_regs.transfer_size;
                    useTimer_r          = settings_regs.useTimer;
                    ipDestination_r     = settings_regs.ipDestination;
                    dstPort_r           = settings_regs.dstPort;
                    packet_mss_r        = settings_regs.packet_mss;       // Register input variables
                    errorOpeningConnection = 0;
                    stopWatchEnd        = 0;
                    if (settings_regs.useTimer) {
                        stopWatchStart.write(settings_regs.runTime);        // Start stopwatch
                    }
                    iperfFsmState       = INIT_CON;
                }
            }
            //runExperiment_r = settings_regs.runExperiment;                // Register run Experiment
            break;
        case INIT_CON:
           // Open as many connection as the user wants
            openTuple.ip_address = ipDestination_r;            // Conform the socket
            openTuple.ip_port    = dstPort_r + sessionIt;      // For the time being all the connections are done to the same machine and a incremental port number
            openConnection.write(openTuple);

            cout << "IPERF request to establish a new connection socket " << dec << ((openTuple.ip_address) >> 24 & 0xFF) <<".";
            cout << ((openTuple.ip_address >> 16) & 0xFF) << "." << ((openTuple.ip_address >> 8) & 0xFF) << ".";
            cout << (openTuple.ip_address & 0xFF) << ":" << openTuple.ip_port << endl;

            sessionIt++;
            if (sessionIt == numConnections_r) {
                sessionIt = 0;
                iperfFsmState = WAIT_CON;
                if (useTimer_r) {               // If the timer is going to be used start timer
                    std::cout << "IPERF2 Init timer at " << std::dec << simCycleCounter << std::endl;
                }
            }
            break;
        case WAIT_CON:

            if (!openConStatus.empty()) {
                openConStatus.read(status);
                if (status.success) {
                    experimentID[sessionIt] = status.sessionID;
                    sessionIt++;
                    if (sessionIt == numConnections_r) { //maybe move outside
                        sessionIt = 0;
                        iperfFsmState = REQ_WRITE_FIRST;
                    }
                    std::cout << "Connection successfully opened." << std::endl;
                }
                else {
                    std::cout << "Connection could not be opened." << std::endl;
                    iperfFsmState = ERROR_OPENING_CONNECTION;
                }
            }
            break;

        /* Request space and send the first packet per every connection */    
        case REQ_WRITE_FIRST:

            meta_i.sessionID = experimentID[sessionIt];
            if (useTimer_r) {
                //meta_i.length    = packet_mss_r;
                meta_i.length    = 64;
            }
            else {
                meta_i.length    = 24;
            }
            txMetaData.write(meta_i);
            sessionIt++;
            if (stopWatchEnd) {
                iperfFsmState = CLOSE_CONN;
            }
            else if (sessionIt == numConnections_r) {    // Request space for every connection
                sessionIt = 0;
                iperfFsmState = INIT_RUN;
            }
            break; 

        case INIT_RUN:

            if (!txStatus.empty()){
                txStatus.read(spaceReplay);

                if (spaceReplay.error==0){
                    //cout << "Response for transfer " /*<< setw(5) */<< dec << sessionIt << " length : " << spaceReplay.length;
                    //cout << "\tremaining space: " << spaceReplay.remaining_space << "\terror: " <<  spaceReplay.error << endl;
                    
                    if (useTimer_r) {
                        currWord.data( 63,  0) = 0x3736353400000000;
                        currWord.data( 79, 64) = 0x3938;
                        currWord.data(511, 80) = 0;
                        currWord.keep          = 0xFFFFFFFFFFFFFFFF;
                        currWord.last          = 1;
                    }
                    else {
                        if (settings_regs.dualModeEn) {
                            currWord.data( 63,  0) = 0x0100000001000080;            //run now
                        }
                        else {
                            currWord.data( 63,  0) = 0x0100000000000000;
                        }
                        currWord.data(127, 64) = 0x0000000089130000;
                        currWord.data(159,128) = 0x39383736;
                        currWord.data(191,160) = byteSwap32(transfer_size_r);      // transfer size

                        currWord.data(511,192) = 0;
                        currWord.keep = 0xffffff;
                        currWord.last = 1;
                    }    
                    
                    sessionIt++;
                    
                    if (sessionIt == numConnections_r) {
                        sessionIt = 0;
                        bytes_already_sent = 0;
                        iperfFsmState = COMPUTE_NECESSARY_SPACE;
                    }
                    else {
                        iperfFsmState = REQ_WRITE_FIRST;
                    }
                    txData.write(currWord);
                }
                else {
                    iperfFsmState = REQ_WRITE_FIRST;
                }

            }
            break;

        case COMPUTE_NECESSARY_SPACE:   
            meta_i.sessionID = experimentID[sessionIt];

            if (useTimer_r) {                           // If the timer is being used send the maximum packet
                if (stopWatchEnd) {                     // When time is over finish
                    remaining_bytes_to_send = 0;
                }
                else {
                    remaining_bytes_to_send = packet_mss_r;
                }
            }
            else {
                remaining_bytes_to_send = transfer_size_r - bytes_already_sent;
            }

            //cout << "COMPUTE_NECESSARY_SPACE remaining_bytes_to_send:  " << dec << remaining_bytes_to_send << endl;

            if (remaining_bytes_to_send > 0){
                if (remaining_bytes_to_send >= packet_mss_r){           // Check if we can send a packet
                    
                    bytes_last_word     = packet_mss_r(5,0);            // How many bytes are necessary for the last transaction
                    if (packet_mss_r(5,0) == 0){                        // compute how many transactions are necessary
                        transactions        =  packet_mss_r(15,6);  
                    }
                    else {
                        transactions        =  packet_mss_r(15,6) + 1;  
                    }
                    meta_i.length    = packet_mss_r;
                }
                else {
                    
                    bytes_last_word     = remaining_bytes_to_send(5,0);            // How many bytes are necessary for the last transaction
                    if (remaining_bytes_to_send(5,0) == 0){                        // compute how many transactions are necessary
                        transactions        =  remaining_bytes_to_send(15,6);  
                    }
                    else {
                        transactions        =  remaining_bytes_to_send(15,6) + 1;  
                    }
                    meta_i.length    = remaining_bytes_to_send;
                }
                sessionIt     = 0;
                txMetaData.write(meta_i);
                iperfFsmState = SPACE_RESPONSE;
            }
            else {
                sessionIt     = 0;
                iperfFsmState = CLOSE_CONN;
            }


            //std::cout << "\ttransactions: " << transactions << "\tbytes_last_word: " << bytes_last_word << std::endl;

            transaction_length = meta_i.length;
            wordSentCount = 1;

            break; 

        case SPACE_RESPONSE:    
            if (!txStatus.empty()){
                wordSentCount = 1;
                txStatus.read(spaceReplay);

                //cout << "SPACE_RESPONSE Response for transfer " << setw(5) << std::dec << sessionIt << " length : " << spaceReplay.length;
                //cout << "\tremaining space: " << spaceReplay.remaining_space << "\terror: " <<  spaceReplay.error << std::endl << std::endl;
                

                if (spaceReplay.error==0){
                    if (sessionIt != (numConnections_r-1)){
                        sessionIt++;
                    }
                    iperfFsmState = SEND_PACKET;
                    bytes_already_sent = bytes_already_sent + transaction_length;
                }
                else {
                    waitCounter = 0;
                    iperfFsmState = REQUEST_SPACE;
                }
            }
            last_transfer_keep = len2Keep(bytes_last_word);
            break;

        case SEND_PACKET:

            currWord.data(63 ,  0) = 0x6d61752d6e637068;
            currWord.data(127, 64) = 0x202020202073652e;
            currWord.data(191,128) = 0x2e736d6574737973;
            currWord.data(255,192) = 0x2068632e7a687465;
            currWord.data(319,256) = wordSentCount ;
            currWord.data(383,320) = wordSentCount + 1;
            currWord.data(447,384) = wordSentCount + 2;
            currWord.data(511,448) = wordSentCount + 3;    // Dummy data

            if (wordSentCount==1 && useTimer_r){
                meta_i.sessionID = experimentID[sessionIt];
                meta_i.length    = transaction_length;
                if (!stopWatchEnd){
                    txMetaData.write(meta_i);
                }
                sessionIt++;
            }


            if (wordSentCount == transactions){
                currWord.keep = last_transfer_keep;
                //currWord.keep = len2Keep(bytes_last_word);
                currWord.last = 1;            

                if (sessionIt == numConnections_r){
                    sessionIt=0;
                }

                if (useTimer_r){
                    if (stopWatchEnd){
                        iperfFsmState = CLOSE_CONN;
                        sessionIt=0;
                    }
                    else {
                        iperfFsmState = SPACE_RESPONSE;
                    }
                }
                else {
                  iperfFsmState = COMPUTE_NECESSARY_SPACE;
                }
            }
            else {
                currWord.keep = 0xFFFFFFFFFFFFFFFF;
                currWord.last = 0;
            }
            
            wordSentCount++;

            txData.write(currWord);
            break;
 
        case REQUEST_SPACE:
            if (useTimer_r && stopWatchEnd){
                sessionIt=0;
                iperfFsmState = CLOSE_CONN;
            }
            else {
                meta_i.sessionID = experimentID[sessionIt];
                meta_i.length    = transaction_length;
                txMetaData.write(meta_i);
                iperfFsmState = SPACE_RESPONSE;
            }
            break;

        case CLOSE_CONN:
            if (waitCounter == 10000){
                iperfFsmState = CLOSE_CONN1;
                waitCounter = 0;
                sessionIt = 0;

            }
            waitCounter++;

            break;
        case CLOSE_CONN1:
            cout << endl << endl << "CLOSE_CONN closing connection: " << sessionIt << endl << endl << endl;
            closeConnection.write(experimentID[sessionIt]);
            sessionIt++;
            if (sessionIt == numConnections_r){
                iperfFsmState = WAIT_USER_START;
                sessionIt=0;
            }

            break;    
        case ERROR_OPENING_CONNECTION:
            errorOpeningConnection = 1;
            iperfFsmState = WAIT_USER_START;
            break;

    }

    runExperiment_r = settings_regs.runExperiment;                // Register run Experiment


    if(!stopWatchStop.empty()){
        stopWatchStop.read();
        stopWatchEnd = 1;
    }
}


void stopWatch (
                stream<ap_uint<64> >&            start,
                stream<bool > &                  over){


#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum sw_states {WAIT_SIGNAL, RUNNING};
    static sw_states sw_fsm_state = WAIT_SIGNAL;

    static ap_uint<64>  max_count;
    static ap_uint<64>  counter;

    switch (sw_fsm_state){
        case WAIT_SIGNAL: 
            if (!start.empty()){
                start.read(max_count);
                counter = 1;
                sw_fsm_state = RUNNING;
            }
            break;
        case RUNNING: 
            if (counter == max_count){
                over.write(true);
                //std::cout << "IPERF2 time over at " << std::dec << simCycleCounter << std::endl;
                sw_fsm_state = WAIT_SIGNAL;
            }
            counter++;
            break;
    }

}

void server(    
                stream<ap_uint<16> >&           listenPort, 
                stream<listenPortStatus>&       listenPortRes,
                stream<txApp_client_status>&    txAppNewClientNoty,
                stream<appNotification>&        notifications, 
                stream<appReadRequest>&         readRequest,
                stream<ap_uint<16> >&           rxMetaData, 
                stream<axiWord>&                rxData)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum server_states {OPEN_PORT, WAIT_RESPONSE, IDLE};
    static server_states server_fsm_state = OPEN_PORT;
#pragma HLS RESET variable=server_fsm_state

    enum consumeFsmStateType {GET_DATA_NOTY , WAIT_SESSION_ID, CONSUME};
    static consumeFsmStateType  serverFsmState = GET_DATA_NOTY;
    static ap_uint<16>      listen_port;
    
    listenPortStatus        listen_rsp;
    txApp_client_status     rx_client_notification;
    appNotification         notification;
    axiWord                 currWord;

    static ap_uint<16>      connectionID=0;

    if (!txAppNewClientNoty.empty()){
        txAppNewClientNoty.read(rx_client_notification);
    }


    switch (server_fsm_state){
        case OPEN_PORT:
            listen_port = 5001 + connectionID;
            listenPort.write(listen_port);              // Open port 5001
            //std::cout << "Request to listen to port: " << std::dec << listen_port << " has been issued." << std::endl;
            server_fsm_state = WAIT_RESPONSE;
            break;
        case WAIT_RESPONSE:
            if (!listenPortRes.empty()){
                listenPortRes.read(listen_rsp);
                if (listen_rsp.port_number == listen_port){
                    if (listen_rsp.open_successfully || listen_rsp.already_open) {
                        if (connectionID == MAX_SESSIONS-1){
                            server_fsm_state = IDLE;
                        }
                        else{
                            connectionID++;
                            server_fsm_state = OPEN_PORT;
                        }
                    }
                    else {
                        server_fsm_state = OPEN_PORT; // If the port was not opened successfully try again
                    }
                }
                //std::cout << "Port: " << std::dec << listen_rsp.port_number << " has been opened successfully " << ((listen_rsp.open_successfully) ? "Yes." : "No.");
                //std::cout << "\twrong port number " << ((listen_rsp.wrong_port_number) ? "Yes." : "No.");
                //std::cout << "\tthe port is already open " << ((listen_rsp.already_open) ? "Yes." : "No.") << std::endl;
            }
            break;
        case IDLE:  // Stay here forever
            server_fsm_state = IDLE;
            break;                      
    }

    switch (serverFsmState) {
        case GET_DATA_NOTY:
            if (!notifications.empty()){
                notifications.read(notification);

                if (notification.length != 0) {
                    readRequest.write(appReadRequest(notification.sessionID, notification.length));
                }
                serverFsmState = WAIT_SESSION_ID;
            }
            break;
        case WAIT_SESSION_ID:
            if (!rxMetaData.empty()) {
                rxMetaData.read();

                serverFsmState = CONSUME;
            }
            break;
        case CONSUME:
            if (!rxData.empty()) {
                rxData.read(currWord);
                if (currWord.last) {
                    serverFsmState = GET_DATA_NOTY;
                }
            }
            break;
    }

}


/**
 * @brief      Wrapper for iperf2 client
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
 * @param[in]  runExperiment            Running the experiment
 * @param[in]  dualModeEn               The dual mode en
 * @param[in]  numConnections           The number connections
 * @param[in]  transfer_size            The transfer size
 * @param[in]  packet_mss               The packet mss
 * @param[in]  ipDestination            The ip destination
 */
void iperf2_client( 
                    stream<ap_uint<16> >&           listenPortReq, 
                    stream<listenPortStatus>&       listenPortRes,
                    stream<appNotification>&        rxAppNotification, 
                    stream<appReadRequest>&         rxApp_readRequest,
                    stream<ap_uint<16> >&           rxApp_readRequest_RspID, 
                    stream<axiWord>&                rxData_to_rxApp,
                    stream<ipTuple>&                txApp_openConnection, 
                    stream<openStatus>&             txApp_openConnStatus,
                    stream<ap_uint<16> >&           closeConnection,
                    stream<appTxMeta>&              txAppDataReqMeta, 
                    stream<appTxRsp>&               txAppDataReqStatus,
                    stream<axiWord>&                txAppData_to_TOE,
                    stream<txApp_client_status>&    txAppNewClientNoty, 

                    /* AXI4-Lite registers */
                    iperf_regs&                     settings_regs)
{
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

#pragma HLS DATA_PACK variable=listenPortRes
#pragma HLS DATA_PACK variable=rxAppNotification
#pragma HLS DATA_PACK variable=rxApp_readRequest
#pragma HLS DATA_PACK variable=txApp_openConnection
#pragma HLS DATA_PACK variable=txApp_openConnStatus
#pragma HLS DATA_PACK variable=txAppDataReqMeta
#pragma HLS DATA_PACK variable=txAppDataReqStatus
#pragma HLS DATA_PACK variable=txAppNewClientNoty

#pragma HLS INTERFACE s_axilite port=settings_regs bundle=settings

    static stream<ap_uint<64> >             stopWatchStart("stopWatchStart");
    #pragma HLS STREAM variable=stopWatchStart           depth=4

    static stream<bool >             stopWatchStop("stopWatchStop");
    #pragma HLS STREAM variable=stopWatchStop           depth=4

    /*
     * Client
     */
    client( 
    //squeze_client( 
            txApp_openConnection,
            txApp_openConnStatus,
            closeConnection,
            txAppDataReqMeta,
            txAppDataReqStatus,
            txAppData_to_TOE,
            stopWatchStart,
            stopWatchStop,
            settings_regs);

    stopWatch(
        stopWatchStart,
        stopWatchStop);

    /*
     * Server
     */
    server( listenPortReq,
            listenPortRes,
            txAppNewClientNoty,
            rxAppNotification,
            rxApp_readRequest,
            rxApp_readRequest_RspID,
            rxData_to_rxApp);

}
