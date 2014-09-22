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
#ifndef __AMT_SOCKET__
#define __AMT_SOCKET__

#include "amt_utility.h"

#define MAX_RETRY_SOCK (3)

typedef struct {
    u32 localIP;
    u32 remoteIP;
    u16 localPort;
    u16 remotePort;
    
    int sockServ, sockClient;
    struct sockaddr_in servAddr, clientAddr;
    
    u32 natSrcIP;
    u16 natSrcPort;
    u16 dump;
} amt_sock_t;

amt_sock_t * amt_makeUDPSock(u32 localIP, u16 *sServerPort, u32 remoteIP, u16 remotePort, int reuse);
void amt_setUDPClient(amt_sock_t *pSock, u32 remoteIP, u16 remotePort);
int amt_setSSMTTL(s32 s, u32 ttl);
int amt_joinSSMGroup(s32 s, u32 groupIP, u32 sourceIP, u32 interfaceIP);
int amt_leaveSSMGroup(s32 s, u32 groupIP, u32 sourceIP, u32 interfaceIP);
int amt_sendPacket(amt_sock_t *pSock, u8 *buf, int size);
int amt_close(amt_sock_t **pSock);

#endif

