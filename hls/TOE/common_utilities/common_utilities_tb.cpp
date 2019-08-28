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

#include "common_utilities.hpp"


using namespace hls;
using namespace std;


int main()
{
	axiWord 	currWord;
	axiWord 	prevWord;
	axiWord 	SendWord;
	ap_uint<6>	byte_offset=1;
	int i;


	for (i=0; i < 64 ; i++){
		prevWord.data(((i*8)+7),i*8) = i;
		prevWord.keep.bit(i)=1;
	}

	for (i=0; i < 64 ; i++){
		currWord.data(((i*8)+7),i*8) = i + 64;
		currWord.keep.bit(i)=1;
	}

	cout << "prevWord " << setw(130) << hex << prevWord.data << endl;
	cout << "currWord " << setw(130) << hex << currWord.data << endl << endl;

	align_words_from_memory (
			currWord,
			prevWord,
			byte_offset,
			SendWord);

	AlingWordFromMemoryStageOne(
			currWord,
			prevWord,
			byte_offset,
			SendWord);

	return 0;
}
