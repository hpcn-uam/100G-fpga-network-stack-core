
#ifndef _UTILITIES_HPP_DEFINED_
#define _UTILITIES_HPP_DEFINED_

//#include "../toe.hpp"

ap_uint<7> keep2len(ap_uint<64> keepValue);

ap_uint<64> len2Keep(ap_uint<6> length);


void align_words_from_memory (
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

#endif
