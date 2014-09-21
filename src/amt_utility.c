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
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <assert.h>  
#include <unistd.h>

#include "amt_utility.h"
#include "amt_sock.h"
#include "amt.h"
#include "amt_trace.h"

// a simple timer

static int timerState = AMT_TIMER_IDLE;
static int timerQuitReq = 0;


typedef struct _amt_timer{
   struct  _amt_timer *next;
   struct  _amt_timer *prev;
    
    amt_timer_handle timerID;
    u32 expiredTime;
    u32 period;
    int timerType;

    amt_timer_callback_f sinkFunc;
    void *param;
} amt_timer_t;
    
static amt_timer_t *pTimerRoot = NULL;

static  pthread_mutex_t amt_timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static amt_timer_t *removeTimer(amt_timer_t *p)
{
    amt_timer_t *tmp;
    if (p->prev) { p->prev->next = p->next;}
    if (p->next) { p->next->prev = p->prev;}
    if (p==pTimerRoot) {pTimerRoot = p->next;}
    tmp = p->next;
    free(p);
    return tmp;
}
static void insertTimer(amt_timer_t *p)
{
    p->prev = NULL;
    p->next = pTimerRoot;
    if (pTimerRoot) {pTimerRoot->prev = p;}
    pTimerRoot = p;
}

//
// a simple timier
void *amt_timer(void *_p)
{
#ifdef P32BITS
    u32 period = (long)_p;
#else
    u32 period = (long long)_p;
#endif

    AMT_TRACE(AMT_LEVEL_9,"timer thread created with periodic time:%u \n", period);
 
    timerState = AMT_TIMER_RUN;
    period = (period > 10000 || period < 10)?100:period;
    
    // setup the periodic timer  
    // if ((res = setupTimer(period))!=0) return ((void *)res);
    
    // we need not an accurate clock. Let us just use msleep
    while(timerQuitReq!=AMT_TIMER_QUIT_REQ) {
	amt_timer_t *p;
	u32 now =  getCurrentTime();
	pthread_mutex_lock(&amt_timer_mutex);
	p = pTimerRoot ;
	while (p) {
	    if (p->expiredTime <= now) {
		amt_timer_callback_f sinkFunc = p->sinkFunc;
		void *param = p->param;
		amt_timer_handle timerID= p->timerID;

		if (p->timerType==AMT_TIMER_ONESHOT) {
		    p=removeTimer(p);
		} else {
		    p->expiredTime += p->period;
		}
		
		if (sinkFunc) {
		    pthread_mutex_unlock(&amt_timer_mutex);
		    sinkFunc(timerID, param);
		    pthread_mutex_lock(&amt_timer_mutex);
		    p = pTimerRoot ;
		    now =  getCurrentTime();
		    continue;
		}
	    }
	    p=p->next;
	}
	pthread_mutex_unlock(&amt_timer_mutex);
	msleep(0, period); 
    }
    timerState = AMT_TIMER_IDLE;
    pthread_mutex_unlock(&amt_timer_mutex);
    timerQuitReq = AMT_TIMER_IDLE;
    pthread_mutex_unlock(&amt_timer_mutex);

    AMT_TRACE(AMT_LEVEL_9,"timer thread exited\n");
    
    return NULL;
}
 
int amt_createTimer(u32 period)
{
    int res;
    pthread_t t;
    pthread_attr_t	thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    res =pthread_create(&t, &thread_attr, (void *(*)(void *)) amt_timer , (void *) period);
    AMT_ERR_CHECK(res == 0, AMT_ERR, "createTimer: failed to create thread for timer\n");
    timerState = AMT_TIMER_CREATING;
    return AMT_NOERR;
}
    
void amt_destoryTimer(void)
{
    amt_timer_t *p;
    pthread_mutex_lock(&amt_timer_mutex);
    
    // remove and free all timer
    p=pTimerRoot;
    while(p) {
	p = removeTimer(p);
    }
    timerQuitReq = (timerState == AMT_TIMER_RUN)?AMT_TIMER_QUIT_REQ:AMT_TIMER_IDLE;
    pthread_mutex_unlock(&amt_timer_mutex);

    AMT_TRACE(AMT_LEVEL_9,"timer wheel destroyed \n");

}

int  amt_checkTimerRun()
{
    return timerState;
}
 
amt_timer_handle amt_addTimer(u32 elapseTime,  amt_timer_callback_f _sinkFunc, 
			      void *param, int oneshot)
{
    amt_timer_t *p = (amt_timer_t *) malloc(sizeof(amt_timer_t));
    AMT_ERR_CHECK(p, NULL, "no memory\n");

    AMT_TRACE(AMT_LEVEL_9,"timer ID:%p elapse time:%u call back function:%p param=%p oneshot:%u\n",
	      p, elapseTime, _sinkFunc,param,oneshot);

    p->expiredTime = elapseTime + getCurrentTime();
    p->period = elapseTime;
    p->timerType = (oneshot)?AMT_TIMER_ONESHOT:AMT_TIMER_PERIODIC;
    p->timerID = (void *)p;
    p->sinkFunc = _sinkFunc;
    p->param = param;

    // add to link
    pthread_mutex_lock(&amt_timer_mutex);
    insertTimer(p);
    pthread_mutex_unlock(&amt_timer_mutex);
    return p->timerID;
}

void amt_deleteTimer(amt_timer_handle timerID)
{
    amt_timer_t *p;

    pthread_mutex_lock(&amt_timer_mutex);
    
    p=pTimerRoot;
    while(p) {
	if (p->timerID == timerID) {break;}
	p = p->next;
    }
    if (p) {
	removeTimer(p);
    }

    pthread_mutex_unlock(&amt_timer_mutex);

    AMT_TRACE(AMT_LEVEL_9,"time ID:%p\n", timerID);


}
 
