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

#include "statistics.hpp"



/**
 * @brief      This function provides a mechanism to monitorise
 *             the RX and TX path.
 *
 * @param      userID             The user id
 * @param      txBytes            The transmit bytes
 * @param      txPackets          The transmit packets
 * @param      txRetransmissions  The transmit retransmissions
 * @param      rxBytes            The receive bytes
 * @param      rxPackets          The receive packets
 * @param      connectionRTT      The connection rtt
 */
void toeStatistics (
    stream<rxStatsUpdate>&  rxStatsUpd,
    stream<txStatsUpdate>&  txStatsUpd,
    statsRegs&              stat_regs){
#pragma HLS INLINE off
#pragma HLS pipeline II=1   

    static statsRxEntry stats_rx_table[MAX_SESSIONS];
    static statsTxEntry stats_tx_table[MAX_SESSIONS];
    #pragma HLS RESOURCE variable=stats_rx_table core=RAM_T2P_BRAM
    #pragma HLS RESOURCE variable=stats_tx_table core=RAM_T2P_BRAM
    #pragma HLS DEPENDENCE variable=stats_rx_table inter false
    #pragma HLS DEPENDENCE variable=stats_tx_table inter false

    static ap_uint<64>  cycle_counter = 0;
    bool                readEnable_r = 0;
    rxStatsUpdate   rxInfo;
    txStatsUpdate   txInfo;
    static ap_uint<16> tx_id_r = 0xFFFF;
    static ap_uint<16> rx_id_r = 0xFFFF;
    static statsRxEntry stats_rx_table_r;
    static statsTxEntry stats_tx_table_r;
    statsRxEntry stats_rx_table_a;
    statsTxEntry stats_tx_table_a;

    if (stat_regs.readEnable && !readEnable_r){   // Only update user output with a rising edge
        stat_regs.txBytes             = stats_tx_table[stat_regs.userID].txBytes; 
        stat_regs.txPackets           = stats_tx_table[stat_regs.userID].txPackets;
        stat_regs.txRetransmissions   = stats_tx_table[stat_regs.userID].ReTx;
        stat_regs.rxBytes             = stats_rx_table[stat_regs.userID].rxBytes;
        stat_regs.rxPackets           = stats_rx_table[stat_regs.userID].rxPackets;
        stat_regs.connectionRTT       = 0;
    }
    else{
        if (!rxStatsUpd.empty()){
            rxStatsUpd.read(rxInfo);

            if (rxInfo.syn || rxInfo.syn_ack){
                stats_rx_table_a.rxBytes   = 0;
                stats_rx_table_a.rxPackets = 0;
            }
            else {
                if (tx_id_r == rxInfo.id){  // If the ID is the same as previous do not read memory
                    stats_rx_table_a.rxBytes   =stats_rx_table_r.rxBytes + rxInfo.length;
                    stats_rx_table_a.rxPackets =stats_rx_table_r.rxPackets + 1;
                }   
                else {
                    stats_rx_table_a.rxBytes   = stats_rx_table[rxInfo.id].rxBytes + rxInfo.length;
                    stats_rx_table_a.rxPackets = stats_rx_table[rxInfo.id].rxPackets++;
                }
            }
            stats_rx_table[rxInfo.id] = stats_rx_table_a;
            stats_rx_table_r = stats_rx_table_a;

            //std::cout << "Rx statistics " << std::dec <<  std::endl;
            //std::cout << "id " << rxInfo.id;
            //std::cout << "\tlength " << rxInfo.length;
            //std::cout << "\tsyn " << rxInfo.syn;
            //std::cout << "\tsyn_ack " << rxInfo.syn_ack;
            //std::cout << "\tfin " << rxInfo.fin <<  std::endl;
            //
            rx_id_r = rxInfo.id;
        }
        else if (!txStatsUpd.empty()){
            txStatsUpd.read(txInfo);

            if (txInfo.syn || txInfo.syn_ack){
                stats_tx_table_a.txBytes   = 0;
                stats_tx_table_a.txPackets = 0;
                stats_tx_table_a.ReTx = 0;
            }
            else {
                if (tx_id_r == txInfo.id){  // If the ID is the same as previous do not read memory
                    stats_tx_table_a.txBytes   =stats_tx_table_r.txBytes + txInfo.length;
                    stats_tx_table_a.txPackets =stats_tx_table_r.txPackets + 1;
                    stats_tx_table_a.ReTx      =stats_tx_table_r.ReTx + txInfo.reTx;
                }   
                else {
                    stats_tx_table_a.txBytes   = stats_tx_table[txInfo.id].txBytes + txInfo.length;
                    stats_tx_table_a.txPackets = stats_tx_table[txInfo.id].txPackets++;
                    stats_tx_table_a.ReTx      = stats_tx_table[txInfo.id].ReTx + txInfo.reTx;
                }
            }
            stats_tx_table[txInfo.id] = stats_tx_table_a;
            stats_tx_table_r = stats_tx_table_a;

            //std::cout << "Tx statistics " << std::dec <<  std::endl;
            //std::cout << "id " << txInfo.id;
            //std::cout << "\tlength " << txInfo.length;
            //std::cout << "\tsyn " << txInfo.syn;
            //std::cout << "\tsyn_ack " << txInfo.syn_ack;
            //std::cout << "\tfin " << txInfo.fin;
            //std::cout << "\treTx " << txInfo.reTx <<  std::endl;
            tx_id_r = txInfo.id;
        }
    } 


    readEnable_r = stat_regs.readEnable;
    cycle_counter++;    // Increment cycle counter every cycle

}
