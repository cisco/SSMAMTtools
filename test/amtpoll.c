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
#include "amt.h"
 
static void PrintUsage(char *prog)
{
  printf("usage: %s\n"
         "\t[-anycast[+ssm] ip]  ---- try to receive with AMT, ssm or both; default=ssm\n"
         "\t-src ip              ---- source IP address of SSM\n"
	 "\t[-ssm ip1 [ip2 ...]  ---- ssm group(s) ip; default=232.0.0.1 \n"
         "\t[-p port]            ---- UDP listen port to receive packets; default=9010\n"
         "\t[-if interface]      ---- Ethenet interface used to get source IP for streamer; default=eth0\n"
	 "\t[-v [level]]         ---- verbose; default=disable\n"
         "\t[-h]                 ---- print this help\n"
         "\n", prog);
}

typedef struct {
    bool amt;		// through amt 
    bool ssm;		// through ssm
    u32  anycastIP;	// anycast IP
    
    u32  srcIP;		// source IP
    char ssmIP[AMT_MAX_CHANNEL][80]; // ssm group
    int  ssmSize;
    u16  dport;		// listening port
 
    char eIf[80];
    u32  localIP;

    int  verbose;
    
} comLine_param_t;

static int GetEnvParam(comLine_param_t *envP, int argc, char *argv[])
{
    int i;
    char *pIf;
    char srcIP[80]="", srcAnycast[80]="";

    if (envP==NULL) {
	printf("GetEnvParam:null pointer\n");
	return 1;
    }
    memset(envP, 0, sizeof(comLine_param_t));

    // set default parameters
    pIf = getenv("ETHERDEV");
    if (pIf == NULL) {
        pIf = "eth0";
    }
    strcpy(envP->eIf, pIf);
    envP->ssmSize = 1;
    strcpy(envP->ssmIP[0], "232.0.0.1");
    envP->dport = 9010;
    envP->verbose=0;
    envP->ssm = 1;
    
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
        } else if (strcmp(argv[i], "-src")==0) {
            i++;
            if (i<argc)  { strcpy(srcIP, argv[i]); }
        } else if (strcmp(argv[i], "-p")==0) {
            i++;
            if (i<argc)  { envP->dport = atol(argv[i]); }
        } else if (strcmp(argv[i], "-anycast")==0) {
	    i++;
	    if (i<argc)  { 
		strcpy(srcAnycast, argv[i]);
		envP->amt=1;
		envP->ssm=0;
	    }
	} else if (strcmp(argv[i], "-anycast+ssm")==0) {
	    i++;
	    if (i<argc)  { 
		strcpy(srcAnycast, argv[i]);
		envP->amt=1;
		envP->ssm=1;
	    }
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
    envP->srcIP = ntohl(inet_addr(srcIP));
    envP->anycastIP =  ntohl(inet_addr(srcAnycast));
    envP->verbose = (envP->verbose>AMT_LEVEL_10)?AMT_LEVEL_10:envP->verbose;
    envP->verbose = (envP->verbose<AMT_LEVEL_0)?AMT_LEVEL_0:envP->verbose;   

    
    // check if valid
    if (envP->dport==0 || envP->ssmIP[0][0]==0 || envP->eIf[0] == 0 ||
        srcIP[0] == 0 || (envP->amt && srcAnycast[0]==0)) {
	PrintUsage(argv[0]);    
	return 1;
    }
  
    // print env parameters
    {
	char ssmIP[2048]="";
	int i, size = 0;
	for (i=0;i<envP->ssmSize; i++) {
	    snprintf(&ssmIP[strlen(ssmIP)], 2048-size, "%s ",envP->ssmIP[i]);
	} 
     
	printf("recive from: %s%s%s (source IP=%s group IP=%s) udp port=%u \n", 
	       (envP->ssm)?"ssm ":"", (envP->amt)?"amt:":"", (envP->amt)?srcAnycast:"",
	       srcIP, ssmIP, envP->dport);
    }
    
    return 0;
}

static inline u32 getCurrentTime(void) 
{
    static u32 offset = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    u32 time_ms = tv.tv_sec*1000+ tv.tv_usec/1000;
    if (offset==0) {
	offset = time_ms;
    }
    return (time_ms-offset);
}

//
// receive packets from a single channel
//
int receivein(comLine_param_t  *pEnv)
{
    amt_handle_t channelID;
    amt_connect_req_e req=AMT_CONNECT_REQ_NONE;
    amt_connect_state_e state;
    amt_read_event_t rs;
    u32 group; 
 
    u8   buf[1500];
    int  i;

    // get request types
    req  = (pEnv->ssm)?AMT_CONNECT_REQ_SSM:AMT_CONNECT_REQ_NONE;
    req += (pEnv->amt)?AMT_CONNECT_REQ_RELAY:AMT_CONNECT_REQ_NONE;
    
    // open a channel
    group = ntohl(inet_addr(pEnv->ssmIP[0]));
    channelID = amt_openChannel(pEnv->anycastIP, group, pEnv->srcIP,  pEnv->dport, req);
    if (channelID == NULL) {
	return 1;
    }
    
    // check the state
    while(1) {
	state = amt_getState(channelID);
	if ((state & AMT_SSM_STATE_JOINING) == AMT_SSM_STATE_JOINING ||
	    (state & AMT_SSM_STATE_SSM_JOINING) == AMT_SSM_STATE_SSM_JOINING) {
	    break;
	}
 	printf("waiting ...... \n");
        msleep(1, 0);
    }

    // receive the packets
    rs.handle = channelID;
    while(1) {
	int n =  amt_poll(&rs,  1, 2000);
	if (n>0) {
	    int  len = amt_recvfrom(channelID, buf, 1500); 
	    
	    // print it out
	    printf("%u: pkt len=%u: ", getCurrentTime(), len);
	    for (i=0;i<len && i<16; i++) {
		printf("%02x ",buf[i]);
	    }
	    printf(" ");
	    for (i=0;i<len && i<32; i++) {
		char c= isprint(buf[i])?buf[i]:'.';
		printf("%c",c);
	    }
	    printf("\n");
	} 
    }
  
    return 0;
}


//
// the main function to get packets from AMT/SSM
int main(int argc, char *argv[])
{
    int res;
    comLine_param_t envParam;
    
    // setup environment
    res = GetEnvParam(&envParam, argc, argv);
    if (res) {
        printf("failed in getting the environment parameters\n");
        return 1;
    }
    
    // set trace; Use the internal trace
    // amt_setTraceSink(int level,  amt_traceSink_f traceFunc); 
    
    // set trace level 
     amt_setTraceLevel(envParam.verbose); 
    
     // call the initial function. Do nothing for now
     amt_init(envParam.anycastIP); 
     while((amt_getState(NULL) & AMT_SSM_STATE_CONNECTED) != AMT_SSM_STATE_CONNECTED) {
	 msleep(1,0);
     }
    
    // enter the loop to receive packets
    receivein(&envParam);
   
    return 0;
}
