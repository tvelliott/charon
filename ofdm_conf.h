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

#define RX_TIMEOUT (4096) 

#define DECIMATE_INTERPOLATE_FACTOR 8
#define OFDM_TX_BW_FACTOR 1.15
#define OFDM_RX_BW_FACTOR (OFDM_TX_BW_FACTOR * 0.8)
#define OFDM_RX_STOP_DB 80.0
#define OFDM_TX_STOP_DB 80.0
#define OFDM_FEC0 LIQUID_FEC_SECDED7264
#define OFDM_FEC1 LIQUID_FEC_HAMMING128
#define OFDM_MODULATION LIQUID_MODEM_QAM16
#define PAYLOAD_LEN 1514 

#define OFDM_M 64 
#define CP_LEN  4 
#define TAPER_LEN 2
#define OFDM_TIMEOUT_SEC 5

#define OFDM_CRC LIQUID_CRC_32
