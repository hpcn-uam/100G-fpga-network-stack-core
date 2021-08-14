/*
 * Copyright (c) 2019, Systems Group, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UDP_HPP
#define UDP_HPP

#include "../TOE/toe.hpp"
#include "../TOE/common_utilities/common_utilities.hpp"

using namespace hls;
using namespace std;

#define NUMBER_SOCKETS 16

struct userMetadata {
    ap_uint<32>     myIP;
    ap_uint<32>     theirIP;
    ap_uint<16>     myPort;
    ap_uint<16>     theirPort;

    userMetadata () {}
    userMetadata (ap_uint<32> mi, ap_uint<32> ti, ap_uint<16> mp, ap_uint<16> tp) 
        : myIP(mi), theirIP(ti), myPort(mp), theirPort(tp) {}

};

template<int D>
struct axis_udp {
    ap_uint< D >    data;
    ap_uint<D/8>    keep;
    ap_uint< 1>     last;
    axis_udp() {}
    axis_udp(ap_uint<D>   data, ap_uint<D/8> keep, ap_uint<1> last)
                : data(data), keep(keep), last(last) {}
};

typedef ap_axiu<ETH_INTERFACE_WIDTH, 96, 0, 16> axiWordUdp;

typedef axis_udp<ETH_INTERFACE_WIDTH> axiWordi;

struct socket_table {
    ap_uint<32>     theirIP;
    ap_uint<16>     theirPort;
    ap_uint<16>     myPort;
    ap_uint< 1>     valid;

    socket_table() {}
    socket_table(ap_uint<32> ti, ap_uint<16> tp, ap_uint<16> mp, ap_uint<1> v) 
        : theirIP(ti), theirPort(tp), myPort(mp), valid(v) {}
};

struct udpMetadata {
    ap_uint<32>     theirIP;
    ap_uint<32>     myIP;
    ap_uint<16>     theirPort;
    ap_uint<16>     myPort;
    ap_uint< 1>     valid;

    udpMetadata () {}
    udpMetadata (ap_uint<32> ti, ap_uint<32> mi, ap_uint<16> tp, ap_uint<16> mp, ap_uint<1> v) 
        : theirIP(ti), myIP(mi), theirPort(tp), myPort(mp), valid(v) {}
};

struct tableResponse
{
    ap_uint<16>     id;
    userMetadata    user;
    ap_uint< 1>     drop;

    tableResponse () {}
    tableResponse (ap_uint<16> i, ap_uint<1> d) 
        : id(i), drop(d) {}
};


template<int D>
ap_uint<D> byteSwap(ap_uint<D> inputVector) {
    
    ap_uint<D> aux = 0;

    for (unsigned int i =0; i < D ; i +=8){
        aux(i+7 , i) = inputVector(D-1-i, D-8-i);
    }

    return aux;
}

void udp(
    stream<axiWord>         &rxUdpDataIn,
    stream<axiWord>         &txUdpDataOut,
    stream<axiWordUdp>      &DataOutApp,  
    stream<axiWordUdp>      &DataInApp,
    ap_uint<32>             &myIpAddress,
    socket_table            SocketTable[NUMBER_SOCKETS],
    ap_uint<16>             &numberSockets );

#endif
