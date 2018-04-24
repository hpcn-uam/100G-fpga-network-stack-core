#ifndef _PCAP2STREAM_H_
#define _PCAP2STREAM_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include "../toe.hpp"


void pcap2stream(
        char                      *file2load,
        stream<axiWord>&          output_data );


void pcap2stream_no_eth(
        char                      *file2load,
        stream<axiWord>&          output_data);

#endif
