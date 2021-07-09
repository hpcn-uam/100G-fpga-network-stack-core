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
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

#include <hls_stream.h>
#include "ap_int.h"
#include <stdint.h>
#include <cstdlib>
#include "ap_axi_sdata.h"

using namespace hls;
using namespace std;

#ifndef _PACKET_HANDLER_HPP_
#define _PACKET_HANDLER_HPP_

const ap_uint<16> TYPE_IPV4 	= 0x0800;
const ap_uint<16> TYPE_ARP 		= 0x0806;

const ap_uint< 8> PROTO_ICMP	=  1;
const ap_uint< 8> PROTO_TCP		=  6;
const ap_uint< 8> PROTO_UDP 	= 17;



typedef ap_uint<3>		dest_type;
typedef ap_axiu<512,0,0,3> axiWord;

struct axiWordi {
  ap_uint<512>  data;
  ap_uint<64>   keep;
  ap_uint<1>    last;
  dest_type     dest;
};


/**
 * @brief      packet_handler: wrapper for packet identification and Ethernet remover
 *
 * @param      dataIn   Incoming data from the network interface, at Ethernet level
 * @param      dataOut  Output data. The tdest says which kind of packet it is.
 * 						The Ethernet header is shoved off for IPv4 packets
 *   
 */
void packet_handler(
			stream<axiWord>&			dataIn,
			stream<axiWord>&			dataOut);

#endif