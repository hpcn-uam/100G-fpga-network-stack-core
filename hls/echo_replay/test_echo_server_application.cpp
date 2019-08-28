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
