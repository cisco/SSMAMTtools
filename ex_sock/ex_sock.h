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
#ifndef __EX_SOCKET__
#define __EX_SOCKET__

typedef char 		s8;
typedef unsigned char 	u8;
typedef short 		s16;
typedef unsigned short 	u16;
typedef int 		s32;
typedef unsigned int 	u32;
typedef unsigned char	bool;


#define PORT_BASE 		(30000)
#define PORT_RANGE 		(1000)
#define MAX_PORT_SEARCH_LOOP 	(100)
#define MAX_RETRY_SEND 		(2)

// sleep milisecond
static inline void inline msleep(u32 sec, u32 time)
{
  struct timespec req;
  req.tv_nsec = time*1000000;
  req.tv_sec = sec;
  nanosleep(&req, NULL);
}

#define EX_ERR_CHECK(x,fmt,r) {  if (!(x)) { printf(fmt); return r;}}
#define EX_ERR_CHECK1(x,fmt,p,r) {  if (!(x)) { printf(fmt,p); return r;}}
#define EX_TRACE4(fmt,p1,p2,p3,p4) { printf(fmt,p1,p2,p3,p4);}


u32  getLocalIPDev(char *pIf);
u32  getLocalIP(void);

#include "ex_sock_exp.h"

#endif

