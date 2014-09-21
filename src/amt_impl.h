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
#ifndef __AMT_IMPL_H__
#define __AMT_IMPL_H__
#include "amt_sock.h"

#define AMT_MAGIC_CODE (0xfeedbee1)  // feedbee ....
#define AMT_ANYCAST_PORT      (2268)
#define AMT_RETRY_MAX         (30)
#define AMT_RELAY_TIMEOUT     (120000)
#define AMT_SSM_COM           (0xe0000016)
#define AMT_BUF_SIZE          (1500)
#define AMT_MIN_INTEVAL_RETRY (1000) // 1000 ms
#define AMT_MAX_RELAY_CONNECT_TIME (120000) // 120 seconds
#define AMT_MAX_RESPONSE_WAIT_TIME (1000)
#define AMT_MIN_LEAVE_RETRY   (3)
#define AMT_KEEPALIVE_COUNT   (7)	// 1-128 seconds
#define AMT_MAX_KEEP_ALIVE_TIME (30000) // 30 seconds
#define AMT_MAX_RETRY_JOIN_TIME (5000) // 5 seconds
#define AMT_TIME_RESOLUTION	(10) // 10 ms 
#define AMT_DEFAULT_REFLUSH_INTERVAL (125) //RFC3376, section 8.2 in second
#define AMT_MIN_DEFAULT_REFLUSH_INTERVAL (5)  // in second
#define AMT_MIN_DEFAULT_REQ_RETRY (10000)
#define AMT_DEFAULT_RETRY 	(2)
#define AMT_MAX_DEFAULT_KEEP_ALIVE (500)

#define AMT_IGMPV3_MEMBERSHIP_QUERY_TYPEID (0x11)
#define AMT_IGMPV3_MEMBERSHIP_REPORT_TYPEID (0x22)
#define AMT_IP_IGMP_PROTOCOL 		(2)

#define AMT_DISCOVERY_TYPEID 		(1)
#define AMT_RELAY_ADV_TYPEID 	        (2)
#define AMT_REQUEST_TYPEID   		(3)
#define AMT_MEMBERSHIP_QUERY_TYPEID 	(4)
#define AMT_MEMBERSHIPP_UPDATE_TYPEID 	(5)
#define AMT_MULTICAT_DATA_TYPEID 	(6)

#define AMT_GROUP_RECORD_MODE_IS_INCLUDE (1)
#define AMT_GROUP_RECORD_MODE_IS_EXCLUDE (2)
#define AMT_GROUP_RECORD_CHANGE_TO_INCLUDE_MODE (3)
#define AMT_GROUP_RECORD_CHANGE_TO_EXCLUDE_MODE (4)
#define AMT_GROUP_RECORD_ALLOW_NEW_SOURCES (5)
#define AMT_GROUP_RECORD_BLOCK_OLD_SOURCES (6)

#pragma pack(push, 1) 

typedef struct _amt_msg_hdr {
    u8 type;
} amt_msg_hdr_t;

typedef struct _amt_discovry {
    u8  type;
    u8  resv;
    u16 resv2;
    u32 nonce;
}amt_discovery_t;

typedef struct _amt_relay_adv {
    u8  type;
    u8  resv;
    u16 resv2;
    u32 nonce;
    u32 relayIP[4];
}amt_relay_adv_t;

typedef struct _amt_ip_alert {
    u8  ver_ihl;
    u8	tos;
    u16 tot_len;
    u16 id;
    u16 frag_off;
    u8  ttl;
    u8  protocol;
    u16 check;
    u32 saddr;
    u32 daddr;
    u32 alert;    
}amt_ip_alert_t;

typedef struct _amt_ip {
    u8  ver_ihl;
    u8	tos;
    u16 tot_len;
    u16 id;
    u16 frag_off;
    u8  ttl;
    u8  protocol;
    u16 check;
    u32 saddr;
    u32 daddr;
}amt_ip_t;

typedef struct _amt_udpHeader{
    u16 srcPort;
    u16 dstPort;
    u16 len;
    u16 check;
}amt_udpHeader_t;

typedef struct _amt_ipv6{
    u8  ver:4,		// =6
	ihl:4;
    u8	dump[39];
}amt_ipv6_t;

typedef struct _amt_igmpv3_membership_query {
    u8  type;
    u8  max_resp_code; //in 100ms, Max Resp Time = (mant | 0x10) << (exp + 3)
    u16 checksum;
    u32 ssmIP;
    u8  s_qrv;
    u8  qqic; //in second, query Time = (mant | 0x10) << (exp + 3)
    u16 nSrc;
    u32 srcIP[1];
} amt_igmpv3_membership_query_t;

typedef struct _amt_igmpv3_groupRecord {
    u8 	type;
    u8 	auxDatalen;
    u16 nSrc;
    u32 ssm;
    u32 srcIP[1];
} amt_igmpv3_groupRecord_t;

typedef struct _amt_igmpv3_membership_report {
    u8  type;
    u8  resv;  
    u16 checksum;
    u16 resv2;
    u16 nGroupRecord;
    amt_igmpv3_groupRecord_t grp[1];
} amt_igmpv3_membership_report_t;

typedef struct _amt_mldv2_listener_query {
} amt_mldv2_listener_query_t;

typedef struct _amt_mldv2_listener_report {
} amt_mldv2_listener_report_t;

typedef struct _amt_membership_query {
    u8  type;
    u8  l_g;
    u16 mac_h;
    u32 mac_l;
    u32 nonce;
    amt_ip_alert_t ip;
    amt_igmpv3_membership_query_t mq;
    u16 gport;
    u32 gIP;
} amt_membership_query_t;

typedef struct _amt_membership_query_ipv6 {
    u8  type;
    u8  l_g;
    u16 mac_h;
    u32 mac_l;
    u32 nonce;
    amt_ipv6_t ip;
    amt_mldv2_listener_query_t mld;
    u16 gport;
    u32 gIP[4];
} amt_membership_query_ipv6_t;


typedef struct _amt_request {
    u8  type;
    u8  p;
    u16 resv2;
    u32 nonce;
}amt_request_t;

typedef struct _amt_membership_update {
    u8  type; 
    u8  resv;
    u16 mac_h;
    u32 mac_l;
    u32 nonce;
    amt_ip_alert_t ip;
    amt_igmpv3_membership_report_t mr;
} amt_membership_update_t;

typedef struct _amt_membership_update_ipv6 {
    u8  type;
    u8  resv;
    u16 mac_h;
    u32 mac_l;
    u32 nonce;
    amt_ipv6_t ip;
    amt_mldv2_listener_report_t mld;
} amt_membership_udpate_ipv6_t;

typedef struct _amt_multicast_data {
    u8       		type;
    u8  		resv;
    amt_ip_t 		ip;
    amt_udpHeader_t 	udp;
    u8  		buf[1];
} amt_multicast_data_t;

struct _amt_ssm;
typedef struct _amt_relay {
    struct _amt_relay *next;
    struct _amt_relay *prev;
    int refCount;

    amt_sock_t *sock;

    u32 anycastIP;
    u32 relayIP;
    u32 natSrcIP;
    u16 natUdpPort;

    // msg and records
    u32 nonce;		// pre-recorded nonce
    u32 mac_l;		// pre-record nonce
    u16 mac_h;		
    u16 relay_full;	// limit flag in membership query
    u16 minretry;	// from query message, qrv
    u16 qqic;		// in second
    u32 flushTime;     // from qqic
    int reflush;	// to reflush
    u32 prevFlushtime;

    u32 expiredTime;
    u32 retryCount;      
    amt_timer_handle reqTimerID;
    
    // socket state and buffer
    u8  buf[AMT_BUF_SIZE];
    u32 bufSize;
    struct _amt_ssm *pSSM;  // data for pSSM

    amt_connect_state_e state;
} amt_relay_t;

typedef struct _amt_ssm {
    struct _amt_ssm *next;
    struct _amt_ssm *prev;
    struct _amt_ssm *sister;
    int refCount;

    u32  magicCode;
    s32  active;

    // ssm
    amt_sock_t *sock;
    amt_relay_t *relay;
    u32 srcIP;
    u16 udpPort;
    u32 groupIP;

    // retry 
    u32 expiredTime;
    u32 retryCount;
    int reflush;

    // state
    amt_connect_state_e relayState;
    amt_connect_state_e ssmState;
    amt_connect_req_e   req;
    
    // socket state and buffer
    u8  buf[AMT_BUF_SIZE];
    u32 bufSize;
    struct _amt_ssm *pSSM;
    
    // working variable
    int mark;
} amt_ssm_t;

typedef struct _amt_leave_ssm {
    struct _amt_leave_ssm *next;
    struct _amt_leave_ssm *prev;
    int refCount;
    
    amt_ssm_t *ssm;
} amt_leave_ssm_t;

typedef struct _amt_ssm_index {
    amt_ssm_t *pSSM;
    void      *bPointer[AMT_MAX_CHANNEL];
} amt_ssm_index_t;

amt_ssm_t *getSSM(u32 source, u32 group, u16 port,  amt_ssm_t **pSister);
amt_ssm_t *openChannel_impl(u32 anycast, u32 group, 
			    u32 source, u16 port,
			    amt_connect_req_e req);
int poll_impl(amt_read_event_t *rs, int size, int timeout);
int recvfrom_impl(amt_ssm_t *pSSM, u8 *buf, int size);
int closeChannel_impl(amt_ssm_t *pSSM);
void reset_impl(void);
int init_impl(u32 anycastIP);
amt_connect_state_e getState_impl(amt_ssm_t *pSSM);
int addRecvHook_impl(amt_recvSink_f recvSinkfuc, void *param);

// trace
void setTraceSink_impl(int level,  amt_traceSink_f traceFunc);
void setTraceLevel_impl(int level);

#endif
