#include <stdio.h>			
#include <stdlib.h>			
#include <string.h>			
#include <unistd.h>			
#include <errno.h>			
#include <signal.h>			
#include <semaphore.h>                 
#include <pthread.h>	             
#include <time.h>			

#include "gbn.h"
#include "application.h"
#include "physical.h"

static int g_windowSize;
static timer_t *g_timerList = NULL;

static int g_statsPktRecv = 0;
static int g_statsPktSent = 0;
static pthread_t g_masterThread;

static int g_tries = 0;			// Count of times sent 
static int g_sendflag = 1;



void dummy_handler(int ingored) {
    g_tries += 1;
    g_sendflag = 1;
}

void die (char *errorMessage) {
	perror (errorMessage);
	exit (1);
}

int max (int a, int b) {
	return (b > a) ? b : a;
}

int min(int a, int b) {
	return (b > a) ? a : b;
}

static void handler(union sigval si) {
	int i;
//      printf("Enter Handler.    \n");
	for (i = 0; i < g_windowSize; i++) {
		if (&(g_timerList[i]) == si.sival_ptr) {
			g_tries += 1;
			g_sendflag = 1;
			g_gbnStat.frameRetrans += g_statsPktSent - g_statsPktRecv;
			pthread_kill(g_masterThread, SIGALRM);
			return;
		}
	}
}

void gbn_init(int windowSize, double lossRate, double corruptionRate) {
	g_windowSize = windowSize;
	g_masterThread = pthread_self();
	// create timer list, although GBN only uses one
	g_timerList = malloc(sizeof(timer_t) * g_windowSize);
	make_timer(g_timerList, 0, handler, 0);

	physical_init(lossRate, corruptionRate);
	memset(&g_gbnStat, 0, sizeof(g_gbnStat));
}     




size_t gbn_send(int sockfd, char* buffer, size_t length) {
	struct sigaction timerAction;	// For setting signal handler 
	int respLen;			// Size of received frame 
	int pktRecv = -1;	        // highest ack received 
	int pktSent = -1;		// highest packet sent
	int nPackets = 0;		// number of packets need to send
	int base = 0;
	int bytesSent = 0;

	nPackets = length / CHUNKSIZE;
	if (length % CHUNKSIZE) {
		nPackets++;			
	}
	g_gbnStat.actualFrames = nPackets;
	g_gbnStat.filesize = length;

	timerAction.sa_handler = dummy_handler;
	if (sigfillset (&timerAction.sa_mask) < 0) {
		die ("sigfillset() failed");
	}
	timerAction.sa_flags = 0;
	if (sigaction (SIGALRM, &timerAction, 0) < 0) {
		die ("sigaction() failed for SIGALRM");
	}

	clock_t start = clock(), diff;
	// Send the string to the server 
	while ((pktRecv < nPackets-1) && (g_tries < MAXTRIES)) {

		if (g_sendflag > 0)	{
                        //printf("neng jin if    \n");
			g_sendflag = 0;
			int ctr; 
			for (ctr = 0; ctr < g_windowSize; ctr++) {
				pktSent = min(max (base + ctr, pktSent),nPackets-1);
				Frame_t currFrame;
				if ((base + ctr) < nPackets) {
					memset(&currFrame,0,sizeof(currFrame));
					printf ("SEND PACKET %d packet_sent %d packet_received %d\n",
							base+ctr, pktSent, pktRecv);

					//convert
					currFrame.pkt.type = htonl (PACKET);
					currFrame.pkt.seq_no = htonl (base + ctr);
					int currlength;
					if ((length - ((base + ctr) * CHUNKSIZE)) >= CHUNKSIZE)
						currlength = CHUNKSIZE;
					else
						currlength = length % CHUNKSIZE;
					bytesSent += currlength;
					g_gbnStat.bytesSent += currlength;
					currFrame.pkt.length = htonl (currlength);

					memcpy (currFrame.pkt.data, //copy buffer into packet
							buffer + ((base + ctr) * CHUNKSIZE), currlength);
					currFrame.checksum = htonl (calcChecksum(&(currFrame.pkt)));
					int bytes = physical_send(sockfd, &currFrame, FRAME_HEADER + currlength);
					if ( bytes < 0) {
						perror
						("physical_send() sent a different number of bytes than expected");
					} else if (bytes == 0) {
						printf("drop packet %d\n", ntohl(currFrame.pkt.seq_no));
					}
				}
			}
		}
                alarm(TIMEOUT);
		Frame_t currAck;
		respLen = physical_recv (sockfd, &currAck, sizeof(currAck));
		while (respLen < 0) {
			if (errno == EINTR) { // Alarm went off
				if (g_tries < MAXTRIES)	{ 
					printf ("timed out, %d more tries...\n", MAXTRIES - g_tries);
					break;
				} else {
					perror ("No Response");
					break;
				}
			} else {
				perror ("physical_recv failed");
			}
		}

		
		if (respLen) {
			int acktype = ntohl (currAck.pkt.type); // convert
			int ackno = ntohl (currAck.pkt.seq_no);
			unsigned int checksum = ntohl (currAck.checksum);
			if (ackno > pktRecv && acktype == ACK_MSG) {
				printf ("RECEIVE IN-ORDER ACK %d\n", ackno); 
				pktRecv = ackno;
				base = pktRecv + 1; 
				g_statsPktRecv = pktRecv;
				g_statsPktSent = pktSent;
				if (pktRecv == pktSent) { 
					alarm (0); // clear alarm 
					g_tries = 0;
					g_sendflag = 1;
				}
				else { 
					g_tries = 0; // reset
					g_sendflag = 0;
                                        alarm(TIMEOUT);
                       
				}
			}
		}
	}
	int ctr;
	for (ctr = 0; ctr < 1; ctr++) 
	{
		Frame_t teardown;
		teardown.pkt.type = htonl (TEARDOWN);
		teardown.pkt.seq_no = htonl (0);
		teardown.pkt.length = htonl (0);
		teardown.checksum = htonl (calcChecksum(&(teardown.pkt)));
		int count = 100;
		while (count > 0) {
			int bytesSent = physical_send (sockfd, &teardown, sizeof(teardown));
			printf("send out teardown msg\n");
			if (bytesSent == sizeof(teardown)) {
				printf("teardown msg reach\n");
				break;
			}
			count--;
		}
	}
	diff = clock() - start;
	int msec = diff * 1000 / CLOCKS_PER_SEC;
	g_gbnStat.timeTaken = msec;

	return bytesSent;
}


size_t gbn_recv(int sockfd, char* buffer) {
	int recvMsgSize = 0;		
	int packet_rcvd = -1;		
	struct sigaction myAction;
	while (1) {
		Frame_t currFrame;
		memset(&currFrame, 0, sizeof(currFrame));
		if (physical_recv (sockfd, &currFrame, sizeof (currFrame)) == 0) {
			continue; // corruption detected
		}

		currFrame.pkt.type = ntohl (currFrame.pkt.type);
		currFrame.pkt.length = ntohl (currFrame.pkt.length); //convert 
		currFrame.pkt.seq_no = ntohl (currFrame.pkt.seq_no);
		if (currFrame.pkt.type == TEARDOWN) 
		{
			// assume tear-down message is always received
			printf("RECEIVE TEARDOWN\n");
			return recvMsgSize;
		} else if (currFrame.pkt.type == PACKET) {
			printf ("RECEIVE PACKET %d length %d\n", currFrame.pkt.seq_no, currFrame.pkt.length);

			// Send ack and store in buffer 
			if (currFrame.pkt.seq_no == packet_rcvd + 1) {
				packet_rcvd++;
				int buff_offset = CHUNKSIZE * currFrame.pkt.seq_no;
                                //printf("buff_offset is : %d   \n", buff_offset);
				memcpy (&buffer[buff_offset], currFrame.pkt.data, currFrame.pkt.length);
                                //printf("In gbn_recv, currframe.pkt.data is %d   \n", currFrame.pkt.data[0]);
                                //printf("in gbn_recv, buffer is : %p   \n", buffer);
				recvMsgSize += currFrame.pkt.length;
			} 
                        else {
				g_gbnStat.dupFrameRecv++;
			}
			printf ("SEND ACK %d\n", packet_rcvd);
			Frame_t currAck;
			currAck.pkt.type = htonl (ACK_MSG); //convert
			currAck.pkt.seq_no = htonl (packet_rcvd);
			currAck.pkt.length = htonl(0);
			currAck.checksum = htonl(calcChecksum(&(currAck.pkt)));
			if (physical_send (sockfd, &currAck, sizeof(currAck)) != sizeof(currAck)) {
				printf("ack msg %d loss\n", packet_rcvd);
			}
			g_gbnStat.ackSent++;
		}
	}

	return recvMsgSize;
}


