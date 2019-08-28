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

/* This piece of hardware performs the reduction of 34 words into 
 * 2 words taking advantage of the Carry Save Adder.
 * An extra addition with carry-logic has to be performed in order to 
 * get the correct result value
 */

module checksumRed34to2 (
          input  wire                   clk, 
          input  wire    [511 :  0]     currentData,
          input  wire    [ 15 :  0]     prevWord0,
          input  wire    [ 15 :  0]     prevWord1,
          
          output wire    [ 15 :  0]     ResWord0,
          output wire    [ 15 :  0]     ResWord1,
          output wire    [ 15:   0]     result
);


      reg [15 :  0] packetData[0 : 33];         // Arrange data into a matrix

      /*Assign the input vector a matrix, in order to perform the reduction CSA tree*/

      integer i;

      always @(*) begin
        for (i=0 ; i < 32 ; i = i+1) begin
          packetData[i][15:8] <= currentData[(i*16)+7  -: 8];
          packetData[i][ 7:0] <= currentData[(i*16)+15 -: 8];
        end
        packetData[32] <= prevWord0;
        packetData[33] <= prevWord1;
      end

      /* First Level of reduction 
       Four groups of 7-bit CSA each plus
       one group of 6-bit CSA */ 

      wire [15 : 0] sum_L1[0:14];

      genvar j;
      generate
        for (j = 0 ; j < 16 ; j= j+1) begin
          reducer_7to3_i reduc0( 
            .x6( packetData[ 6][j]), 
            .x5( packetData[ 5][j]), 
            .x4( packetData[ 4][j]),
            .x3( packetData[ 3][j]), 
            .x2( packetData[ 2][j]),  
            .x1( packetData[ 1][j]), 
            .x0( packetData[ 0][j]),
            .s2( sum_L1    [ 2][(j+2) % 16]), 
            .s1( sum_L1    [ 1][(j+1) % 16]), 
            .s0( sum_L1    [ 0][j]) 
          ); 
          reducer_7to3_i reduc1( 
            .x6(packetData[13][j]), 
            .x5(packetData[12][j]), 
            .x4(packetData[11][j]),
            .x3(packetData[10][j]), 
            .x2(packetData[ 9][j]),  
            .x1(packetData[ 8][j]), 
            .x0(packetData[ 7][j]),
            .s2(sum_L1    [ 5][(j+2) % 16]), 
            .s1(sum_L1    [ 4][(j+1) % 16]), 
            .s0(sum_L1    [ 3][j]) 
          ); 
          reducer_7to3_i reduc2( 
            .x6(packetData[20][j]), 
            .x5(packetData[19][j]), 
            .x4(packetData[18][j]),
            .x3(packetData[17][j]), 
            .x2(packetData[16][j]),  
            .x1(packetData[15][j]), 
            .x0(packetData[14][j]),
            .s2(sum_L1    [ 8][(j+2) % 16]), 
            .s1(sum_L1    [ 7][(j+1) % 16]), 
            .s0(sum_L1    [ 6][j])
          ); 
          reducer_7to3_i reduc3( 
            .x6(packetData[27][j]), 
            .x5(packetData[26][j]), 
            .x4(packetData[25][j]),
            .x3(packetData[24][j]), 
            .x2(packetData[23][j]),  
            .x1(packetData[22][j]), 
            .x0(packetData[21][j]),
            .s2(sum_L1    [11][(j+2) % 16]), 
            .s1(sum_L1    [10][(j+1) % 16]), 
            .s0(sum_L1    [ 9][j]) 
          ); 
          reducer_6to3_i reduc4( 
            .x5(packetData[33][j]), 
            .x4(packetData[32][j]),
            .x3(packetData[31][j]), 
            .x2(packetData[30][j]),  
            .x1(packetData[29][j]), 
            .x0(packetData[28][j]),
            .s2(sum_L1    [14][(j+2) % 16]), 
            .s1(sum_L1    [13][(j+1) % 16]), 
            .s0(sum_L1    [12][j])
          );           
        end
      endgenerate


      /* Second Level of reduction 
       two groups of 7-bit CSA each plus
       one free word */ 

      wire [15 : 0] sum_L2[0:6];
      assign sum_L2[6] = sum_L1[14]; 

      genvar k;
      generate
        for (k = 0 ; k < 16 ; k= k+1) begin
          reducer_7to3_i reduc0( 
            .x6(sum_L1    [ 6][k]), 
            .x5(sum_L1    [ 5][k]), 
            .x4(sum_L1    [ 4][k]),
            .x3(sum_L1    [ 3][k]), 
            .x2(sum_L1    [ 2][k]),  
            .x1(sum_L1    [ 1][k]), 
            .x0(sum_L1    [ 0][k]),
            .s2(sum_L2    [ 2][(k+2) % 16]), 
            .s1(sum_L2    [ 1][(k+1) % 16]), 
            .s0(sum_L2    [ 0][k]) 
          ); 
          reducer_7to3_i reduc1( 
            .x6(sum_L1    [13][k]), 
            .x5(sum_L1    [12][k]), 
            .x4(sum_L1    [11][k]),
            .x3(sum_L1    [10][k]), 
            .x2(sum_L1    [ 9][k]),  
            .x1(sum_L1    [ 8][k]), 
            .x0(sum_L1    [ 7][k]),
            .s2(sum_L2    [ 5][(k+2) % 16]), 
            .s1(sum_L2    [ 4][(k+1) % 16]), 
            .s0(sum_L2    [ 3][k])
          ); 
        end
      endgenerate

      /* Third Level of reduction 
       one groups of 7-bit CSA
       */ 

      wire [15 : 0] sum_L3[0:2];

      genvar m;
      generate
        for (m = 0 ; m < 16 ; m= m+1) begin
          reducer_7to3_i reduc0( 
            .x6(sum_L2    [ 6][m]), 
            .x5(sum_L2    [ 5][m]), 
            .x4(sum_L2    [ 4][m]),
            .x3(sum_L2    [ 3][m]), 
            .x2(sum_L2    [ 2][m]),  
            .x1(sum_L2    [ 1][m]), 
            .x0(sum_L2    [ 0][m]),
            .s2(sum_L3    [ 2][(m+2) % 16]), 
            .s1(sum_L3    [ 1][(m+1) % 16]), 
            .s0(sum_L3    [ 0][m])
          );
        end
      endgenerate

      /* Fourth Level of reduction 
       3 -> 2 CSA
       */ 

      wire [15 : 0] sum_L4[0:1];

      genvar h;
      generate
        for (h = 0 ; h < 16 ; h= h+1) begin
          assign sum_L4[0][h]          = sum_L3[2][h] ^ sum_L3[1][h] ^ sum_L3[0][h];
          assign sum_L4[1][(h+1) % 16] = (sum_L3[2][h] & sum_L3[1][h]) |
                                         (sum_L3[2][h] & sum_L3[0][h]) |
                                         (sum_L3[1][h] & sum_L3[0][h])  ;
        end
      endgenerate

      assign ResWord0 = sum_L4[0];
      assign ResWord1 = sum_L4[1];

      wire    [ 16 :   0]     result_n;
      wire    [ 16 :   0]     result_o;

      assign result_n = sum_L4[0] + sum_L4[1];
      assign result_o = sum_L4[0] + sum_L4[1] + 1;

      assign result = (result_n[16]) ? result_o : result_n;

endmodule
