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

module tcp_checksum_axis (

  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 clk CLK" *)
  (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS, ASSOCIATED_RESET rst_n" *)
  input  wire                           clk            ,
  (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 rst_n RST" *)
  (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
  input  wire                           rst_n          ,

(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDATA" *)  
  input wire [    511 : 0]              S_AXIS_TDATA,
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TKEEP" *)
  input wire [     63 : 0]              S_AXIS_TKEEP,
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TVALID" *)    
  input wire                            S_AXIS_TVALID,
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TLAST" *)    
  input wire                            S_AXIS_TLAST,
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TREADY" *)      
  output wire                           S_AXIS_TREADY,

(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDATA" *)
  output reg [     15 : 0]              M_AXIS_TDATA,
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *)  
  output reg                            M_AXIS_TVALID,
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *)  
  input wire                            M_AXIS_TREADY

);


    reg  [ 15:  0]              prevWord0_r = 16'h0;
    reg  [ 15:  0]              prevWord1_r = 16'h0;
    wire [ 15:  0]              ResWord0_w  ;
    wire [ 15:  0]              ResWord1_w  ;
    wire [ 15:  0]              result      ;

    /* Stage 0*/

    reg                         internal_ready = 1'b1;
    integer                     i;
    wire         [ 16 :  0]     final_add_r;
    wire         [ 16 :  0]     final_add_o;

    reg          [511 :  0]     data_r;
    reg                         valid_r;
    reg                         ready_r;
    reg                         last_r;

    /* If the channel is busy clear s_ready*/
    assign S_AXIS_TREADY = (!M_AXIS_TVALID || M_AXIS_TREADY) && internal_ready;

    /* Register input data and verify keep signal to ensure that only the valid data is taken into account */
    always @(posedge clk) begin
      for (i = 0 ; i < 64 ; i=i+1) begin
        if (S_AXIS_TKEEP[i]) begin
          data_r[i*8 +: 8] <= S_AXIS_TDATA[i*8 +:8];
        end
        else begin
          data_r[i*8 +: 8] <= 8'h0;
        end
      end
      valid_r   <= S_AXIS_TVALID;
      ready_r   <= S_AXIS_TREADY;
      last_r    <= S_AXIS_TLAST;
    end


    /* Register the output of the checksum computation, that it is the current checksum
       and clear it when the packet finishes*/
    always @(posedge clk) begin
      if (valid_r && ready_r) begin
        if (last_r) begin
          prevWord0_r     <= 16'h0;
          prevWord1_r     <= 16'h0;                      
        end
        else begin
          prevWord0_r     <= ResWord0_w;
          prevWord1_r     <= ResWord1_w;
        end
      end
    end

    /* Write the checksum when a last is received, also keep valid set until the data is consumed*/
    always @(posedge clk) begin
      if (~rst_n) begin
        M_AXIS_TDATA    <= 16'h0;
        M_AXIS_TVALID   <= 1'b0;
      end
      else begin
        M_AXIS_TVALID   <= M_AXIS_TVALID & !M_AXIS_TREADY; 
        if (valid_r && ready_r && last_r) begin
          M_AXIS_TDATA    <= ~result;
          M_AXIS_TVALID   <= 1'b1;
        end
      end
    end

    /* Manage internal_ready */
    always @(posedge clk) begin
      if (S_AXIS_TVALID && S_AXIS_TREADY &&S_AXIS_TLAST) begin
        internal_ready <= 1'b0;
      end
      else if (M_AXIS_TVALID && M_AXIS_TREADY) begin
        internal_ready <= 1'b1;
      end
    end


    checksumRed34to2 checksumRed34to2_i (
      .clk         (           clk),
      .currentData (        data_r),

      .prevWord0   (   prevWord0_r),
      .prevWord1   (   prevWord1_r),
      .ResWord0    (    ResWord0_w),
      .ResWord1    (    ResWord1_w)
    );

    assign  final_add_r  = ResWord0_w + ResWord1_w;  
    assign  final_add_o  = ResWord0_w + ResWord1_w + 1;  

    assign result = final_add_r[16] ? final_add_o : final_add_r;

endmodule