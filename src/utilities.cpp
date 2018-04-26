
#include "utilities.hpp"

ap_uint<7> keep2len(ap_uint<64> keepValue){

	if (keepValue.bit(63))
		return 64;
	else if (keepValue.bit(62))
		return 63;
	else if (keepValue.bit(61))
		return 62;
	else if (keepValue.bit(60))
		return 61;
	else if (keepValue.bit(59))
		return 60;
	else if (keepValue.bit(58))
		return 59;
	else if (keepValue.bit(57))
		return 58;
	else if (keepValue.bit(56))
		return 57;
	else if (keepValue.bit(55))
		return 56;
	else if (keepValue.bit(54))
		return 55;
	else if (keepValue.bit(53))
		return 54;
	else if (keepValue.bit(52))
		return 53;
	else if (keepValue.bit(51))
		return 52;
	else if (keepValue.bit(50))
		return 51;
	else if (keepValue.bit(49))
		return 50;
	else if (keepValue.bit(48))
		return 49;
	else if (keepValue.bit(47))
		return 48;
	else if (keepValue.bit(46))
		return 47;
	else if (keepValue.bit(45))
		return 46;
	else if (keepValue.bit(44))
		return 45;
	else if (keepValue.bit(43))
		return 44;
	else if (keepValue.bit(42))
		return 43;
	else if (keepValue.bit(41))
		return 42;
	else if (keepValue.bit(40))
		return 41;
	else if (keepValue.bit(39))
		return 40;
	else if (keepValue.bit(38))
		return 39;
	else if (keepValue.bit(37))
		return 38;
	else if (keepValue.bit(36))
		return 37;
	else if (keepValue.bit(35))
		return 36;
	else if (keepValue.bit(34))
		return 35;
	else if (keepValue.bit(33))
		return 34;
	else if (keepValue.bit(32))
		return 33;
	else if (keepValue.bit(31))
		return 32;
	else if (keepValue.bit(30))
		return 31;
	else if (keepValue.bit(29))
		return 30;
	else if (keepValue.bit(28))
		return 29;
	else if (keepValue.bit(27))
		return 28;
	else if (keepValue.bit(26))
		return 27;
	else if (keepValue.bit(25))
		return 26;
	else if (keepValue.bit(24))
		return 25;
	else if (keepValue.bit(23))
		return 24;
	else if (keepValue.bit(22))
		return 23;
	else if (keepValue.bit(21))
		return 22;
	else if (keepValue.bit(20))
		return 21;
	else if (keepValue.bit(19))
		return 20;
	else if (keepValue.bit(18))
		return 19;
	else if (keepValue.bit(17))
		return 18;
	else if (keepValue.bit(16))
		return 17;
	else if (keepValue.bit(15))
		return 16;
	else if (keepValue.bit(14))
		return 15;
	else if (keepValue.bit(13))
		return 14;
	else if (keepValue.bit(12))
		return 13;
	else if (keepValue.bit(11))
		return 12;
	else if (keepValue.bit(10))
		return 11;
	else if (keepValue.bit(9))
		return 10;
	else if (keepValue.bit(8))
		return 9;
	else if (keepValue.bit(7))
		return 8;
	else if (keepValue.bit(6))
		return 7;
	else if (keepValue.bit(5))
		return 6;
	else if (keepValue.bit(4))
		return 5;
	else if (keepValue.bit(3))
		return 4;
	else if (keepValue.bit(2))
		return 3;
	else if (keepValue.bit(1))
		return 2;
	else if	(keepValue.bit(0))							
		return 1;
	else
		return 0;

}

ap_uint<64> len2Keep(ap_uint<7> length) {

	const ap_uint<64> keep_table[65]={
		0x0000000000000000,
		0x0000000000000001,
		0x0000000000000003,
		0x0000000000000007,
		0x000000000000000F,
		0x000000000000001F,
		0x000000000000003F,
		0x000000000000007F,
		0x00000000000000FF,
		0x00000000000001FF,
		0x00000000000003FF,
		0x00000000000007FF,
		0x0000000000000FFF,
		0x0000000000001FFF,
		0x0000000000003FFF,
		0x0000000000007FFF,
		0x000000000000FFFF,
		0x000000000001FFFF,
		0x000000000003FFFF,
		0x000000000007FFFF,
		0x00000000000FFFFF,
		0x00000000001FFFFF,
		0x00000000003FFFFF,
		0x00000000007FFFFF,
		0x0000000000FFFFFF,
		0x0000000001FFFFFF,
		0x0000000003FFFFFF,
		0x0000000007FFFFFF,
		0x000000000FFFFFFF,
		0x000000001FFFFFFF,
		0x000000003FFFFFFF,
		0x000000007FFFFFFF,
		0x00000000FFFFFFFF,
		0x00000001FFFFFFFF,
		0x00000003FFFFFFFF,
		0x00000007FFFFFFFF,
		0x0000000FFFFFFFFF,
		0x0000001FFFFFFFFF,
		0x0000003FFFFFFFFF,
		0x0000007FFFFFFFFF,
		0x000000FFFFFFFFFF,
		0x000001FFFFFFFFFF,
		0x000003FFFFFFFFFF,
		0x000007FFFFFFFFFF,
		0x00000FFFFFFFFFFF,
		0x00001FFFFFFFFFFF,
		0x00003FFFFFFFFFFF,
		0x00007FFFFFFFFFFF,
		0x0000FFFFFFFFFFFF,
		0x0001FFFFFFFFFFFF,
		0x0003FFFFFFFFFFFF,
		0x0007FFFFFFFFFFFF,
		0x000FFFFFFFFFFFFF,
		0x001FFFFFFFFFFFFF,
		0x003FFFFFFFFFFFFF,
		0x007FFFFFFFFFFFFF,
		0x00FFFFFFFFFFFFFF,
		0x01FFFFFFFFFFFFFF,
		0x03FFFFFFFFFFFFFF,
		0x07FFFFFFFFFFFFFF,
		0x0FFFFFFFFFFFFFFF,
		0x1FFFFFFFFFFFFFFF,
		0x3FFFFFFFFFFFFFFF,
		0x7FFFFFFFFFFFFFFF,
		0xFFFFFFFFFFFFFFFF
	};
#pragma HLS RESOURCE variable=keep_table core=ROM_2P latency=1

	return keep_table[length];

}

void tx_align_two_64bytes_words (
			axiWord 	currWord,
			axiWord 	prevWord,
			ap_uint<6>	byte_offset,

			axiWord* 	SendWord,
			axiWord* 	next_prev_word
			
	){
//#pragma HLS INLINE

	if (currWord.last){
		if (currWord.keep.bit(byte_offset)){
			SendWord->last = 0;
			next_prev_word->last = 1;
		}
		else{
			SendWord->last = 1;
			next_prev_word->last = 0;
		}
	}
	else {
		SendWord->last = currWord.last;
		next_prev_word->last = currWord.last;
	}


	next_prev_word->data = 0;
	next_prev_word->keep = 0;

	switch(byte_offset){	
		case 0:
			SendWord->data 		 = currWord.data;
			SendWord->keep 		 = currWord.keep;
			next_prev_word->data = prevWord.data;
			next_prev_word->keep = prevWord.keep;
			break;
		case 1: 
			SendWord->data       = (currWord.data(503,  0),prevWord.data(  7,  0));
			SendWord->keep       = (currWord.keep( 62,  0),prevWord.keep(  0,  0));
			next_prev_word->data(  7,  0) = currWord.data(511,504);
			next_prev_word->keep(  0,  0) = currWord.keep( 63, 63);
			break;
		case 2: 
			SendWord->data       = (currWord.data(495,  0),prevWord.data( 15,  0));
			SendWord->keep       = (currWord.keep( 61,  0),prevWord.keep(  1,  0));
			next_prev_word->data( 15,  0) = currWord.data(511,496);
			next_prev_word->keep(  1,  0) = currWord.keep( 63, 62);
			break;
		case 3: 
			SendWord->data       = (currWord.data(487,  0),prevWord.data( 23,  0));
			SendWord->keep       = (currWord.keep( 60,  0),prevWord.keep(  2,  0));
			next_prev_word->data( 23,  0) = currWord.data(511,488);
			next_prev_word->keep(  2,  0) = currWord.keep( 63, 61);
			break;
		case 4: 
			SendWord->data       = (currWord.data(479,  0),prevWord.data( 31,  0));
			SendWord->keep       = (currWord.keep( 59,  0),prevWord.keep(  3,  0));
			next_prev_word->data( 31,  0) = currWord.data(511,480);
			next_prev_word->keep(  3,  0) = currWord.keep( 63, 60);
			break;
		case 5: 
			SendWord->data       = (currWord.data(471,  0),prevWord.data( 39,  0));
			SendWord->keep       = (currWord.keep( 58,  0),prevWord.keep(  4,  0));
			next_prev_word->data( 39,  0) = currWord.data(511,472);
			next_prev_word->keep(  4,  0) = currWord.keep( 63, 59);
			break;
		case 6: 
			SendWord->data       = (currWord.data(463,  0),prevWord.data( 47,  0));
			SendWord->keep       = (currWord.keep( 57,  0),prevWord.keep(  5,  0));
			next_prev_word->data( 47,  0) = currWord.data(511,464);
			next_prev_word->keep(  5,  0) = currWord.keep( 63, 58);
			break;
		case 7: 
			SendWord->data       = (currWord.data(455,  0),prevWord.data( 55,  0));
			SendWord->keep       = (currWord.keep( 56,  0),prevWord.keep(  6,  0));
			next_prev_word->data( 55,  0) = currWord.data(511,456);
			next_prev_word->keep(  6,  0) = currWord.keep( 63, 57);
			break;
		case 8: 
			SendWord->data       = (currWord.data(447,  0),prevWord.data( 63,  0));
			SendWord->keep       = (currWord.keep( 55,  0),prevWord.keep(  7,  0));
			next_prev_word->data( 63,  0) = currWord.data(511,448);
			next_prev_word->keep(  7,  0) = currWord.keep( 63, 56);
			break;
		case 9: 
			SendWord->data       = (currWord.data(439,  0),prevWord.data( 71,  0));
			SendWord->keep       = (currWord.keep( 54,  0),prevWord.keep(  8,  0));
			next_prev_word->data( 71,  0) = currWord.data(511,440);
			next_prev_word->keep(  8,  0) = currWord.keep( 63, 55);
			break;
		case 10: 
			SendWord->data       = (currWord.data(431,  0),prevWord.data( 79,  0));
			SendWord->keep       = (currWord.keep( 53,  0),prevWord.keep(  9,  0));
			next_prev_word->data( 79,  0) = currWord.data(511,432);
			next_prev_word->keep(  9,  0) = currWord.keep( 63, 54);
			break;
		case 11: 
			SendWord->data       = (currWord.data(423,  0),prevWord.data( 87,  0));
			SendWord->keep       = (currWord.keep( 52,  0),prevWord.keep( 10,  0));
			next_prev_word->data( 87,  0) = currWord.data(511,424);
			next_prev_word->keep( 10,  0) = currWord.keep( 63, 53);
			break;
		case 12: 
			SendWord->data       = (currWord.data(415,  0),prevWord.data( 95,  0));
			SendWord->keep       = (currWord.keep( 51,  0),prevWord.keep( 11,  0));
			next_prev_word->data( 95,  0) = currWord.data(511,416);
			next_prev_word->keep( 11,  0) = currWord.keep( 63, 52);
			break;
		case 13: 
			SendWord->data       = (currWord.data(407,  0),prevWord.data(103,  0));
			SendWord->keep       = (currWord.keep( 50,  0),prevWord.keep( 12,  0));
			next_prev_word->data(103,  0) = currWord.data(511,408);
			next_prev_word->keep( 12,  0) = currWord.keep( 63, 51);
			break;
		case 14: 
			SendWord->data       = (currWord.data(399,  0),prevWord.data(111,  0));
			SendWord->keep       = (currWord.keep( 49,  0),prevWord.keep( 13,  0));
			next_prev_word->data(111,  0) = currWord.data(511,400);
			next_prev_word->keep( 13,  0) = currWord.keep( 63, 50);
			break;
		case 15: 
			SendWord->data       = (currWord.data(391,  0),prevWord.data(119,  0));
			SendWord->keep       = (currWord.keep( 48,  0),prevWord.keep( 14,  0));
			next_prev_word->data(119,  0) = currWord.data(511,392);
			next_prev_word->keep( 14,  0) = currWord.keep( 63, 49);
			break;
		case 16: 
			SendWord->data       = (currWord.data(383,  0),prevWord.data(127,  0));
			SendWord->keep       = (currWord.keep( 47,  0),prevWord.keep( 15,  0));
			next_prev_word->data(127,  0) = currWord.data(511,384);
			next_prev_word->keep( 15,  0) = currWord.keep( 63, 48);
			break;
		case 17: 
			SendWord->data       = (currWord.data(375,  0),prevWord.data(135,  0));
			SendWord->keep       = (currWord.keep( 46,  0),prevWord.keep( 16,  0));
			next_prev_word->data(135,  0) = currWord.data(511,376);
			next_prev_word->keep( 16,  0) = currWord.keep( 63, 47);
			break;
		case 18: 
			SendWord->data       = (currWord.data(367,  0),prevWord.data(143,  0));
			SendWord->keep       = (currWord.keep( 45,  0),prevWord.keep( 17,  0));
			next_prev_word->data(143,  0) = currWord.data(511,368);
			next_prev_word->keep( 17,  0) = currWord.keep( 63, 46);
			break;
		case 19: 
			SendWord->data       = (currWord.data(359,  0),prevWord.data(151,  0));
			SendWord->keep       = (currWord.keep( 44,  0),prevWord.keep( 18,  0));
			next_prev_word->data(151,  0) = currWord.data(511,360);
			next_prev_word->keep( 18,  0) = currWord.keep( 63, 45);
			break;
		case 20: 
			SendWord->data       = (currWord.data(351,  0),prevWord.data(159,  0));
			SendWord->keep       = (currWord.keep( 43,  0),prevWord.keep( 19,  0));
			next_prev_word->data(159,  0) = currWord.data(511,352);
			next_prev_word->keep( 19,  0) = currWord.keep( 63, 44);
			break;
		case 21: 
			SendWord->data       = (currWord.data(343,  0),prevWord.data(167,  0));
			SendWord->keep       = (currWord.keep( 42,  0),prevWord.keep( 20,  0));
			next_prev_word->data(167,  0) = currWord.data(511,344);
			next_prev_word->keep( 20,  0) = currWord.keep( 63, 43);
			break;
		case 22: 
			SendWord->data       = (currWord.data(335,  0),prevWord.data(175,  0));
			SendWord->keep       = (currWord.keep( 41,  0),prevWord.keep( 21,  0));
			next_prev_word->data(175,  0) = currWord.data(511,336);
			next_prev_word->keep( 21,  0) = currWord.keep( 63, 42);
			break;
		case 23: 
			SendWord->data       = (currWord.data(327,  0),prevWord.data(183,  0));
			SendWord->keep       = (currWord.keep( 40,  0),prevWord.keep( 22,  0));
			next_prev_word->data(183,  0) = currWord.data(511,328);
			next_prev_word->keep( 22,  0) = currWord.keep( 63, 41);
			break;
		case 24: 
			SendWord->data       = (currWord.data(319,  0),prevWord.data(191,  0));
			SendWord->keep       = (currWord.keep( 39,  0),prevWord.keep( 23,  0));
			next_prev_word->data(191,  0) = currWord.data(511,320);
			next_prev_word->keep( 23,  0) = currWord.keep( 63, 40);
			break;
		case 25: 
			SendWord->data       = (currWord.data(311,  0),prevWord.data(199,  0));
			SendWord->keep       = (currWord.keep( 38,  0),prevWord.keep( 24,  0));
			next_prev_word->data(199,  0) = currWord.data(511,312);
			next_prev_word->keep( 24,  0) = currWord.keep( 63, 39);
			break;
		case 26: 
			SendWord->data       = (currWord.data(303,  0),prevWord.data(207,  0));
			SendWord->keep       = (currWord.keep( 37,  0),prevWord.keep( 25,  0));
			next_prev_word->data(207,  0) = currWord.data(511,304);
			next_prev_word->keep( 25,  0) = currWord.keep( 63, 38);
			break;
		case 27: 
			SendWord->data       = (currWord.data(295,  0),prevWord.data(215,  0));
			SendWord->keep       = (currWord.keep( 36,  0),prevWord.keep( 26,  0));
			next_prev_word->data(215,  0) = currWord.data(511,296);
			next_prev_word->keep( 26,  0) = currWord.keep( 63, 37);
			break;
		case 28: 
			SendWord->data       = (currWord.data(287,  0),prevWord.data(223,  0));
			SendWord->keep       = (currWord.keep( 35,  0),prevWord.keep( 27,  0));
			next_prev_word->data(223,  0) = currWord.data(511,288);
			next_prev_word->keep( 27,  0) = currWord.keep( 63, 36);
			break;
		case 29: 
			SendWord->data       = (currWord.data(279,  0),prevWord.data(231,  0));
			SendWord->keep       = (currWord.keep( 34,  0),prevWord.keep( 28,  0));
			next_prev_word->data(231,  0) = currWord.data(511,280);
			next_prev_word->keep( 28,  0) = currWord.keep( 63, 35);
			break;
		case 30: 
			SendWord->data       = (currWord.data(271,  0),prevWord.data(239,  0));
			SendWord->keep       = (currWord.keep( 33,  0),prevWord.keep( 29,  0));
			next_prev_word->data(239,  0) = currWord.data(511,272);
			next_prev_word->keep( 29,  0) = currWord.keep( 63, 34);
			break;
		case 31: 
			SendWord->data       = (currWord.data(263,  0),prevWord.data(247,  0));
			SendWord->keep       = (currWord.keep( 32,  0),prevWord.keep( 30,  0));
			next_prev_word->data(247,  0) = currWord.data(511,264);
			next_prev_word->keep( 30,  0) = currWord.keep( 63, 33);
			break;
		case 32: 
			SendWord->data       = (currWord.data(255,  0),prevWord.data(255,  0));
			SendWord->keep       = (currWord.keep( 31,  0),prevWord.keep( 31,  0));
			next_prev_word->data(255,  0) = currWord.data(511,256);
			next_prev_word->keep( 31,  0) = currWord.keep( 63, 32);
			break;
		case 33: 
			SendWord->data       = (currWord.data(247,  0),prevWord.data(263,  0));
			SendWord->keep       = (currWord.keep( 30,  0),prevWord.keep( 32,  0));
			next_prev_word->data(263,  0) = currWord.data(511,248);
			next_prev_word->keep( 32,  0) = currWord.keep( 63, 31);
			break;
		case 34: 
			SendWord->data       = (currWord.data(239,  0),prevWord.data(271,  0));
			SendWord->keep       = (currWord.keep( 29,  0),prevWord.keep( 33,  0));
			next_prev_word->data(271,  0) = currWord.data(511,240);
			next_prev_word->keep( 33,  0) = currWord.keep( 63, 30);
			break;
		case 35: 
			SendWord->data       = (currWord.data(231,  0),prevWord.data(279,  0));
			SendWord->keep       = (currWord.keep( 28,  0),prevWord.keep( 34,  0));
			next_prev_word->data(279,  0) = currWord.data(511,232);
			next_prev_word->keep( 34,  0) = currWord.keep( 63, 29);
			break;
		case 36: 
			SendWord->data       = (currWord.data(223,  0),prevWord.data(287,  0));
			SendWord->keep       = (currWord.keep( 27,  0),prevWord.keep( 35,  0));
			next_prev_word->data(287,  0) = currWord.data(511,224);
			next_prev_word->keep( 35,  0) = currWord.keep( 63, 28);
			break;
		case 37: 
			SendWord->data       = (currWord.data(215,  0),prevWord.data(295,  0));
			SendWord->keep       = (currWord.keep( 26,  0),prevWord.keep( 36,  0));
			next_prev_word->data(295,  0) = currWord.data(511,216);
			next_prev_word->keep( 36,  0) = currWord.keep( 63, 27);
			break;
		case 38: 
			SendWord->data       = (currWord.data(207,  0),prevWord.data(303,  0));
			SendWord->keep       = (currWord.keep( 25,  0),prevWord.keep( 37,  0));
			next_prev_word->data(303,  0) = currWord.data(511,208);
			next_prev_word->keep( 37,  0) = currWord.keep( 63, 26);
			break;
		case 39: 
			SendWord->data       = (currWord.data(199,  0),prevWord.data(311,  0));
			SendWord->keep       = (currWord.keep( 24,  0),prevWord.keep( 38,  0));
			next_prev_word->data(311,  0) = currWord.data(511,200);
			next_prev_word->keep( 38,  0) = currWord.keep( 63, 25);
			break;
		case 40: 
			SendWord->data       = (currWord.data(191,  0),prevWord.data(319,  0));
			SendWord->keep       = (currWord.keep( 23,  0),prevWord.keep( 39,  0));
			next_prev_word->data(319,  0) = currWord.data(511,192);
			next_prev_word->keep( 39,  0) = currWord.keep( 63, 24);
			break;
		case 41: 
			SendWord->data       = (currWord.data(183,  0),prevWord.data(327,  0));
			SendWord->keep       = (currWord.keep( 22,  0),prevWord.keep( 40,  0));
			next_prev_word->data(327,  0) = currWord.data(511,184);
			next_prev_word->keep( 40,  0) = currWord.keep( 63, 23);
			break;
		case 42: 
			SendWord->data       = (currWord.data(175,  0),prevWord.data(335,  0));
			SendWord->keep       = (currWord.keep( 21,  0),prevWord.keep( 41,  0));
			next_prev_word->data(335,  0) = currWord.data(511,176);
			next_prev_word->keep( 41,  0) = currWord.keep( 63, 22);
			break;
		case 43: 
			SendWord->data       = (currWord.data(167,  0),prevWord.data(343,  0));
			SendWord->keep       = (currWord.keep( 20,  0),prevWord.keep( 42,  0));
			next_prev_word->data(343,  0) = currWord.data(511,168);
			next_prev_word->keep( 42,  0) = currWord.keep( 63, 21);
			break;
		case 44: 
			SendWord->data       = (currWord.data(159,  0),prevWord.data(351,  0));
			SendWord->keep       = (currWord.keep( 19,  0),prevWord.keep( 43,  0));
			next_prev_word->data(351,  0) = currWord.data(511,160);
			next_prev_word->keep( 43,  0) = currWord.keep( 63, 20);
			break;
		case 45: 
			SendWord->data       = (currWord.data(151,  0),prevWord.data(359,  0));
			SendWord->keep       = (currWord.keep( 18,  0),prevWord.keep( 44,  0));
			next_prev_word->data(359,  0) = currWord.data(511,152);
			next_prev_word->keep( 44,  0) = currWord.keep( 63, 19);
			break;
		case 46: 
			SendWord->data       = (currWord.data(143,  0),prevWord.data(367,  0));
			SendWord->keep       = (currWord.keep( 17,  0),prevWord.keep( 45,  0));
			next_prev_word->data(367,  0) = currWord.data(511,144);
			next_prev_word->keep( 45,  0) = currWord.keep( 63, 18);
			break;
		case 47: 
			SendWord->data       = (currWord.data(135,  0),prevWord.data(375,  0));
			SendWord->keep       = (currWord.keep( 16,  0),prevWord.keep( 46,  0));
			next_prev_word->data(375,  0) = currWord.data(511,136);
			next_prev_word->keep( 46,  0) = currWord.keep( 63, 17);
			break;
		case 48: 
			SendWord->data       = (currWord.data(127,  0),prevWord.data(383,  0));
			SendWord->keep       = (currWord.keep( 15,  0),prevWord.keep( 47,  0));
			next_prev_word->data(383,  0) = currWord.data(511,128);
			next_prev_word->keep( 47,  0) = currWord.keep( 63, 16);
			break;
		case 49: 
			SendWord->data       = (currWord.data(119,  0),prevWord.data(391,  0));
			SendWord->keep       = (currWord.keep( 14,  0),prevWord.keep( 48,  0));
			next_prev_word->data(391,  0) = currWord.data(511,120);
			next_prev_word->keep( 48,  0) = currWord.keep( 63, 15);
			break;
		case 50: 
			SendWord->data       = (currWord.data(111,  0),prevWord.data(399,  0));
			SendWord->keep       = (currWord.keep( 13,  0),prevWord.keep( 49,  0));
			next_prev_word->data(399,  0) = currWord.data(511,112);
			next_prev_word->keep( 49,  0) = currWord.keep( 63, 14);
			break;
		case 51: 
			SendWord->data       = (currWord.data(103,  0),prevWord.data(407,  0));
			SendWord->keep       = (currWord.keep( 12,  0),prevWord.keep( 50,  0));
			next_prev_word->data(407,  0) = currWord.data(511,104);
			next_prev_word->keep( 50,  0) = currWord.keep( 63, 13);
			break;
		case 52: 
			SendWord->data       = (currWord.data( 95,  0),prevWord.data(415,  0));
			SendWord->keep       = (currWord.keep( 11,  0),prevWord.keep( 51,  0));
			next_prev_word->data(415,  0) = currWord.data(511, 96);
			next_prev_word->keep( 51,  0) = currWord.keep( 63, 12);
			break;
		case 53: 
			SendWord->data       = (currWord.data( 87,  0),prevWord.data(423,  0));
			SendWord->keep       = (currWord.keep( 10,  0),prevWord.keep( 52,  0));
			next_prev_word->data(423,  0) = currWord.data(511, 88);
			next_prev_word->keep( 52,  0) = currWord.keep( 63, 11);
			break;
		case 54: 
			SendWord->data       = (currWord.data( 79,  0),prevWord.data(431,  0));
			SendWord->keep       = (currWord.keep(  9,  0),prevWord.keep( 53,  0));
			next_prev_word->data(431,  0) = currWord.data(511, 80);
			next_prev_word->keep( 53,  0) = currWord.keep( 63, 10);
			break;
		case 55: 
			SendWord->data       = (currWord.data( 71,  0),prevWord.data(439,  0));
			SendWord->keep       = (currWord.keep(  8,  0),prevWord.keep( 54,  0));
			next_prev_word->data(439,  0) = currWord.data(511, 72);
			next_prev_word->keep( 54,  0) = currWord.keep( 63,  9);
			break;
		case 56: 
			SendWord->data       = (currWord.data( 63,  0),prevWord.data(447,  0));
			SendWord->keep       = (currWord.keep(  7,  0),prevWord.keep( 55,  0));
			next_prev_word->data(447,  0) = currWord.data(511, 64);
			next_prev_word->keep( 55,  0) = currWord.keep( 63,  8);
			break;
		case 57: 
			SendWord->data       = (currWord.data( 55,  0),prevWord.data(455,  0));
			SendWord->keep       = (currWord.keep(  6,  0),prevWord.keep( 56,  0));
			next_prev_word->data(455,  0) = currWord.data(511, 56);
			next_prev_word->keep( 56,  0) = currWord.keep( 63,  7);
			break;
		case 58: 
			SendWord->data       = (currWord.data( 47,  0),prevWord.data(463,  0));
			SendWord->keep       = (currWord.keep(  5,  0),prevWord.keep( 57,  0));
			next_prev_word->data(463,  0) = currWord.data(511, 48);
			next_prev_word->keep( 57,  0) = currWord.keep( 63,  6);
			break;
		case 59: 
			SendWord->data       = (currWord.data( 39,  0),prevWord.data(471,  0));
			SendWord->keep       = (currWord.keep(  4,  0),prevWord.keep( 58,  0));
			next_prev_word->data(471,  0) = currWord.data(511, 40);
			next_prev_word->keep( 58,  0) = currWord.keep( 63,  5);
			break;
		case 60: 
			SendWord->data       = (currWord.data( 31,  0),prevWord.data(479,  0));
			SendWord->keep       = (currWord.keep(  3,  0),prevWord.keep( 59,  0));
			next_prev_word->data(479,  0) = currWord.data(511, 32);
			next_prev_word->keep( 59,  0) = currWord.keep( 63,  4);
			break;
		case 61: 
			SendWord->data       = (currWord.data( 23,  0),prevWord.data(487,  0));
			SendWord->keep       = (currWord.keep(  2,  0),prevWord.keep( 60,  0));
			next_prev_word->data(487,  0) = currWord.data(511, 24);
			next_prev_word->keep( 60,  0) = currWord.keep( 63,  3);
			break;
		case 62: 
			SendWord->data       = (currWord.data( 15,  0),prevWord.data(495,  0));
			SendWord->keep       = (currWord.keep(  1,  0),prevWord.keep( 61,  0));
			next_prev_word->data(495,  0) = currWord.data(511, 16);
			next_prev_word->keep( 61,  0) = currWord.keep( 63,  2);
			break;
		case 63: 
			SendWord->data       = (currWord.data(  7,  0),prevWord.data(503,  0));
			SendWord->keep       = (currWord.keep(  0,  0),prevWord.keep( 62,  0));
			next_prev_word->data(503,  0) = currWord.data(511,  8);
			next_prev_word->keep( 62,  0) = currWord.keep( 63,  1);
			break;
			}

	

}

