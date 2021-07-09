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

#ifndef _TOE_HPP_DEFINED_
#define _TOE_HPP_DEFINED_

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <hls_stream.h>
#include "ap_int.h"
#include <stdint.h>
#include <vector>
#include "ap_axi_sdata.h"

#define ETH_INTERFACE_WIDTH 512

static const ap_uint<16> MSS=4096; //536


// TCP_NODELAY flag, to disable Nagle's Algorithm
#define TCP_NODELAY 1

// RX_DDR_BYPASS flag, to enable DDR bypass on RX path
// This MACRO also modifies the buffer address for the TX path
// When DDR is not bypassed the RX buffers have the first 2 GB of the memory
// and the TX buffers have the remaining 2 GB
// However, when the DDR is bypassed the TX buffers have the 4 GB
#define RX_DDR_BYPASS 1

// FAST_RETRANSMIT flag, to enable TCP fast recovery/retransmit mechanism
#define FAST_RETRANSMIT 1

// WINDOW_SCALE, to enable TCP Window Scale option
#define WINDOW_SCALE 1

// This macro defines the amount of bits for the window scale option
// It is used to extend the window size, increasing the throughput.
static const uint8_t WINDOW_SCALE_BITS = 2;

// Statistics such as number of packets, bytes and retransmissions are implemented
#define STATISTICS_MODULE 0

// If the window scale option is enable the the MAX session have to be computed
#if (WINDOW_SCALE)

// Do NOT touch from this point
static const uint16_t WINDOW_BITS=(16+WINDOW_SCALE_BITS);

// If the Window is 64 KB there are 64K possible sessions.
// Since we want to scale the window size the number of connection is reduced by the (2^WINDOW_SCALE_BITS)
//static const uint16_t MAX_SESSIONS = (65536/(1<<WINDOW_SCALE_BITS)/(1+!RX_DDR_BYPASS));

#else
static const uint8_t  WINDOW_BITS=16;
//static const uint16_t MAX_SESSIONS = 10000;
#endif
// Delete afterwards
static const uint16_t MAX_SESSIONS = 64;

static const uint32_t BUFFER_SIZE=(1<<WINDOW_BITS);
static const ap_uint<WINDOW_BITS> CONGESTION_WINDOW_MAX = (BUFFER_SIZE-2048);

#define CLOCK_PERIOD 0.003103

extern uint32_t packetCounter;
extern uint32_t cycleCounter;
extern unsigned int	simCycleCounter;
// Forward declarations.
struct rtlSessionUpdateRequest;
struct rtlSessionUpdateReply;
struct rtlSessionLookupReply;
struct rtlSessionLookupRequest;

using namespace hls;

#ifndef __SYNTHESIS__
static const ap_uint<32> TIME_64us		= 1;
static const ap_uint<32> TIME_128us		= 1;
static const ap_uint<32> TIME_1ms		= 1;
static const ap_uint<32> TIME_5ms		= 1;
static const ap_uint<32> TIME_25ms		= 1;
static const ap_uint<32> TIME_50ms		= 1;
static const ap_uint<32> TIME_100ms		= 1;
static const ap_uint<32> TIME_250ms		= 1;
static const ap_uint<32> TIME_500ms		= 2;
static const ap_uint<32> TIME_1s		= 5;
static const ap_uint<32> TIME_5s		= 6;
static const ap_uint<32> TIME_7s		= 7;
static const ap_uint<32> TIME_10s		= 8;
static const ap_uint<32> TIME_15s		= 9;
static const ap_uint<32> TIME_20s		= 10;
static const ap_uint<32> TIME_30s		= 11;
static const ap_uint<32> TIME_60s		= 12;
static const ap_uint<32> TIME_120s		= 13;
#else
static const ap_uint<32> TIME_64us		= (       64.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_128us		= (      128.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_1ms		= (     1000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_5ms		= (     5000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_25ms		= (    25000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_50ms		= (    50000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_100ms		= (   100000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_250ms		= (   250000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_500ms		= (   500000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_1s		= (  1000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_5s		= (  5000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_7s		= (  7000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_10s		= ( 10000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_15s		= ( 15000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_20s		= ( 20000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_30s		= ( 30000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_60s		= ( 60000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_120s		= (120000000.0/CLOCK_PERIOD/MAX_SESSIONS) + 1;
#endif


enum eventType {TX, RT, ACK, SYN, SYN_ACK, FIN, RST, ACK_NODELAY, RT_CONT};
/*
 * There is no explicit LISTEN state
 * CLOSE-WAIT state is not used, since the FIN is sent out immediately after we receive a FIN, the application is simply notified
 * FIN_WAIT_2 is also unused
 * LISTEN is merged into CLOSED
 */
enum sessionState {CLOSED, SYN_SENT, SYN_RECEIVED, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSING, TIME_WAIT, LAST_ACK};

typedef ap_axiu<ETH_INTERFACE_WIDTH,0,0,0> axiWord;


struct fourTuple
{
	ap_uint<32> srcIp;
	ap_uint<32> dstIp;
	ap_uint<16> srcPort;
	ap_uint<16> dstPort;
	fourTuple() {}
	fourTuple(ap_uint<32> srcIp, ap_uint<32> dstIp, ap_uint<16> srcPort, ap_uint<16> dstPort)
			  : srcIp(srcIp), dstIp(dstIp), srcPort(srcPort), dstPort(dstPort) {}
};

struct threeTuple
{
	ap_uint<16> myPort;
	ap_uint<16> theirPort;
	ap_uint<32> theirIp;
	threeTuple() {}
	threeTuple(ap_uint<16> myPort, ap_uint<16> theirPort, ap_uint<32> theirIp)
			  : myPort(myPort), theirPort(theirPort), theirIp(theirIp) {}

	bool operator<(const threeTuple& other) const
	{
		if (theirIp < other.theirIp) {
			return true;
		}
		else if (theirIp == other.theirIp) {
			if (myPort < other.myPort) {
				return true;
			}
			else if (myPort == other.myPort) {
				if (theirPort < other.theirPort) {
					return true;
				}
			}
		}
		return false;
	}
};

inline bool operator < (fourTuple const& lhs, fourTuple const& rhs) {
		return lhs.dstIp < rhs.dstIp || (lhs.dstIp == rhs.dstIp && lhs.srcIp < rhs.srcIp);
	}

struct ipTuple
{
	ap_uint<32>	ip_address;
	ap_uint<16>	ip_port;
	ipTuple() {}
	ipTuple(ap_uint<32> a , ap_uint<16> p)
		: ip_address(a), ip_port(p) {}

};

struct sessionLookupQuery
{
	fourTuple	tuple;
	bool		allowCreation;
	sessionLookupQuery() {}
	sessionLookupQuery(fourTuple tuple, bool allowCreation)
			:tuple(tuple), allowCreation(allowCreation) {}
};

struct sessionLookupReply
{
	ap_uint<16> 	sessionID;
	bool			hit;
	sessionLookupReply() {}
	sessionLookupReply(ap_uint<16> id, bool hit)
			:sessionID(id), hit(hit) {}
};


struct stateQuery
{
	ap_uint<16> 	sessionID;
	sessionState 	state;
	ap_uint<1>		write;
	stateQuery() {}
	stateQuery(ap_uint<16> id)
				:sessionID(id), state(CLOSED), write(0){}
	stateQuery(ap_uint<16> id, sessionState state, ap_uint<1> write)
				:sessionID(id), state(state), write(write) {}
};

/** @ingroup rx_sar_table
 *  @ingroup rx_engine
 *  @ingroup tx_engine
 *  This struct defines an entry of the @ref rx_sar_table
 */
struct rxSarEntry
{
	ap_uint<32> 			recvd;
	ap_uint<WINDOW_BITS> 	appd;
#if (WINDOW_SCALE)	
	ap_uint<4>				rx_win_shift;
#endif	
};

struct rxSarEntry_rsp
{
	ap_uint<32> 			recvd;
	ap_uint<16> 			windowSize;
#if (WINDOW_SCALE)	
	ap_uint<4>				rx_win_shift;
#endif	
};

struct rxSarRecvd
{
	ap_uint<16> 			sessionID;
	ap_uint<32> 			recvd;
	ap_uint<1> 				write;
	ap_uint<1> 				init;
#if (WINDOW_SCALE)
	ap_uint<4>				rx_win_shift;
#endif	
	rxSarRecvd() {}
	rxSarRecvd(ap_uint<16> id)
				:sessionID(id), recvd(0), write(0), init(0) {}
	rxSarRecvd(ap_uint<16> id, ap_uint<32> recvd, ap_uint<1> write)
				:sessionID(id), recvd(recvd), write(write), init(0) {}
	rxSarRecvd(ap_uint<16> id, ap_uint<32> recvd, ap_uint<1> write, ap_uint<1> init)
					:sessionID(id), recvd(recvd), write(write), init(init) {}

#if (WINDOW_SCALE)
	rxSarRecvd(ap_uint<16> id, ap_uint<32> recvd, ap_uint<1> write, ap_uint<1> init, ap_uint<4> wsopt)
					:sessionID(id), recvd(recvd), write(write), init(init), rx_win_shift(wsopt)  {}
#endif					

};

struct rxSarAppd
{
	ap_uint<16> 			sessionID;
	ap_uint<WINDOW_BITS> 	appd;
	ap_uint<1>				write;
	rxSarAppd() {}
	rxSarAppd(ap_uint<16> id)
				:sessionID(id), appd(0), write(0) {}
	rxSarAppd(ap_uint<16> id, ap_uint<WINDOW_BITS> appd)
				:sessionID(id), appd(appd), write(1) {}
};

struct txSarEntry
{
	ap_uint<32> 			ackd;
	ap_uint<32> 			not_ackd;
	ap_uint<16> 			recv_window;
#if (WINDOW_SCALE)
	ap_uint< 4>				tx_win_shift;
#endif	
	ap_uint<WINDOW_BITS> 	cong_window;
	ap_uint<WINDOW_BITS> 	slowstart_threshold;
	ap_uint<WINDOW_BITS> 	app;
	ap_uint<2>				count;
	bool					fastRetransmitted;
	bool					finReady;
	bool					finSent;
	bool 					use_cong_window;
};

struct rxTxSarQuery
{
	ap_uint<16> 			sessionID;
	ap_uint<32> 			ackd;
	ap_uint<16> 			recv_window;
	ap_uint<2>  			count;
	bool					fastRetransmitted;
	ap_uint<1> 				write;
#if (WINDOW_SCALE)
	ap_uint<4>				tx_win_shift;
	bool 					tx_win_shift_write;
#endif	
	ap_uint<WINDOW_BITS>	cong_window;
	rxTxSarQuery () {}
	rxTxSarQuery(ap_uint<16> id)
				:sessionID(id), ackd(0), recv_window(0), count(0), fastRetransmitted(false), write(0) {}
	rxTxSarQuery(ap_uint<16> id, ap_uint<32> ackd, ap_uint<WINDOW_BITS> recv_win, ap_uint<WINDOW_BITS> cong_win, ap_uint<2> count, bool fastRetransmitted)
				:sessionID(id), ackd(ackd), recv_window(recv_win), cong_window(cong_win), count(count), fastRetransmitted(fastRetransmitted), write(1) {}
#if (WINDOW_SCALE)
	rxTxSarQuery(ap_uint<16> id, ap_uint<32> ackd, ap_uint<16> recv_win, ap_uint<WINDOW_BITS> cong_win, ap_uint<2> count, bool fastRetransmitted, 
				 ap_uint<4> ws)
				:sessionID(id), ackd(ackd), recv_window(recv_win), cong_window(cong_win), count(count), fastRetransmitted(fastRetransmitted), write(1),
				 tx_win_shift_write(0),  tx_win_shift(ws) {}

	rxTxSarQuery(ap_uint<16> id, ap_uint<32> ackd, ap_uint<16> recv_win, ap_uint<WINDOW_BITS> cong_win, ap_uint<2> count, bool fastRetransmitted, 
				 bool tx_win_shift_write, ap_uint<4> ws)
				:sessionID(id), ackd(ackd), recv_window(recv_win), cong_window(cong_win), count(count), fastRetransmitted(fastRetransmitted), write(1),
				 tx_win_shift_write(tx_win_shift_write),  tx_win_shift(ws) {}
#endif				
};

struct txTxSarQuery
{
	ap_uint<16> 			sessionID;
	ap_uint<32> 			not_ackd;
	ap_uint<1>				write;
	ap_uint<1>				init;
	bool					finReady;
	bool					finSent;
	bool					isRtQuery;
	txTxSarQuery() {}
	txTxSarQuery(ap_uint<16> id)
				:sessionID(id), not_ackd(0), write(0), init(0), finReady(false), finSent(false), isRtQuery(false) {}
	txTxSarQuery(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write)
				:sessionID(id), not_ackd(not_ackd), write(write), init(0), finReady(false), finSent(false), isRtQuery(false) {}
	txTxSarQuery(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write, ap_uint<1> init)
				:sessionID(id), not_ackd(not_ackd), write(write), init(init), finReady(false), finSent(false), isRtQuery(false) {}
	txTxSarQuery(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write, ap_uint<1> init, bool finReady, bool finSent)
				:sessionID(id), not_ackd(not_ackd), write(write), init(init), finReady(finReady), finSent(finSent), isRtQuery(false) {}
	txTxSarQuery(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write, ap_uint<1> init, bool finReady, bool finSent, bool isRt)
				:sessionID(id), not_ackd(not_ackd), write(write), init(init), finReady(finReady), finSent(finSent), isRtQuery(isRt) {}
};

struct txTxSarRtQuery : public txTxSarQuery
{
	txTxSarRtQuery() {}
	txTxSarRtQuery(const txTxSarQuery& q)
			:txTxSarQuery(q.sessionID, q.not_ackd, q.write, q.init, q.finReady, q.finSent, q.isRtQuery) {}
	txTxSarRtQuery(ap_uint<16> id, ap_uint<WINDOW_BITS> ssthresh)
			:txTxSarQuery(id, ssthresh, 1, 0, false, false, true) {}
	ap_uint<WINDOW_BITS> getThreshold()
	{
	return not_ackd(WINDOW_BITS-1, 0);
	}
};

struct txAppTxSarQuery
{
	ap_uint<16> 			sessionID;
	//ap_uint<16> ackd;
	ap_uint<WINDOW_BITS> 	mempt;
	bool					write;
	txAppTxSarQuery() {}
	txAppTxSarQuery(ap_uint<16> id)
				:sessionID(id), mempt(0), write(false) {}
	txAppTxSarQuery(ap_uint<16> id, ap_uint<WINDOW_BITS> pt)
			:sessionID(id), mempt(pt), write(true) {}
};

struct rxTxSarReply
{
	ap_uint<32>				prevAck;
	ap_uint<32> 			nextByte;
	ap_uint<2>				count;
	bool					fastRetransmitted;
#if (WINDOW_SCALE)
	ap_uint<4>				tx_win_shift;
#endif	
	ap_uint<WINDOW_BITS>	cong_window;
	ap_uint<WINDOW_BITS> 	slowstart_threshold;
	rxTxSarReply() {}
	rxTxSarReply(ap_uint<32> ack, ap_uint<32> next, ap_uint<WINDOW_BITS> cong_win, ap_uint<WINDOW_BITS> sstresh, ap_uint<2> count, bool fastRetransmitted)
			:prevAck(ack), nextByte(next), cong_window(cong_win), slowstart_threshold(sstresh), count(count), fastRetransmitted(fastRetransmitted) {}
#if (WINDOW_SCALE)
	rxTxSarReply(ap_uint<32> ack, ap_uint<32> next, ap_uint<WINDOW_BITS> cong_win, ap_uint<WINDOW_BITS> sstresh, ap_uint<2> count, bool fastRetransmitted, ap_uint<4> txws)
		:prevAck(ack), nextByte(next), cong_window(cong_win), slowstart_threshold(sstresh), count(count), fastRetransmitted(fastRetransmitted), tx_win_shift(txws) {}		
#endif			
};

struct txAppTxSarReply
{
	ap_uint<16> 			sessionID;
	ap_uint<WINDOW_BITS> 	ackd;
	ap_uint<WINDOW_BITS> 	mempt;
#if (TCP_NODELAY)
	ap_uint<WINDOW_BITS> 	min_window;
#endif	
	txAppTxSarReply() {}
	txAppTxSarReply(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<WINDOW_BITS> pt)
		:sessionID(id), ackd(ackd), mempt(pt) {}
#if (TCP_NODELAY)
	txAppTxSarReply(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<WINDOW_BITS> pt, ap_uint<WINDOW_BITS> min_window)
		:sessionID(id), ackd(ackd), mempt(pt), min_window(min_window) {}
#endif
};

struct txAppTxSarPush
{
	ap_uint<16> 			sessionID;
	ap_uint<WINDOW_BITS> 	app;
	txAppTxSarPush() {}
	txAppTxSarPush(ap_uint<16> id, ap_uint<WINDOW_BITS> app)
			:sessionID(id), app(app) {}
};

struct txSarAckPush
{
	ap_uint<16> 			sessionID;
	ap_uint<WINDOW_BITS> 	ackd;
#if (TCP_NODELAY)
	ap_uint<WINDOW_BITS> 	min_window;
#endif	
	ap_uint<1>	init;
	txSarAckPush() {}
#if (!TCP_NODELAY)
	txSarAckPush(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd)
		:sessionID(id), ackd(ackd), init(0) {}
	txSarAckPush(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<1> init)
		:sessionID(id), ackd(ackd), init(init) {}
#else
	txSarAckPush(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<WINDOW_BITS> min_window)
		:sessionID(id), ackd(ackd), min_window(min_window), init(0) {}
	txSarAckPush(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<WINDOW_BITS> min_window, ap_uint<1> init)
		:sessionID(id), ackd(ackd), min_window(min_window), init(init) {}
#endif
};

struct txTxSarReply
{
	ap_uint<32> 			ackd;
	ap_uint<32> 			not_ackd;
	ap_uint<WINDOW_BITS> 	min_window;
	ap_uint<WINDOW_BITS> 	app;
	bool					finReady;
	bool					finSent;
	ap_uint<WINDOW_BITS> 	currLength;
	ap_uint<WINDOW_BITS> 	usedLength;
	ap_uint<WINDOW_BITS> 	usedLength_rst;
	ap_uint<WINDOW_BITS> 	UsableWindow;

	bool 					ackd_eq_not_ackd;
	ap_uint<32> 			not_ackd_short;
	bool 					usablewindow_b_mss;
	ap_uint<32> 			not_ackd_plus_mss;
#if (WINDOW_SCALE)
	ap_uint<4>				tx_win_shift;	
#endif	

	//ap_uint<16> Send_Window;
	txTxSarReply() {}
	txTxSarReply(ap_uint<32> ack, ap_uint<32> nack, ap_uint<WINDOW_BITS> min_window, ap_uint<WINDOW_BITS> app, bool finReady, bool finSent)
		:ackd(ack), not_ackd(nack), min_window(min_window), app(app), finReady(finReady), finSent(finSent) {}
};

struct rxRetransmitTimerUpdate {
	ap_uint<16> sessionID;
	bool		stop;
	rxRetransmitTimerUpdate() {}
	rxRetransmitTimerUpdate(ap_uint<16> id)
				:sessionID(id), stop(0) {}
	rxRetransmitTimerUpdate(ap_uint<16> id, bool stop)
				:sessionID(id), stop(stop) {}
};

struct txRetransmitTimerSet {
	ap_uint<16> sessionID;
	eventType	type;
	txRetransmitTimerSet() {}
	txRetransmitTimerSet(ap_uint<16> id)
				:sessionID(id), type(RT) {} //FIXME??
	txRetransmitTimerSet(ap_uint<16> id, eventType type)
				:sessionID(id), type(type) {}
};

struct event
{
	eventType				type;
	ap_uint<16>				sessionID;
	ap_uint<WINDOW_BITS> 	address;
	ap_uint<16> 			length;
	ap_uint<3>				rt_count;
	event() {}
	//event(const event&) {}
	event(eventType type, ap_uint<16> id)
			:type(type), sessionID(id), address(0), length(0), rt_count(0) {}
	event(eventType type, ap_uint<16> id, ap_uint<3> rt_count)
			:type(type), sessionID(id), address(0), length(0), rt_count(rt_count) {}
	event(eventType type, ap_uint<16> id, ap_uint<WINDOW_BITS> addr, ap_uint<16> len)
			:type(type), sessionID(id), address(addr), length(len), rt_count(0) {}
	event(eventType type, ap_uint<16> id, ap_uint<WINDOW_BITS> addr, ap_uint<16> len, ap_uint<3> rt_count)
			:type(type), sessionID(id), address(addr), length(len), rt_count(rt_count) {}
};

struct extendedEvent : public event
{
	fourTuple	tuple;
	extendedEvent() {}
	extendedEvent(const event& ev)
			:event(ev.type, ev.sessionID, ev.address, ev.length, ev.rt_count) {}
	extendedEvent(const event& ev, fourTuple tuple)
			:event(ev.type, ev.sessionID, ev.address, ev.length, ev.rt_count), tuple(tuple) {}
};

struct rstEvent : public event
{
	rstEvent() {}
	rstEvent(const event& ev)
		:event(ev.type, ev.sessionID, ev.address, ev.length, ev.rt_count) {}
	rstEvent(ap_uint<32> seq)
			//:event(RST, 0, false), seq(seq) {}
			:event(RST, 0, seq(31, 16), seq(15, 0), 0) {}
	rstEvent(ap_uint<16> id, ap_uint<32> seq)
			:event(RST, id, seq(31, 16), seq(15, 0), 1) {}
			//:event(RST, id, true), seq(seq) {}
	rstEvent(ap_uint<16> id, ap_uint<32> seq, bool hasSessionID)
		:event(RST, id, seq(31, 16), seq(15, 0), hasSessionID) {}
		//:event(RST, id, hasSessionID), seq(seq) {}
	ap_uint<32> getAckNumb()
	{
		ap_uint<32> seq;					// TODO FIXME REVIEW
		seq(31, 16) = address;
		seq(15, 0) = length;
		return seq;
	}
	bool hasSessionID()
	{
		return (rt_count != 0);
	}
};

struct mmCmd
{
	ap_uint<23>	bbt;
	ap_uint<1>	type;
	ap_uint<6>	dsa;
	ap_uint<1>	eof;
	ap_uint<1>	drr;
	ap_uint<32>	saddr;
	ap_uint<4>	tag;
	ap_uint<4>	rsvd;
	mmCmd() {}
	mmCmd(ap_uint<32> addr, ap_uint<16> len)
		:bbt(len), type(1), dsa(0), eof(1), drr(1), saddr(addr), tag(0), rsvd(0) {}
};

struct cmd_internal {
	ap_uint<16>				length;
	ap_uint<32>				addr;
	ap_uint<WINDOW_BITS+1>	next_addr;

	cmd_internal() {}
	cmd_internal(ap_uint<32>	addr, ap_uint<16> length)
		:addr(addr), length(length), next_addr(addr + length) {}

//	ap_uint<WINDOW_BITS+1> compute_next_address (){
//		return addr(WINDOW_BITS-1,0) + length;
//	}	
};

struct mmStatus
{
	ap_uint<4>	tag;
	ap_uint<1>	interr;
	ap_uint<1>	decerr;
	ap_uint<1>	slverr;
	ap_uint<1>	okay;
};


struct openStatus
{
	ap_uint<16>	sessionID;
	bool		success;
	openStatus() {}
	openStatus(ap_uint<16> id, bool success)
		:sessionID(id), success(success) {}
};

struct appNotification
{
	ap_uint<16>			sessionID;
	ap_uint<16>			length;
	ap_uint<32>			ipAddress;
	ap_uint<16>			dstPort;
	bool				closed;
	appNotification() {}
	appNotification(ap_uint<16> id, ap_uint<16> len, ap_uint<32> addr, ap_uint<16> port)
				:sessionID(id), length(len), ipAddress(addr), dstPort(port), closed(false) {}
	appNotification(ap_uint<16> id, bool closed)
				:sessionID(id), length(0), ipAddress(0),  dstPort(0), closed(closed) {}
	appNotification(ap_uint<16> id, ap_uint<32> addr, ap_uint<16> port, bool closed)
				:sessionID(id), length(0), ipAddress(addr),  dstPort(port), closed(closed) {}
	appNotification(ap_uint<16> id, ap_uint<16> len, ap_uint<32> addr, ap_uint<16> port, bool closed)
			:sessionID(id), length(len), ipAddress(addr), dstPort(port), closed(closed) {}
};

struct txApp_client_status
{
	ap_uint<16> 		sessionID;			// Tells to the tx application the ID
	/* 
	 * not used for the time being, tells to the app the negotiated buffer size. 
	 * 65536 * (buffersize+1) 
	 */
	ap_uint<8>			buffersize;			
	ap_uint<11>			max_transfer_size;	// max 2048
	/*
	 * Tells the application that the design uses TCP_NODELAY which means that the data 
	 * transfers has to be done in chunks of a maximum size
	 */
	bool 				tcp_nodelay;
	bool				buffer_empty;		// Tells when the Tx buffer is empty. It is also used as a way to indicate that a new connection was opened. 
	txApp_client_status() {}
	txApp_client_status(ap_uint<16> id, ap_uint<8> buffersize, ap_uint<11> max_trans, bool tcp_nodelay, bool buf_empty ):
		sessionID(id), buffersize(buffersize), max_transfer_size(max_trans),
		tcp_nodelay(tcp_nodelay), buffer_empty(buf_empty) {}
};

struct appReadRequest
{
	ap_uint<16> sessionID;
	//ap_uint<16> address;
	ap_uint<16> length;
	appReadRequest() {}
	appReadRequest(ap_uint<16> id, ap_uint<16> len)
			:sessionID(id), length(len) {}
};

struct appTxMeta
{
	ap_uint<16> sessionID;
	ap_uint<16> length;
	appTxMeta() {}
	appTxMeta(ap_uint<16> id, ap_uint<16> len)
		:sessionID(id), length(len) {}
};

enum txApp_error_msg {NO_ERROR, ERROR_NOCONNECTION, ERROR_NOSPACE, ERROR_WINDOW};

struct appTxRsp
{
	ap_uint<16> 				length;
	txApp_error_msg				error;
	ap_uint<WINDOW_BITS> 		remaining_space;
	appTxRsp() {}
	appTxRsp(ap_uint<16> len, ap_uint<WINDOW_BITS> rem_space, txApp_error_msg err)
		:length(len), remaining_space(rem_space), error(err) {}
};

struct memDoubleAccess
{
	bool 		double_access;
	ap_uint<6>	offset;
	memDoubleAccess() {}
	memDoubleAccess(bool double_access, ap_uint<6>	offset)
		: double_access(double_access), offset(offset) {}
};

struct listenPortStatus
{	
	bool 			open_successfully;
	bool 			wrong_port_number;
	bool			already_open;
	ap_uint<16>		port_number;
	
};


struct rxStatsUpdate {
	ap_uint<16> 	id;
	ap_uint<16>		length;
	bool 			syn;
	bool 			syn_ack;
	bool 			fin;

	rxStatsUpdate(){}
	rxStatsUpdate(ap_uint<16> id)
		: id(id), length(0), syn(0), syn_ack(0), fin(0){}
	rxStatsUpdate(ap_uint<16> id, ap_uint<16> length)
		: id(id), length(length), syn(0), syn_ack(0), fin(0){}		
	rxStatsUpdate(ap_uint<16> id, ap_uint<16> length, bool syn, bool syn_ack, bool fin)
		: id(id), length(length), syn(syn), syn_ack(syn_ack), fin(fin) {}		

};

struct txStatsUpdate {
	ap_uint<16> 	id;
	ap_uint<16>		length;
	bool 			syn;
	bool 			syn_ack;
	bool 			fin;
	bool 			reTx;

	txStatsUpdate(){}
	txStatsUpdate(ap_uint<16> id)
		: id(id), length(0), syn(0), syn_ack(0), fin(0), reTx(0) {}
	txStatsUpdate(ap_uint<16> id, ap_uint<16> length)
		: id(id), length(length), syn(0), syn_ack(0), fin(0), reTx(0) {}
	txStatsUpdate(ap_uint<16> id, ap_uint<16> length, bool reTx)
		: id(id), length(length), syn(0), syn_ack(0), fin(0), reTx(reTx) {}	
	txStatsUpdate(ap_uint<16> id, ap_uint<16> length, bool syn, bool syn_ack, bool fin, bool reTx)
		: id(id), length(length), syn(syn), syn_ack(syn_ack), fin(fin), reTx(reTx) {}		

};

struct statsRegs {
	bool 			readEnable;
    ap_uint<16> 	userID;
    ap_uint<64> 	txBytes;
    ap_uint<54> 	txPackets;
    ap_uint<54> 	txRetransmissions;
    ap_uint<64> 	rxBytes;
    ap_uint<54> 	rxPackets;
    ap_uint<32> 	connectionRTT;
};

struct iperf_regs {
     ap_uint< 1>    runExperiment;      
     ap_uint< 1>    dualModeEn;         
     ap_uint< 1>    useTimer;         
     ap_uint<64>    runTime;         
     ap_uint<14>    numConnections;         
     ap_uint<32>    transfer_size;      
     ap_uint<16>    packet_mss;
     ap_uint<32>    ipDestination;
     ap_uint<16>    dstPort;
     ap_uint<16>    maxConnections;
     ap_uint<16>    currentState;
     ap_uint< 1>    errorOpenningConnection;
};

void toe(	
			// Data & Memory Interface
			stream<axiWord>&						ipRxData,
#if (!RX_DDR_BYPASS)
			stream<mmStatus>&						rxBufferWriteStatus,
			stream<mmCmd>&							rxBufferWriteCmd,
			stream<mmCmd>&							rxBufferReadCmd,
			stream<axiWord>&						rxBufferReadData,
			stream<axiWord>&						rxBufferWriteData,
#endif
			stream<mmStatus>&						txBufferWriteStatus,
			stream<axiWord>&						txBufferReadData,
			stream<axiWord>&						ipTxData,		
			stream<mmCmd>&							txBufferWriteCmd,
			stream<mmCmd>&							txBufferReadCmd,
			stream<axiWord>&						txBufferWriteData,
			// SmartCam Interface
			stream<rtlSessionLookupReply>&			sessionLookup_rsp,
			stream<rtlSessionUpdateReply>&			sessionUpdate_rsp,
			stream<rtlSessionLookupRequest>&		sessionLookup_req,
			stream<rtlSessionUpdateRequest>&		sessionUpdate_req,
			// Application Interface
			stream<ap_uint<16> >&					listenPortRequest,
			// This is disabled for the time being, due to complexity concerns
			stream<appReadRequest>&					rxApp_readRequest,
			stream<ipTuple>&						openConnReq,
			stream<ap_uint<16> >&					closeConnReq,
			stream<appTxMeta>&					   	txDataReqMeta,
			stream<axiWord>&						txApp_Data2send, 

			stream<listenPortStatus>&				listenPortResponse, 				
			stream<appNotification>&				rxAppNotification,
			stream<txApp_client_status>& 			rxEng2txAppNewClientNoty,
			stream<ap_uint<16> >&					rxApp_readRequest_RspID,
			stream<axiWord>&						rxDataRsp,							
			stream<openStatus>&						openConnRsp,
			stream<appTxRsp>&						txAppDataRsp,

#if (STATISTICS_MODULE)
		   	statsRegs& 								stat_regs,			
#endif	

			//IP Address Input
			ap_uint<32>&							myIpAddress,
			//statistic
			ap_uint<16>&							regSessionCount,
			stream<axiWord>&						tx_pseudo_packet_to_checksum,	
			stream<ap_uint<16> >&					tx_pseudo_packet_res_checksum,
			stream<axiWord>&						rxEng_pseudo_packet_to_checksum,
			stream<ap_uint<16> >&					rxEng_pseudo_packet_res_checksum);

#endif
