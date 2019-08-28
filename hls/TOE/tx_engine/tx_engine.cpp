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

#include "tx_engine.hpp"

using namespace hls;
using namespace std;

/** @ingroup tx_engine
 *  @name txEng_metaLoader
 *  The txEng_metaLoader reads the Events from the EventEngine then it loads all the necessary MetaData from the data
 *  structures (RX & TX Sar Table). Depending on the Event type it generates the necessary MetaData for the
 *  txEng_ipHeader_Const and the txEng_pseudoHeader_Const.
 *  Additionally it requests the IP Tuples from the Session. In some special cases the IP Tuple is delivered directly
 *  from @ref rx_engine and does not have to be loaded from the Session Table. The isLookUpFifo indicates this special cases.
 *  Lookup Table for the current session.
 *  Depending on the Event Type the retransmit or/and probe Timer is set.
 *  @param[in]		eventEng2txEng_event
 *  @param[in]		rxSar2txEng_upd_rsp
 *  @param[in]		txSar2txEng_upd_rsp
 *  @param[out]		txEng2rxSar_upd_req
 *  @param[out]		txEng2txSar_upd_req
 *  @param[out]		txEng2timer_setRetransmitTimer
 *  @param[out]		txEng2timer_setProbeTimer
 *  @param[out]		txEng_ipMetaFifoOut
 *  @param[out]		txEng_tcpMetaFifoOut
 *  @param[out]		txBufferReadCmd
 *  @param[out]		txEng2sLookup_rev_req
 *  @param[out]		txEng_isLookUpFifoOut
 *  @param[out]		txEng_tupleShortCutFifoOut
 */
void txEng_metaLoader(
				stream<extendedEvent>&				eventEng2txEng_event,
				stream<rxSarEntry_rsp>&				rxSar2txEng_rsp,
				stream<txTxSarReply>&				txSar2txEng_upd_rsp,
				stream<ap_uint<16> >&				txEng2rxSar_req,
				stream<txTxSarQuery>&				txEng2txSar_upd_req,
				stream<txRetransmitTimerSet>&		txEng2timer_setRetransmitTimer,
				stream<ap_uint<16> >&				txEng2timer_setProbeTimer,
				stream<ap_uint<16> >&				txEng_ipMetaFifoOut,
				stream<tx_engine_meta>&				txEng_tcpMetaFifoOut,
				stream<cmd_internal>&				txBufferReadCmd,
				stream<ap_uint<16> >&				txEng2sLookup_rev_req,
				stream<bool>&						txEng_isLookUpFifoOut,
#if (TCP_NODELAY)
				stream<bool>&						txEng_isDDRbypass,
#endif
#if (STATISTICS_MODULE)
				stream<txStatsUpdate>&  			txEngStatsUpdate,
#endif				
				stream<fourTuple>&					txEng_tupleShortCutFifoOut,
				stream<ap_uint<1> >&				readCountFifo)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	static ap_uint<1> ml_FsmState = 0;
	static bool ml_sarLoaded = false;
	static extendedEvent ml_curEvent;
	static ap_uint<32> ml_randomValue= 0x562301af; //Random seed initialization

	static ap_uint<2> 		ml_segmentCount = 0;
	static rxSarEntry_rsp	rxSar;
	txTxSarReply			txSar;
	static txTxSarReply		txSar_r;
	static tx_engine_meta 	meta;
	
	ap_uint<16> slowstart_threshold;
	ap_uint<32> pkgAddr;
	rstEvent resetEvent;
	
	static ap_uint<WINDOW_BITS> 	currLength;
	static ap_uint<WINDOW_BITS> 	usedLength;
	static ap_uint<WINDOW_BITS> 	usableWindow;
	static bool     				ackd_eq_not_ackd;
	ap_uint<WINDOW_BITS> 			usableWindow_w;
	

	
	ap_uint<16> 			next_currLength;
	ap_uint<16> 			next_usedLength;
	ap_uint<16> 			next_usableWindow;	
	ap_uint<32>             txSar_not_ackd_w;
	ap_uint<32>             txSar_ackd;
	static ap_uint<17>				remaining_window;

	switch (ml_FsmState) {
		case 0:
			if (!eventEng2txEng_event.empty()) {
				eventEng2txEng_event.read(ml_curEvent);
				readCountFifo.write(1);
				ml_sarLoaded = false;
				//NOT necessary for SYN/SYN_ACK only needs one
				switch (ml_curEvent.type) {
					case RT:
						txEng2rxSar_req.write(ml_curEvent.sessionID);
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						break;
					case TX:
						txEng2rxSar_req.write(ml_curEvent.sessionID);
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						break;
					case SYN_ACK:
						txEng2rxSar_req.write(ml_curEvent.sessionID);
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						break;
					case FIN:
						txEng2rxSar_req.write(ml_curEvent.sessionID);
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						break;
					case RST:
						// Get txSar for SEQ numb
						resetEvent = ml_curEvent;
						if (resetEvent.hasSessionID()) {
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						}
						break;
					case ACK_NODELAY:
					case ACK:
						txEng2rxSar_req.write(ml_curEvent.sessionID);
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						break;
					case SYN:
						if (ml_curEvent.rt_count != 0) {
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID));
						}
						break;
					default:
						break;
				}
				ml_FsmState = 1;
				ml_randomValue++; //make sure it doesn't become zero TODO move this out of if, but breaks my testsuite
			} //if not empty
			ml_segmentCount = 0;
			break;
		case 1:
			switch(ml_curEvent.type)
			{
			// When Nagle's algorithm disabled
			// Can bypass DDR
#if (TCP_NODELAY)
			case TX:
				if ((!rxSar2txEng_rsp.empty() && !txSar2txEng_upd_rsp.empty()) || ml_sarLoaded) {
					if (!ml_sarLoaded) {
						rxSar2txEng_rsp.read(rxSar);
						txSar2txEng_upd_rsp.read(txSar);
					}

					
					meta.window_size = rxSar.windowSize; //Get our space, Advertise at least a quarter/half, otherwise 0
					meta.ackNumb = rxSar.recvd;
					meta.seqNumb = txSar.not_ackd;
					meta.ack = 1; // ACK is always set when established
					meta.rst = 0;
					meta.syn = 0;
					meta.fin = 0;
					//meta.length = 0;

					currLength = ml_curEvent.length;
					usedLength = txSar.not_ackd(WINDOW_BITS-1,0) - txSar.ackd;
					// min_window, is the min(txSar.recv_window, txSar.cong_window)
					/*if (txSar.min_window > usedLength) {
						usableWindow = txSar.min_window - usedLength;
					}
					else {
						usableWindow = 0;
					}*/
					if (txSar.UsableWindow < ml_curEvent.length) {
						txEng2timer_setProbeTimer.write(ml_curEvent.sessionID);
					}

					meta.length = ml_curEvent.length;

					//TODO some checking
					txSar.not_ackd += ml_curEvent.length;

					txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd, 1));
					ml_FsmState = 0;

					// Send a packet only if there is data or we want to send an empty probing message
					if (meta.length != 0) {// || ml_curEvent.retransmit) //TODO retransmit boolean currently not set, should be removed
					
						txEng_ipMetaFifoOut.write(meta.length);
						txEng_tcpMetaFifoOut.write(meta);
						txEng_isLookUpFifoOut.write(true);
						txEng_isDDRbypass.write(true);
						txEng2sLookup_rev_req.write(ml_curEvent.sessionID);

						// Only set RT timer if we actually send sth, TODO only set if we change state and sent sth
						txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID));
					}//TODO if probe send msg length 1
					ml_sarLoaded = true;
#if (STATISTICS_MODULE)					
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,meta.length)); // Update Statistics
#endif
				}

				break;
#else
			case TX:
				// Sends everything between txSar.not_ackd and txSar.app
				if ((!rxSar2txEng_rsp.empty() && !txSar2txEng_upd_rsp.empty()) || ml_sarLoaded) {
					if (!ml_sarLoaded) {
						rxSar2txEng_rsp.read(rxSar);
						txSar2txEng_upd_rsp.read(txSar);

						currLength 			= txSar.currLength;
						usedLength 			= txSar.usedLength;
						usableWindow_w 		= txSar.UsableWindow;
						ackd_eq_not_ackd 	= txSar.ackd_eq_not_ackd;

						meta.window_size 	= rxSar.windowSize;	//Get our space, Advertise at least a quarter/half, otherwise 0
						meta.rst 			= 0;
						meta.syn 			= 0;
						meta.fin 			= 0;
						meta.length 		= 0;
						meta.ack 			= 1; 							// ACK is always set when established
						meta.ackNumb 		= rxSar.recvd;
						pkgAddr(31, 30) 	= (!RX_DDR_BYPASS);				// If DDR is not used in the RX start from the beginning of the memory
						pkgAddr(29, 16) 	= ml_curEvent.sessionID(13, 0);
					}
					else{

						txSar = txSar_r;

						if (!remaining_window.bit(16)) {
							usableWindow_w = remaining_window(15,0);
						}
						else {
							usableWindow_w = 0;
						}
					}

					meta.seqNumb = txSar.not_ackd;
					
					// Construct address before modifying txSar.not_ackd
					pkgAddr(15,  0) = txSar.not_ackd(15, 0); //ml_curEvent.address;

					// Check length, if bigger than Usable Window or MMS
					if (currLength <= usableWindow_w) {
						if (currLength >= MSS) { //TODO change to >= MSS, use maxSegmentCount
							// We stay in this state and sent immediately another packet
							txSar_not_ackd_w = txSar.not_ackd + MSS;
							meta.length 	= MSS;

							next_currLength = currLength - MSS; 	//Update next current length in case the module is in a loop
							next_usedLength = usedLength + MSS;
						}
						else {
							// If we sent all data, there might be a fin we have to sent too
							if (txSar.finReady && (txSar.ackd_eq_not_ackd || currLength == 0)) {
								ml_curEvent.type = FIN;
							}
							else {
								ml_FsmState = 0;			// MR TODO: this not should be conditional
							}
							// Check if small segment and if unacknowledged data in pipe (Nagle)
							if (txSar.ackd_eq_not_ackd) {
								next_currLength = txSar.not_ackd(WINDOW_BITS-1,0) - currLength; 	// todo maybe precompute
								next_usedLength = txSar.not_ackd(WINDOW_BITS-1,0) + currLength;
								txSar_not_ackd_w = txSar.not_ackd_short;
								meta.length = currLength;
							}
							else {
								txSar_not_ackd_w = txSar.not_ackd ;
								txEng2timer_setProbeTimer.write(ml_curEvent.sessionID); // 	TODO: if the app tries to write data which is not multiple of MSS this lead to retransmission
							}
							// Write back txSar not_ackd pointer
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar_not_ackd_w, 1));
						}
					}
					else {
						// code duplication, but better timing..
						if (usableWindow_w >= MSS) {
							txSar_not_ackd_w = txSar.not_ackd + MSS;
							meta.length 	 = MSS;
							next_currLength  = currLength - MSS; 
							next_usedLength  = usedLength + MSS;
						}
						else {
							// Check if we sent >= MSS data
							if (txSar.ackd_eq_not_ackd) {
								txSar_not_ackd_w = txSar.not_ackd + usableWindow_w;
								meta.length = usableWindow_w;
								next_currLength = currLength - usableWindow_w;
								next_usedLength = usedLength + usableWindow_w; 
							}
							else
								txSar_not_ackd_w = txSar.not_ackd;
							// Set probe Timer to try again later
							txEng2timer_setProbeTimer.write(ml_curEvent.sessionID);
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar_not_ackd_w, 1));
							ml_FsmState = 0;
						}
					}

					if (meta.length != 0) {
						txBufferReadCmd.write(cmd_internal(pkgAddr, meta.length));
					// Send a packet only if there is data or we want to send an empty probing message
						txEng_ipMetaFifoOut.write(meta.length);
						txEng_tcpMetaFifoOut.write(meta);
						txEng_isLookUpFifoOut.write(true);
						txEng2sLookup_rev_req.write(ml_curEvent.sessionID);
						// Only set RT timer if we actually send sth, TODO only set if we change state and sent sth
						txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID));
					}//TODO if probe send msg length 1


					remaining_window = txSar.min_window - next_usedLength;

					currLength 		= next_currLength;		// Update next iteration variables
					usedLength 		= next_usedLength;
					txSar.not_ackd  = txSar_not_ackd_w;
					ackd_eq_not_ackd = (txSar.ackd == txSar_not_ackd_w) ? true : false;
					txSar_r = txSar;
					ml_sarLoaded = true;
#if (STATISTICS_MODULE)						
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,meta.length)); // Update Statistics
#endif

				}
				break;
#endif
			case RT:
				if ((!rxSar2txEng_rsp.empty() && !txSar2txEng_upd_rsp.empty())) {
					rxSar2txEng_rsp.read(rxSar);
					txSar2txEng_upd_rsp.read(txSar);

					// Compute how many bytes have to be retransmitted, If the fin was sent, subtract 1 byte
					currLength = txSar.usedLength_rst;

					meta.ackNumb = rxSar.recvd;
					meta.seqNumb = txSar.ackd;
					meta.window_size = rxSar.windowSize;	//Get our space, Advertise at least a quarter/half, otherwise 0
					meta.ack = 1; // ACK is always set when session is established
					meta.rst = 0;
					meta.syn = 0;
					meta.fin = 0;

					// Construct address before modifying txSar.ackd
					pkgAddr(31, 30) 			= (!RX_DDR_BYPASS);					// If DDR is not used in the RX start from the beginning of the memory
					pkgAddr(30, WINDOW_BITS)  	= ml_curEvent.sessionID(13, 0);
					pkgAddr(WINDOW_BITS-1, 0) 	= txSar.ackd(WINDOW_BITS-1, 0); //ml_curEvent.address;

					// Decrease Slow Start Threshold, only on first RT from retransmitTimer
					if (!ml_sarLoaded && (ml_curEvent.rt_count == 1)) {
						if (currLength > (4*MSS)) {// max( FlightSize/2, 2*MSS) RFC:5681
							slowstart_threshold = currLength/2;
						}
						else {
							slowstart_threshold = (2 * MSS);
						}
						txEng2txSar_upd_req.write(txTxSarRtQuery(ml_curEvent.sessionID, slowstart_threshold));
					}

					// Since we are retransmitting from txSar.ackd to txSar.not_ackd, this data is already inside the usableWindow
					// => no check is required
					// Only check if length is bigger than MMS
					if (currLength > MSS) {
						// We stay in this state and sent immediately another packet
						meta.length = MSS;
						txSar.ackd += MSS;
						txSar.usedLength_rst	-= MSS;	
						// TODO replace with dynamic count, remove this
						if (ml_segmentCount == 3) {
							// Should set a probe or sth??
							//txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd, 1));
							ml_FsmState = 0;
						}
						ml_curEvent.type = RT_CONT;
						ml_segmentCount++;
					}
					else {
						meta.length = currLength;
						if (txSar.finSent) {			// In case a FIN was sent and not ack, has to be sent again 
							ml_curEvent.type = FIN;
						}
						else {
							// set RT here???
							ml_FsmState = 0;
						}
					}

					// Only send a packet if there is data
					if (meta.length != 0) {
						txBufferReadCmd.write(cmd_internal(pkgAddr, meta.length));
						txEng_ipMetaFifoOut.write(meta.length);
						txEng_tcpMetaFifoOut.write(meta);
						txEng_isLookUpFifoOut.write(true);
#if (TCP_NODELAY)
						txEng_isDDRbypass.write(false);
#endif
						txEng2sLookup_rev_req.write(ml_curEvent.sessionID);
						// Only set RT timer if we actually send sth
						txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID));
					}
					ml_sarLoaded = true;
					txSar_r = txSar;
#if (STATISTICS_MODULE)						
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,0,true)); // Update Statistics retransmission
#endif				
				}
				break;

			case RT_CONT:

				// Compute how many bytes have to be retransmitted, If the fin was sent, subtract 1 byte
				currLength = txSar_r.usedLength_rst;

				meta.ackNumb = rxSar.recvd;
				meta.seqNumb = txSar_r.ackd;
				meta.window_size = rxSar.windowSize;	//Get our space, Advertise at least a quarter/half, otherwise 0
				meta.ack = 1; // ACK is always set when session is established
				meta.rst = 0;
				meta.syn = 0;
				meta.fin = 0;

				// Construct address before modifying txSar_r.ackd
				pkgAddr(31, 30) 			= (!RX_DDR_BYPASS);					// If DDR is not used in the RX start from the beginning of the memory
				pkgAddr(30, WINDOW_BITS)  	= ml_curEvent.sessionID(13, 0);
				pkgAddr(WINDOW_BITS-1, 0) 	= txSar_r.ackd(WINDOW_BITS-1, 0); //ml_curEvent.address;

				// Decrease Slow Start Threshold, only on first RT from retransmitTimer
				if (!ml_sarLoaded && (ml_curEvent.rt_count == 1)) {
					if (currLength > (4*MSS)) {// max( FlightSize/2, 2*MSS) RFC:5681
						slowstart_threshold = currLength/2;
					}
					else {
						slowstart_threshold = (2 * MSS);
					}
					txEng2txSar_upd_req.write(txTxSarRtQuery(ml_curEvent.sessionID, slowstart_threshold));
				}

				// Since we are retransmitting from txSar_r.ackd to txSar_r.not_ackd, this data is already inside the usableWindow
				// => no check is required
				// Only check if length is bigger than MMS
				if (currLength > MSS) {
					// We stay in this state and sent immediately another packet
					meta.length = MSS;
					txSar_r.ackd += MSS;
					txSar_r.usedLength_rst	-= MSS;	
					// TODO replace with dynamic count, remove this
					if (ml_segmentCount == 3) {
						// Should set a probe or sth??
						//txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar_r.not_ackd, 1));
						ml_FsmState = 0;
					}
					ml_segmentCount++;
				}
				else {
					meta.length = currLength;
					if (txSar_r.finSent) {			// In case a FIN was sent and not ack, has to be sent again 
						ml_curEvent.type = FIN;
					}
					else {
						// set RT here???
						ml_FsmState = 0;
					}
				}

				// Only send a packet if there is data
				if (meta.length != 0) {
					txBufferReadCmd.write(cmd_internal(pkgAddr, meta.length));
					txEng_ipMetaFifoOut.write(meta.length);
					txEng_tcpMetaFifoOut.write(meta);
					txEng_isLookUpFifoOut.write(true);
#if (TCP_NODELAY)
					txEng_isDDRbypass.write(false);
#endif
					txEng2sLookup_rev_req.write(ml_curEvent.sessionID);
					// Only set RT timer if we actually send sth
					txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID));
				}

#if (STATISTICS_MODULE)						
				txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,0,true)); // Update Statistics retransmission
#endif				

				break;


			case ACK:
			case ACK_NODELAY:
				if (!rxSar2txEng_rsp.empty() && !txSar2txEng_upd_rsp.empty()) {
					rxSar2txEng_rsp.read(rxSar);
					txSar2txEng_upd_rsp.read(txSar);

					meta.ackNumb = rxSar.recvd;
					meta.seqNumb = txSar.not_ackd; //Always send SEQ
					meta.window_size = rxSar.windowSize;	//Get our space, Advertise at least a quarter/half, otherwise 0
					meta.length = 0;
					meta.ack = 1;
					meta.rst = 0;
					meta.syn = 0;
					meta.fin = 0;
					txEng_ipMetaFifoOut.write(meta.length);
					txEng_tcpMetaFifoOut.write(meta);
					txEng_isLookUpFifoOut.write(true);
					txEng2sLookup_rev_req.write(ml_curEvent.sessionID);
					ml_FsmState = 0;
#if (STATISTICS_MODULE)						
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID)); // Update Statistics ACK number of packets
#endif				
				}
				break;
			case SYN:
				if (((ml_curEvent.rt_count != 0) && !txSar2txEng_upd_rsp.empty()) || (ml_curEvent.rt_count == 0)) {
					if (ml_curEvent.rt_count != 0) {
						txSar2txEng_upd_rsp.read(txSar);
						meta.seqNumb = txSar.ackd;
					}
					else {
						//txSar = txSar_r;
						txSar.not_ackd = ml_randomValue; // FIXME better rand()
						ml_randomValue = (ml_randomValue* 8) xor ml_randomValue;
						meta.seqNumb = txSar.not_ackd;
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd+1, 1, 1));
					}
					meta.ackNumb = 0;
					//meta.seqNumb = txSar.not_ackd;
					meta.window_size = 0xFFFF;
					meta.ack = 0;
					meta.rst = 0;
					meta.syn = 1;
					meta.fin = 0;
					meta.length = 4; 									// For MSS Option, 4 bytes
#if (WINDOW_SCALE)
					meta.length = 8; 							// Anounce our window scale
					meta.rx_win_shift = WINDOW_SCALE_BITS;
#else				
					meta.length = 4;	
#endif						
					txEng_ipMetaFifoOut.write(meta.length); 		//length
					txEng_tcpMetaFifoOut.write(meta);
					txEng_isLookUpFifoOut.write(true);
					txEng2sLookup_rev_req.write(ml_curEvent.sessionID);
					// set retransmit timer
					txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID, SYN));
					//txSar_r = txSar ;
					ml_FsmState = 0;
#if (STATISTICS_MODULE)						
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,0,true,false,false,false)); // Initialize Statistics SYN
#endif				
				}
				break;
			case SYN_ACK:
				if (!rxSar2txEng_rsp.empty() && !txSar2txEng_upd_rsp.empty()) {
					rxSar2txEng_rsp.read(rxSar);
					txSar2txEng_upd_rsp.read(txSar);

					// construct SYN_ACK message
					meta.ackNumb = rxSar.recvd;
					meta.window_size = 0xFFFF;
					meta.ack = 1;
					meta.rst = 0;
					meta.syn = 1;
					meta.fin = 0;
					if (ml_curEvent.rt_count != 0) {
						meta.seqNumb = txSar.ackd;
					}
					else {
						txSar.not_ackd = ml_randomValue; // FIXME better rand();
						ml_randomValue = (ml_randomValue* 8) xor ml_randomValue;
						meta.seqNumb = txSar.not_ackd;
						txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd+1, 1, 1));
					}

#if (WINDOW_SCALE)
					meta.length = 4 + 4* (rxSar.rx_win_shift!=0); 				// For MSS Option and WScale, 8 bytes
					meta.rx_win_shift = rxSar.rx_win_shift;
#else					
					meta.length = 4; 									// For MSS Option, 4 bytes
#endif						
					txEng_ipMetaFifoOut.write(meta.length); 		//length
					txEng_tcpMetaFifoOut.write(meta);
					txEng_isLookUpFifoOut.write(true);
					txEng2sLookup_rev_req.write(ml_curEvent.sessionID);

					// set retransmit timer
					txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID, SYN_ACK));
					ml_FsmState = 0;
#if (STATISTICS_MODULE)						
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,0,false,true,false,false)); // Initialize Statistics SYN-ACK
#endif				
				}
				break;
			case FIN:
				// The term ml_sarLoaded is used for retransmission proposes 
				if ((!rxSar2txEng_rsp.empty() && !txSar2txEng_upd_rsp.empty()) || ml_sarLoaded) {
					if (!ml_sarLoaded) {
						rxSar2txEng_rsp.read(rxSar);
						txSar2txEng_upd_rsp.read(txSar);
					}
					else {
						txSar = txSar_r;
					}
					//construct FIN message
					meta.ackNumb = rxSar.recvd;
					meta.window_size = rxSar.windowSize;	//Get our space
					meta.length = 0;
					meta.ack = 1; // has to be set for FIN message as well
					meta.rst = 0;
					meta.syn = 0;
					meta.fin = 1;

					// Check if retransmission, in case of RT, we have to reuse not_ackd number
					meta.seqNumb = txSar.not_ackd - (ml_curEvent.rt_count != 0); 
					if (ml_curEvent.rt_count == 0) {
#if (!TCP_NODELAY)
						// Check if all data is sent, otherwise we have to delay FIN message
						// Set fin flag, such that probeTimer is informed
						if (txSar.app == txSar.not_ackd(WINDOW_BITS-1, 0)) {
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd+1, 1, 0, true, true));
						}
						else {
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd, 1, 0, true, false));
						}
#else
							txEng2txSar_upd_req.write(txTxSarQuery(ml_curEvent.sessionID, txSar.not_ackd+1, 1, 0, true, true));
#endif						
					}
#ifndef __SYNTHESIS__
					std::cout << "Generating a FIN packet rt_count: "<< std::dec << ml_curEvent.rt_count << "\tat " << simCycleCounter << std::endl ;
					std::cout << "\ttxSar.app " << std::dec << txSar.app << "\ttxSar.not_ackd " << txSar.not_ackd(WINDOW_BITS-1, 0) << "\tmeta.seqNumb " << meta.seqNumb(WINDOW_BITS-1, 0) << std::endl;
					std::cout << "\ttxSar.app " << std::hex << txSar.app << "\ttxSar.not_ackd " << txSar.not_ackd(WINDOW_BITS-1, 0) << "\tmeta.seqNumb " << meta.seqNumb(WINDOW_BITS-1, 0) << std::endl << std::endl;
#endif
#if (!TCP_NODELAY)
					// Check if there is a FIN to be sent
					if (meta.seqNumb(WINDOW_BITS-1, 0) == txSar.app) 
#endif						
					{
						txEng_ipMetaFifoOut.write(meta.length);
						txEng_tcpMetaFifoOut.write(meta);
						txEng_isLookUpFifoOut.write(true);
						txEng2sLookup_rev_req.write(ml_curEvent.sessionID);
						// set retransmit timer
						//txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID, FIN));
						txEng2timer_setRetransmitTimer.write(txRetransmitTimerSet(ml_curEvent.sessionID));
					}

//					txSar_r = txSar ;
					ml_FsmState = 0;
#if (STATISTICS_MODULE)						
					txEngStatsUpdate.write(txStatsUpdate(ml_curEvent.sessionID,0,false,false,true,false)); // Update Statistics FIN
#endif				
				}
				break;
			case RST:
				// Assumption RST length == 0
				resetEvent = ml_curEvent;
				if (!resetEvent.hasSessionID()) {
					txEng_ipMetaFifoOut.write(0);
					txEng_tcpMetaFifoOut.write(tx_engine_meta(0, resetEvent.getAckNumb(), 1, 1, 0, 0));
					txEng_isLookUpFifoOut.write(false);
					txEng_tupleShortCutFifoOut.write(ml_curEvent.tuple);
					//ml_FsmState = 0;
				}
				else if (!txSar2txEng_upd_rsp.empty()) {
					txSar2txEng_upd_rsp.read(txSar);
					txEng_ipMetaFifoOut.write(0);
					txEng_isLookUpFifoOut.write(true);
					txEng2sLookup_rev_req.write(resetEvent.sessionID); //there is no sessionID??
					//if (resetEvent.getAckNumb() != 0)
					//{
						txEng_tcpMetaFifoOut.write(tx_engine_meta(txSar.not_ackd, resetEvent.getAckNumb(), 1, 1, 0, 0));
					/*}
					/else
					{
						metaDataFifoOut.write(tx_engine_meta(txSar.not_ackd, rxSar.recvd, 1, 1, 0, 0));
					}*/
				}
				ml_FsmState = 0;
				break;
			} //switch
			break;
	} //switch
}

/** @ingroup tx_engine
 *  Forwards the incoming tuple from the SmartCam or RX Engine to the 2 header construction modules
 *  @param[in]	sLookup2txEng_rev_rsp
 *  @param[in]	txEng_tupleShortCutFifoIn
 *  @param[in]	txEng_isLookUpFifoIn
 *  @param[out]	txEng_ipTupleFifoOut
 *  @param[out]	txEng_tcpTupleFifoOut
 */
void txEng_tupleSplitter(	
					stream<fourTuple>&		sLookup2txEng_rev_rsp,
					stream<fourTuple>&		txEng_tupleShortCutFifoIn,
					stream<bool>&			txEng_isLookUpFifoIn,
					stream<twoTuple>&		txEng_ipTupleFifoOut,
					stream<fourTuple>&		txEng_tcpTupleFifoOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1
	static bool ts_getMeta = true;
	static bool ts_isLookUp;

	fourTuple tuple;

	if (ts_getMeta) {
		if (!txEng_isLookUpFifoIn.empty()) {
			txEng_isLookUpFifoIn.read(ts_isLookUp);
			ts_getMeta = false;
		}
	}
	else {
		if (!sLookup2txEng_rev_rsp.empty() && ts_isLookUp) {
			sLookup2txEng_rev_rsp.read(tuple);
			txEng_ipTupleFifoOut.write(twoTuple(tuple.srcIp, tuple.dstIp));
			txEng_tcpTupleFifoOut.write(tuple);
			ts_getMeta = true;
		}
		else if(!txEng_tupleShortCutFifoIn.empty() && !ts_isLookUp) {
			txEng_tupleShortCutFifoIn.read(tuple);
			txEng_ipTupleFifoOut.write(twoTuple(tuple.srcIp, tuple.dstIp));
			txEng_tcpTupleFifoOut.write(tuple);
			ts_getMeta = true;
		}
	}
}

/** @ingroup tx_engine
 * 	Reads the IP header metadata and the IP addresses. From this data it generates the IP header and streams it out.
 *  @param[in]		txEng_ipMetaDataFifoIn
 *  @param[in]		txEng_ipTupleFifoIn
 *  @param[out]		txEng_ipHeaderBufferOut
 */
void txEng_ipHeader_Const(
							stream<ap_uint<16> >&			txEng_ipMetaDataFifoIn,
							stream<twoTuple>&				txEng_ipTupleFifoIn,
							stream<axiWord>&				txEng_ipHeaderBufferOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	static twoTuple ihc_tuple;

	axiWord sendWord = axiWord(0,0,1);
	ap_uint<16> length = 0;
	ap_uint<16> length_i;


	if (!txEng_ipMetaDataFifoIn.empty() && !txEng_ipTupleFifoIn.empty()){
		
		txEng_ipMetaDataFifoIn.read(length_i);
		txEng_ipTupleFifoIn.read(ihc_tuple);
		length = length_i + 40;

		// Compose the IP header
		sendWord.data(  7,  0) = 0x45;
		sendWord.data( 15,  8) = 0;
		sendWord.data( 31, 16) = byteSwap16(length); 	//length
		sendWord.data( 47, 32) = 0;
		sendWord.data( 50, 48) = 0; 				//Flags
		sendWord.data( 63, 51) = 0x0;				//Fragment Offset
		sendWord.data( 71, 64) = 0x40;
		sendWord.data( 79, 72) = 0x06; 				// TCP
		sendWord.data( 95, 80) = 0; 				// IP header checksum 	
		sendWord.data(127, 96) = ihc_tuple.srcIp; 	// srcIp
		sendWord.data(159,128) = ihc_tuple.dstIp; 	// dstIp
		
		sendWord.last 		   = 1;
		sendWord.keep = 0xFFFFF;

#ifndef __SYNTHESIS__
		ap_uint<16> checksum;
		ap_uint<21> first_sum=0;

		ap_uint<16> tmp;

		for (int i=0 ; i < 160 ; i+=16){
			tmp( 7,0) = sendWord.data( i+15, i+8);
			tmp(15,8) = sendWord.data( i+7, i);
			first_sum += tmp;
		}
		first_sum = first_sum(15,0) + first_sum(20,16);
		first_sum = first_sum(15,0) + first_sum(16,16);
		checksum = ~(first_sum(15,0));
		
		sendWord.data( 95, 80) = (checksum(7,0),checksum(15,8));

#endif		

		txEng_ipHeaderBufferOut.write(sendWord);

	}

}

/** @ingroup tx_engine
 * 	Reads the TCP header metadata and the IP tuples. From this data it generates the TCP pseudo header and streams it out.
 *  @param[in]		tcpMetaDataFifoIn
 *  @param[in]		tcpTupleFifoIn
 *  @param[out]		dataOut
 */

void txEng_pseudoHeader_Const(
								stream<tx_engine_meta>&		tcpMetaDataFifoIn,
								stream<fourTuple>&			tcpTupleFifoIn,
								stream<axiWord>&			dataOut,
								stream<bool>& 				txEng_packet_with_payload)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	axiWord 				sendWord = axiWord(0,0,0);
	static tx_engine_meta 	phc_meta;
	static fourTuple 		phc_tuple;
	bool 					packet_has_payload;
	//static bool phc_done = true;
	ap_uint<16> 			length = 0;
	ap_uint<16> 			window_size;

	if (!tcpTupleFifoIn.empty() && !tcpMetaDataFifoIn.empty()){
		tcpTupleFifoIn.read(phc_tuple);
		tcpMetaDataFifoIn.read(phc_meta);
		length = phc_meta.length + 0x14;  // 20 bytes for the header
		
		if (phc_meta.length == 0 || phc_meta.syn){ // If length is 0 the packet or it is a SYN packet the payload is not needed
			packet_has_payload = false;
		}
		else{
			packet_has_payload = true;
		}
/*
#if (WINDOW_SCALE)
		window_size = phc_meta.window_size >> phc_meta.recv_window_scale;
		cout << "txEng_pseudoHeader_Const window_size: " << dec << phc_meta.window_size << "\tWindow scale: " << phc_meta.recv_window_scale;
		cout << "\tResulting window size:" << window_size << endl;
#else
		window_size = phc_meta.window_size;
#endif		
*/
		// Generate pseudoheader
		sendWord.data( 31,  0) 	= phc_tuple.srcIp;
		sendWord.data( 63, 32) 	= phc_tuple.dstIp;

		sendWord.data( 79, 64) 	= 0x0600; // TCP

		sendWord.data( 95, 80) 	= byteSwap16(length);
		sendWord.data(111, 96) 	= phc_tuple.srcPort; // srcPort
		sendWord.data(127,112) 	= phc_tuple.dstPort; // dstPort

		// Insert SEQ number
		sendWord.data(159,128) 	= byteSwap32(phc_meta.seqNumb);
		// Insert ACK number
		sendWord.data(191,160) 	= byteSwap32(phc_meta.ackNumb);

		sendWord.data.bit(192) 	= 0; //NS bit
		sendWord.data(195,193) 	= 0; // reserved
		/* Control bits:
		 * [200] == FIN
		 * [201] == SYN
		 * [202] == RST
		 * [203] == PSH
		 * [204] == ACK
		 * [205] == URG
		 */
		sendWord.data.bit(200) 	= phc_meta.fin; //control bits
		sendWord.data.bit(201) 	= phc_meta.syn;
		sendWord.data.bit(202) 	= phc_meta.rst;
		sendWord.data.bit(203) 	= 0;
		sendWord.data.bit(204)  = phc_meta.ack;
		sendWord.data(207, 205) = 0; //some other bits
		sendWord.data(223, 208) = byteSwap16(phc_meta.window_size);
		sendWord.data(255, 224) = 0; //urgPointer & checksum

		if (phc_meta.syn) {
			sendWord.data(263, 256) = 0x02; 			// Option Kind
			sendWord.data(271, 264) = 0x04; 			// Option length
			sendWord.data(287, 272) = byteSwap16(MSS); 	// Set Maximum MSS

#if WINDOW_SCALE
			// Only send WINDOW SCALE in SYN and SYN-ACK, in the latter only send if WSopt was received RFC 7323 1.3
			if (phc_meta.rx_win_shift !=0){
				sendWord.data(199, 196) = 0x7; 		//data offset
				sendWord.data(295, 288) = 0x03; 	// Option Kind
				sendWord.data(303, 296) = 0x03; 		// Option length
				sendWord.data(311, 304) = phc_meta.rx_win_shift;
				sendWord.data(319, 312) = 0X0; 		// End of Option List
				sendWord.keep = 0xFFFFFFFFFF;
			}
			else {
				sendWord.data(199,196) = 0x6; 		//data offset
				sendWord.keep = 0xFFFFFFFFF;
			}
#else
			sendWord.data(199,196) = 0x6; 		//data offset
			sendWord.keep = 0xFFFFFFFFF;
#endif			
		}
		else {
			sendWord.data(199,196) = 0x5; //data offset
			sendWord.keep = 0xFFFFFFFF;
		}

		sendWord.last=1;
		dataOut.write(sendWord);
		txEng_packet_with_payload.write(packet_has_payload);
	}
}

/** @ingroup tx_engine
 *  It appends the pseudo TCP header with the corresponding payload stream.
 *	@param[in]		txEng_pseudo_tcpHeader, incoming TCP pseudo header stream
 *	@param[in]		txBufferReadData, incoming payload stream
 *	@param[out]		dataOut, outgoing data stream
 */
void txEng_payload_stitcher(
					stream<axiWord>&		txEng_pseudo_tcpHeader,
					stream<bool>& 			txEng_packet_with_payload,
					stream<axiWord>&		txBufferReadData,
					stream<axiWord>&		txEngTcpSegOut,
					stream<axiWord>&		txEng2cksum)
{
#pragma HLS INLINE off
#pragma HLS LATENCY max=1
#pragma HLS pipeline II=1
	
	static axiWord 		prevWord;
	axiWord 			payload_word;
	axiWord 			sendWord = axiWord(0, 0, 0);
	bool 				packet_has_payload;


	enum teps_states {READ_PSEUDO, READ_PAYLOAD, EXTRA_WORD};
	static teps_states teps_fsm_state = READ_PSEUDO;

	switch (teps_fsm_state) {
		case READ_PSEUDO: 
			if (!txEng_pseudo_tcpHeader.empty() && !txEng_packet_with_payload.empty()){
				txEng_pseudo_tcpHeader.read(prevWord);
				txEng_packet_with_payload.read(packet_has_payload);

				if (!packet_has_payload){ 				// Payload is not needed because length==0 or is a SYN packet, send it immediately 
					txEngTcpSegOut.write(prevWord);
					txEng2cksum.write(prevWord);
				}
				else {
					teps_fsm_state = READ_PAYLOAD;
				}

			}
			break;
		case READ_PAYLOAD: 
			if (!txBufferReadData.empty()){
				txBufferReadData.read(payload_word);
				
				sendWord.data(255,  0) = prevWord.data (255,  0);		// TODO this is only valid with no TCP options
				sendWord.data(511,256) = payload_word.data(255,  0);
				sendWord.keep( 31,  0) = prevWord.keep ( 31,  0);
				sendWord.keep( 63, 32) = payload_word.keep( 31,  0);
				sendWord.last 		   = payload_word.last;

				if (payload_word.last){
					if (payload_word.keep.bit(32)){
						sendWord.last 		   = 0;
						teps_fsm_state = EXTRA_WORD;		// An extra word is necessary because the payload transaction has more than 32-bye
					}
					else {
						teps_fsm_state = READ_PSEUDO;
					}
				}

				txEngTcpSegOut.write(sendWord);
				txEng2cksum.write(sendWord);
				
				prevWord.data(255,  0) 	= 	payload_word.data(511,256);
				prevWord.keep( 31,  0) 	= 	payload_word.keep( 63, 32);
				prevWord.last       	= 	payload_word.last;

			}
			break;
		case EXTRA_WORD: 
			sendWord.data(255,  0) 	= prevWord.data(255,  0);			// TODO this is only valid with no TCP options
			sendWord.keep( 31,  0) 	= prevWord.keep( 31,  0);
			sendWord.last 		   	= 1;
			//cout << "pseudo 3: " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
			txEngTcpSegOut.write(sendWord);
			txEng2cksum.write(sendWord);
			teps_fsm_state = READ_PSEUDO;
			break;
	}

}

/** @ingroup tx_engine
 *  Reads the IP header stream and the payload stream. It also inserts TCP checksum
 *  The complete packet is then streamed out of the TCP engine. 
 *  The IP checksum must be computed and inserted after
 *  @param[in]		headerIn
 *  @param[in]		txEng_tcp_level_packet
 *  @param[in]		ipChecksumFifoIn
 *  @param[in]		tcpChecksumFifoIn
 *  @param[out]		DataOut
 */
void txEng_ip_pkt_stitcher(	
					stream<axiWord>& 		txEng_ipHeaderBufferIn,
					stream<axiWord>& 		txEng_tcp_level_packet,
					stream<ap_uint<16> >& 	txEng_tcpChecksumFifoIn,
					stream<axiWord>& 		DataOut)
{
#pragma HLS INLINE off
#pragma HLS pipeline II=1

	axiWord ip_word;
	axiWord payload;
	axiWord sendWord= axiWord(0,0,0);
	static axiWord prevWord;
	
	ap_uint<16> tcp_checksum;
	enum teips_states {READ_FIRST, READ_PAYLOAD, EXTRA_WORD};
	static teips_states teips_fsm_state = READ_FIRST;

	switch(teips_fsm_state){
		case READ_FIRST :
			if (!txEng_ipHeaderBufferIn.empty() && !txEng_tcp_level_packet.empty() && !txEng_tcpChecksumFifoIn.empty()){
				txEng_ipHeaderBufferIn.read(ip_word);
				txEng_tcp_level_packet.read(payload);
				txEng_tcpChecksumFifoIn.read(tcp_checksum);

				sendWord.data(159,  0) = ip_word.data(159,  0); 			// TODO: no IP options supported
				sendWord.keep( 19,  0) = 0xFFFFF;
				sendWord.data(511,160) = payload.data(351,  0);
				sendWord.data(304,288) = (tcp_checksum(7,0),tcp_checksum(15,8)); 	// insert checksum
				sendWord.keep( 63, 20) = payload.keep( 43,  0);

				sendWord.last 	= 0;

				if (payload.last){
					if (payload.keep.bit(44))
						teips_fsm_state = EXTRA_WORD;
					else
						sendWord.last 	= 1;
				}
				else
					teips_fsm_state = READ_PAYLOAD;

				prevWord = payload;
				//cout << "IP Stitcher 0: " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
				DataOut.write(sendWord);				
			}
			break;
		case READ_PAYLOAD :
			if (!txEng_tcp_level_packet.empty()){
					txEng_tcp_level_packet.read(payload);
		
					sendWord.data(159,  0) = prevWord.data(511,352);
					sendWord.keep( 19,  0) = prevWord.keep( 63, 44);
					sendWord.data(511,160) = payload.data(351,  0);
					sendWord.keep( 63, 20) = payload.keep( 43,  0);
					sendWord.last 	= payload.last;

					if (payload.last){
						if (payload.keep.bit(44)){
							sendWord.last 	= 0;
							teips_fsm_state = EXTRA_WORD;
						}
						else
							teips_fsm_state = READ_FIRST;
					}
					
					prevWord = payload;
					//cout << "IP Stitcher 1: " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
					DataOut.write(sendWord);
			}
			break;
		case EXTRA_WORD :
			sendWord.data(159,  0) = prevWord.data(511,352);
			sendWord.keep( 19,  0) = prevWord.keep( 63, 44);
			sendWord.last 	= 1;
			//cout << "IP Stitcher Extra  : " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
			DataOut.write(sendWord);
			teips_fsm_state = READ_FIRST;
			break;
	}

}

void txEng_PseudoHeader_Remover(
		stream<axiWord>&	dataIn, 
		stream<axiWord>&	dataOut 
	){

#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	static axiWord prevWord;
	static bool first_word = true;
	axiWord currWord;
	axiWord sendWord=axiWord(0,0,0);
	enum tpr_states {READ, EXTRA_WORD};
	static tpr_states tpr_fsm_state = READ;

	switch (tpr_fsm_state){
		case READ :
			if(!dataIn.empty()){
				dataIn.read(currWord);
				//cout << "HR In   : " << hex << currWord.data << "\tkeep: " << currWord.keep << "\tlast: " << dec << currWord.last << endl;
				if (first_word){
					first_word = false;
					if (currWord.last){										// Short packets such as ACK or SYN-ACK
						sendWord.data(415,  0) 	= currWord.data(511, 96);
						sendWord.keep( 51,  0) 	= currWord.keep( 63, 12);
						sendWord.last 			= 1;
						dataOut.write(sendWord);
						//cout << "HR Out 1   : " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
					}
				}
				else{
					sendWord.data(415,  0) 	= prevWord.data(511, 96);
					sendWord.data(511,416)	= currWord.data( 95,  0);
					sendWord.keep( 51,  0) 	= prevWord.keep( 63, 12);
					sendWord.keep( 63, 52)	= currWord.keep( 11,  0);

					sendWord.last = currWord.last;
					
					if (currWord.last){
						if (currWord.keep.bit(12)){
							tpr_fsm_state = EXTRA_WORD;
							sendWord.last = 0;
						}
					}
					//cout << "HR Out 2  : " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
					dataOut.write(sendWord);
				}
				if (currWord.last)
					first_word = true;

				prevWord = currWord;				
			}
			break;
		case EXTRA_WORD :
				sendWord.data(415,  0) 	= prevWord.data(511, 96);
				sendWord.keep( 51,  0) 	= prevWord.keep( 63, 12);
				sendWord.last 			= 1;
				dataOut.write(sendWord);
				tpr_fsm_state = READ;
				//cout << "HR Out Extra 3  : " << hex << sendWord.data << "\tkeep: " << sendWord.keep << "\tlast: " << dec << sendWord.last << endl;
			break;
	}

}

/** @ingroup tx_engine
 *  @param[in]		eventEng2txEng_event
 *  @param[in]		rxSar2txEng_upd_rsp
 *  @param[in]		txSar2txEng_upd_rsp
 *  @param[in]		txBufferReadData_unaligned
 *  @param[in]		sLookup2txEng_rev_rsp
 *  @param[out]		txEng2rxSar_upd_req
 *  @param[out]		txEng2txSar_upd_req
 *  @param[out]		txEng2timer_setRetransmitTimer
 *  @param[out]		txEng2timer_setProbeTimer
 *  @param[out]		txBufferReadCmd
 *  @param[out]		txEng2sLookup_rev_req
 *  @param[out]		ipTxData
 */
void tx_engine(	stream<extendedEvent>&			eventEng2txEng_event,
				stream<rxSarEntry_rsp>&		    rxSar2txEng_rsp,
				stream<txTxSarReply>&			txSar2txEng_upd_rsp,
				stream<axiWord>&				txBufferReadData_unaligned,
#if (TCP_NODELAY)
				stream<axiWord>&				txApp2txEng2PseudoHeader,
#endif
#if (STATISTICS_MODULE)
				stream<txStatsUpdate>&  		txEngStatsUpdate,
#endif					
				stream<fourTuple>&				sLookup2txEng_rev_rsp,
				stream<ap_uint<16> >&			txEng2rxSar_req,
				stream<txTxSarQuery>&			txEng2txSar_upd_req,
				stream<txRetransmitTimerSet>&	txEng2timer_setRetransmitTimer,
				stream<ap_uint<16> >&			txEng2timer_setProbeTimer,
				stream<mmCmd>&					txBufferReadCmd,
				stream<ap_uint<16> >&			txEng2sLookup_rev_req,
				stream<axiWord>&				ipTxData,
				stream<ap_uint<1> >&			readCountFifo,
				stream<axiWord>&				tx_pseudo_packet_to_checksum,
				stream<ap_uint<16> >&			tx_pseudo_packet_res_checksum)
{
#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INLINE

	// Memory Read delay around 76 cycles, 10 cycles/packet, so keep meta of at least 8 packets
	static stream<tx_engine_meta>		txEng_metaDataFifo("txEng_metaDataFifo");
	#pragma HLS stream variable=txEng_metaDataFifo depth=16
	#pragma HLS DATA_PACK variable=txEng_metaDataFifo

	static stream<ap_uint<16> >			txEng_ipMetaFifo("txEng_ipMetaFifo");
	#pragma HLS stream variable=txEng_ipMetaFifo depth=16

	static stream<tx_engine_meta>		txEng_tcpMetaFifo("txEng_tcpMetaFifo");
	#pragma HLS stream variable=txEng_tcpMetaFifo depth=16
	#pragma HLS DATA_PACK variable=txEng_tcpMetaFifo

	static stream<axiWord>		txEng_ipHeaderBuffer("txEng_ipHeaderBuffer");
	#pragma HLS stream variable=txEng_ipHeaderBuffer depth=512 					// Ip header is 1 word. We don't have to worry about the depth because it uses the same amount as depth=32
	#pragma HLS DATA_PACK variable=txEng_ipHeaderBuffer
	
	static stream<axiWord>		txEng_pseudo_tcpHeader("txEng_pseudo_tcpHeader");
	#pragma HLS stream variable=txEng_pseudo_tcpHeader depth=512 				// TCP pseudo header is 1 word
	#pragma HLS DATA_PACK variable=txEng_pseudo_tcpHeader
	
	static stream<axiWord>		tx_Eng_pseudo_pkt("tx_Eng_pseudo_pkt");			// It carries pseudo header plus TCP payload if applies
	#pragma HLS stream variable=tx_Eng_pseudo_pkt depth=512
	#pragma HLS DATA_PACK variable=tx_Eng_pseudo_pkt

	static stream<axiWord>		txEng_tcp_level_packet("txEng_tcp_level_packet");
	#pragma HLS stream variable=txEng_tcp_level_packet depth=512  				// critical, has to keep complete packet for checksum computation
	#pragma HLS DATA_PACK variable=txEng_tcp_level_packet
	
	static stream<fourTuple> 		txEng_tupleShortCutFifo("txEng_tupleShortCutFifo");
	#pragma HLS stream variable=txEng_tupleShortCutFifo depth=4
	#pragma HLS DATA_PACK variable=txEng_tupleShortCutFifo

	static stream<bool>				txEng_isLookUpFifo("txEng_isLookUpFifo");
	#pragma HLS stream variable=txEng_isLookUpFifo depth=32

	static stream<twoTuple>			txEng_ipTupleFifo("txEng_ipTupleFifo");
	#pragma HLS stream variable=txEng_ipTupleFifo depth=4
	#pragma HLS DATA_PACK variable=txEng_ipTupleFifo

	static stream<fourTuple>		txEng_tcpTupleFifo("txEng_tcpTupleFifo");
	#pragma HLS stream variable=txEng_tcpTupleFifo depth=4
	#pragma HLS DATA_PACK variable=txEng_tcpTupleFifo

	static stream<cmd_internal> txMetaloader2memAccessBreakdown("txMetaloader2memAccessBreakdown");
	#pragma HLS stream variable=txMetaloader2memAccessBreakdown depth=32
	#pragma HLS DATA_PACK variable=txMetaloader2memAccessBreakdown
	
	static stream<memDoubleAccess> memAccessBreakdown("memAccessBreakdown");
	#pragma HLS stream variable=memAccessBreakdown depth=32
	#pragma HLS DATA_PACK variable=memAccessBreakdown
	
	static stream<bool> txEng_isDDRbypass("txEng_isDDRbypass");
	#pragma HLS stream variable=txEng_isDDRbypass depth=32

	static stream<axiWord>		txBufferReadData_aligned("txBufferReadData_aligned");
	#pragma HLS stream variable=txBufferReadData_aligned depth=512  
	#pragma HLS DATA_PACK variable=txBufferReadData_aligned

	static stream<bool>				txEng_packet_with_payload("txEng_packet_with_payload");
	#pragma HLS stream variable=txEng_packet_with_payload depth=32
	#pragma HLS DATA_PACK variable=txEng_packet_with_payload

	txEng_metaLoader(	
				eventEng2txEng_event,
				rxSar2txEng_rsp,
				txSar2txEng_upd_rsp,
				txEng2rxSar_req,
				txEng2txSar_upd_req,
				txEng2timer_setRetransmitTimer,
				txEng2timer_setProbeTimer,
				txEng_ipMetaFifo,
				txEng_tcpMetaFifo,
				txMetaloader2memAccessBreakdown,
				txEng2sLookup_rev_req,
				txEng_isLookUpFifo,
#if (TCP_NODELAY)
				txEng_isDDRbypass,
#endif
#if (STATISTICS_MODULE)
				txEngStatsUpdate,
#endif						
				txEng_tupleShortCutFifo,
				readCountFifo);
	tx_ReadMemAccessBreakdown(
				txMetaloader2memAccessBreakdown, 
				txBufferReadCmd, 
				memAccessBreakdown);

	txEng_tupleSplitter(	
				sLookup2txEng_rev_rsp,
				txEng_tupleShortCutFifo,
				txEng_isLookUpFifo,
				txEng_ipTupleFifo,
				txEng_tcpTupleFifo);

	txEng_ipHeader_Const(
				txEng_ipMetaFifo, 
				txEng_ipTupleFifo, 
				txEng_ipHeaderBuffer);

	txEng_pseudoHeader_Const(
				txEng_tcpMetaFifo, 
				txEng_tcpTupleFifo, 
				txEng_pseudo_tcpHeader,
				txEng_packet_with_payload);

	tx_MemDataRead_aligner(
				txBufferReadData_unaligned,
				memAccessBreakdown,
	#if (TCP_NODELAY)
				txEng_isDDRbypass,
				txApp2txEng2PseudoHeader,
	#endif
				txBufferReadData_aligned);

	txEng_payload_stitcher(	
				txEng_pseudo_tcpHeader,
				txEng_packet_with_payload,
				txBufferReadData_aligned,
				tx_Eng_pseudo_pkt,
				tx_pseudo_packet_to_checksum);

	txEng_PseudoHeader_Remover(
				tx_Eng_pseudo_pkt,
				txEng_tcp_level_packet);

	txEng_ip_pkt_stitcher(
				txEng_ipHeaderBuffer, 
				txEng_tcp_level_packet, 
				tx_pseudo_packet_res_checksum, 
				ipTxData);
}
