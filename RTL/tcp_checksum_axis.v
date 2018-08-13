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


    wire [ 15:  0]              result_computation_i;
    reg  [ 15:  0]              previous_computation = 16'h0;


    /* Stage 0*/
    
    reg [511:  0]               data_r;
    reg                         valid_r;
    reg                         ready_r;
    reg                         last_r;
    integer                     i;

    /* If the channel is busy clear s_ready*/
    assign S_AXIS_TREADY = !M_AXIS_TVALID | M_AXIS_TREADY;

    /* Register data in and verify keep signal to ensure that only the valid data is taken into account */
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
          previous_computation <= 16'h0;                        
        end
        else begin
          previous_computation <= result_computation_i;
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
        M_AXIS_TVALID <= M_AXIS_TVALID & !M_AXIS_TREADY; 
        if (valid_r && ready_r && last_r) begin
          M_AXIS_TDATA    <= ~result_computation_i;
          M_AXIS_TVALID   <= 1'b1;
        end
      end
    end


    cksum_528_r03 #(
      .REGISTER_INPUT_DATA(               0)
    )
    checksum_i ( 
      .SysClk_in   (                    clk),
      .PktData     (                 data_r),
      .pre_cks     (   previous_computation),
      .ChksumFinal (   result_computation_i)
    );

endmodule