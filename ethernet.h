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

#ifndef __ETH_RAW_FILTER_H__
#define __ETH_RAW_FILTER_H__

#include <stdint.h>

#define ICMP_ECHO_REQ 8
#define ICMP_ECHO_REPLY 0

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_CTL 0x3f

#define TCP_OPT_END     0
#define TCP_OPT_NOOP    1
#define TCP_OPT_MSS     2
#define TCP_OPT_WIN_SCALE 3
#define TCP_OPT_SACK  4
#define TCP_OPT_TIMESTAMP 8

#define IP_PROTO_ICMP  0x01
#define IP_PROTO_TCP   0x06
#define IP_PROTO_UDP   0x11
#define ETHERNET_MTU 1500
#define TCP_MAX_OPTIONS_LEN 20

#define ETH_IP_TYPE 0x0800
#define ETH_ARP_TYPE 0x0806
#define ETH_BATMAN_TYPE 0x4305
#define ETH_CHARON_TYPE 0x0420
#define CHARON_ACK_MAGIC 0x2984302A

#ifndef htons
#define htons(A) ((((uint16_t)(A) & 0xff00) >> 8) | \
                    (((uint16_t)(A) & 0x00ff) << 8))
#endif

#ifndef htonl
#define htonl(A) ((((uint32_t)(A) & 0xff000000) >> 24) | \
                    (((uint32_t)(A) & 0x00ff0000) >> 8)  | \
                    (((uint32_t)(A) & 0x0000ff00) << 8)  | \
                    (((uint32_t)(A) & 0x000000ff) << 24))
#endif

typedef struct __attribute__((packed))
{
  uint8_t   dst_mac[6];
  uint8_t   src_mac[6];
  uint16_t     eth_type;
  uint8_t   payload[14];
}
eth_hdr_payload;

typedef struct __attribute__((packed))
{
  uint8_t   dst_mac[6];
  uint8_t   src_mac[6];
  uint16_t     eth_type;
}
eth_hdr;

typedef struct __attribute__((packed))
{
  uint8_t type;
  uint8_t version;
  uint8_t ttl;
  uint32_t seq;
  uint8_t orig_mac[6];

}
batman_special;

typedef struct __attribute__((packed))
{
  uint8_t type;
  uint8_t version;
  uint8_t ttl;
  uint8_t tt_version;
  uint8_t dst_mac[6];
}
batman_unicast;

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_CTL 0x3f

#define TCP_OPT_END     0
#define TCP_OPT_NOOP    1
#define TCP_OPT_MSS     2
#define TCP_OPT_WIN_SCALE 3
#define TCP_OPT_SACK  4
#define TCP_OPT_TIMESTAMP 8

#define TCP_MAX_OPTIONS_LEN 20

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

typedef struct __attribute__((packed))
{
  uint8_t     version;
  uint8_t        tos;
  uint16_t       len;
  uint16_t       id;
  uint16_t       offset;
  uint8_t        ttl;
  uint8_t        proto;
  uint16_t       chksum;
  uint32_t    src_ip;
  uint32_t    dst_ip;
}
ip_hdr;

typedef struct __attribute__((packed))
{
  uint16_t  src_port;
  uint16_t  dst_port;
  uint32_t     tcp_seq;
  uint32_t     tcp_ack;
  uint8_t      hdr_len;
  uint8_t      tcp_flags;
  uint16_t     win_size;
  uint16_t     chksum;
  uint16_t     urgent;
  uint8_t      payload[0];
}
tcp_hdr;

typedef struct __attribute__((packed))
{
  uint16_t     src_port;
  uint16_t     dst_port;
  uint16_t     len;
  uint16_t     chksum;
  uint8_t      payload[0];
}
udp_hdr;
typedef struct __attribute__((packed))
{
  uint8_t type;
  uint8_t code;
  uint16_t     chksum;
  uint8_t      payload[0];
}
icmp_hdr;

typedef struct __attribute__((packed))
{
  uint16_t hardware_type;
  uint16_t protocol_type;
  uint8_t  hardware_size;
  uint8_t  protocol_size;
  uint16_t opcode;
  uint8_t  sender_mac[6];
  uint32_t sender_ip;
  uint8_t  target_mac[6];
  uint32_t target_ip;
}
arp_hdr;

typedef struct __attribute__((packed))
{
  eth_hdr ethhdr;
  arp_hdr arphdr;

}
eth_arp_frame;


typedef struct __attribute__((packed))
{
  eth_hdr ethhdr;
  ip_hdr iphdr;

  union __attribute__((packed)) {
    tcp_hdr tcphdr;
    udp_hdr udphdr;
    icmp_hdr icmphdr;
  }
  ip_proto_payload;

}
eth_frame;

typedef struct __attribute__((packed))
{
  eth_hdr ethhdr_bat;
  batman_special bathdr_special;
  eth_hdr ethhdr;
  ip_hdr iphdr;

  union __attribute__((packed)) {
    tcp_hdr tcphdr;
    udp_hdr udphdr;
    icmp_hdr icmphdr;
  }
  ip_proto_payload;

}
batman_frame_special;

typedef struct __attribute__((packed))
{
  eth_hdr ethhdr_bat;
  batman_unicast bathdr_unicast;
  eth_hdr ethhdr;
  ip_hdr iphdr;

  union __attribute__((packed)) {
    tcp_hdr tcphdr;
    udp_hdr udphdr;
    icmp_hdr icmphdr;
  }
  ip_proto_payload;

}
batman_frame_unicast;

typedef struct __attribute__((packed))
{
  uint32_t first_four;
  eth_hdr ethhdr_charon;
  union __attribute__((packed)) {
    batman_frame_special bat_special;
    batman_frame_unicast bat_unicast;
  }
  bat_frame;
}
charon_frame;
#endif
