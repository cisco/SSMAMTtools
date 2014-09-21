#include <stdio.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "amt.h"
#include "ex_sock.h"

#define _EMPTY
static void packetRecv(amt_handle_t handle,  void *_buf,  int size,  void *param)
{
    unsigned char *buf = (unsigned char *)_buf;
    unsigned int *renderIP = (unsigned int *)param;
    static ex_sock_t *pSock=NULL;
    unsigned short sport=0;
    
    if (pSock == NULL) { // to the render
	sport=0;
	pSock = ex_makeUDPSock(0, &sport, *renderIP, 9010);
	EX_ERR_CHECK(pSock != NULL, " ", _EMPTY);
    }
    ex_sendPacket(pSock,  buf, size);
}
 
int main(int argc, char *argv[])
{
    unsigned char   buf[1500];
    unsigned int group = ntohl(inet_addr("232.200.0.1"));
    unsigned int srcIP =  ntohl(inet_addr("209.197.204.73"));
    amt_handle_t channelID;
    unsigned int anycast, renderIP;
    if (argc < 3) {
	printf ("usage: %s anycast renderIP\n", argv[0]);
	return 1;
    }
    anycast = ntohl(inet_addr(argv[1]));
    renderIP = ntohl(inet_addr(argv[2]));
    
    // set trace level 
    //amt_setTraceLevel(AMT_LEVEL_10); 
    
    // call the initial function. 
    amt_init(anycast); 

    // add a callback function to receive all packets
    amt_addRecvHook(packetRecv, &renderIP);

    // open a channel
    channelID = amt_openChannel(anycast, group, srcIP, 
				9010, AMT_CONNECT_REQ_RELAY);
    
    pause();
    amt_reset();
    
    return 0;
}
 
