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
#include "timers.h"
//
#include <iio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <complex.h>
#include "liquid/liquid.h"

#include "pluto.h"
#include "ofdm_conf.h"
#include "ofdm_rx.h"
#include "ofdm_tx.h"
#include "charon.h"
#include "fftw3.h"
#include "ofdm.h"
#include "tap_device.h"
#include "crc.h"
#include "ethernet.h"
#include "config.h"

static unsigned int M = OFDM_M;        // number of subcarriers
static unsigned int cp_len = CP_LEN;   // cyclic prefix length
static unsigned int taper_len = TAPER_LEN; // taper length
static unsigned char p[OFDM_M];         // subcarrier allocation (null/pilot/data)
static void * userdata;            // user-defined data

static ofdmflexframesync fs;

int did_rx_ok;


// options
static unsigned int DM = DECIMATE_INTERPOLATE_FACTOR;         // decimation factor
// generate input signal and decimate
static float complex x[DECIMATE_INTERPOLATE_FACTOR];         // input samples
static float complex y;            // output sample

static float h[8192];             // filter coefficients
static unsigned int h_len;
static firdecim_crcf q;

static int dec_mod;
static long long time_start;
static long long time_secs;
static long long kbps;
static float per;
static long long rx_gain;
static float rssi_dbm;

#define MAX_CONSTELLATION_POINTS 1024
static float complex frame_sym_samples[MAX_CONSTELLATION_POINTS];

static struct timeval start;
static struct timeval end;
static long long kbit_bytes;
static long long kbit_count;
static long long drate;
static long long drate_bps;

static float last_good_nco_freq;

static int total_good_frames=0;
static int total_bad_frames=0;
static int64_t total_bytes=0;

static struct ofdmflexframesync_s *_q;
static struct ofdmframesync_s *_qq;
static uint8_t frame_buffer[2346];
static int frame_len;

static nco_crcf ofdm_nco;

static uint32_t in_pid;
static uint8_t ack_mac[6];
static uint8_t rec_ack[6];
static int is_broadcast = 0;
static int is_ack=0;
static int is_accept=0;
static int i;

static int data_rate_kbps;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void bump_nco() {
  static int nco_mod=0;
  if(nco_mod++%128==0) {
    nco_crcf_set_frequency(ofdm_nco, (rand()/RAND_MAX)*1e-4  );  //spread some of the dc offset /flicker noise out
                                                                 // by +/- 140Hz
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void do_ofdm_mix_down(float complex sample, float complex *y) {
  nco_crcf_step(ofdm_nco);
  nco_crcf_mix_down(ofdm_nco, sample, y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
int ofdm_rx_state() {

  if(_q == NULL) {
    _q = fs;
    _qq = _q->fs;
  }

  return _qq->state; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
int ofdm_rx_callback(unsigned char *  _header,
int              _header_valid,
unsigned char *  _payload,
unsigned int     _payload_len,
int              _payload_valid,
framesyncstats_s _stats,
void *           _userdata) {

charon_frame *charonframe = (charon_frame *) &_payload[0];  //skip past pid area
eth_hdr *ethhdr_charon = (eth_hdr *) &(charonframe->ethhdr_charon);

in_pid=0;
is_broadcast = 0;
is_ack=0;
is_accept=0;

  if(data_rate_kbps==0) {
    data_rate_kbps = (((sample_freq_hz/8 / DECIMATE_INTERPOLATE_FACTOR)/(OFDM_M+CP_LEN+TAPER_LEN)) * (46*_stats.mod_bps)) /1e3; //46 assumes 64-subcarriers with 46 being data
    data_rate_kbps *= ( (8.0/12.0) * (64.0/72.0) );   //adjust for FEC coding rate
                                                      //HAMMING (8/12), SECDED (64/72)
  }

  lbt_backoff();  //we received something, so wait at least an ofdm symbol before transmitting

  if( _header_valid && _payload_valid ) {


    if( _payload_len==6) {
      memcpy(rec_ack, _payload, 6);

      if( memcmp(rec_ack, ofdm0_mac, 6)==0) {
        is_ack=1;
        is_accept=1;
      }
      else {
        fprintf(stderr, "\nACK_NOT_FOR_US  %02x:%02x:%02x:%02x:%02x:%02x",
            rec_ack[0],
            rec_ack[1],
            rec_ack[2],
            rec_ack[3],
            rec_ack[4],
            rec_ack[5]
          );
        if(tcp_share_backoff<max_tcp_share_backoff) tcp_share_backoff += symbol_delay_timeout; 
        return 0;
      }
    }
    else {
      is_ack=0;

      if( ethhdr_charon->eth_type == htons( ETH_CHARON_TYPE ) ) {
        memcpy( &in_pid, &(charonframe->first_four), 4 );

        is_accept=1;
        if( memcmp( &(ethhdr_charon->dst_mac), mac_all_one, 6) == 0 ) is_broadcast=1;

       // fprintf(stderr, "\nRF_IN is charon type, bc:%d, ", is_broadcast);

        if( memcmp( &(ethhdr_charon->src_mac), ofdm0_mac, 6) == 0 ) {
          fprintf(stderr,"\nRF_IN-> dropped (srcmac==ofdm0_mac)");
          //we sent this, so just drop it.
          return 0;
        }


        if( !is_broadcast && memcmp( &(ethhdr_charon->dst_mac), ofdm0_mac, 6) != 0 ) {
          //this is not our tap mac, so drop it.
          fprintf(stderr,"\nRF_IN-> not-acking. drop (dstmac!=ofdm0_mac)");
          //for(i=0;i<32;i++) {
           // fprintf(stderr, "%02x,", _payload[i] );
          //}
          if(tcp_share_backoff<max_tcp_share_backoff) tcp_share_backoff += symbol_delay_timeout; 
          return 0;
        }

        memcpy( ack_mac, &(ethhdr_charon->src_mac), 6);

      }
      else {
        //fprintf(stderr, "\nRF_IN not charon, len=%d , type: %04x, first 32->  ", _payload_len, htons(ethhdr_charon->eth_type));
        //for(i=0;i<32;i++) {
         // fprintf(stderr, "%02x,", _payload[i] );
        //}
        return 0;
      }
    }

  }

  if(_q == NULL) {
    _q = fs;
    _qq = _q->fs;
  }


  if(time_start==0) {
    time_start = time(NULL);
  }
  else {
    time_secs = time(NULL)-time_start;
  }

  if(_header_valid && !_payload_valid) {

    //ths seems to be working well
    if(_stats.rssi > -25) {
      pluto_bump_agc_down( (-(_stats.rssi+25)) / 3);
    }
    else if(_stats.rssi < -30) {
      pluto_bump_agc_up( (-(_stats.rssi+30)) / 3);
    }
  }

  if(_header_valid && _payload_valid && is_accept) {

    if(first_rx_after_agc_reset) {
      first_rx_after_agc_reset=0;
      rx_gain = pluto_get_in_gain();
      pluto_set_in_gain( pluto_get_in_gain() + 15 );
    }

    if(!is_ack) {
      frame_len = _payload_len - charon_added_length;
      memcpy(frame_buffer, &_payload[charon_added_length], frame_len);
    }

    rx_gain = pluto_get_in_gain();
    rssi_dbm = (float) -rx_gain + _stats.rssi + 12.0f -4.7;  //12dB for OFDM crest factor @ p = 10e-7,  -4.7 dB for QAM crest

    reset_agc_timer(); 

    //ths seems to be working well
    if( _stats.evm > -28 ) {
      if(_stats.rssi > -20 && rx_gain > 12) {
        pluto_bump_agc_down(-(_stats.rssi+25)/3);
      }
      else if(_stats.rssi < -26 && rx_gain <73) {
        pluto_bump_agc_up(-(_stats.rssi+26)/3);
      }
    }

    //we didn't transmit this and it is an ack frame,  cancel tx retries
    if( is_ack ) {
      last_good_nco_freq = nco_crcf_get_frequency(_qq->nco_rx);
      fprintf(stderr, "\nRF_IN-> GOT ACK, %02x:%02x:%02x:%02x:%02x:%02x",
          rec_ack[0],
          rec_ack[1],
          rec_ack[2],
          rec_ack[3],
          rec_ack[4],
          rec_ack[5]
        );
      do_got_ack();
      return 0;
    }

    if(!is_ack ) { 

      if( !is_broadcast && is_dup(in_pid) ) {
        fprintf(stderr, "\nRF_IN->dropping duplicate.  sending ACK, gain: %lld", rx_gain);
        last_good_nco_freq = nco_crcf_get_frequency(_qq->nco_rx);
        if(!is_broadcast) do_send_ack(ack_mac);
        return 0;
      }
      
      if(!is_broadcast) add_dup(in_pid);
      if(!is_broadcast) do_send_ack(ack_mac);

      if( write_tap_dev(frame_buffer, frame_len) == frame_len) {
        fprintf(stderr, "\nRF_IN->TAPDEV_OUT_, len=%d, rssi: %3.1f dBm, rx EVM %3.1f dB, in-gain: %lld dB, d-rate %d Kbps, ", frame_len, rssi_dbm, _stats.evm, rx_gain, data_rate_kbps);
        fprintf(stderr,"#sub-carriers=%d, mod: %s", OFDM_M, modulation_types[_stats.mod_scheme].name);
        last_good_nco_freq = nco_crcf_get_frequency(_qq->nco_rx);
      }
      rx_timeout = RX_TIMEOUT;
    }


    did_rx_ok=1;
    total_good_frames++;
    total_bytes += frame_len;

    drate += _stats.num_framesyms * _stats.mod_bps;
    kbit_bytes += frame_len; 
    gettimeofday(&end, NULL);

    if(kbit_count++==4 ) {
      kbps = (long long) ((kbit_bytes*8L*1e3L)/ ( (end.tv_sec*1e6+end.tv_usec) - (start.tv_sec*1e6+start.tv_usec)) );
      drate_bps = (long long) ((drate*1e3L)/ ( (end.tv_sec*1e6+end.tv_usec) - (start.tv_sec*1e6+start.tv_usec)) );
      
      gettimeofday(&start, NULL);
      kbit_count=0;
      kbit_bytes=0;
      drate = 0;
    }


    //only send constellation if it was a validated frame
    //memset(frame_sym_samples,0x00,sizeof(frame_sym_samples));
    //int i;
    //for(i=0;i<MAX_CONSTELLATION_POINTS;i++) {
      //frame_sym_samples[i] = _stats.framesyms[i];
      //fprintf(stderr, "\n%f,%f", creal(frame_sym_samples[i]), cimag(frame_sym_samples[i]) );
    //}

  }
  else {
    nco_crcf_set_frequency(_qq->nco_rx, last_good_nco_freq);

    did_rx_ok=0;
    total_bad_frames++;
    if(time(NULL)%4==0) {
      kbps=0;
      drate_bps=0;
    }
  }

  if(total_good_frames>0) per = (float) ( (float) total_bad_frames / (float) (total_good_frames+total_bad_frames)) * 100.0f;

  //framesyncstats_print(&_stats);
  //ofdmflexframesync_print(fs);


  if(total_good_frames%32==0) {
    //fprintf(stderr, "\r\ntotal good/bad frames: %d / %d   %lld bytes, PER: %3.2f %, h_, p_: %d, %d, time: %lld, tput: %lld kbps, tput: %4.1f KBytes/sec, drate %lld kbps, rssi %3.1f dBm\n\0", total_good_frames, total_bad_frames, total_bytes, per, _header_valid, _payload_valid, time_secs, kbps, (float) kbps/8.0f, drate_bps, rssi_dbm );
  }




    //fprintf(stderr,"    EVM                 :   %12.8f dB\n", _stats->evm);
    //fprintf(stderr,"    rssi                :   %12.8f dB\n", _stats->rssi);
    //fprintf(stderr,"    carrier offset      :   %12.8f Fs\n", _stats->cfo);
    //fprintf(stderr,"    num symbols         :   %u\n", _stats->num_framesyms);
    //fprintf(stderr,"    validity check      :   %s\n", crc_scheme_str[_stats->check][0]);
    //fprintf(stderr,"    fec (inner)         :   %s\n", fec_scheme_str[_stats->fec0][0]);
    //fprintf(stderr,"    fec (outer)         :   %s\n", fec_scheme_str[_stats->fec1][0]);

  return 0; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void init_ofdm_rx(void) {

  ofdmframe_init_default_sctype(M, p);  //default pilot/guard-nulls allocation

  fs = ofdmflexframesync_create(M, cp_len, taper_len, p, ofdm_rx_callback, userdata);

  h_len = estimate_req_filter_len( (1.0/(DECIMATE_INTERPOLATE_FACTOR*2.0*OFDM_RX_BW_FACTOR)), OFDM_RX_STOP_DB );
  fprintf(stderr, "\nrx h_len: %d", h_len);

  liquid_firdes_kaiser(h_len,  1.0/(DECIMATE_INTERPOLATE_FACTOR*2.0*OFDM_RX_BW_FACTOR)  ,OFDM_RX_STOP_DB,0.0f,h);
  q = firdecim_crcf_create(DM,h,h_len);

  ofdm_nco = nco_crcf_create(LIQUID_NCO);
  nco_crcf_set_frequency(ofdm_nco, 0); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
void do_ofdm_rx(float complex sample) {

  x[dec_mod++] = sample;

  if(dec_mod==DM) {
    dec_mod=0;
    firdecim_crcf_execute(q, x, &y);

    ofdmflexframesync_execute(fs, &y, 1);
  }
}
