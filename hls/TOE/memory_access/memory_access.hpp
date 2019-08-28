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

#ifndef _MEMMORY_ACCESS_HPP_
#define _MEMMORY_ACCESS_HPP_

#include "../toe.hpp"
#include "../common_utilities/common_utilities.hpp"

using namespace hls;


void app_ReadMemAccessBreakdown(
					stream<cmd_internal>& 		inputMemAccess, 
					stream<mmCmd>& 				outputMemAccess, 
					stream<memDoubleAccess>& 	memAccessBreakdown);

void tx_ReadMemAccessBreakdown(
					stream<cmd_internal>& 		inputMemAccess, 
					stream<mmCmd>& 				outputMemAccess, 
					stream<memDoubleAccess>& 	memAccessBreakdown);

void tx_MemDataRead_aligner(
					stream<axiWord>& 			mem_payload_unaligned,
					stream<memDoubleAccess>& 	mem_two_access,
				
#if (TCP_NODELAY)
					stream<bool>&				txEng_isDDRbypass,
					stream<axiWord>&			txApp2txEng_data_stream,
#endif
					stream<axiWord>& 			tx_payload_aligned);

void app_MemDataRead_aligner(
					stream<axiWord>& 			mem_payload_unaligned,
					stream<memDoubleAccess>& 	mem_two_access,
					stream<axiWord>& 			app_data_aligned);

void Rx_Data_to_Memory(
					stream<axiWord>& 				rxMemWrDataIn,
					stream<mmCmd>&					rxMemWrCmdIn,
					stream<mmCmd>&					rxMemWrCmdOut,
					stream<axiWord>&				rxMemWrDataOut,
					stream<ap_uint<1> >&			doubleAccess);

void tx_Data_to_Memory(
					stream<axiWord>& 				txMemWrDataIn,
					stream<mmCmd>&					txMemWrCmdIn,
					stream<mmCmd>&					txMemWrCmdOut,
					stream<axiWord>&				txMemWrDataOut);

#endif
