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

#include <stdint.h>
#include <stdio.h>
#include "tcp_subs.h"
#include "ethernet.h"
#include "charon.h"
#include "config.h"

#define TCP_MSS_SIZE 1460
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
static uint16_t inet_chksum(uint16_t sum, uint8_t *data, uint16_t len)
{
  uint16_t t=0;
  int16_t i=0;

  for (i=0; i<len; i+=2) {
    t = (data[i] << 8) + data[i+1];
    sum += t;
    //carry
    if (sum < t) {
      sum++;
    }
  }
  return sum;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
uint16_t ipchksum(uint8_t *ipbuffer)
{
  uint16_t sum=0;
  sum = inet_chksum(0, ipbuffer, sizeof(ip_hdr));
  return (sum == 0) ? 0xffff : htons(sum);
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
static uint16_t upper_layer_chksum(uint8_t proto, uint8_t *eth_buffer)
{

  uint16_t upper_layer_len=0;
  uint16_t sum=0;

  eth_frame *ethframe = (eth_frame *) eth_buffer;
  ip_hdr *ip = (ip_hdr *) &(ethframe->iphdr);
  tcp_hdr *tcp = (tcp_hdr *) &(ethframe->ip_proto_payload);
  udp_hdr *udp = (udp_hdr *) &(ethframe->ip_proto_payload);

  upper_layer_len = htons( ip->len ) - sizeof(ip_hdr);

  switch (proto) {

  case  IP_PROTO_UDP  :
  case  IP_PROTO_TCP  :

    sum = upper_layer_len + proto;

    if ( (upper_layer_len & 0x01) ) {
      eth_buffer[upper_layer_len + sizeof(ip_hdr) + sizeof(eth_hdr)] = 0x00;
      sum = inet_chksum(sum, (uint8_t *)&eth_buffer[sizeof(eth_hdr) + 12], 2 * 4); //2 * length of ip address
      sum = inet_chksum(sum, &eth_buffer[sizeof(ip_hdr) + sizeof(eth_hdr)], upper_layer_len+1);
    } else {
      sum = inet_chksum(sum, (uint8_t *)&eth_buffer[sizeof(eth_hdr) + 12], 2 * 4); //2 * length of ip address
      sum = inet_chksum(sum, &eth_buffer[sizeof(ip_hdr) + sizeof(eth_hdr)], upper_layer_len);
    }
    break;

  case  IP_PROTO_ICMP :
    sum = inet_chksum(sum, &eth_buffer[sizeof(ip_hdr) + sizeof(eth_hdr)], upper_layer_len);
    break;


  }

  return (sum == 0) ? 0xffff : htons(sum);
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
uint16_t tcpchksum(uint8_t *framebuffer)
{
  return upper_layer_chksum(IP_PROTO_TCP, framebuffer);
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
uint16_t udpchksum(uint8_t *framebuffer)
{
  return upper_layer_chksum(IP_PROTO_UDP, framebuffer);
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
uint16_t icmpchksum(uint8_t *framebuffer)
{
  return upper_layer_chksum(IP_PROTO_ICMP, framebuffer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
static void sub_mss(int16_t mss_sub_size, uint8_t *eth_frame_buffer)
{

  eth_frame *ethframe = (eth_frame *) eth_frame_buffer;
  eth_hdr *ethhdr = (eth_hdr *) &(ethframe->ethhdr);
  ip_hdr *ip = (ip_hdr *) &(ethframe->iphdr);
  tcp_hdr *tcp = (tcp_hdr *) &(ethframe->ip_proto_payload);
  udp_hdr *udp = (udp_hdr *) &(ethframe->ip_proto_payload);
  int16_t opt_len=0;
  uint8_t *opt_ptr = (uint8_t *) tcp->payload;
  int opt_index=0;

  //options length
  int16_t tcp_len = (int16_t) ((tcp->hdr_len&0xf0) >> 2);
  int16_t options_len = tcp_len-20; //sub 20 to get the size of the options


  if ( options_len > 1) {

    while ( opt_index <= options_len) {

      switch ((int16_t) *opt_ptr) {
      //end of options list
      case  TCP_OPT_END :
        return;
        break;

      //don't allow window scaling
      case  TCP_OPT_WIN_SCALE :

        *opt_ptr++ = TCP_OPT_NOOP;
        *opt_ptr++ = TCP_OPT_NOOP;
        *opt_ptr++ = TCP_OPT_NOOP;
        opt_len = 3;
        break;

      //no operation
      case  TCP_OPT_NOOP  :
        opt_ptr++;
        opt_len = 1;
        break;

      //option max segment size
      case  TCP_OPT_MSS   :

        opt_ptr++;
        opt_len = (int16_t) *opt_ptr++;
        //replace mss option
        *opt_ptr++ = (uint8_t) ((mss_sub_size)>>8)&0xff;
        *opt_ptr++ = (uint8_t) (mss_sub_size)&0xff;
        break;

      default :
        //skip past unknown option  
        opt_ptr++;
        opt_len = (int16_t) *opt_ptr;
        break;
      }

      opt_index+=opt_len;
    }
  }

}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
int do_tcp_subs(uint8_t *eth_frame_buffer) {

  int16_t i=0;
  int8_t recalc_chksum=0;


  eth_frame *ethframe = (eth_frame *) eth_frame_buffer;
  eth_hdr *ethhdr = (eth_hdr *) &(ethframe->ethhdr);
  ip_hdr *ip = (ip_hdr *) &(ethframe->iphdr);
  tcp_hdr *tcp = (tcp_hdr *) &(ethframe->ip_proto_payload);
  udp_hdr *udp = (udp_hdr *) &(ethframe->ip_proto_payload);
  uint8_t *payload = (uint8_t *) tcp->payload;

  if( htons( ethhdr->eth_type ) != ETH_IP_TYPE ) {
    //fprintf(stderr, "\nnot ip4, exiting tcp_subs");
    return 0;
  }

  int16_t sub_mss_size = TCP_MSS_SIZE; 

  if ( ip->proto==IP_PROTO_TCP && (tcp->tcp_flags == TCP_SYN || tcp->tcp_flags == (TCP_SYN | TCP_ACK)) ) {
    sub_mss(sub_mss_size, eth_frame_buffer);  
    recalc_chksum=1;
  }

  //pretty much no chance buffers will be a problem for the remotes, so re-write a constant window that is reasonable for bandwidth-limited wireless system
  if ( ip->proto==IP_PROTO_TCP ) {
    tcp->win_size = htons( max_tcp_segs * TCP_MSS_SIZE ); 
    recalc_chksum=1;
  }

  if ( ip->proto==IP_PROTO_TCP && recalc_chksum) {
    tcp->chksum = 0;
    tcp->chksum = ~tcpchksum(eth_frame_buffer);
    ip->chksum = 0;
    ip->chksum = ~ipchksum( (void *) ip );

    //new frame length
    //fprintf(stderr, "\ndid ip4 tcp_sub");
    return htons(ip->len) + sizeof(eth_hdr);
  }


  return 0;
}
