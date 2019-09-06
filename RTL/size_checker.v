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


//  Description   : This Module completes the packet to 60 bytes when the packets are smaller than 60 bytes

`timescale 1ns/1ps

module size_checker#
    (
        // Width of S_AXI data bus
        parameter C_AXIS_DATA_WIDTH             = 512,
        parameter TUSER_WIDTH                   =  1,
        parameter TDEST_WIDTH                   =  1

    )
    (
      (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 CLK CLK" *)
      (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS, ASSOCIATED_RESET RST_N" *)
      input  wire                      CLK            ,
      (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 RST_N RST" *)
      (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
      input  wire                      RST_N          ,
    
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDATA" *)  
      input  wire              [511:0] S_AXIS_TDATA   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TKEEP" *)
      input  wire               [63:0] S_AXIS_TKEEP   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TLAST" *) 
      input  wire                      S_AXIS_TLAST   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TVALID" *) 
      input  wire                      S_AXIS_TVALID  ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDEST" *) 
      input  wire  [TDEST_WIDTH-1 : 0] S_AXIS_TDEST   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TUSER" *) 
      input  wire  [TUSER_WIDTH-1 : 0] S_AXIS_TUSER   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TREADY" *) 
      output wire                      S_AXIS_TREADY  ,

      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDATA" *)   
      output reg               [511:0] M_AXIS_TDATA   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *)
      output reg                [63:0] M_AXIS_TKEEP   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TLAST" *)
      output reg                       M_AXIS_TLAST   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *)
      output reg                       M_AXIS_TVALID  ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDEST" *) 
      output reg   [TDEST_WIDTH-1 : 0] M_AXIS_TDEST   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TUSER" *)
      output reg   [TUSER_WIDTH-1 : 0] M_AXIS_TUSER   ,
      (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *)
      input  wire                      M_AXIS_TREADY 

);

  reg     first_transaction= 1'b1;

  assign  S_AXIS_TREADY = M_AXIS_TREADY;

  always @(*) begin
    M_AXIS_TDATA      <= S_AXIS_TDATA;    
    M_AXIS_TKEEP      <= S_AXIS_TKEEP;    
    M_AXIS_TLAST      <= S_AXIS_TLAST;    
    M_AXIS_TVALID     <= S_AXIS_TVALID;    
    M_AXIS_TDEST      <= S_AXIS_TDEST;    
    M_AXIS_TUSER      <= S_AXIS_TUSER;          

    if (S_AXIS_TVALID && S_AXIS_TREADY && S_AXIS_TLAST) begin         // one-transaction packet
      if (first_transaction && ~S_AXIS_TKEEP[59]) begin
        M_AXIS_TKEEP    <= {4'd0,{60{1'b1}}};                         // complete packet to the minimum size (60 bytes)
      end
    end
  end


  always @(posedge CLK) begin
    if (~RST_N) begin
      first_transaction   <= 1'b1;
    end
    else begin
      if (S_AXIS_TVALID && S_AXIS_TREADY) begin                         // when a valid transaction is active tie low flag
        first_transaction   <= 1'b0;
      end

      if (S_AXIS_TVALID && S_AXIS_TREADY && S_AXIS_TLAST) begin         // one-transaction packet
        first_transaction <= 1'b1;                                      // Asserting flag with the last transaction of a packet (it could be one-transaction packet)
      end
    end
  end

endmodule