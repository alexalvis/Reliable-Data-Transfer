#include <string.h>
#include <stdio.h>

#include <signal.h>		// for sigaction() 
#include <sys/socket.h>		// for socket(), connect(), sendto(), and recvfrom() 
#include <arpa/inet.h>		// for sockaddr_in and inet_addr() 
#include <stdlib.h>		// for atoi() and exit() 
#include <unistd.h>		// for close() and alarm() 
#include <errno.h>		// for errno and EINTR 
#include <time.h>		// for gettime()

#include "datalink.h"
#include "application.h"
#include "gbn.h"
#include "sr.h"

static Object g_datalink;

void datalink_init(char * protocol, int windowSize, double lossRate, double corruptionRate) {
    if (strcmp(protocol, "gbn") == 0) {
    	gbn_init(windowSize, lossRate, corruptionRate);
    	g_datalink.send = &gbn_send;
    	g_datalink.recv = &gbn_recv;
    } else if (strcmp(protocol, "sr") == 0) {
    	sr_init(windowSize, lossRate, corruptionRate);
    	g_datalink.send = &sr_send;
    	g_datalink.recv = &sr_recv;
    } else {
    	printf("Unrecognized transfer protocol. \n");
    	exit(1);
    }
    printf("protocol is %s, window_size is %d, loss_rate is %lf corruption_rate is %lf\n",
    			protocol, windowSize, lossRate, corruptionRate);
}

size_t datalink_send(int sockfd, char* buf, size_t length) {
        printf("g_Datalink.send buf is : %p   \n", buf);
        printf("g_Datalink.send length is : %d   \n", length);
	return (g_datalink.send)(sockfd, buf, length);
}

size_t datalink_recv(int sockfd, char* buf) {
        printf("g_datalink.recv buf is: %p   \n", buf);
        printf("g_datalink.recv  \n");
	return (g_datalink.recv)(sockfd, buf);
}
