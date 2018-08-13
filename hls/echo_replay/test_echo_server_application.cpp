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
#include "echo_server_application.hpp"
#include <iostream>

using namespace hls;



int main()
{

	stream<ap_uint<16> > 			listenPort("listenPort");
	stream<bool> 					listenPortStatus("listenPortStatus");
	stream<appNotification> 		notifications;
	stream<appReadRequest> 			readRequest;
	stream<ap_uint<16> > 			rxMetaData;
	stream<axiWord> 				rxData;
	stream<ipTuple> 				openConnection;
	stream<openStatus> 				openConStatus;
	stream<ap_uint<16> > 			closeConnection;
	stream<appTxMeta> 				txMetaData;
	stream<axiWord> 				txData;
	stream<appTxRsp>				txStatus;
	stream<txApp_client_status>		txApp_client_notification;

	int end_loop=0;

	do {
		echo_server_application(	
			listenPort, 
			listenPortStatus,
			notifications, 
			readRequest,
			rxMetaData, 
			rxData,
			openConnection, 
			openConStatus,
			closeConnection,
			txMetaData, 
			txData,
			txStatus,
			txApp_client_notification);

		if (end_loop !=0){
			end_loop++;
		}

		if (!listenPort.empty()) {
			listenPort.read();
			end_loop = 1;
			listenPortStatus.write(true);
		}


	} while (end_loop < 4);
	return 0;
}
