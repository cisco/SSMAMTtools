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
 * 07/4/2014       Duanpei Wu       email: duanpei@cisco.com         *
 *                                                                    *
 **********************************************************************/ 
#ifndef __AMT_H__
#define __AMT_H__

#define AMT_MAX_CHANNEL (32)

typedef enum {
    AMT_SSM_STATE_IDLE = 0,

    AMT_SSM_STATE_PREPARING  =0x1,
    AMT_SSM_STATE_DISCOVERING=0x3,
    AMT_SSM_STATE_REQUESTING =0x7,
    AMT_SSM_STATE_CONNECTING =0xf,
    AMT_SSM_STATE_JOINING    =0x1f,
    AMT_SSM_STATE_CONNECTED =0x3f,
    AMT_SSM_STATE_JOINED     =0x3f,

    AMT_SSM_STATE_SSM_CONNECTING =0x0f00,
    AMT_SSM_STATE_SSM_JOINING    =0x1f00,
    AMT_SSM_STATE_SSM_JOINED     =0x3f00,

    AMT_SSM_STATE_LEAVING=0x10000,    
    AMT_SSM_STATE_ERROR=0x100000,    
    AMT_SSM_STATE_END,
} amt_connect_state_e;

typedef enum {
    AMT_CONNECT_REQ_NONE  = 0,     
    AMT_CONNECT_REQ_SSM   = 1, 	/* req for ssm connection only. anycast not used. */
    AMT_CONNECT_REQ_RELAY = 2, 	/* req for amt connection only */
    AMT_CONNECT_REQ_ANY   = 3, 	/* req for either ssm or amt connection. Whichever */
    				/*  gets connected first is selected as the connection.*/
    AMT_CONNECT_REQ_END,           
} amt_connect_req_e;

typedef enum {
    AMT_READ_NONE 	= 0,
    AMT_READ_IN  	= 1,    /* there is data in buffer */
    AMT_READ_CLOSE 	= 2, 	/* amt channel close */
    AMT_READ_ERR 	= 4,	/* unexpected error */
    AMT_READ_END
} amt_read_event_e;

typedef void *amt_handle_t;
typedef struct {
    amt_handle_t       handle;
    amt_read_event_e   rstate;
} amt_read_event_t;

typedef unsigned int ipv4_t;
typedef struct { 
    unsigned char addr[16];
} ipv6_t;

/*
 * get version
 */
unsigned int  amt_getVer(void);

/*
 * init amt module
 */
int amt_init(unsigned int anycastIP);  // only one anycast IP is supported

/*
 * reset the module
 */
void amt_reset(void);

/*
 * hook for trace
 * level: 0-10 with 0: to disable all traces and 10 for enable all traces
 */
#define AMT_LEVEL_0 (0)
#define AMT_LEVEL_1 (1)
#define AMT_LEVEL_2 (2)
#define AMT_LEVEL_3 (3)
#define AMT_LEVEL_4 (4)
#define AMT_LEVEL_5 (5)
#define AMT_LEVEL_6 (6)
#define AMT_LEVEL_7 (7)
#define AMT_LEVEL_8 (8)
#define AMT_LEVEL_9 (9)
#define AMT_LEVEL_10 (10)

/*
 * set trace sink and level
 */
typedef void (*amt_traceSink_f)(int level, char *msg, int size);
int amt_setTraceSink(int level,  /* 0-10 with 0 to turn off tracing and 10 to enable the max tracing */
		     amt_traceSink_f traceFunc /* a sink function to pass the traces */
		     ); 
int amt_setTraceLevel(int level /* 0-10 with 0 to turn off tracing and 10 to enable the max tracing */
		      ); 

/*
 * open a ssm or amt channel or both to try
 * return a handle for success or NULL for failure
 */
amt_handle_t amt_openChannel(ipv4_t anycast,  	  /* not use if req for ssm only */
			     ipv4_t group,    	  /* group IP of SSM */
			     ipv4_t source,       /* source IP of SSM */
			     unsigned short port, /* the destination IP */
			     amt_connect_req_e req /* through ssm or amt or both */
			     );

/*
 * close an open channel
 */
int amt_closeChannel(amt_handle_t handle	/* the handle open with amt_openChannel() */
		     );

/*
 * poll to check if there are packets for the given handle array.
 * return n events for success with event set in rs, and -1 for failure. 
 */
int amt_poll(amt_read_event_t 	*rs,    /* points to a handle array */
	     int 	    	size, 	/* handle array size  */
	     int 	    	timeout /* in millisecond */
	     );

/*
 * retrieve a packet from the channel the "handle" points to
 * return the size of packet; -1 for failure
 */
int amt_recvfrom(amt_handle_t handle, 	/* the handle open with amt_openChannel() */
		 unsigned char *buf, 	/* buffer */
		 int maxBufSize		/* max buffer size */
		 ); 

/*
 * get the channel state for none zero handle or 
 *  relay connection state with zero handle
 */
amt_connect_state_e amt_getState(amt_handle_t handle
				 ); 

/*
 * add packet receiving sink
 */
typedef void (*amt_recvSink_f)(amt_handle_t handle, void *buf, int size, void *param);
int amt_addRecvHook(amt_recvSink_f recvSinkfuc, /* a sink function to receive packets */
		    void *param			/* a parameter to pass from the sink function */
		    ); 

#endif
