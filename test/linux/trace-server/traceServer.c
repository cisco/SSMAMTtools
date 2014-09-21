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
 
int main(int argc, char *argv[])
{
    ex_sock_t *pTraceSock=NULL;
    unsigned short dstPort=20000;
    if (argc>1) {
	if (strcmp(argv[1], "-p")==0) {
            if (argc>2)  { dstPort = atol(argv[2]); }
	} else {
	    printf("usage: %s [-p port] .....default=20000\n", argv[0]);
	    return 1;
	}
    }

    printf("port =%u\n", dstPort);
 
    // create a UDP to receive the trace data
    pTraceSock = ex_makeUDPSock(0, &dstPort,0,0);
    if (pTraceSock==NULL) {
	printf("Failed to make a UDP socket\n");
	return 1;
    }

    // listen to the port
    while(1) {
	char buf[1500];
	int res = recvfrom(pTraceSock->sockServ, buf, 1500, 0, NULL, NULL);
	if (res>0) {
	    buf[res] = 0;
	    printf("%s", buf);
	}
    }
    
    return 0;
}
