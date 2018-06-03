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
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <complex.h>
#include "liquid/liquid.h"

#include "pluto.h"
#include "ofdm_conf.h"
#include "ofdm_rx.h"
#include "charon.h"
#include "fftw3.h"
#include "ofdm.h"
#include "tap_device.h"
#include "timers.h"


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
//typedef struct timer_obj {
//  struct timeval start;
//  struct timeval end;
//} timer_obj;

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
timer_obj * create_timer(void) {
  timer_obj *new_obj = malloc( sizeof(timer_obj) );
  return new_obj;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
void timer_reset(timer_obj *o) {
  gettimeofday(&o->start, NULL);
  gettimeofday(&o->end, NULL);
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
long long timer_elapsed_usec(timer_obj *o) {
  gettimeofday(&o->end, NULL);
  return (o->end.tv_sec*1e6+o->end.tv_usec) - (o->start.tv_sec*1e6+o->start.tv_usec);

}
