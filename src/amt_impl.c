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
#include <poll.h>

#include "amt_sock.h"
#include "amt.h"
#include "amt_impl.h"
#include "amt_trace.h"

#define MIN_WAIT_TIME (5000)
#define DEFAULT_WAIT_TIME (10000)
//static  u32 amt_max_timeout = 10000;
static  int resetReq = 0;
static  pthread_mutex_t amt_mutex = PTHREAD_MUTEX_INITIALIZER;
static  pthread_mutex_t amt_reflush_mutex= PTHREAD_MUTEX_INITIALIZER;
static  pthread_t timerThread=0;


//static amt_ssm_t *pSSMChanRoot=NULL;
static amt_ssm_t 	*pAMTChanRoot=NULL;
static amt_leave_ssm_t 	*pAMTLeaveChanRoot=NULL;
static amt_relay_t *pAMTRelay=NULL; // support single relay only
//static u32 ssmChanCount = 0;
static u32 amtChanCount = 0;
static u32 amtLeaveChanCount = 0;
static u32 amtRelayCount = 0;

//------------------ calculate checksum -----------
static u16 checksum(u16 *buf, int size)
{
    u32 cksum=0;
    
    while(size>0) {
	cksum += *buf++;
	size--;
    }
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >> 16);
    return (u16)(~cksum);
}
 
//------------ read packet ----------------
static void dump_msg(u8 *buf, int size)
{
    char msg[2048];
    int i, ndx=0, max_l=2048;
    AMT_TRACE(AMT_LEVEL_9, "message dump with size=%u -- \n", size);
    for (i=0;i< size; i++) {
	snprintf(&msg[ndx], max_l,"0x%02x ", buf[i]);
	ndx+=5;max_l -= 5;
    }
    AMT_TRACE(AMT_LEVEL_9,"%s\n", msg);  
}

static int readPacket_auxmsg(struct pollfd *ufds, u8 *buf, int maxSize, u32 *group, u32 *srcIP)
{
    char cmbuf[0x100];	// the control data is dumped here
    struct iovec  iobuf={buf,maxSize};
    struct msghdr auxMsg={NULL, 0, &iobuf, 1, cmbuf,sizeof(cmbuf), 0};
    int size=0, res=0;
 
    *group= 0;
    *srcIP = 0;
    
    if (ufds->revents & POLLERR || ufds->revents & POLLHUP) {
         recvmsg(ufds->fd, &auxMsg, MSG_ERRQUEUE);
	// size = -1;
        //handleExcept(pEp);
    } else if (ufds->revents & POLLIN || ufds->revents & POLLPRI) { 
	struct cmsghdr *cmsg;	
	res = recvmsg(ufds->fd, &auxMsg, 0);
	AMT_ERR_CHECK(res>=0, AMT_ERR, "failed to read packet with "
		      "errno=%u errmsg=%s \n", errno, strerror(errno));
	size = res;	
	for (cmsg = CMSG_FIRSTHDR(&auxMsg); cmsg != NULL; cmsg = CMSG_NXTHDR(&auxMsg, cmsg)) {
	    if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
		*srcIP = ntohl(((struct in_pktinfo*)CMSG_DATA(cmsg))->ipi_spec_dst.s_addr);
		*group = ntohl(((struct in_pktinfo*)CMSG_DATA(cmsg))->ipi_addr.s_addr);
		// pi->ipi_spec_dst is the destination in_addr
		// pi->ipi_addr is the receiving interface in_addr
		break;
	    }
	}
    }
    return size;
}

static int readPacket(struct pollfd *ufds, u8 *buf, int maxSize)
{
    struct sockaddr tmp;
    u32 tmp_size = sizeof(struct sockaddr );
    struct msghdr errMsg;
    int size=0;
    
    if (ufds->revents & POLLERR || ufds->revents & POLLHUP) {
         recvmsg(ufds->fd, &errMsg, MSG_ERRQUEUE);
	// size = -1;
        //handleExcept(pEp);
    } else if (ufds->revents & POLLIN || ufds->revents & POLLPRI) { 
         size = recvfrom(ufds->fd, buf, maxSize, 0, &tmp, &tmp_size);
    } 
    return size;
}

static int readPacketExt(int fd, u8 *buf, int size, u32 timeout)
{
    int count=MAX_RETRY_SOCK, n;
    struct pollfd ufds;
    u32 startTime = getCurrentTime();
    u32 timeout_bak = timeout;
    memset(&ufds,0, sizeof(ufds));
    ufds.fd = fd;
    ufds.events =  POLLPRI | POLLIN | POLLERR | POLLHUP;
      
    while(count>0) {
        n = poll(&ufds, 1, timeout);  
        if (n<0) {
            if (errno==EINTR) {
		timeout = timeout_bak - (getCurrentTime()-startTime);
                if (timeout<=0) {
                    n=0;
                    break;
                }
                count--;
                continue;
            }
            break;
        }
        break;
     }

    if ( n>0) {
	n = readPacket(&ufds, buf, size);
    }
    return n;
}

amt_ssm_t *findSister(u16 port,  amt_ssm_t *pRoot)
{
    amt_ssm_t *p = pRoot;
    while(p) {
        if (p->udpPort == port) { break; }
	p = p->next;
    }
    return p;
}

static void inline amt_remove(amt_ssm_t *pSSM, amt_ssm_t **pRoot)
{	    
    amt_ssm_t *p=findSister(pSSM->udpPort, *pRoot);
    if (pSSM == p) { // the sister root
	if (p->sister==NULL) { // no sisters
	    if (pSSM->prev) {
		pSSM->prev->next = pSSM->next;
	    }
	    if (pSSM->next) {
		pSSM->next->prev = pSSM->prev;
	    }
	    if (pSSM == *pRoot) {
		*pRoot = pSSM->next;
	    }
	} else { // there are one or more sisters
	    amt_ssm_t *pSister = p->sister;
	    pSister->prev = pSSM->prev;
	    pSister->sister = pSister->next;
	    pSister->next = pSSM->next;	
	    if (p==*pRoot) {
		*pRoot = pSister;
	    }
	    
	}
    } else { // not root sister
	if (pSSM ->prev == p) {
	    p->sister = pSSM->next;
	} else {
	    pSSM ->prev->next = pSSM->next;
	}
	if (pSSM->next) {
	    pSSM->next->prev = pSSM->prev;
	}
    }

    pSSM->refCount--;

}

static void inline amt_leave_remove(amt_leave_ssm_t *pSSM, amt_leave_ssm_t **pRoot)
{	    
    if (pSSM->prev) {
	pSSM->prev->next = pSSM->next;
    }
    if (pSSM->next) {
	pSSM->next->prev = pSSM->prev;
    }
    if (pSSM == *pRoot) {
	*pRoot = pSSM->next;
    }
    
    pSSM->refCount--;
}

static void inline amt_insert_leave(amt_leave_ssm_t *pSSM, amt_leave_ssm_t **pRoot)
{	    
    pSSM->next = *pRoot;
    pSSM->prev = NULL;
    if (*pRoot) {
	(*pRoot)->prev = pSSM;
    }
    *pRoot = pSSM;
    pSSM->refCount++;
}

static void inline amt_insert(amt_ssm_t *pSSM, amt_ssm_t **pRoot)
{	    
    amt_ssm_t *p = findSister(pSSM->udpPort, *pRoot);
    if (p) { // there are one or more sisters
	pSSM->next = p->sister;
	if (p->sister) {p->sister->prev = pSSM;}
	pSSM->prev = p;
	p->sister = pSSM;
    } else { // first element
	pSSM->next = *pRoot;
	pSSM->prev = NULL;
	if (*pRoot) { (*pRoot)->prev = pSSM;}
	*pRoot = pSSM;
	pSSM->sister = NULL;
    }
    pSSM->refCount++;
    // AMT_TRACE(0, "+++++++++++ pSSM->refCount=%u\n", pSSM->refCount);
}

static inline void releaseRef(amt_ssm_t *pSSM)
{
    pSSM->refCount--;
    if (pSSM->refCount<=0) {
	pSSM->magicCode = 0xdeadbee1;
	free(pSSM);
    }
    //AMT_TRACE(0, "+++++++++++ pSSM->refCount=%u\n", pSSM->refCount);
}


//----------- begin of msg handling ----------- 
static u32 getExpiredTime(int retryCount, int maxWait) 
{
    u32 curTime = getCurrentTime();
    u32 max=(retryCount< 1)?1:1<<retryCount;
    u32 waitGap = 1000 + rand()%(1000*max);
    waitGap = ((waitGap>maxWait)?maxWait:waitGap)&0x0ffffff00;// to 256 ms 
    return ((curTime + waitGap));  
}

static int  sendDiscoveryMsg(amt_relay_t *pRelay)
{
    int res;
    u32 now = getCurrentTime();
    amt_discovery_t discoverMsg={AMT_DISCOVERY_TYPEID,0,0,0};
    discoverMsg.nonce =  pRelay->nonce;
    
    if (pRelay->retryCount<AMT_RETRY_MAX && 
	now >= pRelay->expiredTime) {
        res = amt_sendPacket( pRelay->sock,(u8 *)&discoverMsg, sizeof(amt_discovery_t));
        AMT_ERR_CHECK(res>=0, AMT_ERR, "failed to send discovery message with errmsg=%s \n", 
		      strerror(errno));
        AMT_TRACE(AMT_LEVEL_5, "send discovery msg (nonce=%u) to anycast %s\n",
		  pRelay->nonce, inet_ntoa(pRelay->sock->clientAddr.sin_addr));
        pRelay->retryCount++;
        pRelay->expiredTime = getExpiredTime(pRelay->retryCount, AMT_RELAY_TIMEOUT);
	pRelay->state = AMT_SSM_STATE_DISCOVERING;
    }
    return AMT_NOERR;
}

static int  sendRequestMsg(amt_relay_t *pRelay)
{
    int res;
    u32 now = getCurrentTime();
    amt_request_t req = {AMT_REQUEST_TYPEID, 0 /*ip4*/, 0, 0};

     req.nonce =  pRelay->nonce;
    
    if (pRelay->retryCount<AMT_RETRY_MAX && 
	now >= pRelay->expiredTime) {
	res = amt_sendPacket( pRelay->sock,(u8 *)&req, sizeof(req));
        AMT_ERR_CHECK(res>=0, AMT_ERR, "failed to send request message with errmsg=%s \n", 
                       strerror(errno));
        AMT_TRACE(AMT_LEVEL_5, "send request msg (nonce=0x%08x) to relay %s\n",
                   pRelay->nonce, inet_ntoa(pRelay->sock->clientAddr.sin_addr));
	pRelay->retryCount++;
	pRelay->expiredTime = getExpiredTime(pRelay->retryCount, AMT_RELAY_TIMEOUT);
    }
    return AMT_NOERR;
}

static void makeIPHeader(amt_ip_alert_t *ip, int size, amt_relay_t *pRelay) 
{
    ip->ver_ihl = 0x46;
    ip->tos = 0xc0;
    ip->tot_len = htons(size);
    ip->id = 0;
    ip->frag_off = 0;
    ip->ttl = 255;
    ip->protocol = AMT_IP_IGMP_PROTOCOL; // IGMP
    ip->check = 0; 
    ip->saddr = htonl(pRelay->natSrcIP);
    ip->daddr = htonl(AMT_SSM_COM);
    ip->alert = htonl(0x94040000);
    ip->check =checksum((u16 *)ip, 24/2);
}

#if 0
static void makeUDPHeader(amt_udpHeader_t *udp, int size,  amt_relay_t *pRelay) 
{
    udp->srcPort = htons(pRelay->natUdpPort);
    udp->dstPort = htons(AMT_ANYCAST_PORT);
    udp->len = htons(size);
    udp->check = 0;
}
#endif

static void makeReport(amt_igmpv3_membership_report_t *mr, int size,  amt_ssm_t *ssm) 
{
    mr->type = AMT_IGMPV3_MEMBERSHIP_REPORT_TYPEID;
    mr->checksum=0;
    mr->nGroupRecord=htons(1);
    mr->grp[0].type = (ssm->reflush)?AMT_GROUP_RECORD_MODE_IS_INCLUDE:AMT_GROUP_RECORD_ALLOW_NEW_SOURCES;
    mr->grp[0].auxDatalen = 0;
    mr->grp[0].nSrc = htons(1);
    mr->grp[0].ssm = htonl(ssm->groupIP);
    mr->grp[0].srcIP[0] = htonl(ssm->srcIP);
    mr->checksum = checksum((u16 *) mr, size/2);
}

static void makeLeaveReport(amt_igmpv3_membership_report_t *mr, int size,  amt_ssm_t *ssm) 
{
    mr->type = AMT_IGMPV3_MEMBERSHIP_REPORT_TYPEID;
    mr->checksum=0;
    mr->nGroupRecord=htons(1);
    mr->grp[0].type =  AMT_GROUP_RECORD_BLOCK_OLD_SOURCES; // AMT_GROUP_RECORD_CHANGE_TO_EXCLUDE_MODE; //AMT_GROUP_RECORD_MODE_IS_EXCLUDE;
    mr->grp[0].auxDatalen = 0;
    mr->grp[0].nSrc = htons(1);
    mr->grp[0].ssm = htonl(ssm->groupIP);
    mr->grp[0].srcIP[0] = htonl(ssm->srcIP);
    mr->checksum = checksum((u16 *) mr, size/2);
}

static int relay_leaveSSM(amt_ssm_t *pSSM) 
{
    u32 now = getCurrentTime();
    int res;
    amt_relay_t *pRelay = pSSM->relay;
    amt_membership_update_t req = {
	AMT_MEMBERSHIPP_UPDATE_TYPEID, 0,pRelay->mac_h, pRelay->mac_l, pRelay->nonce};
    amt_igmpv3_membership_report_t *mr = &req.mr;

 
    AMT_TRACE(AMT_LEVEL_8, "pSSM=%p relayState=0x%x retryCount=%u now=%u expiredTime=%u\n", 
	      pSSM, pSSM->relayState, pSSM->retryCount, now,  pSSM->expiredTime);
    
    if (pSSM->retryCount>=pRelay->minretry) {
	AMT_TRACE(AMT_LEVEL_8, "time out for pSSM=%p\n", pSSM);
	return AMT_TIMEOUT;
    }
   
    if (now >= pSSM->expiredTime) {
	pSSM->relayState = AMT_SSM_STATE_LEAVING;

	// make ip and udp
	makeIPHeader(&req.ip, sizeof(amt_membership_update_t)-12, pRelay);
	memset(mr, 0, sizeof(amt_igmpv3_membership_report_t));
	makeLeaveReport(mr, sizeof(amt_igmpv3_membership_report_t), pSSM);
	dump_msg((u8 *)&req,sizeof(amt_membership_update_t));
	res = amt_sendPacket(pRelay->sock,(u8 *)&req, sizeof(req));
	AMT_ERR_CHECK(res>=0, AMT_ERR, "failed to send leave report message "
		       "with errmsg=%s \n", strerror(errno));
	AMT_TRACE(AMT_LEVEL_6,"send update msg (mac=0x%04x%08x nonce=0x%08x) to relay %s "
		   "ssm=0x%08x src=0x%08x\n",
		  pRelay->mac_h, pRelay->mac_l, pRelay->nonce, 
		  inet_ntoa(pRelay->sock->clientAddr.sin_addr),
		   ntohl(mr->grp[0].ssm), ntohl(mr->grp[0].srcIP[0]));
	// reset count if the channel is active
	pSSM->retryCount = (pSSM->active>0)?0:++pSSM->retryCount;
	pSSM->active--;

	pSSM->expiredTime = getExpiredTime(pSSM->retryCount, AMT_RELAY_TIMEOUT); 
 	return AMT_SUCCESS;
   }
   return AMT_NOERR;
}


static void printMU(amt_membership_update_t *pMU, int size)
{
    if (amt_isTraced(AMT_LEVEL_9)) { // print all buf
	AMT_TRACE(0, "Membership update -- \n");
	dump_msg((u8 *)pMU, size);
    }
    AMT_TRACE(AMT_LEVEL_7,"Membership update size =%u\n", size);
    AMT_TRACE(AMT_LEVEL_7,"Header: type=%u mac=0x%04x%08x nonce=0x%08x\n",
	      pMU->type, pMU->mac_h, pMU->mac_l, pMU->nonce);
    
    AMT_TRACE(AMT_LEVEL_7,"IP: ver_ihl=0x%02x tos=0x%x tot_len=%u id=%u frag_off=0x%04x "
	      "ttl=%u proto=%u checkSum=%0x04x saddr=0x%08x daddr-0x%08x alert=0x%08x\n", 
	      pMU->ip.ver_ihl, pMU->ip.tos,ntohs(pMU->ip.tot_len), ntohs(pMU->ip.id), 
	      ntohs(pMU->ip.frag_off), pMU->ip.ttl,pMU->ip.protocol, ntohs(pMU->ip.check), 
	      ntohl(pMU->ip.saddr), ntohl(pMU->ip.daddr), ntohl(pMU->ip.alert));
    
    AMT_TRACE(AMT_LEVEL_7,"Tunneled membership report -- \n");
    AMT_TRACE(AMT_LEVEL_7,"type=0x%x checksum=%u nGroupRecord=%u \n", 
	      pMU->mr.type, ntohs(pMU->mr.checksum), ntohs(pMU->mr.nGroupRecord));
    
    AMT_TRACE(AMT_LEVEL_7,"group Record: type=%u auxDatalen=0x%x "
	      "nSrc=%u ssm=0x%08x srcIP=0x%08x\n",
	      pMU->mr.grp[0].type, pMU->mr.grp[0].auxDatalen, ntohs(pMU->mr.grp[0].nSrc),
	      ntohl(pMU->mr.grp[0].ssm), ntohl(pMU->mr.grp[0].srcIP[0]));

}

static int relay_joinSSM(amt_ssm_t *pSSM)
{
    u32 now = getCurrentTime();
    int res;
    amt_relay_t *pRelay = pSSM->relay;
    amt_membership_update_t req = {
	AMT_MEMBERSHIPP_UPDATE_TYPEID, 0,pRelay->mac_h, pRelay->mac_l, pRelay->nonce};
    amt_igmpv3_membership_report_t *mr = &req.mr;
    
    if (pSSM->reflush) {
	pSSM->expiredTime = 0;
	pSSM->retryCount = 0;
    }

    if (0 && pSSM->retryCount>=AMT_RETRY_MAX) {
	return AMT_TIMEOUT;
    }
    
    AMT_TRACE(AMT_LEVEL_8, "pSSM=%p now=%u expiredTime=%u\n", pSSM, now,  pSSM->expiredTime);
    if  (now >= pSSM->expiredTime) {
	
	// make ip and udp
	makeIPHeader(&req.ip, sizeof(amt_membership_update_t)-12, pRelay);
	memset(mr, 0, sizeof(amt_igmpv3_membership_report_t));
	makeReport(mr, sizeof(amt_igmpv3_membership_report_t), pSSM);
	if (amt_isTraced(AMT_LEVEL_7)) {
	    printMU(&req,  sizeof(req));
	}
	res = amt_sendPacket(pRelay->sock,(u8 *)&req, sizeof(req));
	AMT_ERR_CHECK(res>=0, AMT_ERR, "failed to send join report message "
		       "with errmsg=%s \n", strerror(errno));
	AMT_TRACE(AMT_LEVEL_3, "send update msg ((mac=0x%04x%08x nonce=0x%08x) to relay %s "
		   "ssm=0x%08x src=0x%08x\n",
		    pRelay->mac_h, pRelay->mac_l, pRelay->nonce, inet_ntoa(pRelay->sock->clientAddr.sin_addr),
		   ntohl(mr->grp[0].ssm), ntohl(mr->grp[0].srcIP[0]));
	pSSM->relayState |= AMT_SSM_STATE_JOINING;
	pSSM->retryCount++;
	pSSM->expiredTime = getExpiredTime(pSSM->retryCount, AMT_MAX_RETRY_JOIN_TIME); 
    }

    pSSM->reflush = 0;
    return AMT_NOERR;
}

static int relay_prepareForLeave(amt_ssm_t *pSSM)
{
    amt_leave_ssm_t *plssm = (amt_leave_ssm_t *)malloc(sizeof(amt_leave_ssm_t));
    AMT_ERR_CHECK(plssm!=NULL, AMT_ERR, "no memory for leaving SSM\n");
    AMT_TRACE(AMT_LEVEL_8, "prepare leave = %p\n", pSSM); 
 

    plssm->ssm = pSSM;
    pSSM->refCount++;
    pSSM->relayState = AMT_SSM_STATE_LEAVING;
    amt_insert_leave(plssm, &pAMTLeaveChanRoot);
    amtLeaveChanCount++;

    // send out unsubscription
    pSSM->retryCount = 0;
    pSSM->expiredTime = 0;

    return AMT_NOERR;
}

static int  sendUpdateMsg(amt_relay_t *pRelay)
{
    int res1=0, res=0;
    amt_ssm_t *pSSM, *pSSMNext;
    		
    // send all updates
    pthread_mutex_lock(&amt_mutex);	
    pSSM = pAMTChanRoot;
    
    while(pSSM) {
	if (pSSM->relay && pSSM->relayState != AMT_SSM_STATE_LEAVING &&  
	    (pRelay->reflush || 
	     (pSSM->relayState&AMT_SSM_STATE_JOINED)!=AMT_SSM_STATE_JOINED)) {
	    pSSM->reflush = pRelay->reflush;
	    res1 = relay_joinSSM(pSSM);
	}

	if (pSSM->sister) {
	    amt_ssm_t *pSister =pSSM->sister;
	    while(pSister) {
		if (pSister->relay && pSister->relayState != AMT_SSM_STATE_LEAVING&&  
		    (pRelay->reflush || 
		     (pSister->relayState&AMT_SSM_STATE_JOINED)!=AMT_SSM_STATE_JOINED)) {
		    pSister->reflush = pRelay->reflush;
		    res = relay_joinSSM(pSister);
		    if (res == AMT_TIMEOUT) {
			// move it to the leave-chain
			pSSMNext = pSister->next;
			relay_prepareForLeave(pSister);
			pSister= pSSMNext;
			continue;
		    }
		}
		pSister = pSister->next;
	    }
	}
	
	if (res1 == AMT_TIMEOUT) {
	    // move it to the leave-chain
	    pSSMNext = pSSM->next;
	    relay_prepareForLeave(pSSM);
	    pSSM= pSSMNext;
	    continue;
	}
	pSSM = pSSM->next;
    }
    pthread_mutex_unlock(&amt_mutex);	
    return AMT_NOERR;
}
static int  sendLeaveUpdateMsg(amt_relay_t *pRelay)
{
    int res;
    amt_ssm_t *pSSM;
    amt_leave_ssm_t *plSSM, *plSSMNext;
    	
    // send all updates
    pthread_mutex_lock(&amt_mutex);	
    plSSM = pAMTLeaveChanRoot;
    
    while(plSSM) {
	pSSM = plSSM->ssm;
	if (pSSM->relay) {
	    res = relay_leaveSSM(pSSM);

	    if (res == AMT_TIMEOUT) {
		// remove it from the ssm link
		plSSMNext = plSSM->next;
		amt_leave_remove(plSSM, &pAMTLeaveChanRoot);
		amtLeaveChanCount--;
		pSSM->refCount--;
		pSSM->relay=NULL;
		free(plSSM);
		plSSM = plSSMNext;
		// delete this ssm if needed
		if (pSSM->refCount<=0) {
		    pSSM->magicCode = 0xdeadbee1;
		    free(pSSM);
		}
		    
		continue;
	    }
	}
	plSSM = plSSM->next;
    }

    pthread_mutex_unlock(&amt_mutex);	
    return AMT_NOERR;
}

static int checkMQ(u8 *buf, int size)
{
    amt_membership_query_t *pMQ = (amt_membership_query_t *)buf;
    amt_igmpv3_membership_query_t *mq;
    int alertOffset;
    int cSize = 12;
    if (size <= cSize) {
	return AMT_ERR;
    }
    cSize += ((pMQ->l_g&1)?6:0)+sizeof(amt_ip_t)+sizeof(amt_igmpv3_membership_query_t)-4;
    if (size >= cSize) {
	int alertSize =  ((pMQ->ip.ver_ihl&0x0f)-5)*4;
	cSize +=alertSize;
	alertOffset = (alertSize)?0:-4;
	mq =  (amt_igmpv3_membership_query_t *)(((u32)&pMQ->mq) + alertOffset);
	cSize += ntohs(mq->nSrc)*4;
	if (size < cSize) { 
	    return AMT_ERR;
	}
	return AMT_NOERR;
    }
    return AMT_ERR;
}


static void printMQ(u8 *buf, int size)
{
    amt_membership_query_t *pMQ = (amt_membership_query_t *)buf;
    amt_igmpv3_membership_query_t *mq;
    int nSrc =0,  i, alertOffset=0;
    
    if (amt_isTraced(AMT_LEVEL_9)) { // print all buf
	AMT_TRACE(0, "Membership query -- \n");
	dump_msg(buf, size);
    }
    
    AMT_TRACE(AMT_LEVEL_7,"Membership Query size =%u\n", size);
    AMT_TRACE(AMT_LEVEL_7,"Header: type=%u l_g=%u mac=0x%04x%08x nonce=0x%08x\n",
	      pMQ->type, pMQ->l_g, pMQ->mac_h, pMQ->mac_l, pMQ->nonce);
    
    if ((pMQ->ip.ver_ihl&0x0f) > 5) {
	AMT_TRACE(AMT_LEVEL_7,"IP: ver_ihl=0x%02x tos=0x%x tot_len=%u id=%u frag_off=0x%04x "
	      "ttl=%u proto=%u checkSum=%0x04x saddr=0x%08x daddr-0x%08x alert=0x%08x\n", 
	      pMQ->ip.ver_ihl, pMQ->ip.tos,ntohs(pMQ->ip.tot_len), ntohs(pMQ->ip.id), 
	      ntohs(pMQ->ip.frag_off), pMQ->ip.ttl,pMQ->ip.protocol, ntohs(pMQ->ip.check), 
	      ntohl(pMQ->ip.saddr), ntohl(pMQ->ip.daddr), ntohl(pMQ->ip.alert));
    } else {
	alertOffset = -4;
	AMT_TRACE(AMT_LEVEL_7,"IP: ver_ihl=0x%02x tos=0x%x tot_len=%u id=%u frag_off=0x%04x "
	      "ttl=%u proto=%u checkSum=%0x04x saddr=0x%08x daddr-0x%08x\n", 
	      pMQ->ip.ver_ihl, pMQ->ip.tos,ntohs(pMQ->ip.tot_len), ntohs(pMQ->ip.id), 
	      ntohs(pMQ->ip.frag_off), pMQ->ip.ttl,pMQ->ip.protocol, ntohs(pMQ->ip.check), 
	      ntohl(pMQ->ip.saddr), ntohl(pMQ->ip.daddr));
    }
    
    mq = (amt_igmpv3_membership_query_t *)(((u32)&pMQ->mq) + alertOffset);
    nSrc = ntohs(mq->nSrc);
    AMT_TRACE(AMT_LEVEL_7,"Tunneled Membership Query -- \n");
    AMT_TRACE(AMT_LEVEL_7,"type=0x%x max_resp_code=0x%02x checksum=%u ssmIP=0x%08x s_qrv=%u qqic=%u\n", 
	      mq->type, mq->max_resp_code,ntohs(mq->checksum), 
	      ntohl(mq->ssmIP), mq->s_qrv, mq->qqic);
    if (amt_isTraced(AMT_LEVEL_7)) {
	char msg[2048];
	int ndx=0, max_l=2048;
	snprintf(msg, max_l,"nSrc=%02u: ", nSrc);
	ndx+=8;max_l -= 8;
	for (i=0; i< 10 && i< nSrc; i++) {
	    snprintf(&msg[ndx], max_l,"0x%08x ",ntohl(mq->srcIP[i]));
	    ndx+=11;max_l -= 11;
	}
	if (nSrc > 10) {
	    AMT_TRACE(0,"%s ..... total:%u\n",  msg, nSrc);
	} else {
	    AMT_TRACE(0,"%s\n", msg);
	}
    }

    if (pMQ->l_g & 1) {
	int offset =sizeof(amt_membership_query_t)-4+nSrc*4-6; 
	AMT_TRACE(AMT_LEVEL_7,"gateway: gateway port =%u gateway ip=0x%08x \n", 
		  buf[offset]*256 + buf[offset+1], 
		  (buf[offset+2]<<24) + ((buf[offset+3]<<16)&0x00ff0000) + 
		  ((buf[offset+4]<<8)&0x0000ff00) + (buf[offset+5]&0x000000ff));
    } else {
	AMT_TRACE(AMT_LEVEL_7," no gateway information\n");
    }
}

static int  handleDiscoveryResp(amt_relay_t *pRelay, u8 *buf, int size)
{
    int ip6Size = sizeof(amt_relay_adv_t);
    int ip4Size = ip6Size-12;
    amt_relay_adv_t *pRelayAdv = (amt_relay_adv_t *)buf;

    AMT_ERR_CHECK(size >= ip4Size, AMT_ERR, "not discoverResp\n");
    AMT_ERR_CHECK(pRelayAdv->type == AMT_RELAY_ADV_TYPEID && 
		  pRelayAdv->nonce == pRelay->nonce, AMT_ERR,
		  "not discovery resp (type=%u nonce:%u (req:%u)\n", 
		  pRelayAdv->type, pRelayAdv->nonce, pRelay->nonce);

    // retrieve relay IP
    pRelay->relayIP = ntohl(pRelayAdv->relayIP[0]);
    pRelay->state = AMT_SSM_STATE_REQUESTING;
   
    // update socket
    amt_setUDPClient(pRelay->sock, pRelay->relayIP, AMT_ANYCAST_PORT);
    AMT_TRACE(AMT_LEVEL_3, "discovered relay address:%s \n",  
	      inet_ntoa(pRelay->sock->clientAddr.sin_addr));
    
    pRelay->retryCount = 0;
    pRelay->expiredTime = 0;
    sendRequestMsg(pRelay);
    
    return AMT_NOERR;
}
static void amt_retry(void *timerID, void *param);
static int  handleRequestResp(amt_relay_t *pRelay, u8 *buf, int size)
{
    int res, alertOffset;
    int qqic;
    amt_membership_query_t *pMQ = (amt_membership_query_t *)buf;
    amt_igmpv3_membership_query_t *mq;

    if (size < 1 ||  pMQ->type!=AMT_MEMBERSHIP_QUERY_TYPEID) {
	if (amt_isTraced(AMT_LEVEL_7)) {
	    AMT_ERR_CHECK(0,AMT_ERR,
		  "not Membership query: msg type=%u \n", pMQ->type);
	}
	return AMT_ERR;
    }
    res = checkMQ(buf, size);
    AMT_ERR_CHECK(res==AMT_NOERR, AMT_ERR, "invalid Membership query: msg type=%u \n",  pMQ->type);
    AMT_ERR_CHECK(pMQ->nonce == pRelay->nonce, AMT_ERR,
		  "nonce not matched in  membership query resp (nonce:%u (req:%u)\n", 
		  pMQ->nonce,  pRelay->nonce);
    
    // trace it
    if (amt_isTraced(AMT_LEVEL_7)) {
	printMQ(buf, size);
    } 
    
    pRelay->mac_l = pMQ->mac_l;
    pRelay->mac_h = pMQ->mac_h;

    // check if alert is included
    alertOffset = ((pMQ->ip.ver_ihl&0x0f) > 5)?0:-4;
    mq = (amt_igmpv3_membership_query_t *)(((u32)&pMQ->mq) + alertOffset);

    // retrieve qrv and qqic
    pRelay -> minretry = mq->s_qrv&0x7;
    qqic = (mq->qqic<128)?mq->qqic:((mq->qqic&0x0f) + 0x10)<< (((mq->qqic>>4)&0x07) + 3);
    qqic = (qqic<AMT_MIN_DEFAULT_REFLUSH_INTERVAL)?AMT_DEFAULT_REFLUSH_INTERVAL:qqic;
    pRelay->qqic = qqic;
    pthread_mutex_lock(&amt_reflush_mutex);		    
    pRelay->flushTime = getCurrentTime() + qqic*1000;
    pthread_mutex_unlock(&amt_reflush_mutex);		    

    pRelay->minretry = ((qqic>>(pRelay->minretry+1))!=0)?AMT_DEFAULT_RETRY:pRelay->minretry;  

    // retrieve gateway NAT IP and port
    pRelay->relay_full = (pMQ->l_g&2);
     
    // retrive gateway info 
    if (pMQ->l_g & 1) {
	int nSrc = ntohs(mq->nSrc);
	int offset =sizeof(amt_membership_query_t)-4 + nSrc*4 -6; 
	pRelay->natUdpPort= buf[offset]*256 + buf[offset+1]; 
	pRelay->natSrcIP =  (buf[offset+2]<<24) + ((buf[offset+3]<<16)&0x00ff0000) + 
	    ((buf[offset+4]<<8)&0x0000ff00) + (buf[offset+5]&0x000000ff);
    }
    pRelay->state |= AMT_SSM_STATE_CONNECTED;
    
   // send update
    pRelay->retryCount = 0;
    pRelay->expiredTime = 0;
    pRelay->reflush=1;
    amt_addTimer(0,  amt_retry, NULL, 1);
    return res;
}

//----------- end of msg handling -----------  

//-----------  module process -----------  
//void amt_timeOut(u32 maxWaitTime)
//{
//    amt_max_timeout = (maxWaitTime < MIN_WAIT_TIME)?DEFAULT_WAIT_TIME:maxWaitTime;
//}


amt_ssm_t *getSSM(u32 source, u32 group, u16 port,  amt_ssm_t **_pSister)
{
    // active channels
    amt_ssm_t *p = pAMTChanRoot;
    amt_ssm_t *pSister = NULL;
    while(p) {
        if (p->udpPort == port) {
	    if (p->srcIP == source && p->groupIP==group) { 
		break; // get it
	    }
	    pSister = p->sister;
	    while(pSister) {
		if (pSister->srcIP == source && pSister->groupIP==group) { 
		    break; // get it
		}
		pSister = pSister->next;
	    }
	}
         p=p->next;
    }
    *_pSister = pSister; 
    return p;
}

static int setupSSM(amt_ssm_t *pSSM) 
{
    int res, enable=1;
 
    AMT_TRACE(AMT_LEVEL_9,"setting up ssm ...\n");

    // make a UDP socket
    // pHdl->ssmSock = amt_makeUDPSock(getLocalIP(), &pHdl->ssmUdpPort, 0 ,  0);
    pSSM->sock = amt_makeUDPSock(0, &pSSM->udpPort, 0 ,  0);
    if (pSSM->sock==NULL) { return AMT_ERR; }

    // make it reusable to receive all the ssm data
    res = setsockopt( pSSM->sock->sockClient, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (res < 0 ) {
	amt_close(&pSSM->sock);
	AMT_ERR_CHECK(0, AMT_ERR, "failed to set SO_REUSEADDR\n");
    }


    
    res = amt_joinSSMGroup(pSSM->sock->sockServ,  pSSM->groupIP, pSSM->srcIP,INADDR_ANY);
    //res = amt_joinSSMGroup(pSSM->sock->sockServ,  pSSM->groupIP, pSSM->srcIP, getLocalIP());
    AMT_ERR_CHECK(res>=0, AMT_ERR, "res =%d failed to join SSM errno=%u, errmsg=%s \n", 
		  res, errno, strerror(errno));
    
    {
        struct sockaddr_in ssmAddr, srcAddr, localAddr;
        srcAddr.sin_addr.s_addr =htonl(pSSM->srcIP);
        localAddr.sin_addr.s_addr =htonl(pSSM->sock->localIP); 
        ssmAddr.sin_addr.s_addr =htonl(pSSM->groupIP);
        
        AMT_TRACE(AMT_LEVEL_3, "ssm:%s \n",  inet_ntoa(ssmAddr.sin_addr));
        AMT_TRACE(AMT_LEVEL_3, "srcIP=%s \n", inet_ntoa(srcAddr.sin_addr));
        AMT_TRACE(AMT_LEVEL_3, "localIP=%s\n", inet_ntoa(localAddr.sin_addr));
    }
    pSSM->ssmState = AMT_SSM_STATE_SSM_JOINING;

    AMT_TRACE(AMT_LEVEL_9,"setting up ssm ...done\n");
    
    return AMT_NOERR;
}
static void amt_retry(void *timerID, void *param)
{
    if (pAMTRelay && (pAMTRelay->state & AMT_SSM_STATE_CONNECTED) ==
	AMT_SSM_STATE_CONNECTED) {
	u32 now = getCurrentTime();

	// check to reflush
	pthread_mutex_lock(&amt_reflush_mutex);		    
	if (pAMTRelay->flushTime != 0 && pAMTRelay->flushTime < now) {
	    if (pAMTRelay->prevFlushtime != pAMTRelay->flushTime) {

		pAMTRelay->nonce = rand();
		pAMTRelay->prevFlushtime=pAMTRelay->flushTime;
	    }
	    sendRequestMsg(pAMTRelay);
	}
	pthread_mutex_unlock(&amt_reflush_mutex);		    
	sendUpdateMsg(pAMTRelay);
	sendLeaveUpdateMsg(pAMTRelay);
	pAMTRelay->reflush = 0;
    }
   
}


static int quitRelayConnection = 0;
static int relayConnThreadRunning = 0;
static void amt_connectRelayTimeOut(amt_timer_handle timeID, void *param)
{
    if (relayConnThreadRunning) {
	quitRelayConnection = 1; // later....
    }
}
static void *amt_connectRelay(void *timerID)
{
    int res = 0, size;
    amt_relay_t *pRelay = pAMTRelay;
    u16  lRelayPort = 0;

    AMT_TRACE(AMT_LEVEL_9,"thread to connect relay started with anycast IP:0x%08x:%u\n", 
	      pRelay->anycastIP, AMT_ANYCAST_PORT);

    // make a socket to connect to relay
    pRelay->sock = amt_makeUDPSock(0, &lRelayPort, pRelay->anycastIP, AMT_ANYCAST_PORT);
    if (pRelay->sock==NULL) { 
	amt_deleteTimer((amt_timer_handle)timerID);
	relayConnThreadRunning = 0;
	AMT_ERR_CHECK(0, NULL, "failed to create a socket to connect to relay\n" );
    }
    
    // send discovery message
    pRelay->nonce = rand();
    pRelay->retryCount = 0;
    pRelay->expiredTime = 0;
    pRelay->natSrcIP = pRelay->sock->localIP; 
    pRelay->natUdpPort = pRelay->sock->localPort;
    //printf("------------ localPort=%u\n",  pRelay->natUdpPort);
    if (sendDiscoveryMsg(pRelay)) {
	relayConnThreadRunning=0;
	return NULL;
    }
  
    // get the response
    while(!quitRelayConnection) {
	res = readPacketExt(pRelay->sock->sockClient, pRelay->buf, AMT_BUF_SIZE,
			    AMT_MAX_RESPONSE_WAIT_TIME);
	if (res<0) { // error 
	    amt_deleteTimer((amt_timer_handle)timerID);
	    relayConnThreadRunning = 0;
	    AMT_ERR_CHECK(0, NULL, "failed to receive response with errmsg=%s ", 
			   strerror(errno));
	}
	if (res==0) { // timeout
	    if (pRelay->state == AMT_SSM_STATE_DISCOVERING) {
		res = sendDiscoveryMsg(pRelay);
	    } else if (pRelay->state == AMT_SSM_STATE_REQUESTING) {
		res = sendRequestMsg(pRelay);
	    } 
	    continue;
	}

	size = res;
	res = AMT_NOERR;
     
	if (pRelay->state == AMT_SSM_STATE_DISCOVERING) {
	    // handle the discovery response msg
	    res = handleDiscoveryResp(pRelay, pRelay->buf, size);

    	} else if (pRelay->state == AMT_SSM_STATE_REQUESTING) {
	    // handle the request response msg
	    res = handleRequestResp(pRelay, pRelay->buf, size);
	    if (res==AMT_NOERR) {
		break;
	    }
	}
	if (res == AMT_ERR) {
	    dump_msg(pRelay->buf, size);
	}
    }

    // delete the clean up timer
    amt_deleteTimer((amt_timer_handle)timerID);

    // set up a re_try timer
    amt_addTimer(AMT_MAX_DEFAULT_KEEP_ALIVE,  amt_retry, NULL, 0);
    
    relayConnThreadRunning = 0;
    quitRelayConnection = 0;
    AMT_TRACE(AMT_LEVEL_9,"thread to connect relay exits normally.\n");

    return NULL;
}

static void removeLeaveChannel(amt_ssm_t *pNewSSM)
{
    amt_ssm_t *pSSM;
    amt_leave_ssm_t *plSSM;
    
    pthread_mutex_lock(&amt_mutex);	
    plSSM = pAMTLeaveChanRoot;
    
    while(plSSM) {
	pSSM = plSSM->ssm;
	if (pSSM->srcIP == pNewSSM->srcIP && 
	    pSSM->groupIP == pNewSSM->groupIP &&
	    pSSM->udpPort == pNewSSM->udpPort) {
	    amt_leave_remove(plSSM, &pAMTLeaveChanRoot);
	    AMT_TRACE(AMT_LEVEL_7,"re-opened old channel:%p removed \n", pSSM);
	    amtLeaveChanCount--;
	    pSSM->refCount--;
	    pSSM->relay=NULL;
	    free(plSSM);
	    
	    // delete this ssm if needed
	    if (pSSM->refCount<=0) {
		pSSM->magicCode = 0xdeadbee1;
		free(pSSM);
	    }
	    break;
	}
	plSSM = plSSM->next;
    }
    pthread_mutex_unlock(&amt_mutex);	
}

// 
// setup AMT
static int amtSetup(amt_relay_t *pRelay)
{
    int res=0;
    amt_timer_handle timerID=NULL;
    pthread_t t;
    pthread_attr_t	thread_attr;

    AMT_TRACE(AMT_LEVEL_8,"setting up relay ... \n");
    pRelay->state = AMT_SSM_STATE_PREPARING;

    // idle, make a timer to clean it up
    timerID = amt_addTimer(AMT_MAX_RELAY_CONNECT_TIME,  amt_connectRelayTimeOut, NULL, 1);

    // make a thread to connect to relay
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    quitRelayConnection = 0;
    relayConnThreadRunning=1;
    res =pthread_create(&t, &thread_attr, (void *(*)(void *)) amt_connectRelay , timerID);
    if (res!=0) {
	relayConnThreadRunning = 0;
	pRelay->state = AMT_SSM_STATE_IDLE;
	AMT_ERR_CHECK(res == 0, AMT_ERR, "failed to create thread for connect to relay\n");
    }
    AMT_TRACE(AMT_LEVEL_9,"setting up relay thread ...done\n");

    return res;
}

// 
// join AMT if the relay has been setup or 
// create a thread to setup the relay
static int joinAMT(amt_ssm_t *pSSM)
{
    amt_relay_t *pRelay = pSSM->relay;
    int res = 0;
 
    AMT_TRACE(AMT_LEVEL_9,"hooking up with relay \n");

    if ((pRelay->state&AMT_SSM_STATE_CONNECTED) == AMT_SSM_STATE_CONNECTED) {  
	
	// check if it is one of leaving channels
	removeLeaveChannel(pSSM);

	// send join message
	pSSM->retryCount=0;
	pSSM->expiredTime = 0;
	pSSM->relayState = AMT_SSM_STATE_JOINING;
	amt_addTimer(0,  amt_retry, NULL, 1);
        return res;
    } else if ((pRelay->state&AMT_SSM_STATE_PREPARING) == AMT_SSM_STATE_PREPARING) { // relay in connecting
        pSSM->relayState = AMT_SSM_STATE_CONNECTING;
        return res;
    }
    pSSM->relayState = AMT_SSM_STATE_CONNECTING;
 
    // AMT is not yet setup. setup the AMT 
    res = amtSetup(pRelay); 
    
    return(res);
}
    

static int setupChannel(amt_ssm_t *pSSM)
{
    int res;

    if (pSSM->req & AMT_CONNECT_REQ_SSM) {
        res = setupSSM(pSSM);
        if (res) { return res;}
    }

    if (pSSM->req & AMT_CONNECT_REQ_RELAY) {
        pSSM->relay = pAMTRelay; // single relay for now
        res =  joinAMT(pSSM);
        if (res) { return res;} 
    }

    return AMT_NOERR;
}

amt_ssm_t *openChannel_impl(u32 anycast, u32 group, u32 source, u16 port,
			    amt_connect_req_e req)
{   int size;
    int res, first=0;
    amt_ssm_t *pSSM;
    
    AMT_ERR_CHECK(pAMTRelay==NULL|| (pAMTRelay!=NULL && pAMTRelay->anycastIP == anycast), 
		  NULL, "only single relay is supported\n");
    AMT_ERR_CHECK(amtChanCount<AMT_MAX_CHANNEL, NULL, "too many channels: %u (Max:%u)\n",
		  amtChanCount+1, AMT_MAX_CHANNEL);
    AMT_ERR_CHECK(pAMTRelay==NULL|| 
		  (pAMTRelay!=NULL && 
		   (relayConnThreadRunning==1 || 
		    (pAMTRelay->state&AMT_SSM_STATE_CONNECTED)==AMT_SSM_STATE_CONNECTED)), NULL,
		  "Error in the Relay setup. Please reset and re-initialze the module\n");
    
    // check if to create a timer
    if (amt_checkTimerRun() == AMT_TIMER_IDLE) {
	amt_createTimer(AMT_TIMER_RESOLUTION);
    }
    
    // relay
    if (pAMTRelay==NULL && (req&AMT_CONNECT_REQ_RELAY)) {
	size = sizeof(amt_relay_t);
	pAMTRelay = (amt_relay_t *) malloc(size);
	AMT_ERR_CHECK(pAMTRelay!=NULL, NULL, "no memory for relay\n");
	memset(pAMTRelay, 0, size);
	pAMTRelay->anycastIP = anycast;
	amtRelayCount++;
	first = 1;
    }

    // new amt channel
    size = sizeof(amt_ssm_t);
    pSSM = (amt_ssm_t *)malloc(size);
    if (pSSM==NULL) {
	if (first) {
	    free(pAMTRelay);
	    pAMTRelay = NULL;
	    amtRelayCount--;
	}
	AMT_ERR_CHECK(0, NULL, "no memory for ssm\n");
    }
    memset(pSSM, 0, size);
    pSSM->magicCode = AMT_MAGIC_CODE;                                   
    pSSM->srcIP = source;
    pSSM->udpPort = port;
    pSSM->req = req;
    pSSM->groupIP = group;
 
    res = setupChannel(pSSM);
    if (res<0) { 
	if (first) {
	    free(pAMTRelay);
	    pAMTRelay = NULL;
	    amtRelayCount--;
	}
	pSSM->magicCode = 0xdeadbee1;
	free(pSSM);
	return NULL;
    }
    
    // add to the chain
    pthread_mutex_lock(&amt_mutex);
    amt_insert(pSSM, &pAMTChanRoot);
    amtChanCount++;
    pthread_mutex_unlock(&amt_mutex);	
 
    return (amt_handle_t) pSSM;
}
static int handleMulticastData(amt_ssm_index_t *relayPointer, int rc, amt_relay_t *pRelay, 
			       u8 *buf, int size, int new)
{
    int i, consumed=0;
    amt_multicast_data_t *pMD = (amt_multicast_data_t *)buf;
    u32 src, grp;
    u16 port;
    amt_ssm_t *pSSM;
    AMT_ERR_CHECK(size>=sizeof(amt_multicast_data_t)-1, AMT_ERR,
	       "handleMulticastData: too small packet with size=%u", size);
    pRelay->bufSize = 0;
    pRelay->pSSM = NULL;
    dump_msg(buf,size);
    src = ntohl(pMD->ip.saddr);
    grp = ntohl(pMD->ip.daddr);
    port = ntohs(pMD->udp.dstPort);
    
    if (amt_isTraced(AMT_LEVEL_9)) { 
	struct sockaddr_in saddr, daddr;          ;
	saddr.sin_addr.s_addr = pMD->ip.saddr;
	daddr.sin_addr.s_addr = pMD->ip.daddr; 
        
	AMT_TRACE(AMT_LEVEL_9,"size=%u\n",size);
	AMT_TRACE(AMT_LEVEL_3, "group:%s \n",  inet_ntoa(daddr.sin_addr));
        AMT_TRACE(AMT_LEVEL_3, "srcIP=%s \n", inet_ntoa(saddr.sin_addr));
        AMT_TRACE(AMT_LEVEL_3, "port=%u\n", port);
    }
  
    for (i=0;i<rc;i++) {
	pSSM = relayPointer[i].pSSM;
	if (pSSM->srcIP==src && pSSM->groupIP==grp && pSSM->udpPort==port) {
	    if (new) {pSSM->active = 1;}
	    consumed = 1;
	    if ((pSSM->relayState&AMT_SSM_STATE_LEAVING) != AMT_SSM_STATE_LEAVING) {
		pSSM->relayState = AMT_SSM_STATE_JOINED;
		((amt_read_event_t *)relayPointer[i].bPointer[0])->rstate = AMT_READ_IN;
	    }
	    
	    // disconnected from ssm
	    if (pSSM->ssmState != AMT_SSM_STATE_SSM_JOINED) {
		if (pSSM->ssmState == AMT_SSM_STATE_SSM_JOINING) {
		    AMT_TRACE(AMT_LEVEL_8, "------ pSSM=%p ssm close\n", pSSM);
		    amt_leaveSSMGroup(pSSM->sock->sockClient, grp, src, 0);
		    pSSM->ssmState = AMT_SSM_STATE_IDLE;
		}
	    }
	    break;
	}
    }

    // check if the old connection
    if (consumed==0) { 
	amt_leave_ssm_t *p = pAMTLeaveChanRoot;
	while(p) {
	    pSSM=p->ssm;
	    if (pSSM->srcIP == src && pSSM->groupIP==grp) { // get it
		break;
	    }
	    p=p->next;
	}
	if (p) {
	    if (new) {
		pSSM->active = 1;
		AMT_TRACE(AMT_LEVEL_8, "packet from leaving channel:%p\n", pSSM);
	    }
	}

    } else {
	pRelay->bufSize = size-sizeof(amt_multicast_data_t)+1;
	pRelay->pSSM = pSSM;
	return 1;
    }
    return 0;
}

static int  handleRelayMsg(struct pollfd *ufds,  
			   amt_ssm_index_t *relayPointer, 
			   int rc, 
			   amt_read_event_t *rs,
			   int *haveData)
{
    int  size;
    
    // need to revise it .....
    *haveData = 0;
    size = readPacket(ufds, pAMTRelay->buf, 1500);
    if (size < 0) { // error
	pAMTRelay->bufSize = 0;
	pAMTRelay->pSSM=NULL;
 	AMT_ERR_CHECK(0, AMT_ERR, "failed to receive packet from relay with errmsg=%s ", 
		      strerror(errno));
  } else if (size == 0) { // no data
	if ((pAMTRelay->bufSize != 0 &&  pAMTRelay->pSSM)) {
	    size = pAMTRelay->bufSize;
	    if (pAMTRelay->buf[0] == AMT_MULTICAT_DATA_TYPEID) {
		int res = handleMulticastData(relayPointer, rc, pAMTRelay, 
					      pAMTRelay->buf, size, 0);
		if (res>0) {
		    *haveData = 1;
		}	    
	    }
	}
	return 0;
    }

    AMT_TRACE(AMT_LEVEL_7,"msg type=%u size=%u \n", pAMTRelay->buf[0], size);
    pAMTRelay->bufSize = size;
    

    // check the message
    if (pAMTRelay->buf[0] == AMT_MEMBERSHIP_QUERY_TYPEID) {
	handleRequestResp(pAMTRelay, pAMTRelay->buf, size);
    } else if (pAMTRelay->buf[0] == AMT_MULTICAT_DATA_TYPEID) {
	int res = handleMulticastData(relayPointer, rc, pAMTRelay, 
				      pAMTRelay->buf, size, 1);
	if (res>0) {
	    *haveData = 1;
	}	    
    }
	
    return size;
}

static void preparePoll(struct pollfd *ufds, 
			amt_ssm_index_t *ssmPointer,
			int *nSSM,
			amt_ssm_index_t *relayPointer,
			int *nRSSM,
			amt_read_event_t *rs, int size)
{
    int i, c = 0, rc=0, k;
    for (i=0; i< size; i++) {
        amt_ssm_t *pSSM = (amt_ssm_t *)rs[i].handle; 
        if (pSSM->relay && pAMTRelay && pAMTRelay->sock && 
	    (pAMTRelay->state & AMT_SSM_STATE_CONNECTED) ==
	    AMT_SSM_STATE_CONNECTED ) {
            relayPointer[rc].pSSM = pSSM;
	    relayPointer[rc++].bPointer[0] = &rs[i];
	}
        if (pSSM->sock && (pSSM->ssmState & AMT_SSM_STATE_SSM_JOINING) ==
	    AMT_SSM_STATE_SSM_JOINING) {
	    amt_ssm_t *p;

	    // find the sister root
	    pthread_mutex_lock(&amt_mutex);	
	    p=findSister(pSSM->udpPort, pAMTChanRoot);
	    pthread_mutex_unlock(&amt_mutex);	
	    if (p==NULL) {
		AMT_TRACE(AMT_LEVEL_0,"internal errors! handle=%p\n", pSSM);
		continue;
	    }
	    if (p->mark==0) {
		ufds[c].fd = pSSM->sock->sockClient;
		ufds[c].events =  POLLPRI | POLLIN | POLLERR | POLLHUP;
		ssmPointer[c].pSSM = p;
		ssmPointer[c++].bPointer[p->mark++] = &rs[i];
	    } else {
		for (k=0; k<c; k++) {
		    if (ssmPointer[k].pSSM == p) {
			ssmPointer[k].bPointer[p->mark++] = &rs[i];
			break;
		    }
		}
	    }
        }
	rs[i].rstate = AMT_READ_NONE;
    }
    
    if (rc) { // add the relay socket
        ufds[c].fd = pAMTRelay->sock->sockClient;
        ufds[c].events =  POLLPRI | POLLIN | POLLERR | POLLHUP;
	ssmPointer[c].pSSM = relayPointer[0].pSSM;
	ssmPointer[c].bPointer[0] =relayPointer[0].bPointer[0];
	c++;
    }
    *nSSM = c;
    *nRSSM = rc; 
}

 
int poll_impl(amt_read_event_t *rs, int size, int timeout)
{
    struct pollfd ufds[AMT_MAX_CHANNEL];
    int i,len,  res=0, c=0, rc=0, n, relayData=0, haveData=0;
    int startTime;
    amt_ssm_index_t ssmPointer[AMT_MAX_CHANNEL];
    amt_ssm_index_t relayPointer[AMT_MAX_CHANNEL];
    int count = MAX_RETRY_SOCK;
    int readyCheckCount  = (timeout<=0)?0:(timeout+AMT_TIME_RESOLUTION-1)/AMT_TIME_RESOLUTION; // 10 ms
    int timeleft;

    memset(ufds, 0, sizeof(ufds));
    memset(ssmPointer, 0, sizeof(ssmPointer));
    memset(relayPointer, 0, sizeof(relayPointer));
    startTime = getCurrentTime();

    while(1) { // waiting for connecting to relay
	preparePoll(ufds, ssmPointer, &c, relayPointer, &rc, rs, size);
	if (c!=0) { break; }
	if (readyCheckCount==0) {return 0; } // not ready
	msleep(0, 10);
	readyCheckCount--;
    }

    AMT_TRACE(AMT_LEVEL_8,"poll the socket: #channels:%u #ssm:%u #reley:%u\n", 
	      size, c, rc);

    // poll them
    errno = 0;
    timeleft =  timeout - (getCurrentTime()-startTime);
    while(count>0) {
        n = poll(ufds, c, timeleft);  
        if (n<0) {
            if (errno==EINTR) {
		timeleft =  timeout - (getCurrentTime()-startTime);
                if (timeleft<=0) {
                    n=0;
                    break;
                }
                count--;
                continue;
            }
	}
	break;
    }
  
    // handle the relay socket
    if (rc) {
	c--;
	if (n>0) { // waiting for relay to send data
	    res = handleRelayMsg(&ufds[c], relayPointer, rc, rs, &haveData);
	    if (res>0) {
		n--;
		if (haveData) {relayData  = 1;}
	    }
	}
    }
    
    if (n<=0) {
	int i;
	for (i=0;i<c;i++) {
	    amt_ssm_t *p = ssmPointer[i].pSSM; 
	    p->mark = 0;
	}
	return  n+relayData;
    }
  
    // update individual ssm
    for (i=0;i<c && n>0;i++) {
	int k;
	amt_ssm_t *p = ssmPointer[i].pSSM; // root of sisters

	// read packet
	if (ufds[i].revents & POLLERR || ufds[i].revents&POLLHUP) { // put all socket error
	    for (k=0; k<p->mark; k++) {
		((amt_read_event_t *)ssmPointer[i].bPointer[k])->rstate = AMT_READ_ERR;
	    }
	    readPacket(&ufds[i], NULL, 0); // read error msg
	    p->bufSize = 0;

	} else if (ufds[i].revents & POLLIN || ufds[i].revents&POLLPRI) { // read packet in
	    u32 group, srcIP;
	    p->bufSize = 0;
	    res = readPacket_auxmsg(&ufds[i], p->buf, AMT_BUF_SIZE, &group, &srcIP);
	    if (res<0) {
		p->mark=0;
		AMT_ERR_CHECK(res>=0, AMT_ERR, "failed to read packet with errno=%u errmsg=%s ", 
			  errno, strerror(errno));
	    }
	    
	    // find the handle
	    len = res;
	    if (res>0) {
		for (k=0; k<p->mark; k++) {
		    amt_ssm_t *pSSM = (amt_ssm_t *) ((amt_read_event_t *)ssmPointer[i].bPointer[k])->handle;
		    if (pSSM->srcIP == srcIP && pSSM->groupIP == group) {
			((amt_read_event_t *)ssmPointer[i].bPointer[k])->rstate = AMT_READ_IN;
			pSSM->ssmState = AMT_SSM_STATE_SSM_JOINED;
			p->bufSize = len;
			p->pSSM = pSSM;

			// disconnnected from relay
			pthread_mutex_lock(&amt_mutex);	
			if (pSSM->relay) {
			    if ((pSSM->relayState & AMT_SSM_STATE_JOINING) == AMT_SSM_STATE_JOINING) {
				relay_prepareForLeave(pSSM);
			    } else {
				pSSM->relay = NULL;
			    }
			}
			pthread_mutex_unlock(&amt_mutex);					

		    }
		}
	    }
	} else if (ufds[i].revents & POLLNVAL) {
	    for (k=0; k<p->mark; k++) {
		((amt_read_event_t *)ssmPointer[i].bPointer[k])->rstate = AMT_READ_CLOSE;
		((amt_ssm_t *) ((amt_read_event_t *)ssmPointer[i].bPointer[k])->handle)->ssmState = AMT_SSM_STATE_IDLE;
	    }
	    p->bufSize = 0;
	}
	p->mark = 0;
    }
    return n+relayData;
}

int recvfrom_impl(amt_ssm_t *pSSM, u8 *buf, int maxSize) 
{
    int  size=0;
    
    // check relay
    if (pSSM->relay && 
	(pSSM->relay->state&AMT_SSM_STATE_CONNECTED) == AMT_SSM_STATE_CONNECTED) { //
	if (pSSM->relay->pSSM == pSSM && pSSM->relay->bufSize != 0) {
	    size = (maxSize > pSSM->relay->bufSize)? pSSM->relay->bufSize:maxSize;
	    memcpy(buf, &pSSM->relay->buf[sizeof(amt_multicast_data_t)-1], size);
	    pSSM->relay->pSSM = NULL;
	    pSSM->relay->bufSize = 0;
	    dump_msg(buf,size);
 	    return size;
	}
    }
    
    if (pSSM->sock && (pSSM->ssmState&AMT_SSM_STATE_SSM_JOINING) == AMT_SSM_STATE_SSM_JOINING) {
	amt_ssm_t *p=findSister(pSSM->udpPort, pAMTChanRoot);
	if (p==NULL) {
	    AMT_TRACE(AMT_LEVEL_0,"internal errors! handle=%p\n", pSSM);
	    return 0;
	}	
	if (p->pSSM == pSSM && p->bufSize != 0) {
	    size = (maxSize > p->bufSize)? p->bufSize:maxSize;
	    memcpy(buf, p->buf, size);
	    p->pSSM = NULL;
	    p->bufSize = 0;
	    dump_msg(buf,size);
 	    return size;
	}
    }
    return size;
}

int closeChannel_impl(amt_ssm_t *pSSM)
{
    
    // close SSM
    if (pSSM->sock && (pSSM->ssmState&AMT_SSM_STATE_SSM_JOINING) == AMT_SSM_STATE_SSM_JOINING) {
	amt_leaveSSMGroup(pSSM->sock->sockClient, pSSM->groupIP, pSSM->srcIP, INADDR_ANY);
	pSSM->ssmState = AMT_SSM_STATE_IDLE;
	amt_close(&pSSM->sock);
    }
    
    // check to unsubscribe the channel
    pthread_mutex_lock(&amt_mutex);
  
    if (pSSM->relay) {
	if ((pSSM->relayState & AMT_SSM_STATE_JOINING) == AMT_SSM_STATE_JOINING) {
	    relay_prepareForLeave(pSSM);
	} else {
	    pSSM->relay = NULL;
	}
    }

    amt_remove(pSSM,&pAMTChanRoot);
    amtChanCount--;
  
    if (pSSM->refCount<=0) {
	pSSM->magicCode = 0xdeadbee1;
	free(pSSM);
    } 
      
    pthread_mutex_unlock(&amt_mutex);	    
 
    return AMT_NOERR;   
}

static int recvThreadRunning = 0;
static int quitRecvThread=0;
static void getAllHandle(amt_read_event_t *rs, int *_size)
{
    int i, maxSize=*_size;
    amt_ssm_t *pSSM, *pSister;
    memset(rs, 0, maxSize*sizeof(amt_read_event_t));
    
    *_size = 0;
    pthread_mutex_lock(&amt_mutex);	    
    i=0;
    for (pSSM=pAMTChanRoot;pSSM!=NULL && i<maxSize; pSSM=pSSM->next) {
	if ((pSSM->relay && 
	     ((pSSM->relayState&AMT_SSM_STATE_JOINING) == AMT_SSM_STATE_JOINING)) ||
	    (pSSM->ssmState&AMT_SSM_STATE_SSM_JOINING) == AMT_SSM_STATE_SSM_JOINING) {
	    
	    rs[i].handle = (amt_handle_t)pSSM;
	    rs[i].rstate = 0;
	    i++;
	    pSSM->refCount++;
	}

	// check sisters
	pSister = pSSM->sister;
	while(pSister) {
	    if ((pSister->relay && 
		 ((pSister->relayState&AMT_SSM_STATE_JOINING) == AMT_SSM_STATE_JOINING)) ||
		(pSister->ssmState&AMT_SSM_STATE_SSM_JOINING) == AMT_SSM_STATE_SSM_JOINING) {
		rs[i].handle = (amt_handle_t)pSister;
		rs[i].rstate = 0;
		i++;
		pSister->refCount++;
	    }
	    pSister = pSister->next;
	}
    }
    *_size =i;
    AMT_TRACE(AMT_LEVEL_7,"#channels=%u\n", i);
    pthread_mutex_unlock(&amt_mutex);	    
}

static void releaseAllHandle(amt_read_event_t *rs, int size)
{
    int i;
    pthread_mutex_lock(&amt_mutex);	    
    for (i=0; i< size;i++) {
	releaseRef((amt_ssm_t *)rs[i].handle);
    }
    pthread_mutex_unlock(&amt_mutex);	       
}

typedef struct amt_sink_param {
    amt_recvSink_f recvSinkfunc;
    void *param;
}amt_sink_param_t;

void *amt_recv(void *p)
{
    int i, res;
    amt_sink_param_t *pParam = (amt_sink_param_t *)p;
    amt_recvSink_f recvSinkfunc = pParam->recvSinkfunc;
    void *param = pParam->param;
    u8 buf[1500];

    while(!quitRecvThread) {
	amt_read_event_t rs[AMT_MAX_CHANNEL];
	int size = AMT_MAX_CHANNEL;

	// get all handle
	getAllHandle(rs, &size);
	
	if (size) {
	    int n= poll_impl(rs, size, 500); // one second
	    if (n<0) {
		msleep(0,100);
	    }
	    if (n>0) {
		for (i=0;i<size && n>0; i++) {
		    if (rs[i].rstate==AMT_READ_IN) {
			res = recvfrom_impl((amt_ssm_t *)rs[i].handle, buf, 1500);
			n--;
			if (res>0 && recvSinkfunc) {
			    recvSinkfunc(rs[i].handle, buf, res, param);
			}
		    }
		}
	    }
	    releaseAllHandle(rs, size);
	} else {
	    msleep(0,500);
	}
    }
    if (pParam) {
	free(pParam);
	pParam=NULL;
    }
    recvThreadRunning = 0;
    return NULL;
}

int addRecvHook_impl(amt_recvSink_f recvSinkfunc, void *param)
{
    int res;
    pthread_t t;
    pthread_attr_t thread_attr;
    amt_sink_param_t *p = (amt_sink_param_t *)malloc(sizeof(amt_sink_param_t));
    AMT_ERR_CHECK(p != NULL, AMT_ERR, "no memory to pass parameters\n");
    p->recvSinkfunc = recvSinkfunc;
    p->param = param;
 
    // create a thread to handle it.
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    quitRecvThread = 0;
    recvThreadRunning=1;
    res =pthread_create(&t, &thread_attr, (void *(*)(void *)) amt_recv , p);
    if (res!=0) {
	recvThreadRunning = 0;
	free(p);
	AMT_ERR_CHECK(res == 0, AMT_ERR, "failed to create thread for receive data\n");
    }
  
    return AMT_NOERR;
}

static int amt_init_flag=0;
int init_impl(u32 anycastIP)
{
    AMT_ERR_CHECK(amt_init_flag==0, AMT_ERR, "The AMT module has been initialized\n");
    AMT_TRACE(AMT_LEVEL_8, "AMT module is initialied\n");

    // check if to create a timer
    if (amt_checkTimerRun() == AMT_TIMER_IDLE) {
	amt_createTimer(AMT_TIMER_RESOLUTION);
    }
    
    // relay
    if (pAMTRelay==NULL && anycastIP!=0 && anycastIP != (u32)-1) {
	int res;
	int  size = sizeof(amt_relay_t);
	pAMTRelay = (amt_relay_t *) malloc(size);
	AMT_ERR_CHECK(pAMTRelay!=NULL, AMT_ERR, "no memory to setup AMT\n");
	memset(pAMTRelay, 0, size);
	pAMTRelay->anycastIP = anycastIP;
	amtRelayCount++;
	res = amtSetup(pAMTRelay);
	if (res) {
	    free(pAMTRelay);
	    amtRelayCount--;
	    pAMTRelay=NULL;
	    return AMT_ERR;
 	}
    } 
    amt_init_flag=1;
    return AMT_NOERR;
}

amt_connect_state_e getState_impl(amt_ssm_t *pSSM)
{
    AMT_ERR_CHECK(pSSM!=NULL || (pSSM==NULL &&  pAMTRelay!=NULL), (amt_connect_state_e)AMT_ERR,  
		  "AMT not being connected\n"); 
    if (pSSM==NULL) {
	return(pAMTRelay->state);
    } else { 
	return (pSSM->ssmState + pSSM->relayState); 
    }
}
 
void reset_impl(void) 
{
    int i;
    amt_ssm_t *pn, *pSSM;
    amt_leave_ssm_t *plssm, *pnl ;
    int count= MIN_WAIT_TIME/100;

    // close timer
    amt_destoryTimer();
    while(count>0) {
	if (!amt_checkTimerRun()) { break;}
	msleep(0, 100);
	count--;
    }
    // quit the setup thread
    if (recvThreadRunning) {
	quitRecvThread = 1;
	for (i=0; i<50 && recvThreadRunning; i++) {
	    msleep(0, 100);
	}
   }
    
    // quit the setup thread
    if (relayConnThreadRunning) {
	quitRelayConnection = 1;
	for (i=0; i<50 && relayConnThreadRunning; i++) {
	    msleep(0, 100);
	}
    }
 
    // send optional tear-down msg
    // ... later

    // clear up chain and memory
    pSSM = pAMTChanRoot;
    while(pSSM) {
	pn = pSSM->next;
	closeChannel_impl(pSSM);
	pSSM = pn;
     }
 
    // clear up the leave chain
    plssm = pAMTLeaveChanRoot;
    while(plssm) {
	pSSM = plssm->ssm;
	free(pSSM);
	pnl = plssm->next;
	free(plssm);
	plssm = pnl;
    }
 
    // delete amt relay
    if (pAMTRelay) {
	if (pAMTRelay->sock) {
	    amt_close(&pAMTRelay->sock);
	}
	free(pAMTRelay);
	pAMTRelay = NULL;
	amtRelayCount--;
    }
 
    timerThread = 0;
    resetReq = 0;
    amtRelayCount = 0;
    quitRecvThread=0;
    recvThreadRunning=0;
    relayConnThreadRunning=0;
    quitRelayConnection=0;
    amt_init_flag = 0;

    setTraceSink_impl(AMT_DEFAULT_LEVEL, NULL);
}
 
