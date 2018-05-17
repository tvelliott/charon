//MIT License
//
//Copyright (c) 2018 tvelliott
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#define _DEFAULT_SOURCE 1 //get rid of warning about usleep

#include "timers.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <complex.h>
#include "tuntap.h"
#include "tap_device.h"
#include "charon.h"
#include "tcp_subs.h"
#include "config.h"

#include "ethernet.h"

static struct device *dev;
static int debug_tap=0;

static FILE *tap_f;

const uint8_t mac_all_zero[6]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t mac_all_one[6]={0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

uint8_t ofdm0_mac[6];
static uint8_t tap_mac_a[18];

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
int init_tap_device(void) {
	int ret;
	char *hwaddr;

	ret = 0;
  //fprintf(stderr, "\ntuntap init");

  dev = tuntap_init();

  if(dev==NULL) {
    fprintf(stderr, "\ngot null device when trying to create tap");
    return -1;
  }
  else {
    fprintf(stderr, "\ncreated tap device");
  }

	if (tuntap_start(dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY) == -1) {
		ret = 1;
		goto clean;
	}



  srandom(usb_ip32);  //always the same mac for a given IP. keeps the mesh from getting behind too much when a 
                      //client comes back up after re-start.
  sprintf(tap_mac_a, "%02x:%02x:%02x:%02x:%02x:%02x\0", 
      (uint8_t) ((random()%255)&0xfc),
      (uint8_t) (random()%255),
      (uint8_t) (random()%255),
      (uint8_t) (random()%255),
      (uint8_t) (random()%255),
    (uint8_t) (random()%255 ));
  srandom(time(NULL));

	if (tuntap_set_hwaddr(dev, tap_mac_a) == -1) {
		ret = 1;
    fprintf(stderr, "\nerr: %s", tap_mac_a);
		goto clean;
	}

	if (tuntap_set_mtu(dev, 2342) == -1) {
		ret = 1;
		goto clean;
	}

	if (tuntap_up(dev) == -1) {
		ret = 1;
		goto clean;
	}

  tap_mac_a[2] = 0;
  tap_mac_a[5] = 0;
  tap_mac_a[8] = 0;
  tap_mac_a[11] = 0;
  tap_mac_a[14] = 0;
  tap_mac_a[17] = 0;
  ofdm0_mac[0] = strtol(&tap_mac_a[0],NULL,16);
  ofdm0_mac[1] = strtol(&tap_mac_a[3],NULL,16);
  ofdm0_mac[2] = strtol(&tap_mac_a[6],NULL,16);
  ofdm0_mac[3] = strtol(&tap_mac_a[9],NULL,16);
  ofdm0_mac[4] = strtol(&tap_mac_a[12],NULL,16);
  ofdm0_mac[5] = strtol(&tap_mac_a[15],NULL,16);

  setNonblocking(dev->tun_fd);

  tap_f = (FILE *) fdopen((int) dev->tun_fd, "r+");
  return 0;

clean:
	tuntap_destroy(dev);
	return ret;
}


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
int setNonblocking(int fd)
{
      int flags;

  #if defined(O_NONBLOCK)
      if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
                flags = 0;
      return fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_SYNC);
  #else
        flags = 1;
      return ioctl(fd, FIOBIO, &flags);
  #endif
}  

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
int read_tap_dev(char *buffer, int max_len, char *dst_mac, char *is_broadcast) {

 int i;
 int nbytes;
 char _buffer[2342];
 int new_len;

 eth_frame *ethframe = (eth_frame *) buffer;

  batman_frame_unicast *batucast = (batman_frame_unicast *) buffer; 
  batman_frame_special *batspecial = (batman_frame_special *) buffer; 
  batman_unicast *bathdr_u = &(batucast->bathdr_unicast);

 eth_hdr *ethhdr = (eth_hdr *) &(ethframe->ethhdr);
 ip_hdr *ip = (ip_hdr *) &(ethframe->iphdr);
 tcp_hdr *tcp = (tcp_hdr *) &(ethframe->ip_proto_payload);
 udp_hdr *udp = (udp_hdr *) &(ethframe->ip_proto_payload);
 uint8_t *payload = (uint8_t *) tcp->payload;

 nbytes = read(dev->tun_fd, _buffer, 2342);//is this source of memory leak, try copy 
 memcpy(buffer,_buffer,nbytes);

 if(nbytes<=0) return 0;

  memcpy(dst_mac, mac_all_zero, 6);
    
  uint16_t frame_type  = htons(ethhdr->eth_type);
  switch( frame_type ) {
    case  ETH_IP_TYPE :
      //fprintf(stderr,"\neth_type: IP");
      memcpy(dst_mac, &(ethhdr->dst_mac), 6);

       if( htons(ethhdr->eth_type)==0x0800 ) {
         new_len = do_tcp_subs(buffer);
         if(new_len > 0) nbytes = new_len;
       }
    break;

    case  ETH_ARP_TYPE :
      //fprintf(stderr,"\neth_type: ARP");
      memcpy(dst_mac, &(ethhdr->dst_mac), 6);
    break;

    case  ETH_BATMAN_TYPE :
      //fprintf(stderr,"\neth_type: BATMAN");

        if( (int) bathdr_u->type >=0x00 && (int) bathdr_u->type<=0x3f ) { 
          is_broadcast[0] = 1;
          memcpy( dst_mac, mac_all_one, 6 );  
          return nbytes;
        }
        else {
          is_broadcast[0] = 0;
          memcpy( dst_mac, bathdr_u->dst_mac, 6 );  //this is the tap destination interface.  
                                                    //need to see if it is one our "next hops in the originators table"

          if( (uint8_t) bathdr_u->type >(uint8_t) 0x3f ) { 
            //fprintf(stderr,"\nbat_unicast, try tcp_sub");
            eth_hdr *ethhdr = (eth_hdr *) &(batucast->ethhdr);
            if( htons(ethhdr->eth_type)==ETH_IP_TYPE) {
              new_len = do_tcp_subs((char *)ethhdr);
              if(new_len > 0) nbytes = new_len + sizeof(batman_unicast)+sizeof(eth_hdr); 
            }
          }
        }
      
    break;

    default :
      memcpy(dst_mac, mac_all_zero, 6);
      is_broadcast[0]=0;
      return 0;
  }

      if( memcmp(mac_all_one,dst_mac,6)==0 ) {
        is_broadcast[0] = 1;
      }
      else {
        is_broadcast[0] = 0;
      }


 // if(nbytes>0 && debug_tap) {
 //  fprintf(stderr,"\n\nRead %d bytes from ofdm0\n", nbytes);
 //  for(i=0;i<nbytes;i++) {
 //    if(i%32==0) fprintf(stderr,"\n");
 //    fprintf(stderr,"%02x,", buffer[i]);
 //  }
 // }
 return nbytes;
}
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
int write_tap_dev(char *buffer, int len) {
 int i;
 int nbytes;
 int new_len;

 eth_frame *ethframe = (eth_frame *) buffer;

batman_frame_unicast *batucast = (batman_frame_unicast *) buffer; 
batman_frame_special *batspecial = (batman_frame_special *) buffer; 
  batman_unicast *bathdr_u = &(batucast->bathdr_unicast);

 eth_hdr *ethhdr = (eth_hdr *) &(ethframe->ethhdr);
 ip_hdr *ip = (ip_hdr *) &(ethframe->iphdr);
 tcp_hdr *tcp = (tcp_hdr *) &(ethframe->ip_proto_payload);
 udp_hdr *udp = (udp_hdr *) &(ethframe->ip_proto_payload);
 uint8_t *payload = (uint8_t *) tcp->payload;


 if(len<=0) return 0;


  uint16_t frame_type  = htons(ethhdr->eth_type);
  switch( frame_type ) {
    case  ETH_IP_TYPE :
      //fprintf(stderr,"\neth_type: IP");
       if( htons(ethhdr->eth_type)==ETH_IP_TYPE ) {
         new_len = do_tcp_subs(buffer);
         if(new_len > 0) len = new_len; 
       }
    break;

    case  ETH_BATMAN_TYPE :
      //fprintf(stderr,"\neth_type: BATMAN");

      if( (uint8_t) bathdr_u->type >(uint8_t) 0x3f ) { 

         //fprintf(stderr,"\nbat_unicast, try tcp_sub");
         eth_hdr *ethhdr = (eth_hdr *) &(batucast->ethhdr);
         if( htons(ethhdr->eth_type)==ETH_IP_TYPE) {
           new_len = do_tcp_subs((char *)ethhdr);
           if(new_len > 0) len = new_len + sizeof(batman_unicast)+sizeof(eth_hdr); 
         }
       }
      
    break;
  }


 //if(len>0 && debug_tap) {
 //  fprintf(stderr,"\n\nRF_IN -> writing %d bytes to ofdm0\n", len);
 //  for(i=0;i<len;i++) {
 //    if(i%32==0) fprintf(stderr,"\n");
 //    fprintf(stderr,"%02x,", buffer[i]);
 //  }
 // }

  i = write(dev->tun_fd, buffer, len);
  return i; 
}
