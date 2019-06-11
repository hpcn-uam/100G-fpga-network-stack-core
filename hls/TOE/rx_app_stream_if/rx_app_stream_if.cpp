/************************************************
Copyright (c) 2018, Xilinx, Inc.
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
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2018 Xilinx, Inc.
************************************************/

#include "rx_app_stream_if.hpp"

using namespace hls;

/** @ingroup rx_app_stream_if
 *  This application interface is used to receive data streams of established connections.
 *  The Application polls data from the buffer by sending a readRequest. The module checks
 *  if the readRequest is valid then it sends a read request to the memory. After processing
 *  the request the MetaData containig the Session-ID is also written back.
 *  @param[in]		appRxDataReq
 *  @param[in]		rxSar2rxApp_upd_rsp
 *  @param[out]		appRxDataRspIDsession
 *  @param[out]		rxApp2rxSar_upd_req
 *  @param[out]		rxBufferReadCmd
 */
void rx_app_stream_if(stream<appReadRequest>&		appRxDataReq,
					  stream<rxSarAppd>&			rxSar2rxApp_upd_rsp,
					  stream<ap_uint<16> >&			appRxDataRspIDsession,
#if (!RX_DDR_BYPASS)
					  stream<cmd_internal>&			rxBufferReadCmd,
#endif
					  stream<rxSarAppd>&			rxApp2rxSar_upd_req)
{
#pragma HLS PIPELINE II=1

	static ap_uint<16>				rasi_readLength;
	static ap_uint<2>				rasi_fsmState 	= 0;
	appReadRequest					app_read_request;
	rxSarAppd						rxSar;
	ap_uint<32> 					pkgAddr = 0;

	switch (rasi_fsmState) {
		case 0:
			if (!appRxDataReq.empty()) {
				appRxDataReq.read(app_read_request);
				if (app_read_request.length != 0) { 	// Make sure length is not 0, otherwise Data Mover will hang up
					// Get app pointer
					rxApp2rxSar_upd_req.write(rxSarAppd(app_read_request.sessionID));
					rasi_readLength = app_read_request.length;
					rasi_fsmState = 1;
				}
			}
			break;
		case 1:
			if (!rxSar2rxApp_upd_rsp.empty()) {
				rxSar2rxApp_upd_rsp.read(rxSar);
				appRxDataRspIDsession.write(rxSar.sessionID);
#if (!RX_DDR_BYPASS)
				
				pkgAddr(31, WINDOW_BITS) = rxSar.sessionID(13, 0);
				pkgAddr(WINDOW_BITS-1, 0) = rxSar.appd;
				rxBufferReadCmd.write(cmd_internal(pkgAddr, rasi_readLength));
#endif
				// Update app read pointer
				rxApp2rxSar_upd_req.write(rxSarAppd(rxSar.sessionID, rxSar.appd+rasi_readLength)); // Update me
				rasi_fsmState = 0;
			}
			break;
	}
}
