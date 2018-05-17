/* This file was automatically generated.  Do not edit! */
void do_ofdm_rx(float complex sample);
void init_ofdm_rx(void);
int ofdm_rx_callback(unsigned char *_header,int _header_valid,unsigned char *_payload,unsigned int _payload_len,int _payload_valid,framesyncstats_s _stats,void *_userdata);
int ofdm_rx_state();
void do_ofdm_mix_down(float complex sample,float complex *y);
void bump_nco();
extern int did_rx_ok;
