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

#include "../TOE/toe.hpp"
#include "../TOE/common_utilities/common_utilities.hpp"
#include <iostream>

using namespace hls;
using namespace std;

#define MAXIMUM_SEGMENT_SIZE 1408

struct userRegs {
     ap_uint< 1>    openConn;      
     ap_uint< 1>    closeConn;      
     ap_uint<16>    connID;
     ap_uint<32>    dstIP;      
     ap_uint<16>    dstPort;
     ap_uint<16>    maxConnections;
};


template<unsigned int D,unsigned int U>
struct axisUser {
     ap_uint< D >   data;
     ap_uint<D/8>   keep;
     ap_uint<  1>   last;
     ap_uint< U >   user;
     axisUser() {}
     axisUser(ap_uint<D>   data, ap_uint<D/8> keep, ap_uint<1> last, ap_uint< U > user)
                    : data(data), keep(keep), last(last), user(user) {}
};

typedef axisUser<ETH_INTERFACE_WIDTH,16> axiWordUser;


struct txMessageMetaData{
     ap_uint<11>    bytes;
     ap_uint<16>    connID;
     txMessageMetaData(){}
     txMessageMetaData(ap_uint< 11> l, ap_uint<16>  id)
          : bytes(l), connID(id) {}
};



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
                    userRegs&                       settings_regs);
