#include <stdio.h>
#include "amt.h"

static void packetRecv(amt_handle_t handle,  void *_buf,  int size,  void *param)
{
    printf("ex3: receive packet with len = %u from channel:%p\n", size, handle);
}
 
int main(int argc, char *argv[])
{
    unsigned char   buf[1500];
    unsigned int group = ntohl(inet_addr("232.10.10.10"));
    unsigned int srcIP =  ntohl(inet_addr("192.168.0.100"));
    amt_handle_t channelID;
    unsigned int anycast;
    if (argc < 2) {
	printf ("usage: %s anycast\n", argv[0]);
	return 1;
    }
    anycast = ntohl(inet_addr(argv[1]));
    
    // set trace level 
    //amt_setTraceLevel(AMT_LEVEL_10); 
    
    // call the initial function. 
    amt_init(anycast); 

    // add a callback function to receive all packets
    amt_addRecvHook(packetRecv, NULL);

    // open a channel
    channelID = amt_openChannel(anycast, group, srcIP, 
				9010, AMT_CONNECT_REQ_RELAY);
    
    pause();
    amt_reset();
    
    return 0;
}
 
