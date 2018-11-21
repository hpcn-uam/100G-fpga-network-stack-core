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
