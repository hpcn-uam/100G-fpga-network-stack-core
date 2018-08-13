/************************************************
Copyright (c) 2018, Systems Group, ETH Zurich and HPCN Group, UAM Spain.
All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>
************************************************/
#include "iperf_client.hpp"
#include <iostream>

using namespace hls;


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
    ap_uint<1> runExperiment;
    ap_uint<1> dualModeEn;
    ap_uint<14> useConn;
    ap_uint<8> pkgWordCount;
    ap_uint<32> ipDestination = 0x01010101;
    ap_uint<32> transfer_size = 1000;
    ap_uint<12> packet_mss = 1460;

    ap_uint<16> sessionID;
    appTxMeta txMetaDataReq;
    axiWord currWord;
    int count = 0;
    dualModeEn = 0;
    pkgWordCount = 8;

    while (count < 10000)
    {
        useConn = 2;
        runExperiment = 0;
        if (count == 20)
        {
            runExperiment = 1;
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

                        runExperiment,
                        dualModeEn,
                        useConn,
                        transfer_size,
                        packet_mss,
                        ipDestination);

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

