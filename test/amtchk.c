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
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "amt.h"

int main(int argc, char *argv[])
{
    unsigned int anycast;
    if (argc < 3 || strcmp(argv[1], "-anycast")!=0) {
	printf ("usage: %s -anycast ip\n", argv[0]);
	return 1;
    }
     
    anycast = ntohl(inet_addr(argv[2]));
    
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
  
