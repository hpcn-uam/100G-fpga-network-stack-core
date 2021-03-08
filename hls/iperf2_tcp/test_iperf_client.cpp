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
#include <iostream>

using namespace hls;

extern unsigned int simCycleCounter;

int main()
{
    stream<ap_uint<16> > listenPortReq("listenPortReq");
    stream<listenPortStatus> listenPortRes("listenPortRes");
    stream<appNotification> notifications("notifications");
    stream<appReadRequest> readRequest("readRequest");
    stream<ap_uint<16> > rxMetaData("rxMetaData");
    stream<axiWord> rxData("rxData");
    stream<ipTuple> openConnection("openConnection");
    stream<openStatus> openConStatus("openConStatus");
    stream<ap_uint<16> > closeConnection("closeConnection");
    stream<appTxMeta> txMetaData("txMetaData");
    stream<axiWord> txData("txData");
    stream<appTxRsp> txStatus("txStatus");
    stream<txApp_client_status> newClientNoty("newClientNoty");

    iperf_regs  settings_regs;

    ap_uint<8> pkgWordCount;

    ap_uint<16> sessionID;
    appTxMeta txMetaDataReq;
    axiWord currWord;
    int count = 0;
    pkgWordCount = 8;
 
    settings_regs.dualModeEn    = 0;
    settings_regs.useTimer      = 0; 
    settings_regs.runTime       = 0;
    settings_regs.transfer_size = 1000;
    settings_regs.packet_mss    = 1460;
    settings_regs.ipDestination = 0x01010101;
    settings_regs.dstPort       = 5001;

    while (count < 10000)
    {
        settings_regs.numConnections = 2;
        settings_regs.runExperiment = 0;
        if (count == 20)
        {
            settings_regs.runExperiment = 1;
        }
        iperf2_client(  listenPortReq,
                        listenPortRes,
                        notifications,
                        readRequest,
                        rxMetaData,
                        rxData,
                        openConnection,
                        openConStatus,
                        closeConnection,
                        txMetaData,
                        txStatus,
                        txData,
                        newClientNoty,
                        // registers
                        settings_regs);


        if (!openConnection.empty()) {
            openConnection.read();
            std::cout << "Opening connection.. at cycle" << count << std::endl;
            openConStatus.write(openStatus(123+count, true));
        }
        if (!txMetaData.empty()) {
            txMetaData.read(txMetaDataReq);
            std::cout << "New Pkg: " << std::dec << txMetaDataReq.sessionID << std::endl;
            sessionID = txMetaDataReq.sessionID;
        }
        while (!txData.empty())
        {
            txData.read(currWord);
            std::cout << std::hex << std::noshowbase;
            std::cout << std::setfill('0');
            std::cout << std::setw(8) << ((uint32_t) currWord.data(63, 32));
            std::cout << std::setw(8) << ((uint32_t) currWord.data(31, 0));
            std::cout << " " << std::setw(2) << ((uint32_t) currWord.keep) << " ";
            std::cout << std::setw(1) << ((uint32_t) currWord.last) << std::endl;
        }
        if (!closeConnection.empty()) {
            closeConnection.read(sessionID);
            std::cout << "Closing connection: " << std::dec << sessionID << std::endl;
        }
        count++;
    }
    return 0;
}

