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
#include "amt_trace.h"
#include "ex_sock_exp.h"


// make a UDP server/client socket
// When *sServerPort = 0, it is to create an even port.
// When *sServerPort = 1, it is to create an odd port.
amt_sock_t * amt_makeUDPSock(u32 localIP, u16 *sServerPort, u32 remoteIP, u16 remotePort)
{
    ex_sock_t *pExSock;
    amt_sock_t *pSock =  (amt_sock_t *) malloc(sizeof(amt_sock_t));
    AMT_ERR_CHECK(pSock!=NULL, NULL, "No Memory");
    pExSock = ex_makeUDPSock(localIP, sServerPort, remoteIP, remotePort);
    AMT_ERR_CHECK(pExSock!=NULL, NULL, "Failed to make a UDP socket");
    *pSock = *((amt_sock_t *)pExSock);
    ex_free(pExSock);
    return pSock;
}

void amt_setUDPClient(amt_sock_t *pSock, u32 remoteIP, u16 remotePort)
{
    ex_sock_t ex_sock, *p=&ex_sock;
    ex_sock = *(ex_sock_t *)pSock;
    ex_setUDPClient(&ex_sock, remoteIP, remotePort);
    *pSock = *(amt_sock_t*)p;
}
  
int amt_sendPacket(amt_sock_t *pSock, u8 *buf, int size)
{
    ex_sock_t ex_sock;
    ex_sock = *(ex_sock_t *)pSock;;
    return (ex_sendPacket(&ex_sock, buf, size));
}

int amt_setSSMTTL(s32 s, u32 ttl) 
{
    return(ex_setSSMTTL(s, ttl));
}

int amt_joinSSMGroup(s32 s, u32 groupIP, u32 sourceIP, u32 interfaceIP)
{
    return(ex_joinSSMGroup(s, groupIP, sourceIP, interfaceIP));
}

int amt_leaveSSMGroup(s32 s, u32 groupIP, u32 sourceIP, u32 interfaceIP) 
{
    return(ex_leaveSSMGroup(s, groupIP, sourceIP, interfaceIP));
}

int amt_close(amt_sock_t **pSock)
{
    int res;
    ex_sock_t *pExSock  =  (ex_sock_t *) ex_malloc(sizeof(ex_sock_t));
    AMT_ERR_CHECK(pExSock!=NULL, AMT_ERR, "No Memory");
    *pExSock = **(ex_sock_t **)pSock;
    free(*pSock);
    *pSock=NULL;

    res = ex_close(&pExSock);
    if (res) { return res;}
    return 0;
}
  
