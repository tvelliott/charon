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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <complex.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <float.h>
  
#include "liquid/liquid.h"
#include "filters/pluto/pluto_filters.h"
#include "ad9361.h"

#include "pluto.h"
#include "charon.h"
#include "ofdm_conf.h"
#include "ofdm_tx.h"
#include "timers.h"
#include "config.h"

struct iio_buffer {
    const struct iio_device *dev;
    void *buffer, *userdata;
    size_t length, data_length;

    uint32_t *mask;
    unsigned int dev_sample_size;
    unsigned int sample_size;
    bool is_output, dev_is_high_speed;
};

static struct iio_device *phy;
static struct iio_device *tx_dev;
static struct iio_device *rx_dev;
static struct iio_channel *tx0_i, *tx0_q;
static struct iio_channel *rx0_i, *rx0_q;

static struct iio_buffer *rxbuf;
static void *p_dat, *p_end, *p_start;
static ptrdiff_t p_inc;
static int pluto_rx_initialized=0;

static int tx_fd;

static struct iio_buffer *txbuf;
static char *tx_p_dat, *tx_p_end;
static ptrdiff_t tx_p_inc;
static int pluto_tx_initialized=0;

static int tx_enabled=0;

static unsigned int M=DECIMATE_INTERPOLATE_FACTOR;       // interpolation factor
static unsigned int h_len;     // interpolation filter length

    // design filter and create interpolator
static float h[2048];         // filter coefficients
static firinterp_crcf q;


// generate input signal and interpolate
static float complex x;        // input sample
static float complex y[DECIMATE_INTERPOLATE_FACTOR];     // output samples
static int nbytes_tx;
static int llen;
static int index=0;
static int tx_mod=0;
static int ii=0;
static int jj=0;
static int n_rx;

static long long prev_gain;

static int64_t time_to_tx;

static long long agc_gain = 72;
static int agc_locked=0;

int rx_timeout;
static int16_t _txi;
static int16_t _txq;
static int16_t _rxi;
static int16_t _rxq;
static int pushed;
static struct iio_context *ctx;
static long long rssi;
static long long gain_val;
static int more_data;
static int more_tx_data;

long long pluto_current_gain;
long long current_rx_freq;
long long current_sample_freq;

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
struct iio_context * pluto_init_txrx() {

    ctx = iio_create_local_context();

    phy = iio_context_find_device(ctx, "ad9361-phy");

    tx_dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
    rx_dev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    rx0_i = iio_device_find_channel(rx_dev, "voltage0", 0);
    rx0_q = iio_device_find_channel(rx_dev, "voltage1", 0);
    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    tx0_i = iio_device_find_channel(tx_dev, "voltage0", 1);
    tx0_q = iio_device_find_channel(tx_dev, "voltage1", 1);
    iio_channel_enable(tx0_i);
    iio_channel_enable(tx0_q);

    pluto_enable_fir(0);
    pluto_set_filter();
    pluto_enable_fir(1);

    pluto_set_out_gain( -80 );

    iio_channel_attr_write(
        iio_device_find_channel(phy, "voltage0", false),
        "gain_control_mode",
        "fast_attack"); 

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phy, "voltage0", false),
        "hardwaregain",
        73); 


    //RX Buffer
    if(!pluto_rx_initialized) {
      rxbuf = iio_device_create_buffer(rx_dev, 5000, false); //32768 needed for high rate 12.5khz channel


      if (!rxbuf) {
          perror("Could not create RX buffer");
          exit(0);
      }

      pluto_rx_initialized=1;
      iio_buffer_set_blocking_mode(rxbuf,false);

      p_inc = iio_buffer_step(rxbuf);//no need to init this every loop

      //fprintf(stderr, "\npluto rx buffer size: %d", IIO_RX_BUFFER_SIZE ); 
     }

    //TX Buffer
    if(!pluto_tx_initialized) {

      //h_len = OFDM_TX_HLEN; 
      h_len = estimate_req_filter_len( (1.0/(DECIMATE_INTERPOLATE_FACTOR*2.0*OFDM_TX_BW_FACTOR)), OFDM_TX_STOP_DB );
      fprintf(stderr, "\ntx h_len: %d", h_len);

      liquid_firdes_kaiser(h_len,  (1.0/(DECIMATE_INTERPOLATE_FACTOR*2.0*OFDM_TX_BW_FACTOR)),  OFDM_TX_STOP_DB,0.0f,h);
      q = firinterp_crcf_create(DECIMATE_INTERPOLATE_FACTOR,h,h_len);

      txbuf = iio_device_create_buffer(tx_dev, (OFDM_M+CP_LEN+TAPER_LEN)*DECIMATE_INTERPOLATE_FACTOR/4, false); //0==auto 
      //fprintf(stderr, "\npluto tx buffer size: %d", ofdm_get_symbol_count(PAYLOAD_LEN) ); 

      if (!txbuf) {
          perror("Could not create TX buffer");
          exit(0);
      }

      iio_buffer_set_blocking_mode(txbuf,true);
      pluto_tx_initialized=1;

      tx_p_inc = iio_buffer_step(txbuf);  //no need to init this every loop
        
    }


    return ctx;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_filter() {
  iio_device_attr_write_raw( phy, 
        "filter_fir_config", 
        //LTE1p4_MHz_ftr, 
        //LTE1p4_MHz_ftr_len);
        LTE20_MHz_ftr, 
        LTE20_MHz_ftr_len );
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_enable_fir(int enable) {
  ad9361_set_trx_fir_enable(phy, enable);
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
long long pluto_get_in_gain(void) {

  long long gain_val=72;

    iio_channel_attr_read_longlong(
        iio_device_find_channel(phy, "voltage0", false),
      "hardwaregain", 
      &gain_val);

  //fprintf(stderr, "\ngain: %lld", gain_val);

  return gain_val;
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_in_gain_auto_fast(void) {

    iio_channel_attr_write(
        iio_device_find_channel(phy, "voltage0", false),
        "gain_control_mode",
        "fast_attack"); 

}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
long long pluto_get_in_rssi(void) {

  rssi=-110; 

    iio_channel_attr_read_longlong(
        iio_device_find_channel(phy, "voltage0", false),
      "rssi", 
      &rssi);

  //fprintf(stderr, "\nin_rssi: %lld", -rssi);

  return -rssi;
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void enable_rx() {

  iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "altvoltage0", true),
      "frequency",
      current_rx_freq); 
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void disable_rx() {


  prev_gain = pluto_get_in_gain();

  iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "altvoltage0", true),
      "frequency",
      current_rx_freq+1000000 ); 
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_bump_agc_down(int delta) {
  pluto_set_in_gain( pluto_get_in_gain()+delta );
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_bump_agc_up(int delta) {
  pluto_set_in_gain( pluto_get_in_gain()+delta );
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_in_gain(long long gain) {

    if(gain<6) gain = 6;
    if(gain>73) gain = 73;

    iio_channel_attr_write(
        iio_device_find_channel(phy, "voltage0", false),
        "gain_control_mode",
        "manual"); 

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phy, "voltage0", false),
        "hardwaregain",
        gain); 

}


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_out_bw(long long chbw) {

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phy, "voltage0", true),
        "rf_bandwidth",
        chbw); 
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_in_bw(long long chbw) {

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phy, "voltage0", false),
        "rf_bandwidth",
        chbw); 

    pluto_set_out_bw( chbw );
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_in_sample_freq(long long sfreq) {

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phy, "voltage0", false),
        "sampling_frequency",
        sfreq); 

    current_sample_freq = sfreq;

    iio_channel_attr_write_longlong(
          iio_device_find_channel(rx_dev, "voltage0", false),
          "sampling_frequency",
          sfreq/8); //8x decimation on FPGA
    iio_channel_attr_write_longlong(
          iio_device_find_channel(tx_dev, "voltage0", true),
          "sampling_frequency",
          sfreq/8); //8x interpolation on FPGA

}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_out_gain(long long gain) {

  iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "voltage0", true),
      "hardwaregain",
      gain);
  //fprintf(stderr, "\nsetting tx gain %lld", gain);

}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_tx_freq(long long freq_tx_hz) {

  //NOTE: offset correction is done in set_rx_freq!!!!

  iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "altvoltage1", true),
      "frequency",
      (long long)freq_tx_hz);   //tx lo freq

}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_enable_tx( int enable) {
  tx_enabled = enable;

  if(tx_enabled==0) {
    pluto_set_tx_freq(5999000000);
    pluto_set_out_gain( -80 );
  }
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void pluto_set_rx_freq(long long freq_rx_hz) {

  int in_range = 0;
  if( freq_rx_hz > 902200000L && freq_rx_hz < 927800000 ) in_range=1;
  if( freq_rx_hz > 2412200000L && freq_rx_hz < 2461800000 ) in_range=1;

  if(!in_range) {
    fprintf(stderr, "\nwarning tx/rx freq out of ISM range. setting to default");
    freq_rx_hz = 915000000;
  }

  long long offset_hz = (long long) ( (freq_rx_hz/1e6) * ref_correction_ppm );
  fprintf(stderr, "\nfreq correction hz:  %lld", offset_hz ); 

  current_rx_freq = (long long)freq_rx_hz + offset_hz;   //rx lo freq

  iio_channel_attr_read_longlong(
        iio_device_find_channel(phy, "voltage0", false),
        "sampling_frequency",
        &current_sample_freq); 


  iio_channel_attr_write_longlong(
      iio_device_find_channel(phy, "altvoltage0", true),
      "frequency",
      current_rx_freq ); 

  fprintf(stderr, "\nsetting rx_freq %lld", current_rx_freq); 
  fprintf(stderr, "\nsetting tx_freq %lld", current_rx_freq);

  if(tx_enabled) {
    pluto_set_tx_freq(current_rx_freq);
  }
  else {
    pluto_set_tx_freq(5999000000);
    pluto_set_out_gain( -80 );
  }

}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
int pluto_transmit(float complex *buffer, int len, int do_dump_rx, int is_last)
{

    llen = len;

    if(more_tx_data==0) {
      tx_p_dat = (char *) iio_buffer_first(txbuf,tx0_i);
      tx_p_end = (char *) iio_buffer_end(txbuf);
      more_tx_data=1;
    }


    for(ii=0; ii<llen; ii++) {

      x = buffer[ii];

      firinterp_crcf_execute(q, x, y);  //interpolate

      for(jj=0; jj<DECIMATE_INTERPOLATE_FACTOR; jj++) {

        ((int16_t*)tx_p_dat)[0] = ((const int16_t) (creal( y[jj] )*8192.0));  //scale to work well for OFDM waveforms
        ((int16_t*)tx_p_dat)[1] = ((const int16_t) (cimag( y[jj] )*8192.0));

        tx_p_dat += tx_p_inc;

        if(tx_p_dat == tx_p_end) {
          iio_buffer_push(txbuf);
          tx_p_dat = (char *) iio_buffer_first(txbuf,tx0_i);
          tx_p_end = (char *) iio_buffer_end(txbuf);
          more_tx_data=1;
        }
      }

    }
    
    if(is_last) {

      //zero pad the last symbol
      if(more_tx_data==0) {
        while(tx_p_dat != tx_p_end) {
          ((int16_t*)tx_p_dat)[0] = 0; 
          ((int16_t*)tx_p_dat)[1] = 0; 
          tx_p_dat += tx_p_inc;
        }
      }

      more_tx_data=0;
      iio_buffer_push(txbuf); //send out the last symbol to the dma

      while( iio_buffer_refill(rxbuf) > 0); //flush the rx buffer
    }

  return 0; 
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
int pluto_receive() {

  if(p_dat == p_end || !more_data) {
    n_rx = iio_buffer_refill(rxbuf)/4;
    p_dat = iio_buffer_first(rxbuf,rx0_i);
    p_end = iio_buffer_end(rxbuf);
    more_data=1;
  }

  for ( ;p_dat < p_end; p_dat += p_inc) {

    do_process_iq16( ((const int16_t*)p_dat)[0], ((const int16_t*)p_dat)[1] ); //i and q

      if(--n_rx==0) {
        if(p_dat!=p_end) {
          more_data=1;
        }
        else {
          more_data=0;
        }
        return 0; 
      }
  }

  return 0; 
}
