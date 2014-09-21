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
#ifndef __EX_SOCKET_EXP__
#define __EX_SOCKET_EXP__

typedef struct {
    unsigned int localIP;
    unsigned int remoteIP;
    unsigned short localPort;
    unsigned short remotePort;
    
    int sockServ, sockClient;
    struct sockaddr_in servAddr, clientAddr;
    
    unsigned int natSrcIP;
    unsigned short natSrcPort;
    unsigned short dump;
} ex_sock_t;

void ex_free(void *p);
void *ex_malloc(int size);

ex_sock_t * ex_makeUDPSock(unsigned int localIP, unsigned short *sServerPort,
			   unsigned int remoteIP, unsigned short remotePort);
void ex_setUDPClient(ex_sock_t *pSock, unsigned int remoteIP, 
		     unsigned short remotePort);
int ex_setSSMTTL(int s, unsigned int  ttl);
int ex_joinSSMGroup(int s, unsigned int groupIP, 
		    unsigned int sourceIP, unsigned int interfaceIP);
int ex_leaveSSMGroup(int s, unsigned int groupIP, unsigned int sourceIP, 
		     unsigned int interfaceIP);
int ex_sendPacket(ex_sock_t *pSock, unsigned char *buf, int size);
int ex_close(ex_sock_t **pSock);

#endif
