/* This file was automatically generated.  Do not edit! */
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
typedef struct timer_obj {
  struct timeval start;
  struct timeval end;
} timer_obj;
long long timer_elapsed_usec(timer_obj *o);
void timer_reset(timer_obj *o);
timer_obj *create_timer(void);
