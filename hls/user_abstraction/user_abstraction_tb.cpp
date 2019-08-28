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

unsigned int simCycleCounter=0;

int main()
{
    stream<ap_uint<16> >        listenPortReq("listenPortReq");
    stream<listenPortStatus>    listenPortRes("listenPortRes");
    stream<appNotification>     notifications("notifications");
    stream<appReadRequest>      readRequest("readRequest");
    stream<ap_uint<16> >        rxMetaData("rxMetaData");
    stream<axiWord>             rxData("rxData");
    stream<ipTuple>             openConnection("openConnection");
    stream<openStatus>          openConStatus("openConStatus");
    stream<ap_uint<16> >        closeConnection("closeConnection");
    stream<appTxMeta>           appTxSpaceRequest("appTxSpaceRequest");
    stream<appTxRsp>            appTxSpaceResponse("appTxSpaceResponse");
    stream<axiWord>             txData("txData");
    stream<txApp_client_status> newClientNoty("newClientNoty");
    stream<axiWordUser>         rxShell2Usr("rxShell2Usr");        
    stream<axiWordUser>         txUsr2Shell("txUsr2Shell");
    userRegs                    settings_regs;

    ap_uint<16>         listenPort;
    listenPortStatus    listenPortResponse;
    appTxMeta           appTxSceReq;
    appTxRsp            appTxSceRsp;
    ap_uint<16>         sessionID;
    axiWord             currWord;
    axiWordUser         currWordUsr;
    appReadRequest      readReq;


    while (simCycleCounter < 10000) {

        if (simCycleCounter==250){
            axiWordUser currData;
            currData.keep = 0xFFFFFFFFFFFFFFFF;
            currData.user = 0;
            currData.last = 0;
            unsigned int word2send = 25;
            for(unsigned int m=0; m < word2send; m++){
                currData.data = m;
                if (m == word2send-1){
                    currData.keep = 0x1;
                    currData.last = 1;
                }
                txUsr2Shell.write(currData);
            }
        }

        if (simCycleCounter==5000){
            appNotification rxNotification;
            currWord.keep = 0xFFFFFFFFFFFFFFFF;
            currWord.last = 0;
            unsigned int word2gen = 20;
            for(unsigned int m=0; m < word2gen; m++){
                currWord.data = m + 0x100;
                if (m == word2gen-1)
                    currWord.last = 1;
                rxData.write(currWord);
            }
            ap_uint<16> id = 5;
            rxNotification.sessionID = id;
            rxNotification.length    = word2gen * 64;
            rxNotification.ipAddress = 0xc0a10005;
            rxNotification.dstPort   = 5001;
            rxNotification.closed    = 0;
            notifications.write(rxNotification);
            rxMetaData.write(id);
        }


        user_abstraction(  
                        listenPortReq,
                        listenPortRes,
                        notifications,
                        readRequest,
                        rxMetaData,
                        rxData,
                        openConnection,
                        openConStatus,
                        closeConnection,
                        appTxSpaceRequest,
                        appTxSpaceResponse,
                        txData,
                        newClientNoty,
                        rxShell2Usr,
                        txUsr2Shell,
                        // registers
                        settings_regs);

        if (!listenPortReq.empty()){
            listenPortReq.read(listenPort);
            listenPortResponse.open_successfully=true;
            listenPortResponse.wrong_port_number=false;
            listenPortResponse.already_open=false;
            listenPortResponse.port_number=listenPort;
            listenPortRes.write(listenPortResponse);
            //cout << "Request to open port " << dec << listenPort << " at time " << simCycleCounter << endl;
        }

        if (!readRequest.empty()){
            readRequest.read(readReq);
            cout << "The abstraction level request to read " << dec << readReq.length << "-Byte from ID " << readReq.sessionID;
            cout << " at cycle " << simCycleCounter << endl;
        }

        if (!openConnection.empty()) {
            openConnection.read();
            cout << "Opening connection.. at cycle" << simCycleCounter << endl;
            openConStatus.write(openStatus(123+simCycleCounter, true));
        }
        if (!appTxSpaceRequest.empty()) {
            appTxSpaceRequest.read(appTxSceReq);
            cout << "Request to send a new packet with ID: " << dec << appTxSceReq.sessionID << " length " <<appTxSceReq.length;
            cout << " at cycle "  << simCycleCounter << endl;

            appTxSceRsp.length          = appTxSceReq.length;
            appTxSceRsp.remaining_space = 50000;
            appTxSceRsp.error           = NO_ERROR;

            appTxSpaceResponse.write(appTxSceRsp);

        }
        if (!closeConnection.empty()) {

            closeConnection.read(sessionID);
            cout << "Closing connection: " << dec << sessionID << endl;
        }
        simCycleCounter++;
    }
    unsigned int txPacketCount=0;
    unsigned int txFlitsCount=0;
    cout << "User to TOE packets" << endl;
    while (!txData.empty()) {
        txData.read(currWord);
        cout << "Packet [" << dec << setw(3) << txPacketCount << "][" << setw(2) << txFlitsCount++;
        cout << "] data " << hex << setw(34) << currWord.data << " keep " << setw(18) << currWord.keep << " last " << currWord.last << endl;
        if (currWord.last) {
            txFlitsCount = 0;
            txPacketCount++;
        }
    }
    
    unsigned int rxPacketCount=0;
    unsigned int rxFlitsCount=0;
    cout << "TOE to User packets" << endl;
    while (!rxShell2Usr.empty()) {
        rxShell2Usr.read(currWordUsr);
        cout << "Packet [" << dec << setw(3) << rxPacketCount << "][" << setw(2) << rxFlitsCount++;
        cout << "] data " << hex << setw(34) << currWordUsr.data << " keep " << setw(18) << currWordUsr.keep;
        cout << " last " << currWordUsr.last << " user " << currWordUsr.user << endl;
        if (currWordUsr.last) {
            rxFlitsCount = 0;
            rxPacketCount++;
        }
    }
    return 0;
}

