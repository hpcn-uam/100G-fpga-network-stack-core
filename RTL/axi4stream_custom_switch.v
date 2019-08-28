/************************************************
BSD 3-Clause License

Copyright (c) 2019, HPCN Group, UAM Spain (hpcn-uam.es)
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

/*Custom AXI4-Switch 
  Two slaves one master
  The priority to slave 0 is absolute, no need for
  one clock cycle to arbitrate.
*/


`timescale 1ns/1ps

module custom_switch (

  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 s_axi_aclk CLK" *)
  (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S0_AXI:S1_AXI:M_AXI, ASSOCIATED_RESET s_axi_aresetn" *)
  input  wire         s_axi_aclk            , // Clock
  (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 s_axi_aresetn RST" *)
  (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
  input  wire         s_axi_aresetn         , // Asynchronous reset active low

  /*Packet Stream Input Interface 0*
   This interface has the highest priority*/
  input wire          S0_AXI_TVALID,
  input wire          S0_AXI_TLAST ,
  input wire  [511:0] S0_AXI_TDATA ,
  input wire  [ 63:0] S0_AXI_TSTRB ,
  output reg          S0_AXI_TREADY,

  input wire          S1_AXI_TVALID,
  input wire          S1_AXI_TLAST ,
  input wire  [511:0] S1_AXI_TDATA ,
  input wire  [ 63:0] S1_AXI_TSTRB ,
  output reg          S1_AXI_TREADY,

  output reg          M_AXI_TVALID,
  output reg          M_AXI_TLAST ,
  output reg  [511:0] M_AXI_TDATA ,
  output reg  [ 63:0] M_AXI_TSTRB ,
  input wire          M_AXI_TREADY

);
  
  reg selectInput     = 0;
  reg noFinishPacket  = 0;

  always @(posedge s_axi_aclk) begin
    if (~s_axi_aresetn) begin
      selectInput     <= 1'b0;
      noFinishPacket  <= 1'b0;
    end
    else begin
      if (selectInput == 1'b0 && noFinishPacket == 1'b0) begin
        if (S0_AXI_TVALID == 1'b0 && S1_AXI_TVALID == 1'b1)
          selectInput     <= 1'b1;
      end
      
      if (M_AXI_TVALID && M_AXI_TREADY) begin
        /*While sending a packet set this variable to avoid switching slave in the middle of a packet*/
        noFinishPacket  <= 1'b1;
        /*After a packet ends always return to slave 0, in such a way that it always has the highest priority*/  
        if (M_AXI_TLAST) begin
          selectInput     <= 1'b0;
          noFinishPacket  <= 1'b0;
        end
      end
      
    end
  end

  /*Assign to the master the proper slave interface*/
  always @(*) begin
    S0_AXI_TREADY <= (selectInput == 0) ? M_AXI_TREADY : 1'b0;
    S1_AXI_TREADY <= (selectInput == 1) ? M_AXI_TREADY : 1'b0;

    if (selectInput == 1'b0) begin
      M_AXI_TVALID  <= S0_AXI_TVALID;
      M_AXI_TLAST   <= S0_AXI_TLAST;
      M_AXI_TDATA   <= S0_AXI_TDATA;
      M_AXI_TSTRB   <= S0_AXI_TSTRB;
    end
    else begin
      M_AXI_TVALID  <= S1_AXI_TVALID;
      M_AXI_TLAST   <= S1_AXI_TLAST;
      M_AXI_TDATA   <= S1_AXI_TDATA;
      M_AXI_TSTRB   <= S1_AXI_TSTRB;
    end
  end
     
endmodule
