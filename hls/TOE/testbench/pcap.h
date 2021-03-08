/************************************************
BSD 3-Clause License

Copyright (c) 2019, HPCN Group, UAM Spain (hpcn-uam.es) and 
Naudit HPCN (naudit.es)
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

/**
* @file pcap.h
* @brief Own library inspiry in libpcap (just necessary functions).
* @author José Fernando Zazo Rollón, josefernando.zazo@estudiante.uam.es
* @author Mario Daniel Ruiz Noguera, mario.ruiz@uam.es
* @date 2013-07-25
*/
#ifndef NFP_PCAP_H
#define NFP_PCAP_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define PAGE_SIZE 4096

/*
Pcap file format:


++---------------++---------------+-------------++---------------+-------------++---------------+-------------++
|| Global Header || Packet Header | Packet Data || Packet Header | Packet Data || Packet Header | Packet Data ||
++---------------++---------------+-------------++---------------+-------------++---------------+-------------++

*/


/**
* @brief Global pcap header.
*/
typedef struct pcap_hdr_s {
  uint32_t magic_number;   /**< magic number */
  uint16_t version_major;  /**< major version number */
  uint16_t version_minor;  /**< minor version number */
  uint32_t thiszone;       /**< GMT to local correction */
  uint32_t sigfigs;        /**< accuracy of timestamps */
  uint32_t snaplen;        /**< max length of captured packets, in octets */
  uint32_t network;        /**< data link type */
} pcap_hdr_t;

/**
* @brief Pcap packet header.
*/
typedef struct pcaprec_hdr_s {
  uint32_t ts_sec;         /**< timestamp seconds */
  uint32_t ts_usec;        /**< timestamp microseconds */
  uint32_t incl_len;       /**< number of octets of packet saved in file */
  uint32_t orig_len;       /**< actual length of packet */
} pcaprec_hdr_t;


/**
* @brief  Information from the  pcaprec_hdr_s transferred to the callback function
*/
struct pcap_pkthdr {
  uint32_t len;       /**< Length of the packet */
  struct timeval ts;  /**< Time of arrival of the packet */
};

/**
* @brief Function prototype of the callback function invoked by pcap_loop/simple_loop.
*
* @param user The last argument given to the pcap_loop/simple_loop.
* @param pkthdr The read fields from the file.
* @param bytes The pointer to the read data from the file.
*/
typedef void (*pcap_handler) (unsigned char *user,
                              struct pcap_pkthdr *pkthdr,
                              unsigned char *bytes);

/**
* @brief Open a pcap file in read only mode.
*
* @param path Path to the file.
*
* @return The FILE* associated.
*/
int pcap_open (char *path, bool ethernet);

/**
* @brief Close a previous file opened with pcap_open.
*
* @param descriptor The return value of such functions.
*/
void pcap_close ();

/**
* @brief Function similar to the pcap_loop in libpcap. Iterates over cnt packets in the pcap while
* invoking the callback function
*
* @param cnt Number of packets to process. 0 = All the file.
* @param callback Function invoked for each packet.
* @param user Pointer that will be passed to the callback function.
*
* @return The number of packets that have been processed.
*/
int pcap_loop (int cnt, pcap_handler callback, unsigned char *user);

/**
* @brief Open a pcap file in write only mode.
*
* @param path Path to the file.
*
* @return if was create successful or not.
*/
int pcap_open_write (char *path, bool microseconds);


void pcap_WriteData (uint8_t *data, int data_size);

/**
* @brief Close a previous file opened with pcap_open_write.
*
* @param descriptor The return value of such functions.
*/

void pcap_close_write ();

#endif
