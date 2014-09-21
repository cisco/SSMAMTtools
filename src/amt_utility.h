/*
Copyright 2014 Cisco Systems Inc.. All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:

   1. Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.

THIS SOFTWARE IS PROVIDED BY CISCO ''AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN 
NO EVENT SHALL CISCO OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation 
are those of the authors and should not be interpreted as representing 
official policies, either expressed or implied, of Cisco.

*/
/*
 **********************************************************************
 * Initial revision 0.1                                               *
 * creation date    programmers      comment                          *
 * 07/4/2014       Duanpei Wu       email: Duanpei@cisco.com         *
 *                                                                    *
 **********************************************************************/ 
#ifndef __AMT_UTILITY__
#define __AMT_UTILITY__

#include <stdio.h>

typedef char 		s8;
typedef unsigned char 	u8;
typedef short 		s16;
typedef unsigned short 	u16;
typedef int 		s32;
typedef unsigned int 	u32;
typedef unsigned char	bool;

#define AMT_NOERR 	(0)
#define AMT_SUCCESS 	(1)
#define AMT_TIMEOUT 	(2)
#define AMT_ERR 	(-1)
#define AMT_NOMEM 	(-2)


// sleep milisecond
static inline void inline msleep(u32 sec, u32 time)
{
  struct timespec req;
  req.tv_nsec = time*1000000;
  req.tv_sec = sec;
  nanosleep(&req, NULL);
}

static inline u32 getCurrentTime(void) 
{
    struct timeval tv = {0,0};
    gettimeofday(&tv, NULL);
    return ( tv.tv_sec*1000+ tv.tv_usec/1000);
}

// timer
#define AMT_TIMER_IDLE 		(0)
#define AMT_TIMER_RUN  		(1)
#define AMT_TIMER_CREATING  	(2)
#define AMT_TIMER_QUIT_REQ 	(3)

#define AMT_TIMER_ONESHOT (1)
#define AMT_TIMER_PERIODIC (0)

#define AMT_TIMER_RESOLUTION (100)
typedef void * amt_timer_handle;
typedef void (* amt_timer_callback_f )(amt_timer_handle timeID, void *);
amt_timer_handle amt_addTimer(u32 elapseTime,  
			      amt_timer_callback_f sinkFunc, 
			      void *param, 
			      int oneshot);
void amt_deleteTimer(amt_timer_handle timerID);
int  amt_createTimer(u32 period);
void amt_destoryTimer(void);
int  amt_checkTimerRun(void);

// mutex
/*
#define pthread_mutex_lock(x) amt_mutex_lock(x)
#define pthread_mutex_unlock(x) amt_mutex_unlock(x)
typedef pthread_mutex_t amt_mutex_t;
static inline void  amt_init_mutex(amt_mutex_t *mutex)
{
    pthread_mutex_init(mutex, NULL);
}
*/

#endif
