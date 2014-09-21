#include <stdio.h>
#include "amt.h"

int main(int argc, char *argv[])
{
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
    while((amt_getState(NULL) & AMT_SSM_STATE_CONNECTED) 
	  != AMT_SSM_STATE_CONNECTED) {
	sleep(1);
    }

    // subscribe a AMT channel
    {
	unsigned char   buf[1500];
	unsigned int group = ntohl(inet_addr("232.10.10.10"));
	unsigned int srcIP =  ntohl(inet_addr("209.197.204.73"));
	amt_handle_t channelID = amt_openChannel(anycast, group, srcIP, 
						 9010, AMT_CONNECT_REQ_RELAY);
	amt_read_event_t rs={channelID,0};
	while(1) {
	    int n =  amt_poll(&rs,  1, 2000);
	    if (n>0) {
		int  len = amt_recvfrom(channelID, buf, 1500);
		if (len > 0) {
		    printf("ex2:received packet with len = %u\n", len);
		} else {
		    printf("Oh, get errors with amt_recvfrom() !!, Exit\n");
		    return 1;
		}
	    } else if (n<0) {
		printf("Oh, get errors with amt_poll()!!, Exit\n");
		return 1;
	    }
	} 
    }
    amt_reset();
    
    return 0;
}
 
