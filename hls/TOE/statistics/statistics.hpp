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
#ifndef _STATISTICS_HPP_
#define _STATISTICS_HPP_

#include "../toe.hpp"

struct statsRxEntry {
	ap_uint<64>		rxBytes;
	ap_uint<54>		rxPackets;
};

struct statsTxEntry {
	ap_uint<64>		txBytes;
	ap_uint<54>		txPackets;
	ap_uint<54>		ReTx;
};


void toeStatistics (
    stream<rxStatsUpdate>&  rxStatsUpd,
    stream<txStatsUpdate>&  txStatsUpd,
    statsRegs&              stat_regs);

#endif