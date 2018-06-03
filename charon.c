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

#include <iio.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include <unistd.h>
#include <arpa/inet.h>

#include <getopt.h>
  
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



static int max_retrans;

//opts
static float IF;
static float QF;
static float complex sample;
static int ii=0;

static int first_tx=0;
static int i;

static uint8_t tap_buffer[2342];
static int n;

static int tx_retry;
static timer_obj *ack_timer;
static timer_obj *symbol_timer;
static timer_obj *agc_timer_fast;
static timer_obj *agc_timer_slow;

static int got_ack;
static long long ack_timeout;

static long long cur_time;
static long long ptime;

static int ack_to_scale;
static uint8_t is_broadcast;
static uint8_t dst_mac[6];

static long long rssi;
static long long in_gain;

static uint8_t dst_ofdm0_mac[6];
static uint8_t bat_route[32];
static int is_route;
static uint32_t current_pid;
uint8_t mac_to_ack[6];

int first_rx_after_agc_reset;

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char **argv) {

  srandom(time(NULL));

  read_config();

  pluto_init_txrx();
  pluto_set_enable_tx( 1 );

  init_ofdm_rx();
  init_ofdm_tx();


  pluto_set_in_sample_freq( sample_freq_hz ); 
  pluto_set_in_bw( rf_bandwidth ); 
  pluto_set_out_bw( rf_bandwidth ); 

  pluto_set_rx_freq( freq_rxtx_hz );  //tx freq also set here
  pluto_set_out_gain( -80 );
  

  ack_timer = create_timer();
  timer_reset(ack_timer);

  symbol_timer = create_timer();
  timer_reset(symbol_timer);

  agc_timer_fast = create_timer();
  timer_reset(agc_timer_fast);
  agc_timer_slow = create_timer();
  timer_reset(agc_timer_slow);



  main_loop();

  while(1) {
    main_loop();
  }

}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void do_got_ack() {
  fprintf(stderr, ", time: %lld usec", timer_elapsed_usec(ack_timer) );
  got_ack=1;
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void do_send_ack(uint8_t *ack_mac) {

  memcpy(mac_to_ack, ack_mac, 6);

  disable_rx();

  pluto_set_out_gain( tx_output_power_minus_dbm );
  usleep(1);

  do_ofdm_tx(NULL, 0, 0, 0, 0, mac_to_ack, 0);

  pluto_set_out_gain( -80 );

  enable_rx();
  usleep(200);

  fprintf(stderr, "\nSENT ACK TO ->%02x:%02x:%02x:%02x:%02x:%02x",
      mac_to_ack[0],
      mac_to_ack[1],
      mac_to_ack[2],
      mac_to_ack[3],
      mac_to_ack[4],
      mac_to_ack[5]
    );
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void do_tx( uint8_t *buff, int len, int is_retrans, uint8_t *dst_mac, uint8_t is_broadcast, uint32_t pid) {


 sprintf(bat_route, "%02x:%02x:%02x:%02x:%02x:%02x\0", 
    dst_mac[0],
    dst_mac[1],
    dst_mac[2],
    dst_mac[3],
    dst_mac[4],
   dst_mac[5] );

 
 if(is_broadcast==0x00) {
   is_route = is_batman_route(bat_route, dst_ofdm0_mac);

   fprintf(stderr, " lookup dst_mac,  %02x:%02x:%02x:%02x:%02x:%02x", 
      dst_ofdm0_mac[0],
      dst_ofdm0_mac[1],
      dst_ofdm0_mac[2],
      dst_ofdm0_mac[3],
      dst_ofdm0_mac[4],
      dst_ofdm0_mac[5] );
   if( !is_route ) {
     fprintf(stderr, "\n, not in batman tables. dropping");
     goto tx_done; 
   } 
 }

 disable_rx();

 pluto_set_out_gain( tx_output_power_minus_dbm );
 usleep(1);

  fprintf(stderr,"\nTAPDEV_IN -> RF_OUT , len=%d", n);
  do_ofdm_tx(buff, len, is_retrans, 1, is_broadcast, dst_ofdm0_mac, pid);
  timer_reset(ack_timer);

  if(tx_retry>0 && !is_broadcast) fprintf(stderr,", tx_retry=%d", (max_retrans+1-tx_retry));

  pluto_set_out_gain( -80 );
  enable_rx();
  usleep(200);


tx_done:
  timer_reset(ack_timer);
  timer_reset(symbol_timer);
  timer_reset(agc_timer_fast);
  timer_reset(agc_timer_slow);
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void reset_agc_timer(void) {
  timer_reset(agc_timer_fast);
  timer_reset(agc_timer_slow);
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void lbt_backoff(void) {
  timer_reset(symbol_timer);
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void check_agc(void) {

  if( timer_elapsed_usec(agc_timer_fast) > 10000) {

    rssi = pluto_get_in_rssi();
    in_gain = pluto_get_in_gain();


    if( rssi > -30 && in_gain > 16) {
      pluto_bump_agc_down(-4);
      fprintf(stderr, "\ncurrent RSSI: %d, in_gain %lld", rssi, in_gain); 
    }

    timer_reset(agc_timer_fast);
  }

  if( timer_elapsed_usec(agc_timer_slow) > 1000000) {

    rssi = pluto_get_in_rssi();
    in_gain = pluto_get_in_gain();

    if( in_gain < 73) {
      fprintf(stderr, "\ncurrent RSSI: %d, in_gain %lld", rssi, in_gain); 
      pluto_set_in_gain(73);
      pluto_set_in_gain_auto_fast();

      first_rx_after_agc_reset=1;
    }

    timer_reset(agc_timer_slow);

    //seems like a good timer to put this here
    if(tcp_share_backoff>symbol_delay_timeout) tcp_share_backoff -= symbol_delay_timeout; 
  }
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void main_loop(void) {

  static int rx_mod=0;

  //read, but don't wait around
  pluto_receive();

  //read while receiving
  while( ofdm_rx_state() != OFDMFRAMESYNC_STATE_SEEKPLCP || 
            (tx_retry > 0 && timer_elapsed_usec(ack_timer)<ack_timeout) ) { //make sure we get ACK

    pluto_receive();

    if(rx_mod++%256==0) {
      check_agc();
      timer_reset(symbol_timer);
    }
  } 

  //transmit
  if( ofdm_rx_state() == OFDMFRAMESYNC_STATE_SEEKPLCP) {

    //do retry tx
    if( tx_retry > 0 && got_ack==0) {

        if( timer_elapsed_usec(ack_timer)>( ack_timeout ) && timer_elapsed_usec(symbol_timer) > symbol_delay_timeout) { 
          ack_timeout = ack_delay_timeout;
          ack_timeout += (int) pow( (max_retrans-tx_retry), 2) * (int) (symbol_delay_timeout+(random()%symbol_delay_timeout)); //exp backoff with randomness

          do_tx(tap_buffer, n, 1, dst_mac, is_broadcast, current_pid);

          if(tx_retry>0) {
            tx_retry--;
          }
          if(tx_retry==0) {
             n = read_tap_dev(tap_buffer, sizeof(tap_buffer), dst_mac, &is_broadcast);
          }
        }
     }
     else {
       if(got_ack==1) {
         got_ack=0;
         n=0;
         tx_retry=0;
         //timer_reset(symbol_timer);
       }
       if(tx_retry==0) {
         n = read_tap_dev(tap_buffer, sizeof(tap_buffer), dst_mac, &is_broadcast);
       }
     }

    tcp_share_backoff=0; //don't do this for now
    //start a new tx?
    if(n>0 && tx_retry==0 && timer_elapsed_usec(symbol_timer)>(symbol_delay_timeout+tcp_share_backoff) ) {  //leave some bw.  better sharing of multiple tcp connections 

      ack_timeout = ack_delay_timeout;

      current_pid = (random()%ULONG_MAX); //used as unique identifier to allow droppng duplicate receptions of frames from re-transmission
      do_tx(tap_buffer, n, 0, dst_mac, is_broadcast, current_pid);

      if(!is_broadcast) {
        if( n  < 128 ) max_retrans = max_short_retrans;
            else max_retrans = max_long_retrans;

        tx_retry = max_retrans; //num retries 
        got_ack=0;
      }
      else {
        tx_retry=bcast_retrans; 
        got_ack=1;  //don't wait between for reply
      }

    }

  } 

} //main loop



////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void do_process_iq16(const int16_t i, const int16_t q) {

  IF = ((float) i)/32768.0f;
  QF = ((float) q)/32768.0f;
  sample = (float complex) (IF + _Complex_I * QF);

  bump_nco();
  do_ofdm_mix_down(sample, &sample);
  do_ofdm_rx(sample);

}
