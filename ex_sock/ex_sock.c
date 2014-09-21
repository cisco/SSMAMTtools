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
#define __USE_GNU
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



#include "ex_sock.h"

static u32 localIP=-1;
u32 getLocalIP() {
    if (localIP == -1) {
        getLocalIPDev(NULL);
    }
    return localIP;
}

// get local ip address
u32  getLocalIPDev(char *pIf)
{ 
  int fd;
  struct sockaddr_in *addr_ptr;
  struct ifreq iface;
  int    res;
  u32	lIP=0;

  if (pIf==NULL) {
      pIf = getenv("ETHERDEV");
      if (pIf == NULL) {
          pIf = "eth0";
      }
  }
  
  EX_ERR_CHECK((strlen(pIf) <= IF_NAMESIZE), "getLocalIPDev:get hostname failed\n",0);
  strcpy(iface.ifr_name , pIf); 

  fd = socket(AF_INET, SOCK_DGRAM, 0);  
  EX_ERR_CHECK(fd>=0, "getLocalIPDev:get hostname failed\n", 0);

  res = ioctl(fd, SIOCGIFADDR, &iface);
  if (res < 0) {
	close(fd);
	EX_ERR_CHECK(res>=0, "getLocalIPDev:get hostname failed\n", 0);
  }
  addr_ptr = (struct sockaddr_in *) &iface.ifr_addr;
  lIP = ntohl(addr_ptr->sin_addr.s_addr);
  close(fd) ;

  // record it
  localIP = lIP;

  return (lIP);
}

// make a UDP server/client socket
// When *sServerPort = 0, it is to create an even port.
// When *sServerPort = 1, it is to create an odd port.
ex_sock_t * ex_makeUDPSock(u32 localIP, u16 *sServerPort, u32 remoteIP, u16 remotePort)
{
  char ipStringL[80],ipStringR[80];
  int res=0;
  int enable=1;
  u16 port=*sServerPort;
  int loopCount=0;
  ex_sock_t *pSock =  (ex_sock_t *) malloc(sizeof(ex_sock_t));
  
  EX_ERR_CHECK(pSock != NULL, "ex_makeUDPSock:no memory\n", NULL);
  memset(pSock, sizeof(ex_sock_t), 0);
  
  // make sock    
  pSock->sockClient = socket(AF_INET, SOCK_DGRAM, 0);
  if (pSock->sockClient <0) {
      free(pSock);
      EX_ERR_CHECK(0, "ex_makeUDPSock:failed to create the socket\n", NULL);
  }

  // include struct in_pktinfo in the message "ancilliary" control data
  res = setsockopt(pSock->sockClient, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable));
  if (res < 0 ) {
      EX_ERR_CHECK(0, "ex_makeUDPSock:failed to set IPPROTO_IP\n", NULL);
  }

  // make it reusable
  //res = setsockopt( pSock->sockClient, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
  //if (res < 0 ) {
  //    EX_ERR_CHECK(0, "ex_makeUDPSock:failed to set SO_REUSEADDR\n", NULL);
  //}

  pSock->sockServ =  pSock->sockClient;
  if (localIP==0) {
      localIP = INADDR_ANY;
      //localIP = getLocalIPDev(NULL);
  }
  
  while(1) {
      if (((*sServerPort)&0xffff) < 2) {
          port = (((rand()%PORT_RANGE) + PORT_BASE)&0xfffe) + ((*sServerPort)&0xffff);
      } 
      // set server address
      bzero((char *) &pSock->servAddr, sizeof(pSock->servAddr));
      pSock->servAddr.sin_family = AF_INET;
      pSock->servAddr.sin_port = htons(port);
      pSock->servAddr.sin_addr.s_addr = htonl(localIP); 
      res = bind(pSock->sockServ, (struct sockaddr *)&pSock->servAddr, sizeof(pSock->servAddr));
      if (res <0) {
          if ((*sServerPort < 2) && (errno == EADDRINUSE)) {
              if (loopCount++ < MAX_PORT_SEARCH_LOOP) {
                  continue;
              }
          } else if (errno == EADDRNOTAVAIL) {
              // update IP
              localIP = getLocalIPDev(NULL);
              loopCount += MAX_PORT_SEARCH_LOOP>>2;
              if (loopCount < MAX_PORT_SEARCH_LOOP) {
                  continue;
              }
          }
          close(pSock->sockServ);
          free(pSock);
      }
      break;
  }
  
  EX_ERR_CHECK(res>=0, "ex_makeUDPSock:Socket binding failed\n", NULL);
  pSock->localIP 	= ntohl(pSock->servAddr.sin_addr.s_addr); // localIP;  
  pSock->localPort 	= ntohs(pSock->servAddr.sin_port);//port;  
  *sServerPort		= port;
  
  // make client address
  bzero((char *) &pSock->clientAddr, sizeof(pSock->clientAddr));
  pSock->clientAddr.sin_family = AF_INET;
  pSock->clientAddr.sin_port = htons(remotePort);
  pSock->clientAddr.sin_addr.s_addr = htonl(remoteIP);
  pSock->remoteIP 	= remoteIP;  
  pSock->remotePort = remotePort;
  
  if (0) {
      snprintf(ipStringL, sizeof(ipStringL), "%0u.%0u.%0u.%0u:%u", 
               (pSock->localIP>>24)&0xff,(pSock->localIP>>16)&0xff,
               (pSock->localIP>>8)&0xff,pSock->localIP&0xff, pSock->localPort);
      snprintf(ipStringR, sizeof(ipStringR), "%0u.%0u.%0u.%0u:%u", 
               (pSock->remoteIP>>24)&0xff,(pSock->remoteIP>>16)&0xff,
               (pSock->remoteIP>>8)&0xff,pSock->remoteIP&0xff, pSock->remotePort);
      printf("local:%s remote:%s\n", ipStringL, ipStringR);
  }
  return pSock;
}

void ex_setUDPClient(ex_sock_t *pSock, u32 remoteIP, u16 remotePort)
{
  char ipStringL[20],ipStringR[20];
  //   assert(p!=NULL);

  bzero((char *) &pSock->clientAddr, sizeof(pSock->clientAddr));
  pSock->clientAddr.sin_family = AF_INET;
  pSock->clientAddr.sin_port = htons(remotePort);
  pSock->clientAddr.sin_addr.s_addr = htonl(remoteIP);
  pSock->remoteIP 	= remoteIP;
  pSock->remotePort 	= remotePort;

if (0) {
      snprintf(ipStringL, sizeof(ipStringL), "%0u.%0u.%0u.%0u:%u", 
               (pSock->localIP>>24)&0xff,(pSock->localIP>>16)&0xff,
               (pSock->localIP>>8)&0xff,pSock->localIP&0xff, pSock->localPort);
      snprintf(ipStringR, sizeof(ipStringR), "%0u.%0u.%0u.%0u:%u", 
               (pSock->remoteIP>>24)&0xff,(pSock->remoteIP>>16)&0xff,
               (pSock->remoteIP>>8)&0xff,pSock->remoteIP&0xff, pSock->remotePort);
      printf("local:%s remote:%s\n", ipStringL, ipStringR);
  }
}

int ex_sendPacket(ex_sock_t *pSock, u8 *buf, int size)
{
    int res = 0;
    int count=MAX_RETRY_SEND;
    EX_ERR_CHECK(pSock != NULL && buf!=NULL, "ex_sendPacket:null pointer\n", -1);
    
    while(count>0) {
        res = sendto(pSock->sockClient, buf, size, 0, (struct sockaddr *)&pSock->clientAddr,
                     sizeof(struct sockaddr));
        if (res < 0) {
            int err=errno;
            if( err != ECONNREFUSED && err != EPERM ) {
                EX_TRACE4("%s: sendto() failed with err %d. Local port:%d, Remote port:%d\n",
                           __FUNCTION__, err, pSock->localPort,pSock->remotePort);
                break;
            }
        } else if (res > 0) {
	    break;
	}
        msleep(0,10);
        count--;
    }
    return res;
}

int ex_setSSMTTL(s32 s, u32 ttl) 
{
    return setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(u32));
}

int ex_joinSSMGroup(s32 s, u32 groupIP, u32 sourceIP, u32 interfaceIP)
 {
    struct ip_mreq_source imr; 
    imr.imr_multiaddr.s_addr  = htonl(groupIP);
    imr.imr_sourceaddr.s_addr = htonl(sourceIP);
    //imr.imr_interface.s_addr  = htonl(INADDR_ANY);
    imr.imr_interface.s_addr  = htonl(interfaceIP);

    //setsockopt(s, IPPROTO_IP, IP_UNBLOCK_SOURCE, (char *) &imr, sizeof(imr));  
    
     return setsockopt(s, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *) &imr, sizeof(imr));  
}

int ex_leaveSSMGroup(s32 s, u32 groupIP, u32 sourceIP, u32 interfaceIP) 
{
    struct ip_mreq_source imr; 
    imr.imr_multiaddr.s_addr  = htonl(groupIP);
    imr.imr_sourceaddr.s_addr = htonl(sourceIP);
    imr.imr_interface.s_addr  = htonl(interfaceIP);
    return setsockopt(s, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char *) &imr, sizeof(imr));  
}

int ex_close(ex_sock_t **pSock)
{
    
    EX_ERR_CHECK(pSock != NULL && *pSock!=NULL, "ex_close:NULL pointer\n", 1);
    // shutdown(*pSock->sockClient, SD_BOTH);
    close((*pSock)->sockClient);
    free (*pSock);
    *pSock = NULL;
    return 0;
}

void ex_free(void *p) {
    free(p);
}

void *ex_malloc(int size) {
    return(malloc(size));
}


/*
void Receiver::ConnectSocket() {
    //  Make socket 
    socketId = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

    sockaddr_in Sender;
    int SenderAddrSize = sizeof(Sender);

    struct sockaddr_in binda;

    //  Bind it to listen appropriate UDP port
    binda.sin_family = AF_INET;
    binda.sin_port = htons(port);
    binda.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(socketId,(struct sockaddr*)&binda, sizeof(binda));

    // Join to group
    join_sex_group(socketId, group, source, INADDR_ANY);
}

void Receiver::DisconnectSocket() {
    leave_amt_group(socketId, group, source, INADDR_ANY);

    int iResult = shutdown(socketId, SD_BOTH);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed: %d\n", WSAGetLastError());
    }
    closesocket(socketId);
}
*/
