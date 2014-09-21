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
#include <ctype.h>

#include "ex_sock.h"
#define AMT_MAX_CHANNEL (32)

static void PrintUsage(char *prog)
{
  printf("usage: %s\n"
	 "\t[-if interface]      ---- Ethenet interface used to get source IP for streamer; default=eth0\n"
         "\t[-ssm ip1 [ip2 ...]  ---- ssm group(s) ip; default=232.0.0.1 \n"
         "\t[-p dstport]         ---- destination port to send packets; default=9010\n"
         "\t[-i interval]        ---- packet interval in ms for sending packets; default=1000\n"
         "\t[-len length]        ---- packet size for sending packets; default=64\n"
	 "\t[-f srcFile]         ---- source File to stream out\n"
         "\t[-v [level]]         ---- verbose, 0-10; default=disable\n"
         "\t[-h]                 ---- print this help\n"
         "\n", prog);
}

typedef struct {
    char eIf[80];
    u32  localIP;
    int  ssmSize;
    char ssmIP[AMT_MAX_CHANNEL][80];
    u16  dport;
    u32  interval;
    u32  len;
    char srcFile[1024];
    int  verbose;
    
} comLine_param_t;

static int GetEnvParam(comLine_param_t *envP, int argc, char *argv[])
{
    int i;
    char *pIf;
    EX_ERR_CHECK(envP!=NULL, "GetEnvParam:ull pointer\n", 0);
    
    memset(envP, 0, sizeof(comLine_param_t));

    pIf = getenv("ETHERDEV");
    if (pIf == NULL) {
        pIf = "eth0";
    }
    strcpy(envP->eIf, pIf);
    envP->ssmSize = 1;
    strcpy(envP->ssmIP[0], "232.0.0.1");
    envP->dport = 9010;
    envP->len = 64;
    envP->verbose=0;
    
    // retrieve parameters from commnad lines
    i=1;
    while (i<argc) {
        if (strcmp(argv[i], "-if")==0) {
            i++;
            if (i<argc)  { strcpy(envP->eIf, argv[i]); }
          } else if (strcmp(argv[i], "-ssm")==0) {
            i++;
	    envP->ssmSize = 0;
	    while(i<argc && envP->ssmSize<AMT_MAX_CHANNEL) {
		strcpy(envP->ssmIP[envP->ssmSize], argv[i]); 
		envP->ssmSize++;
		if (i+1 >= argc || argv[i+1][0] == '-') {break;}
		i++;
	    }
        } else if (strcmp(argv[i], "-f")==0) {
            i++;
            if (i<argc)  { strcpy(envP->srcFile, argv[i]); }
	} else if (strcmp(argv[i], "-p")==0) {
            i++;
            if (i<argc)  { envP->dport = atol(argv[i]); }
        } else if (strcmp(argv[i], "-i")==0) {
            i++;
            if (i<argc)  { envP->interval = atol(argv[i]); }
        } else if (strcmp(argv[i], "-len")==0) {
            i++;
            if (i<argc)  { envP->len = atol(argv[i]); }
        } else if (strcmp(argv[i], "-v")==0) {
	    if (i<argc)  { envP->verbose = 1; }
	    if (i+1 < argc && argv[i+1][0] !='-') {
		i++;
		envP->verbose = atol(argv[i]);
	    }
        } else {
            if (strcmp(argv[i], "-h")!=0) {
                printf("invalid argument: %s\n", argv[i]);
            }
            PrintUsage(argv[0]);
            return 1;
        }
        i++; 
    }
    envP->localIP = getLocalIPDev(envP->eIf);
    envP->verbose = (envP->verbose>10)?10:envP->verbose;
    envP->verbose = (envP->verbose<0)?0:envP->verbose;   

    
    // check if valid
    if (envP->dport==0 || envP->ssmIP[0]==0 || envP->eIf[0] == 0) {
	PrintUsage(argv[0]);    
	return 1;
    }

    if (envP->interval==0) {
        envP->interval = 1000;
    }
    envP->len = (envP->len<10)?64:envP->len;

    // print env parameters
    {
        struct sockaddr_in srcAddr ;
	char ssmIP[2048]="";
	int i, size = 0;
	for (i=0;i<envP->ssmSize; i++) {
	    size = strlen(ssmIP);
	    snprintf(&ssmIP[size], 2048-size, "%s ",envP->ssmIP[i]);
	} 
	srcAddr.sin_addr.s_addr = htonl(envP->localIP);
	printf("feed to: ssm (source IP=%s group IP=%s) dstport=%u len=%u interval=%u (ms) srcMedia=%s\n",
	       inet_ntoa(srcAddr.sin_addr),ssmIP, envP->dport, envP->len, envP->interval,
	       (envP->srcFile[0])?envP->srcFile:"<generated>");
    }

    
    return 0;
}

typedef struct _RTP_HEADER {
    u8		vpxcc; //V_P_X_CC;
    u8	    	m_pt;
    u16		seqNo;
    u32		ts; 
    u32   	ssrc;
    u8	    payload[1];
} rtp_header_t;
#define RTPHEAD (12)
#define EX_MAGIC_KEY (0x5f414d54)

static inline s32 getRTPpkt_f(rtp_header_t *rtp, int offset, 
			      u16 seqNo, u32 ts, u32 ssrc, u8 pt, int len)
{
    int i;

    EX_ERR_CHECK(rtp!=NULL && len > RTPHEAD+offset, "getRTPPkt: null pointer", 1);

    rtp->vpxcc = 0x80;
    rtp->m_pt = pt;
    rtp->seqNo = htons(seqNo);
    rtp->ts = htonl(ts);
    rtp->ssrc = htonl(ssrc);
    
    for (i=0; i< len-offset-RTPHEAD; i++) {
	rtp->payload[offset+i] = (char)(seqNo+i);
    }
    return len;
}
static inline s32 getRTPpkt( u8 *pkt, char *fileName, int *len, int *port, u32 now)
{
    static FILE *file=NULL;
    static u8 *buf;
    static int size=0;
    static int index=0;
    static u32 baseTime=0;
    int readSize;
    u32 pktTs, pktsize;

    *port=0;
    *len = 0;

    EX_ERR_CHECK(pkt!=NULL, "getRTPPkt: null pointer", 1);
    if (file==NULL  && fileName[0]) {
	file = fopen(fileName, "rb");
	if (file==NULL) {
	    printf("cannot open file:%s\n", fileName);
	    return -1;
	}
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	buf = (u8 *)malloc(size);
	if (buf==NULL) {
	    printf("no memory to read file %s with size=%u\n", fileName, size);
	    return -1;
	}

	readSize = fread(buf, 1, size, file);
	if (readSize != size) {
	    printf("%d read for expected size=%u\n", readSize, size);
	}
	printf("total size=%u\n", readSize);
	size = readSize;
	fclose(file);

	baseTime = now;
    }
    
    pktTs = (buf[index]<<24) + (buf[index+1]<<16) +(buf[index+2]<<8)+(buf[index+3]) ;
    //printf("--- now = %u expired time=%u+%u=%u \n", now, pktTs, baseTime, pktTs+baseTime);
   if (pktTs+baseTime <= now) {
	pktsize = (buf[index+4]<<8) + buf[index+5];
	*port=(buf[index+7]);
	//printf("%u----ts=%u size=%u port=%u\n", now, pktTs+baseTime, pktsize, buf[index+7]);
	memcpy(pkt, &buf[index+8],pktsize);
	*len = pktsize;
 	index += pktsize+8;
	if (index >= size) {
	    index = 0;
	    baseTime = now;
	}
   }
   return *len;
}

static inline u32 getCurrentTime(void) 
{
    struct timeval tv = {0,0};
    gettimeofday(&tv, NULL);
    return ( tv.tv_sec*1000+ tv.tv_usec/1000);
}

#define TIME_RESOLUTION (2) // 2 ms
int sendout(comLine_param_t *pEnv)
{
    ex_sock_t *pSock[AMT_MAX_CHANNEL];
    u16 sport=0;
    u32 remoteIP;
    u32 ts=0;
    u16 seqNo=0;
    u8  pt = 126;
    u32 ssrc[AMT_MAX_CHANNEL];
    
    int res, i, j, k;
    unsigned char pingMsg[1500];
    int len=pEnv->len;
    u8 offset=4;
    u32 now,sentCount=0,expiredTime=0,base_time;
 
    // make UDP sockets
    for (k=0;k<pEnv->ssmSize;k++) {
	remoteIP = ntohl(inet_addr(pEnv->ssmIP[k]));
	sport=0;
	pSock[k] = ex_makeUDPSock(pEnv->localIP, &sport, remoteIP ,  pEnv->dport  );
	EX_ERR_CHECK(pSock[k] != NULL, " ", 1);
	
	// set ttl to 255. Multcast requires > 1
	res = ex_setSSMTTL(pSock[k]->sockClient, 255);
	EX_ERR_CHECK1(res>=0, "setsockopt failed errno=%u!\n", errno, 1);

	ssrc[k] = rand();
    }

   // send out data
    base_time = getCurrentTime();
    for (j=0;;j++) {
	int sent=0;
	now = getCurrentTime();
 	for (k=0; k<pEnv->ssmSize; k++) {
	    res=0;
	    rtp_header_t *pRTP = (rtp_header_t *)pingMsg;
	    *((u32 *)pRTP->payload) = htonl(EX_MAGIC_KEY);

	    if (pEnv->srcFile[0]==0) {
		if (now >= expiredTime) { // generated rtp
		    getRTPpkt_f(pRTP, offset, seqNo,ts,ssrc[k],pt, len);
		    res = ex_sendPacket(pSock[k],  pingMsg, len);
		    sent++;
		}
	    } else {
		int port, len=sizeof(pingMsg);
		res = getRTPpkt((u8 *)pRTP, pEnv->srcFile, &len, &port, now);
		if (res>0) {
		    if (port==0) {
			res = ex_sendPacket(pSock[k],  pingMsg, len);
		    } else {
			ex_setUDPClient(pSock[k], remoteIP,pEnv->dport+1);
			res = ex_sendPacket(pSock[k],  pingMsg, len);
			ex_setUDPClient(pSock[k], remoteIP,pEnv->dport);
		    }
		    sent++;
		}
	    }
	    if (sent && pEnv->verbose) {
		printf("\npkt ssrc=0x%08x remote:0x%08x len=%u seq #%06u:", 
		       ssrc[k], pSock[k]->remoteIP, len, ntohs(pRTP->seqNo));
		for(i=RTPHEAD;i<len;i++) {
		    char c=isprint(pingMsg[i])?pingMsg[i]:'.';
		    printf("%c",c);
		}
	    } else if (sent && pEnv->srcFile[0]==0) {
		printf("\npkt ssrc=0x%08x remote:0x%08x len=%u seq  #%06u", ssrc[k], 
		       pSock[k]->remoteIP, len, ntohs(pRTP->seqNo));
	    }
	    
	} 
	sentCount = (sent)?sentCount+1:sentCount;
	expiredTime =  base_time + sentCount*pEnv->interval;
	msleep(0, TIME_RESOLUTION);
	ts += 3300;
	seqNo++;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int res;
    comLine_param_t envParam;
    
    // setup environment
    memset(&envParam, 0, sizeof(envParam));
    res = GetEnvParam(&envParam, argc, argv);
    if (res) {
        printf("failed in gettign environment parameters\n");
        return 1;
    }
    sendout(&envParam);
    
    return 0;
}
