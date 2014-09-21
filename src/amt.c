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

#include "amt_sock.h"
#include "amt.h"
#include "amt_trace.h"
#include "amt_impl.h"

static u32 amt_version = 0x20140920;
u32 amt_getVer() {return amt_version;}
static int amt_dataSink_flag = 0;

int amt_setTraceSink(int level,  amt_traceSink_f traceFunc)
{
    AMT_ERR_CHECK(traceFunc!=NULL,AMT_ERR, "null trace function\n");
    AMT_ERR_CHECK(level>=0 && level<=10, AMT_ERR, "invalid trace leve (0-10)\n");

    AMT_TRACE(AMT_LEVEL_8,"level:%u traceFunc:%p\n", level, traceFunc);
    
    setTraceSink_impl(level, traceFunc);
    return AMT_NOERR;
}
int amt_setTraceLevel(int level)
{
    AMT_ERR_CHECK(level>=0 && level<=10, AMT_ERR, "invalid trace leve (0-10)\n");
    setTraceLevel_impl(level);
    return AMT_NOERR;
}

amt_handle_t amt_openChannel(u32 anycast, u32 group, u32 source, u16 port,
			     amt_connect_req_e req)
{
    amt_ssm_t *pSSM, *pSister;
    
    // check if AMT group
    AMT_ERR_CHECK(((group>>24)&0x0ff)==232, NULL, "not ssm group\n");
    AMT_ERR_CHECK(req<AMT_CONNECT_REQ_END && req != AMT_CONNECT_REQ_NONE, 
		  NULL, "invalid request\n");
    
    AMT_TRACE(AMT_LEVEL_8,"anycast:0x%08x ssm:0x%08x source:0x%08x "
	      "port:%u req:0x%x\n",
	      anycast,group,source,port,req);
    
    // get handle
    //pthread_mutex_lock(&amt_mutex);
    pSSM=getSSM(source , group, port, &pSister);
    //pthread_mutex_unlock(&amt_mutex);	
    AMT_ERR_CHECK(pSSM==NULL, NULL, "Channel is already open\n");
  
    // handle the open channel
    pSSM = openChannel_impl(anycast, group, source, port,req);
    AMT_TRACE(AMT_LEVEL_8,"handle:%p\n", pSSM);

    return ((amt_handle_t)pSSM);
 }

int amt_closeChannel(amt_handle_t handle)
{
    amt_ssm_t *pSSM = (amt_ssm_t *)handle; 
    AMT_ERR_CHECK(pSSM && pSSM->magicCode == AMT_MAGIC_CODE,AMT_ERR, 
		      "NULL pointer or not amt handle\n");

    AMT_TRACE(AMT_LEVEL_8,"Handle:%p\n", handle);

    return(closeChannel_impl(pSSM));
   
}

int amt_poll(amt_read_event_t *rs, int size, int timeout)
{
    int i;
    AMT_ERR_CHECK(amt_dataSink_flag==0,AMT_ERR, "data sink is set\n");
    AMT_ERR_CHECK(size<=AMT_MAX_CHANNEL, AMT_ERR,
                       "amt_poll: too many channels: %u\n",size);
    AMT_ERR_CHECK(rs!=NULL, AMT_ERR, "NULL pointer\n");
    
    AMT_TRACE(AMT_LEVEL_8,"size:%u timeout:%u\n", size, timeout);

    for (i=0; i< size; i++) {
        amt_ssm_t *pSSM = (amt_ssm_t *)rs[i].handle; 
        AMT_ERR_CHECK(pSSM && pSSM->magicCode == AMT_MAGIC_CODE,AMT_ERR, 
                          "NULL pointer or not amt handle\n");
	AMT_TRACE(AMT_LEVEL_8,"handle:%p\n", rs[i].handle);
    }
    
    return(poll_impl(rs, size, timeout));
}

int amt_recvfrom(amt_handle_t handle, u8 *buf, int size) 
{
    amt_ssm_t *pSSM = (amt_ssm_t *)handle; 
    AMT_ERR_CHECK(amt_dataSink_flag==0,AMT_ERR, "data sink is set\n");
    AMT_ERR_CHECK(pSSM && pSSM->magicCode == AMT_MAGIC_CODE,AMT_ERR, 
		  "NULL pointer or not amt handle\n");
    AMT_ERR_CHECK(buf || size <=0,AMT_ERR, "NULL pointer\n");
    
    AMT_TRACE(AMT_LEVEL_8,"handle:%p buf:%p size:%u\n", handle, buf, size);
    return(recvfrom_impl(pSSM, buf, size));
}

amt_connect_state_e amt_getState(amt_handle_t handle)
{									
    amt_ssm_t *p = (amt_ssm_t *)handle;
    amt_connect_state_e state;

    AMT_ERR_CHECK(p==NULL || p->magicCode==AMT_MAGIC_CODE, AMT_ERR, "not AMT handle\n");
    state = getState_impl(p);

    AMT_TRACE(AMT_LEVEL_8,"handle:%p state=0x%x\n", handle, state);
    
    return state;
}

int amt_addRecvHook(amt_recvSink_f recvSinkfunc, void *param)
{
    
    AMT_TRACE(AMT_LEVEL_8,"call back function:%p\n", recvSinkfunc);
   
    if (recvSinkfunc) {
	addRecvHook_impl(recvSinkfunc, param);
	amt_dataSink_flag = 1;
    }
    return 0;
}
 
int amt_init(unsigned int anycastIP) 
{
    u32 anycast = htonl(anycastIP);
    AMT_TRACE(AMT_LEVEL_8,"with anycast IP:%s\n", 
	      inet_ntoa(*(struct in_addr *) &anycast));
    return (init_impl(anycastIP));
}

void amt_reset(void)
{
    reset_impl();
    amt_dataSink_flag = 0;
    AMT_TRACE(AMT_LEVEL_8,"-- Reset ---\n");
}
