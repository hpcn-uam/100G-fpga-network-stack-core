/**
* @file pcap.c
* @brief Own implementation of the libpcap (just necessary functions).
* @author José Fernando Zazo Rollón, josefernando.zazo@estudiante.uam.es
* @date 2013-07-25
*/


#include "pcap.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#ifdef _WIN32
  #include "mmap.h"
#endif


static int fd = 0;                   /**< File descriptor of the pcap file */
static uint64_t file_proc_size = 0;     
static void *memfile;
static struct stat sb;



int pcap_open (char *path)
{
  if (fd) {   /* Ensure just one file is opened. */
    return -1;
  }

  fd = open (path, O_RDONLY);
  fstat(fd, &sb);
#ifdef _WIN32
  	  memfile = mmap(NULL, sb.st_size, NULL, MAP_PRIVATE, fd, 0);
#else
  	  memfile = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
#endif
  if (memfile == MAP_FAILED) {
    printf("Error in mmap %d\n",memfile);
	perror("Error in mmap");
    return -1;
  }

  file_proc_size = 0;

  return 0;
}



void pcap_close ()
{
  munmap(memfile, sb.st_size);
  close (fd);
}

static int pcap_eof ()
{
  if (file_proc_size >= sb.st_size) {
    return 1;
  } else {
    return 0;
  }
}

/**
* @brief Read global header of a pcap file.
*
* @return Always 0.
*/
static int readHeader ()
{
  /* Just ignore it */
  file_proc_size+=sizeof (pcap_hdr_t);
  return 0;
}


/**
* @brief Extract a packet in a pcap file.
*
* @param data Region where the data will be stored.
* @param hdr Individual header of the packet (extract from pcap)
* pointing to this region.
*
* @return A value less or equal to 0 is there was an error.
*/
static int readPacket (uint8_t **data, struct pcap_pkthdr *hdr)
{
  pcaprec_hdr_t localheader;

  if (file_proc_size+sizeof (pcaprec_hdr_t) >= sb.st_size)
    return 0;

  localheader = *((pcaprec_hdr_t *)((uint8_t *)memfile+file_proc_size));
  file_proc_size+=sizeof (pcaprec_hdr_t);
  


  hdr->ts.tv_usec = localheader.ts_usec;
  hdr->ts.tv_sec  = localheader.ts_sec;
  hdr->len        = localheader.orig_len;


  *data =  (uint8_t *)memfile+file_proc_size;
  file_proc_size+=localheader.incl_len;

  return 1;
}



/*
pcap_loop()  processes  packets  from  a ‘‘savefile’’ until cnt packets are processed, the end of the ‘‘save-
       file’’ is reached when reading from a ‘‘savefile’’, pcap_breakloop() is called, or an error occurs.  It does not return  when
       live  read timeouts occur.  A value of -1 or 0 for cnt is equivalent to infinity, so that packets are processed until another
       ending condition occurs.
*/
int pcap_loop (int cnt, pcap_handler callback, unsigned char *user)
{
  uint32_t i;
  struct pcap_pkthdr hdr;
  uint8_t *data;

  readHeader ();
  if (cnt == 0) {
    for (cnt = 0; !pcap_eof(); cnt++) {
      if (readPacket (&data, &hdr) <= 0) {
        break;
      }

      callback (user, &hdr, data);
    }
  } else if (cnt > 0) {
    for (i = 0; i < cnt && !pcap_eof(); i++) {
      if (readPacket (&data, &hdr) <= 0) {
        break;
      }

      callback (user, &hdr, data);
    }
  } else {
    return -1;
  }

  return cnt;
}

