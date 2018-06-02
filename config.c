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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <arpa/inet.h>

#include "liquid/liquid.h"

#include "pluto.h"
#include "ofdm_conf.h"
#include "ofdm_rx.h"
#include "ofdm_tx.h"
#include "charon.h"

#include "tap_device.h"
#include "fftw3.h"
#include "ofdm.h"
#include "util.h"
#include "config.h"

//default values if not set manually in the u-boot environment
#define DEFAULT_ENABLE_CHARON 1 
#define DEFAULT_FREQ_OFFSET_PPM 6.15
#define DEFAULT_OGM_INTERVAL 10000 
#define DEFAULT_MAX_TCP_SEGS 2
#define DEFAULT_MAX_SHORT_RETRANS 8 
#define DEFAULT_MAX_LONG_RETRANS 1 
#define DEFAULT_BCAST_RETRANS 1
#define DEFAULT_USB_BATMAN_IF 0
#define DEFAULT_ACK_DELAY_TIMEOUT 25000 
#define DEFAULT_SYMBOL_DELAY ((OFDM_M+CP_LEN+TAPER_LEN)*(DECIMATE_INTERPOLATE_FACTOR/4))
#define DEFAULT_MAX_TCP_SHARE_BACKOFF (DEFAULT_SYMBOL_DELAY*12); 
#define DEFAULT_TXRX_FREQ_HZ 915000000
#define DEFAULT_SAMPLE_FREQ_HZ 11200000;
#define DEFAULT_RF_BANDWIDTH_HZ 250000 
#define DEFAULT_TX_OUTPUT_POWER -10

uint32_t usb_ip32;
char usb_ip[16];
static char cmd_str[128];

//define with fw_set_attr.  these values will be set/reset in init
double ref_correction_ppm;
int enable_charon;
int bat_ogm_interval;
int max_short_retrans;
int max_long_retrans;
int bcast_retrans;
int max_tcp_segs;
int usb_batman_if;
int ack_delay_timeout;
int symbol_delay_timeout;
int max_tcp_share_backoff;
int tcp_share_backoff;
long long freq_rxtx_hz;
long long sample_freq_hz;
long long rf_bandwidth;
long long tx_output_power_minus_dbm;

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void read_config() {

  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("enable_charon", cmd_str, 128) == 0) {  
    enable_charon = atoi(cmd_str);
    fprintf(stderr, "\nsetting enable_charon to %d", enable_charon);
  }
  else {
    enable_charon = DEFAULT_ENABLE_CHARON; 
    fprintf(stderr, "\ncould not find enable_charon key in u-boot env.  setting to default of %d", enable_charon);
  }

  if( enable_charon==0 ) {
    fprintf(stderr, "\ncharon is not enabled.  giving up control of iio context to allow normal Pluto SDR operations");
    exit(0);
  }

  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("freq_rxtx_hz", cmd_str, 128) == 0) {  
    freq_rxtx_hz = (long long) atoll(cmd_str);
    fprintf(stderr, "\nsetting freq_rxtx_hz to %lld hz", freq_rxtx_hz);
  }
  else {
    freq_rxtx_hz = DEFAULT_TXRX_FREQ_HZ; 
    fprintf(stderr, "\ncould not find freq_rxtx_hz key in u-boot env.  setting to default of %lld hz", freq_rxtx_hz);
  }

  //memset(cmd_str, 0x00, sizeof(cmd_str));
  //if( read_fw_attr_value("sample_freq_hz", cmd_str, 128) == 0) {  
  //  sample_freq_hz = (long long) atoll(cmd_str);
  //  fprintf(stderr, "\nsetting sample_freq_hz to %lld hz", sample_freq_hz);
  //}
  //else {
    sample_freq_hz = DEFAULT_SAMPLE_FREQ_HZ; 
    fprintf(stderr, "\ncould not find sample_freq_hz key in u-boot env.  setting to default of %lld hz", sample_freq_hz);
  //}

  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("rf_bandwidth", cmd_str, 128) == 0) {  
    rf_bandwidth = (long long) atoll(cmd_str);
    fprintf(stderr, "\nsetting rf_bandwidth to %lld hz", rf_bandwidth);
  }
  else {
    rf_bandwidth = DEFAULT_RF_BANDWIDTH_HZ; 
    fprintf(stderr, "\ncould not find rf_bandwidth key in u-boot env.  setting to default of %lld hz", rf_bandwidth);
  }

  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("tx_output_power_minus_dbm", cmd_str, 128) == 0) {  
    tx_output_power_minus_dbm = (long long) atoll(cmd_str);
    tx_output_power_minus_dbm *= -1;
    fprintf(stderr, "\nsetting tx_output_power_minus_dbm to %lld dBm", tx_output_power_minus_dbm);
  }
  else {
    tx_output_power_minus_dbm = DEFAULT_TX_OUTPUT_POWER;
    fprintf(stderr, "\ncould not find tx_output_power_minus_dbm key in u-boot env.  setting to default of %lld dBm", tx_output_power_minus_dbm);
  }


  //freq correction value from u-boot env
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("ref_correction_ppm", cmd_str, 128) == 0) {  
    ref_correction_ppm = (double) atof(cmd_str);
    fprintf(stderr, "\nsetting ref_correction_ppm to %3.1f ppm", ref_correction_ppm);
  }
  else {
    ref_correction_ppm = DEFAULT_FREQ_OFFSET_PPM; //default based on data collected so far 
    fprintf(stderr, "\ncould not find ref_correction_ppm key in u-boot env.  setting to default of %3.1f ppm", ref_correction_ppm);
  }

  //batman protocol OGM interval setting from u-boot env
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("bat_ogm_interval", cmd_str, 128) == 0) {  
    bat_ogm_interval = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting bat_ogm_interval to %d ms", bat_ogm_interval);
  }
  else {
    bat_ogm_interval = DEFAULT_OGM_INTERVAL; 
    fprintf(stderr, "\ncould not find bat_ogm_interval in u-boot env.  setting to default of %d ms", bat_ogm_interval);
  }

  //max number of retransmissions before dropping a frame
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("max_long_retrans", cmd_str, 128) == 0) {  
    max_long_retrans = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting max_long_retrans to %d", max_long_retrans);
  }
  else {
    max_long_retrans = DEFAULT_MAX_LONG_RETRANS; 
    fprintf(stderr, "\ncould not find max_long_retrans in u-boot env.  setting to default of %d", max_long_retrans);
  }

  //max number of retransmissions before dropping a frame
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("max_short_retrans", cmd_str, 128) == 0) {  
    max_short_retrans = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting max_short_retrans to %d", max_short_retrans);
  }
  else {
    max_short_retrans = DEFAULT_MAX_SHORT_RETRANS; 
    fprintf(stderr, "\ncould not find max_short in u-boot env.  setting to default of %d", max_short_retrans);
  }

  //number of times to transmit broadcast.  they are never acked
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("bcast_retrans", cmd_str, 128) == 0) {  
    bcast_retrans = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting bcast_retrans to %d", bcast_retrans);
  }
  else {
    bcast_retrans = DEFAULT_BCAST_RETRANS; 
    fprintf(stderr, "\ncould not find bcast_retrans in u-boot env.  setting to default of %d", bcast_retrans);
  }

  //max tcp segments  (set tcp window size between two end points to keep bandwidth limited interface from getting blasted with segments) 
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("max_tcp_segs", cmd_str, 128) == 0) {  
    max_tcp_segs = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting max_tcp_segs to %d", max_tcp_segs);
  }
  else {
    max_tcp_segs = DEFAULT_MAX_TCP_SEGS; 
    fprintf(stderr, "\ncould not find max_tcp_segs in u-boot env.  setting to default of %d", max_tcp_segs);
  }

  //is the usb interface batman enabled?
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("usb_batman_if", cmd_str, 128) == 0) {  
    usb_batman_if = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting usb_batman_if to %d", usb_batman_if);
  }
  else {
    usb_batman_if = DEFAULT_USB_BATMAN_IF; 
    fprintf(stderr, "\ncould not find usb_batman_if in u-boot env.  setting to default of %d", usb_batman_if);
  }

  //
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("ack_delay_timeout", cmd_str, 128) == 0) {  
    ack_delay_timeout = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting ack_delay_timeout to %d", ack_delay_timeout);
  }
  else {
    ack_delay_timeout = DEFAULT_ACK_DELAY_TIMEOUT; 
    fprintf(stderr, "\ncould not find ack_delay_timeout in u-boot env.  setting to default of %d", ack_delay_timeout);
  }

  //
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("symbol_delay_timeout", cmd_str, 128) == 0) {  
    symbol_delay_timeout = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting symbol_delay_timeout to %d", symbol_delay_timeout);
  }
  else {
    symbol_delay_timeout = DEFAULT_SYMBOL_DELAY; 
    fprintf(stderr, "\ncould not find symbol_delay_timeout in u-boot env.  setting to default of %d", symbol_delay_timeout);
  }

  //
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("max_tcp_share_backoff", cmd_str, 128) == 0) {  
    max_tcp_share_backoff = (int) atoi(cmd_str);
    fprintf(stderr, "\nsetting max_tcp_share_backoff to %d", max_tcp_share_backoff);
  }
  else {
    max_tcp_share_backoff = DEFAULT_MAX_TCP_SHARE_BACKOFF; 
    fprintf(stderr, "\ncould not find max_tcp_share_backoff in u-boot env.  setting to default of %d", max_tcp_share_backoff);
  }

  //pluto ip address for mesh-bridge (normally usb0)
  usb_ip32 = get_netif_ip("usb0", usb_ip);
  memset(cmd_str, 0x00, sizeof(cmd_str));
  if( read_fw_attr_value("ipaddr", cmd_str, 128) == 0) {  
    inet_pton( AF_INET, cmd_str, &usb_ip32 ); 
    fprintf(stderr, "\nfound ipaddr in u-boot environment");
  }

  sleep(1);

  if( init_tap_device() != 0) {
    fprintf(stderr, "\ncould not create the ofdm0 interface");
    exit(0);
  }

  struct in_addr ia;
  ia.s_addr = usb_ip32;
  strcpy( usb_ip, inet_ntoa(ia) );

  fprintf(stderr, "\nmesh-bridge: ip_addr %08x, ip_a: %s\n", htonl(usb_ip32), usb_ip);
  fprintf(stderr, "\nofdm mac->  %02x:%02x:%02x:%02x:%02x:%02x", 
    ofdm0_mac[0], 
    ofdm0_mac[1], 
    ofdm0_mac[2], 
    ofdm0_mac[3], 
    ofdm0_mac[4], 
    ofdm0_mac[5] );

  //this option allows for non-batman aware clients to use the mesh network.  it has the advantage of less OGM traffic, but
  //has the disadvantage that the client will have to wait to discover other nodes in the network via ARP after the local ARP tables
  //expire.
  if(usb_batman_if==0) {

      system("/sbin/ip link set down dev ofdm0");
      system("/sbin/ip link set down dev usb0");
      system("/sbin/ip link set down dev mesh-bridge");
      sleep(1);

      system("/sbin/ifconfig ofdm0 0.0.0.0 mtu 2342");
      system("/sbin/ifconfig usb0 0.0.0.0 mtu 2342");

      system("/usr/sbin/batctl if add ofdm0");
      system("/sbin/ip link set up dev ofdm0");
      sleep(1);

      system("/sbin/ip link add name mesh-bridge type bridge");
      system("/sbin/ip link set dev usb0 master mesh-bridge");
      system("/sbin/ip link set dev bat0 master mesh-bridge");

      system("/sbin/ifconfig mesh-bridge 0.0.0.0 mtu 2342");

      sleep(1);
      system("/sbin/ip link set up dev usb0");
      sleep(1);
      system("/sbin/ip link set up dev bat0");
      sleep(1);
      system("/sbin/ip link set up dev mesh-bridge");
      sleep(1);
      sprintf( cmd_str, "/sbin/ifconfig mesh-bridge %s up\0", usb_ip); 
      system(cmd_str);
      sleep(1);
  }
  else {
      system("/sbin/ip link set down dev ofdm0");
      system("/sbin/ip link set down dev usb0");
      system("/sbin/ip link set down dev mesh-bridge");
      sleep(1);

      system("/sbin/ifconfig ofdm0 0.0.0.0 mtu 2342");
      system("/sbin/ifconfig usb0 0.0.0.0 mtu 2342");

      system("/usr/sbin/batctl if add ofdm0");
      system("/sbin/ip link set up dev ofdm0");
      sleep(1);

      system("/usr/sbin/brctl addbr mesh-bridge");
      system("/usr/sbin/brctl addif mesh-bridge bat0");
      system("/usr/sbin/brctl addif mesh-bridge ofdm0");
      system("/usr/sbin/brctl addif mesh-bridge usb0");

      system("/sbin/ifconfig mesh-bridge 0.0.0.0 mtu 2342");

      sleep(1);
      system("/sbin/ip link set up dev usb0");
      sleep(1);
      system("/sbin/ip link set up dev bat0");
      sleep(1);
      system("/sbin/ip link set up dev mesh-bridge");
      sleep(1);
      sprintf( cmd_str, "/sbin/ifconfig mesh-bridge %s up\0", usb_ip); 
      system(cmd_str);
      sleep(1);
  }

  sprintf( cmd_str, "/usr/sbin/batctl it %d\0", bat_ogm_interval); 
  system(cmd_str); //originator frame interval in ms 

  fprintf(stderr,"\ngiving charon a cpu core to itself.");
  sprintf( cmd_str, "/usr/bin/taskset -p 02 %d", getpid());   //give charon a cpu core to itself
  system(cmd_str);
}
