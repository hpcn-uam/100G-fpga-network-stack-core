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

#ifndef _UTILITIES_HPP_DEFINED_
#define _UTILITIES_HPP_DEFINED_

#include "../toe.hpp"

ap_uint<7> keep2len(ap_uint<64> keepValue);

ap_uint<64> len2Keep(ap_uint<6> length);


void align_words_from_memory (
			axiWord 	currWord,
			axiWord 	prevWord,
			ap_uint<6>	byte_offset,
			axiWord& 	SendWord
	);

void AlingWordFromMemoryStageOne(
			axiWord 	currWord,
			axiWord 	prevWord,
			ap_uint<6>	byte_offset,
			axiWord& 	SendWord
	);

void align_words_to_memory (
			axiWord 	currWord,
			axiWord 	prevWord,
	
			ap_uint<6>	byte_offset,
			axiWord& 	SendWord
	);

void DataBroadcast(
			stream<axiWord>& in, 
			stream<axiWord>& out1, 
			stream<axiWord>& out2);

ap_uint<16> byteSwap16(ap_uint<16> inputVector);

ap_uint<32> byteSwap32(ap_uint<32> inputVector);

void combine_words(
					axiWord 	currentWord, 
					axiWord 	previousWord, 
					ap_uint<4> 	ip_headerlen,
					axiWord& 	sendWord);

#endif
