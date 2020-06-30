/**********
Copyright (c) 2020, Xilinx, Inc.
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
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#include "udp.hpp"
#include "../TOE/testbench/pcap2stream.hpp"
#include <string.h>

socket_table            SocketTableRx[NUMBER_SOCKETS];
socket_table            SocketTableTx[NUMBER_SOCKETS];
stream<axiWordUdp>      DataInApp("DataInApp");

#include <fstream>
#include <iterator>
#include <vector>

void readTxtFile () {
    std::ifstream input("shakespeare.txt", std::ios::binary);

    std::vector<char> text(
         (std::istreambuf_iterator<char>(input)),
         (std::istreambuf_iterator<char>())
    );
    input.close();

    /*Create payloads ranging from 1 to 1508 bytes */
    for (unsigned int upper_limit= 1; upper_limit <(text.size() + 1); upper_limit++){
        axiWordUdp currWord = axiWordUdp(0,0,0,0);
        for(unsigned int i = 0; i < upper_limit; i++) {
            unsigned int lb = (i % 64) * 8;
            currWord.data(lb + 7 , lb) = text.at(i);
            currWord.keep.bit(lb/8)    = 1;
            //std::cout << "upper_limit: " << std::setw(3) << upper_limit << "\ti: " << std::setw(3) << i << "\tlb: " << std::setw(2) << lb << "\tvector: " << (char) text.at(i) << std::endl;
            if ((i + 1) == upper_limit) {
                currWord.last = 1;
                DataInApp.write(currWord);
            }
            else if (((i + 1) % 64) == 0){
                DataInApp.write(currWord);
                currWord = axiWordUdp(0,0,0,0);
            }
        }
    }
}

void fillTables(){
    // Initialize tables to 0
    for (unsigned int i=0; i < NUMBER_SOCKETS; i++){
        SocketTableRx[i]= socket_table(0,0,0,0);
    }

    SocketTableRx[ 0] = socket_table(0xC0A8000A,53211,60279,1);
    SocketTableRx[ 6] = socket_table(0xC0A80017,58517,60280,1);
    SocketTableRx[11] = socket_table(0xC0A80085,54591,60281,1);

    memcpy(SocketTableTx, SocketTableRx, sizeof(socket_table) * NUMBER_SOCKETS);
#ifdef PRINT_TABLE
    for (unsigned int m=0; m< NUMBER_SOCKETS; m++){
        std::cout<< "TxTable["<< std::setw(2) << m << "] theirIP: " << SocketTableTx[m].theirIP << "\ttheirPort: " << SocketTableTx[m].theirIP;
        std::cout<< "\tmyPort: " << SocketTableTx[m].myPort << "\tvalid: " << SocketTableTx[m].valid <<std::endl;

    }
#endif    
}


int main(){

    stream<axiWord>         rxUdpDataIn("rxUdpDataIn");
    stream<axiWord>         txUdpDataOut("txUdpDataOut");
    stream<axiWord>         txUdpDataOutpcap("txUdpDataOutpcap");
    stream<axiWord>         txUdpDataOutComp("txUdpDataOutComp");
    stream<axiWord>         txGoldenData("txGoldenData");
    stream<axiWordUdp>      DataOutApp("DataOutApp");
    ap_uint<32>             myIpAddress = byteSwap<32>(0xC0A80005);
    ap_uint<16>             numberSockets;
    axiWord                 currWord;
    stream<axiWordUdp>      DataInAppBK("DataInAppBK");
    char *outputPcap = (char *) "txPathOutputSim.pcap";
    char *outputGolden = (char *) "goldenDataTx.pcap";


    fillTables();
    readTxtFile();

    for (unsigned int cycle_count = 0; cycle_count < 50000 ; cycle_count ++) {
        udp(
            rxUdpDataIn,
            txUdpDataOut,
            DataOutApp,  
            DataInApp,
            myIpAddress,
            SocketTableRx,
            SocketTableTx,
            numberSockets);
    }

    /*while(!txUdpDataOut.empty()){
        txUdpDataOut.read(currWord);
        std::cout << "Data " << std::setw(130) << std::hex << currWord.data << "\tkeep " << std::setw(18);
        std::cout  << currWord.keep << "\tlast " << std::dec << currWord.last << std::endl;
    }*/

    /*Duplicate tx stream to save it as pcap file and to compare*/
    while(!txUdpDataOut.empty()){
        axiWord currWordT;
        txUdpDataOut.read(currWordT);
        txUdpDataOutpcap.write(currWordT);
        txUdpDataOutComp.write(currWordT);
    }

    /*Store Tx path in a pcap file*/
    stream2pcap(outputPcap,false,false,txUdpDataOutpcap,true);

    // read TX golden data into a stream
    pcap2stream(outputGolden,false,txGoldenData);

    /*Check Tx output against golden data*/
    unsigned int txPacket = 0;
    unsigned int txBeat = 0;
    int          txErrors = 0;
    while(!txGoldenData.empty()){
        axiWord currWordS, currWordG;
        txUdpDataOutComp.read(currWordS);
        txGoldenData.read(currWordG);
        
        if (currWordS.data != currWordG.data){
            std::cerr << "Tx path, packet[" << std::setw(4) << txPacket << "][" << std::setw(2);
            std::cerr << txBeat << "] data field does not match golden data" << std::endl;
            txErrors--;
        }

        if (currWordS.keep != currWordG.keep){
            std::cerr << "Tx path, packet[" << std::setw(4) << txPacket << "][" << std::setw(2);
            std::cerr << txBeat << "] keep field does not match golden data" << std::endl;
            txErrors--;
        }
        if (currWordS.last != currWordG.last){
            std::cerr << "Tx path, packet[" << std::setw(4) << txPacket << "][" << std::setw(2);
            std::cerr << txBeat << "] last field does not match golden data" << std::endl;
            txErrors--;
        }

        txBeat++;
        if (currWordG.last){
            txPacket++;
            txBeat = 0;
        }
    }
    if (txErrors == 0){
        std::cout << "Tx path test PASSED" << std::endl;
        return 0;
    }
    else
        return txErrors;



}
