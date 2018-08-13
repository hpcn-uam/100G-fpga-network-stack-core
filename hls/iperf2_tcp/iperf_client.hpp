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
#include "../TOE/toe.hpp"
#include "../TOE/common_utilities/common_utilities.hpp"

using namespace hls;


void iperf2_client( 
                    stream<ap_uint<16> >&           listenPortReq, 
                    stream<listenPortStatus>&       listenPortRes,
                    stream<appNotification>&        rxAppNotification, 
                    stream<appReadRequest>&         rxApp_readRequest,
                    stream<ap_uint<16> >&           rxApp_readRequest_RspID, 
                    stream<axiWord>&                rxData_to_rxApp,
                    stream<ipTuple>&                txApp_openConnection, 
                    stream<openStatus>&             txApp_openConnStatus,
                    stream<ap_uint<16> >&           closeConnection,
                    stream<appTxMeta>&              txAppDataReqMeta, 
                    stream<appTxRsp>&               txAppDataReqStatus,
                    stream<axiWord>&                txAppData_to_TOE,
                    stream<txApp_client_status>&    txAppNewClientNoty, 

                    /* AXI4-Lite registers */
                    ap_uint< 1>&                    runExperiment,      
                    ap_uint< 1>&                    dualModeEn,         
                    ap_uint< 1>&                    useTimer,         
                    ap_uint<64>&                    runTime,         
                    ap_uint<14>&                    numConnections,         
                    ap_uint<32>&                    transfer_size,      
                    ap_uint<12>&                    packet_mss,     
                    ap_uint<32>&                    ipDestination,
                    ap_uint<16>&                    dstPort,
                    ap_uint<16>&                    maxConnections);
