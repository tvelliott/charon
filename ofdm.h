//structs pulled from liquid-dsp .c files for access to liquid-dsp internals

//Copyright (c) 2007 - 2016 Joseph Gaeddert

//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:

//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.

//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.

#define OFDMFLEXFRAME_PROTOCOL  (104+PACKETIZER_VERSION)

// header description
#define OFDMFLEXFRAME_H_USER    (8)                         // user-defined array
#define OFDMFLEXFRAME_H_DEC     (OFDMFLEXFRAME_H_USER+6)    // decoded length
#define OFDMFLEXFRAME_H_CRC     (LIQUID_CRC_32)             // header CRC
#define OFDMFLEXFRAME_H_FEC     (LIQUID_FEC_GOLAY2412)      // header FEC
#define OFDMFLEXFRAME_H_ENC     (36)                        // encoded length
#define OFDMFLEXFRAME_H_MOD     (LIQUID_MODEM_BPSK)         // modulation scheme
#define OFDMFLEXFRAME_H_BPS     (1)                         // modulation depth
#define OFDMFLEXFRAME_H_SYM     (288)                       // number of symbols

#define FFT_PLAN             fftwf_plan
#define FFT_CREATE_PLAN      fftwf_plan_dft_1d
#define FFT_DESTROY_PLAN     fftwf_destroy_plan
#define FFT_EXECUTE          fftwf_execute
#define FFT_DIR_FORWARD      FFTW_FORWARD
#define FFT_DIR_BACKWARD     FFTW_BACKWARD
#define FFT_METHOD           FFTW_ESTIMATE
struct ofdmframesync_s {
    unsigned int M;         // number of subcarriers
    unsigned int M2;        // number of subcarriers (divided by 2)
    unsigned int cp_len;    // cyclic prefix length
    unsigned char * p;      // subcarrier allocation (null, pilot, data)

    // constants
    unsigned int M_null;    // number of null subcarriers
    unsigned int M_pilot;   // number of pilot subcarriers
    unsigned int M_data;    // number of data subcarriers
    unsigned int M_S0;      // number of enabled subcarriers in S0
    unsigned int M_S1;      // number of enabled subcarriers in S1

    // scaling factors
    float g_data;           // data symbols gain
    float g_S0;             // S0 training symbols gain
    float g_S1;             // S1 training symbols gain

    // transform object
    FFT_PLAN fft;           // ifft object
    float complex * X;      // frequency-domain buffer
    float complex * x;      // time-domain buffer
    windowcf input_buffer;  // input sequence buffer

    // PLCP sequences
    float complex * S0;     // short sequence (freq)
    float complex * s0;     // short sequence (time)
    float complex * S1;     // long sequence (freq)
    float complex * s1;     // long sequence (time)

    // gain
    float g0;               // nominal gain (coarse initial estimate)
    float complex * G0;     // complex subcarrier gain estimate, S0[0]
    float complex * G1;     // complex subcarrier gain estimate, S0[1]
    float complex * G;      // complex subcarrier gain estimate
    float complex * B;      // subcarrier phase rotation due to backoff
    float complex * R;      // 

    // receiver state
    enum {
        OFDMFRAMESYNC_STATE_SEEKPLCP=0,   // seek initial PLCP
        OFDMFRAMESYNC_STATE_PLCPSHORT0,   // seek first PLCP short sequence
        OFDMFRAMESYNC_STATE_PLCPSHORT1,   // seek second PLCP short sequence
        OFDMFRAMESYNC_STATE_PLCPLONG,     // seek PLCP long sequence
        OFDMFRAMESYNC_STATE_RXSYMBOLS     // receive payload symbols
    } state;

    // synchronizer objects
    nco_crcf nco_rx;        // numerically-controlled oscillator
    msequence ms_pilot;     // pilot sequence generator
    float phi_prime;        // ...
    float p1_prime;         // filtered pilot phase slope

#if OFDMFRAMESYNC_ENABLE_SQUELCH
    // coarse signal detection
    float squelch_threshold;
    int squelch_enabled;
#endif

    // timing
    unsigned int timer;         // input sample timer
    unsigned int num_symbols;   // symbol counter
    unsigned int backoff;       // sample timing backoff
    float complex s_hat_0;      // first S0 symbol metrics estimate
    float complex s_hat_1;      // second S0 symbol metrics estimate

    // detection thresholds
    float plcp_detect_thresh;   // plcp detection threshold, nominally 0.35
    float plcp_sync_thresh;     // long symbol threshold, nominally 0.30

    // callback
    ofdmframesync_callback callback;
    void * userdata;

#if DEBUG_OFDMFRAMESYNC
    int debug_enabled;
    int debug_objects_created;
    windowcf debug_x;
    windowf  debug_rssi;
    windowcf debug_framesyms;
    float complex * G_hat;  // complex subcarrier gain estimate, S1
    float * px;             // pilot x-value
    float * py;             // pilot y-value
    float p_phase[2];       // pilot polyfit
    windowf debug_pilot_0;  // pilot polyfit history, p[0]
    windowf debug_pilot_1;  // pilot polyfit history, p[1]
#endif
};

struct ofdmflexframesync_s {
    unsigned int M;         // number of subcarriers
    unsigned int cp_len;    // cyclic prefix length
    unsigned int taper_len; // taper length
    unsigned char * p;      // subcarrier allocation (null, pilot, data)

    // constants
    unsigned int M_null;    // number of null subcarriers
    unsigned int M_pilot;   // number of pilot subcarriers
    unsigned int M_data;    // number of data subcarriers
    unsigned int M_S0;      // number of enabled subcarriers in S0
    unsigned int M_S1;      // number of enabled subcarriers in S1

    // header
    modem mod_header;                   // header modulator
    packetizer p_header;                // header packetizer
    unsigned char header[OFDMFLEXFRAME_H_DEC];      // header data (uncoded)
#if OFDMFLEXFRAME_H_SOFT
    unsigned char header_enc[8*OFDMFLEXFRAME_H_ENC];  // header data (encoded, soft bits)
    unsigned char header_mod[OFDMFLEXFRAME_H_BPS*OFDMFLEXFRAME_H_SYM];  // header symbols (soft bits)
#else
    unsigned char header_enc[OFDMFLEXFRAME_H_ENC];  // header data (encoded)
    unsigned char header_mod[OFDMFLEXFRAME_H_SYM];  // header symbols
#endif
    int header_valid;                   // valid header flag

    // header properties
    modulation_scheme ms_payload;       // payload modulation scheme
    unsigned int bps_payload;           // payload modulation depth (bits/symbol)
    unsigned int payload_len;           // payload length (number of bytes)
    crc_scheme check;                   // payload validity check
    fec_scheme fec0;                    // payload FEC (inner)
    fec_scheme fec1;                    // payload FEC (outer)

    // payload
    packetizer p_payload;               // payload packetizer
    modem mod_payload;                  // payload demodulator
    unsigned char * payload_enc;        // payload data (encoded bytes)
    unsigned char * payload_dec;        // payload data (decoded bytes)
    unsigned int payload_enc_len;       // length of encoded payload
    unsigned int payload_mod_len;       // number of payload modem symbols
    int payload_valid;                  // valid payload flag
    float complex * payload_syms;       // received payload symbols

    // callback
    framesync_callback callback;        // user-defined callback function
    void * userdata;                    // user-defined data structure
    framesyncstats_s framestats;        // frame statistic object
    float evm_hat;                      // average error vector magnitude

    // internal synchronizer objects
    ofdmframesync fs;                   // internal OFDM frame synchronizer

    // counters/states
    unsigned int symbol_counter;        // received symbol number
    enum {
        OFDMFLEXFRAMESYNC_STATE_HEADER, // extract header
        OFDMFLEXFRAMESYNC_STATE_PAYLOAD // extract payload symbols
    } state;
    unsigned int header_symbol_index;   // number of header symbols received
    unsigned int payload_symbol_index;  // number of payload symbols received
    unsigned int payload_buffer_index;  // bit-level index of payload (pack array)
};
