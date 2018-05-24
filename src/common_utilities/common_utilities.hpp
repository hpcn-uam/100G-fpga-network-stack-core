
#ifndef _UTILITIES_HPP_DEFINED_
#define _UTILITIES_HPP_DEFINED_


#include "ap_int.h"
#include "../toe.hpp"

ap_uint<7> keep2len(ap_uint<64> keepValue);

ap_uint<64> len2Keep(ap_uint<6> length);


void tx_align_two_64bytes_words (
			axiWord 	currWord,
			axiWord 	prevWord,
			ap_uint<6>	byte_offset,

			axiWord* 	SendWord,
			axiWord* 	next_prev_word
	);

void rx_align_two_64bytes_words (
			axiWord 	currWord,
			axiWord* 	prevWord,
			ap_uint<6>	byte_offset,

			axiWord* 	SendWord
	);

void DataBroadcast(
			stream<axiWord>& in, 
			stream<axiWord>& out1, 
			stream<axiWord>& out2);

#endif
