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
 * 07/4/2014       Duanpei Wu       email: Duanpei@cisco.com          *
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
#include <stdarg.h>


#define MAX_PRINT_LEN (2048)

#include "amt.h"
#include "amt_impl.h"
#include "amt_trace.h"

static amt_traceSink_f amt_traceSink=NULL;
static int amt_traceLevel = AMT_DEFAULT_LEVEL;
void setTraceSink_impl(int level,  amt_traceSink_f traceFunc)
{
    amt_traceLevel = level;
    amt_traceSink = traceFunc;
}

void setTraceLevel_impl(int level)
{
    amt_traceLevel = level;
}

static void GetLocalClockStr(char *buf,	     // buffer to contain the string 
			     int *_size	     // initially with max size of buffer
			     )
{
  int size = *_size;
  struct timeval curTime;
  struct tm     *ptm;

  // get time
  gettimeofday(&curTime, NULL);
  ptm = localtime(&curTime.tv_sec);

  // set the header
  if (ptm) {
    snprintf(buf, size, "%02u:%02u:%02u.%03u ", 
	     ptm->tm_hour,ptm->tm_min,ptm->tm_sec,
             (u32)curTime.tv_usec/1000);
    //   snprintf(buf, size, "%02u/%02u/%04u %02u:%02u:%02u.%03u ",
    //        ptm->tm_mon+1,ptm->tm_mday, ptm->tm_year+1900, 
    //	     ptm->tm_hour,ptm->tm_min,ptm->tm_sec,
    //        (u32)curTime.tv_usec/1000);
    *_size = strlen(buf); 
  } else {
    snprintf(buf, size, " ");
    *_size = 1;
  }
}

// may need debug enable and disable.
void amt_printf(int traceLevel,	 
		char *format, ...
		)
{
  if (traceLevel < amt_traceLevel) {
    va_list     ap;
    int size =MAX_PRINT_LEN;
    char msg[MAX_PRINT_LEN], *pBuf = msg;
    
    GetLocalClockStr(pBuf, &size);
    va_start(ap, format);
    vsnprintf(&pBuf[size],MAX_PRINT_LEN-size, format, ap);
    va_end(ap);
 
    if (amt_traceSink) {
	amt_traceSink(traceLevel,pBuf, strlen(pBuf));
    } else {
	printf("%s", pBuf);
    }
  }
}

int amt_isTraced(int traceLevel) 
{
    return(traceLevel < amt_traceLevel);
}


