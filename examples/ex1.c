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
    
    // call the initial function. Do nothing for now
    amt_init(anycast); 
    while((amt_getState(NULL) & AMT_SSM_STATE_CONNECTED) != AMT_SSM_STATE_CONNECTED) {
	sleep(1);
    }
    amt_reset();
    
    return 0;
}
 
