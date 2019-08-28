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
/*
 * This piece of hardware is designed to limit the amount of clock cycles between
 * two consecutive packets. This is done in order not to saturate the TOE.
 * The current limitation is a packet with a segment size of 1024.
 * segment size 1024
 * tcp header     20
 * ip header      20
 *             ------
 *              1064
 * transactions = ceiling(1064/64) = 17
 */

`timescale 1 ns / 1 ps

`ifdef MODEL_TECH
    `define SIMULATION
`elsif XILINX_SIMULATOR
    `define SIMULATION
`endif

module cycle_limiter (

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
  output wire [    511 : 0]             M_AXIS_TDATA,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *)
  output wire [     63 : 0]             M_AXIS_TKEEP,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *)    
  output wire                           M_AXIS_TVALID,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TLAST" *)    
  output wire                           M_AXIS_TLAST,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *)      
  input  wire                           M_AXIS_TREADY

);


    reg  [  4:  0]              cycle_counter = 16'h0;
    reg                         internal_ready=  1'b1;


    assign M_AXIS_TDATA     = S_AXIS_TDATA;
    assign M_AXIS_TKEEP     = S_AXIS_TKEEP;
    assign M_AXIS_TLAST     = S_AXIS_TLAST;

    assign M_AXIS_TVALID    = S_AXIS_TVALID & /* 1'b1  */internal_ready;   // Assert output valid depending on internal ready
    assign S_AXIS_TREADY    = M_AXIS_TREADY & /* 1'b1  */internal_ready;   // Assert output ready depending on internal ready

    localparam integer        MAX_CYCLE_COUNT = 17;

    localparam       INITIAL_STATE   = 0,
                     WAITING_PACKET  = 1,
                     WAIT_PACKET_END = 2,
                     FORWARD_PACKET  = 3;       


    reg  [ 1 : 0]             fsm_state = INITIAL_STATE;


    always @(posedge clk) begin
      if (~rst_n) begin
        internal_ready <= 1'b1;
        fsm_state      <= INITIAL_STATE;
      end
      else begin
        case (fsm_state)

          INITIAL_STATE: begin                        // Initialize variables and jump to WAITING_PACKET
            internal_ready <= 1'b1;
            fsm_state      <= WAITING_PACKET;          
          end

          WAITING_PACKET: begin
            internal_ready <= 1'b1;
            if (S_AXIS_TVALID && M_AXIS_TREADY) begin // Verify a valid packet
              if (S_AXIS_TLAST) begin                 // Short packet, clear internal ready
                internal_ready <= 1'b0;
                fsm_state      <= WAIT_PACKET_END;  
              end
              else begin
                fsm_state      <= FORWARD_PACKET;  
              end
              cycle_counter  <= 'h1;  
            end
          end

          WAIT_PACKET_END : begin                         // The packet is shorter than minimum transactions, then a wait it is necessary
            if (cycle_counter == MAX_CYCLE_COUNT) begin
              internal_ready <= 1'b1;
              fsm_state      <= WAITING_PACKET;           // When the counter reaches its maximum go to wait for an other packer
            end
            cycle_counter = cycle_counter + 1;            
          end

          FORWARD_PACKET: begin
            if (S_AXIS_TVALID && M_AXIS_TREADY && S_AXIS_TLAST) begin // Verify if a packet ends
              if (cycle_counter != MAX_CYCLE_COUNT) begin             // If the packet is shorter than the minimum jump to wait cycles, and clear the internal ready
                internal_ready <= 1'b0;
                fsm_state      <= WAIT_PACKET_END;
              end
              else begin
                internal_ready <= 1'b1;
                fsm_state      <= WAITING_PACKET;
              end
            end
            if (cycle_counter != MAX_CYCLE_COUNT) begin
              cycle_counter = cycle_counter + 1;
            end
          end

          default: begin
            internal_ready <= 1'b1;
            fsm_state      <= WAITING_PACKET;             
          end
        endcase
        
      end
    end


`ifdef SIMULATION
    reg [(8*23)-1:0]  state_char;
    always @(*) begin
        case (fsm_state)
          INITIAL_STATE :
            state_char = "INITIAL_STATE";
          WAITING_PACKET :
            state_char = "WAITING_PACKET";
          WAIT_PACKET_END :
            state_char = "WAIT_PACKET_END";
          FORWARD_PACKET :
            state_char = "FORWARD_PACKET";
          default :
            state_char = "UKNOWN";
        endcase
    end
`endif  

endmodule