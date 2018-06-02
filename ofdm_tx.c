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

#include <iio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <complex.h>

#include "timers.h"

#include "liquid/liquid.h"

#include "ofdm_conf.h"
#include "ofdm_tx.h"
#include "pluto.h"
#include "ethernet.h"
#include "tap_device.h"

static unsigned int ofdm_M;
static unsigned int ofdm_cp_len;
static unsigned int ofdm_taper_len;
static unsigned int ofdm_payload_len;

static float complex ofdm_symbol_buffer[OFDM_M + CP_LEN];   // time-domain buffer
static unsigned char ofdm_header[8];            // header data
static unsigned char ofdm_payload[32*1024]; 
static unsigned char ofdm_p[OFDM_M];                 // subcarrier allocation (null/pilot/data)
static int ofdm_last_symbol;
static int ofdm_index=0;

static int i;

static ofdmflexframegen ofdm_fg;

static timer_obj *timer1;
static uint32_t out_pid;
const uint32_t charon_ack_magic = CHARON_ACK_MAGIC;
const int charon_added_length = 4 + sizeof(eth_hdr);//charon frame adds 1 eth_hdr to the batframe. 
                                        //then we add 4 pid bytes for detecting and dropping re-trans

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
int ofdm_get_sample_count( int payload_len ) {

int ofdm_sample_count=0;

  //if(ofdm_sample_count!=0) return ofdm_sample_count;  //only do this once on ofdm_tx_init

  ofdmflexframegen_assemble(ofdm_fg, ofdm_header, ofdm_payload, payload_len);


  while(1) {
    ofdm_last_symbol = ofdmflexframegen_write(ofdm_fg, ofdm_symbol_buffer, (OFDM_M+CP_LEN) );

    ofdm_sample_count += (OFDM_M + CP_LEN);

    if(ofdm_last_symbol) {
      return ofdm_sample_count;
    }

  }

  return 0; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void init_ofdm_tx() {

    ofdm_M = OFDM_M;                // number of subcarriers
    ofdm_cp_len = CP_LEN;           // cyclic prefix length
    ofdm_taper_len = TAPER_LEN;         // taper length
    ofdm_payload_len = PAYLOAD_LEN;     // length of payload (bytes)


    // initialize frame generator properties
    ofdmflexframegenprops_s ofdm_fgprops;
    ofdmflexframegenprops_init_default(&ofdm_fgprops);


    ofdm_fgprops.check           = OFDM_CRC;
    ofdm_fgprops.fec0            = OFDM_FEC0; 
    ofdm_fgprops.fec1            = OFDM_FEC1;
    ofdm_fgprops.mod_scheme      = OFDM_MODULATION; 

     ofdmframe_init_default_sctype(ofdm_M, ofdm_p);
     ofdm_fg = ofdmflexframegen_create(ofdm_M,ofdm_cp_len,ofdm_taper_len,ofdm_p,&ofdm_fgprops);

    timer1 = create_timer();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int do_ofdm_tx( uint8_t *buffer, int len, int is_retrans, int do_dump_rx, int is_broadcast, uint8_t *dst_ofdm0_mac, uint32_t pid ) {

 long long elapsed;
 charon_frame *charonframe = (charon_frame *) &ofdm_payload[0];  
 eth_hdr *ethhdr_charon = (eth_hdr *) &(charonframe->ethhdr_charon);

 out_pid = pid;
 
 int tlen = len;
  tlen += charon_added_length; 

  if( memcmp( dst_ofdm0_mac, ofdm0_mac, 6)==0) return 0; //don't send to ourself

  //fill-in the charon portion of the frame
    //first 4 bytes are either pid  or ack magic
    if(len>0) {
      memcpy( &(charonframe->first_four) , &out_pid, 4);  //normal frame
    }
    else {
      //charonframe->first_four = htonl( CHARON_ACK_MAGIC );  //ack frame
      memcpy( ofdm_payload, dst_ofdm0_mac, 6 );  //make ack 6-bytes
      tlen = 6;
      goto do_send;
    }

    ethhdr_charon->eth_type = htons( ETH_CHARON_TYPE );

    if( is_broadcast ) {
      memcpy( &(ethhdr_charon->dst_mac), mac_all_one, 6);
    }
    else {
      memcpy( &(ethhdr_charon->dst_mac), dst_ofdm0_mac, 6);
    }

    memcpy( &(ethhdr_charon->src_mac), ofdm0_mac, 6);

  //fill the rest of ofdm_payload with the buffer skipping past charon offset bytes
  //note that len==0 for ACKS, so we use tlen to tx
    for(i=0;i<len;i++) {
     ofdm_payload[charon_added_length+i] = buffer[i];
    }

do_send:
  timer_reset(timer1);
  ofdmflexframegen_assemble(ofdm_fg, ofdm_header, ofdm_payload, tlen);
  elapsed = timer_elapsed_usec(timer1);

  //fprintf(stderr, ", frame assembly time: %lld usec", elapsed);

  timer_reset(timer1);
  while(1) {

    ofdm_last_symbol = ofdmflexframegen_write(ofdm_fg, ofdm_symbol_buffer, (OFDM_M+CP_LEN) );
    pluto_transmit(ofdm_symbol_buffer, (OFDM_M+CP_LEN), do_dump_rx, ofdm_last_symbol); 

    if(ofdm_last_symbol) {
      elapsed = timer_elapsed_usec(timer1);
      //fprintf(stderr, ", frame tx time: %lld usec", elapsed);
      return 0;
    }
  }
}
