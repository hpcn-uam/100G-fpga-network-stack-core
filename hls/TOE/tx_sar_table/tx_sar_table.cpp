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

#include "tx_sar_table.hpp"

using namespace hls;

/** @ingroup tx_sar_table
 *  This data structure stores the TX(transmitting) sliding window
 *  and handles concurrent access from the @ref rx_engine, @ref tx_app_if
 *  and @ref tx_engine
 *  @TODO check if locking is actually required, especially for rxOut
 *  @param[in] rxEng2txSar_upd_req
 *  @param[in] txEng2txSar_upd_req
 *  @param[in] txApp2txSar_app_push
 *  @param[out] txSar2rxEng_upd_rsp
 *  @param[out] txSar2txEng_upd_rsp
 *  @param[out] txSar2txApp_ack_push
 */
void tx_sar_table(	stream<rxTxSarQuery>&			rxEng2txSar_upd_req,
					stream<txTxSarQuery>&			txEng2txSar_upd_req,
					stream<txAppTxSarPush>&			txApp2txSar_app_push,
					stream<rxTxSarReply>&			txSar2rxEng_upd_rsp,
					stream<txTxSarReply>&			txSar2txEng_upd_rsp,
					stream<txSarAckPush>&			txSar2txApp_ack_push)
{
#pragma HLS LATENCY max=3
#pragma HLS PIPELINE II=1

	static txSarEntry tx_table[MAX_SESSIONS];
	#pragma HLS DEPENDENCE variable=tx_table inter false
	#pragma HLS RESOURCE variable=tx_table core=RAM_T2P_BRAM
	
	txTxSarQuery 			tst_txEngUpdate;
	txTxSarRtQuery 			txEngRtUpdate;
	rxTxSarQuery 			tst_rxEngUpdate;
	txAppTxSarPush 			push;
	txSarEntry 				tmp_entry_read;
	txTxSarReply 			tmp_replay;
	ap_uint<WINDOW_BITS> 	minWindow;
	ap_uint<WINDOW_BITS>    scaled_recv_window = 0;

	// TX Engine
	if (!txEng2txSar_upd_req.empty()) {
		txEng2txSar_upd_req.read(tst_txEngUpdate);
		if (tst_txEngUpdate.write) {
			if (!tst_txEngUpdate.isRtQuery) {
				tx_table[tst_txEngUpdate.sessionID].not_ackd = tst_txEngUpdate.not_ackd;
				if (tst_txEngUpdate.init) {
					tx_table[tst_txEngUpdate.sessionID].app = tst_txEngUpdate.not_ackd;
					tx_table[tst_txEngUpdate.sessionID].ackd = tst_txEngUpdate.not_ackd-1;
					tx_table[tst_txEngUpdate.sessionID].cong_window = 0x3908; // 10 x 1460(MSS)
					tx_table[tst_txEngUpdate.sessionID].slowstart_threshold = (BUFFER_SIZE-1);
					tx_table[tst_txEngUpdate.sessionID].finReady = tst_txEngUpdate.finReady;
					tx_table[tst_txEngUpdate.sessionID].finSent = tst_txEngUpdate.finSent;
					// Init ACK to txAppInterface
#if !(TCP_NODELAY)
					txSar2txApp_ack_push.write(txSarAckPush(tst_txEngUpdate.sessionID, tst_txEngUpdate.not_ackd, 1));
#else
					txSar2txApp_ack_push.write(txSarAckPush(tst_txEngUpdate.sessionID, tst_txEngUpdate.not_ackd, 0x3908 /* 10 x 1460(MSS) */, 1));
#endif
				}
				if (tst_txEngUpdate.finReady) {
					tx_table[tst_txEngUpdate.sessionID].finReady = tst_txEngUpdate.finReady;
				}
				if (tst_txEngUpdate.finSent) {
					tx_table[tst_txEngUpdate.sessionID].finSent = tst_txEngUpdate.finSent;
				}
			}
			else {
				txEngRtUpdate = tst_txEngUpdate;
				tx_table[tst_txEngUpdate.sessionID].slowstart_threshold = txEngRtUpdate.getThreshold();
				tx_table[tst_txEngUpdate.sessionID].cong_window = 0x3908; // 10 x 1460(MSS) TODO is this correct or less, eg. 1/2 * MSS
			}
		}
		else {// Read
			tmp_entry_read = tx_table[tst_txEngUpdate.sessionID];


			tmp_replay.ackd			= tmp_entry_read.ackd;
			tmp_replay.not_ackd		= tmp_entry_read.not_ackd;
			tmp_replay.app			= tmp_entry_read.app;
			tmp_replay.finReady		= tmp_entry_read.finReady;
			tmp_replay.finSent		= tmp_entry_read.finSent;	
			tmp_replay.currLength	= tmp_entry_read.app - tmp_entry_read.not_ackd(WINDOW_BITS-1,0);	
			tmp_replay.usedLength	= tmp_entry_read.not_ackd(WINDOW_BITS-1,0) - tmp_entry_read.ackd;	
			tmp_replay.usedLength_rst	= tmp_entry_read.not_ackd(WINDOW_BITS-1,0) - tmp_entry_read.ackd - tmp_entry_read.finSent;	
			
			tmp_replay.ackd_eq_not_ackd	= (tmp_entry_read.ackd == tmp_entry_read.not_ackd);	
			tmp_replay.not_ackd_plus_mss	= tmp_entry_read.not_ackd + MSS;	

#if (!WINDOW_SCALE)			
			if (tmp_entry_read.cong_window < tmp_entry_read.recv_window) {
				minWindow = tmp_entry_read.cong_window;
			}
			else {
				minWindow = tmp_entry_read.recv_window;
			}

#else
			scaled_recv_window(tmp_entry_read.tx_win_shift+15,tmp_entry_read.tx_win_shift)  = tmp_entry_read.recv_window;
			if (tmp_entry_read.cong_window < scaled_recv_window ) {
				minWindow = tmp_entry_read.cong_window;
			}
			else {
				minWindow = scaled_recv_window;
			}

			tmp_replay.tx_win_shift = tmp_entry_read.tx_win_shift;
#endif			

			if (minWindow > tmp_replay.usedLength){
				tmp_replay.UsableWindow	= minWindow - tmp_replay.usedLength;
			}
			else{
				tmp_replay.UsableWindow	= 0;
			}
			tmp_replay.min_window	= minWindow;		
			tmp_replay.not_ackd_short = tmp_entry_read.not_ackd + tmp_replay.currLength;

			txSar2txEng_upd_rsp.write(tmp_replay);
		}
	}


	// TX App Stream If
	else if (!txApp2txSar_app_push.empty()) {//write only
		txApp2txSar_app_push.read(push);
		tx_table[push.sessionID].app = push.app;
		//std::cout << "APP update  " << std::hex << push.app << std::endl;
	}


	// RX Engine
	else if (!rxEng2txSar_upd_req.empty()) {
		rxEng2txSar_upd_req.read(tst_rxEngUpdate);
		if (tst_rxEngUpdate.write) {
#if (WINDOW_SCALE)
			if (tst_rxEngUpdate.tx_win_shift_write){
				tx_table[tst_rxEngUpdate.sessionID].tx_win_shift = tst_rxEngUpdate.tx_win_shift;
				//std::cout << "TX Sar init window scale shift " << std::dec << tst_rxEngUpdate.tx_win_shift << std::endl;
			}
#endif			
			tx_table[tst_rxEngUpdate.sessionID].ackd = tst_rxEngUpdate.ackd;
			tx_table[tst_rxEngUpdate.sessionID].recv_window = tst_rxEngUpdate.recv_window;
			tx_table[tst_rxEngUpdate.sessionID].cong_window = tst_rxEngUpdate.cong_window;
			tx_table[tst_rxEngUpdate.sessionID].count = tst_rxEngUpdate.count;
			tx_table[tst_rxEngUpdate.sessionID].fastRetransmitted = tst_rxEngUpdate.fastRetransmitted;

			//std::cout << "tx_table.not_ackd: " << std::hex << tx_table[tst_rxEngUpdate.sessionID].not_ackd << std::endl;
#if (!TCP_NODELAY)
			txSar2txApp_ack_push.write(txSarAckPush(tst_rxEngUpdate.sessionID, tst_rxEngUpdate.ackd));
#else

#if (WINDOW_SCALE)				
			scaled_recv_window(tst_rxEngUpdate.tx_win_shift+15,tst_rxEngUpdate.tx_win_shift)  = tst_rxEngUpdate.recv_window;
			//scaled_recv_window = (tst_rxEngUpdate.recv_window *  (1 << tst_rxEngUpdate.tx_win_shift));
#else
			scaled_recv_window = tst_rxEngUpdate.recv_window;		
#endif				
			if (tst_rxEngUpdate.cong_window < scaled_recv_window) {
				minWindow = tst_rxEngUpdate.cong_window;
			}
			else {
				minWindow = scaled_recv_window;			
			}
			txSar2txApp_ack_push.write(txSarAckPush(tst_rxEngUpdate.sessionID, tst_rxEngUpdate.ackd, minWindow));
#endif
		}
		else {
			txSar2rxEng_upd_rsp.write(rxTxSarReply(	tx_table[tst_rxEngUpdate.sessionID].ackd,
													tx_table[tst_rxEngUpdate.sessionID].not_ackd,
													tx_table[tst_rxEngUpdate.sessionID].cong_window,
													tx_table[tst_rxEngUpdate.sessionID].slowstart_threshold,
													tx_table[tst_rxEngUpdate.sessionID].count,
													tx_table[tst_rxEngUpdate.sessionID].fastRetransmitted
#if (WINDOW_SCALE)
													,tx_table[tst_rxEngUpdate.sessionID].tx_win_shift
#endif
			));
		}
	}
}
