#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "sr.h"
#include "application.h"


static int g_windowSize;
static pthread_t g_masterThread;
static timer_t *g_timerList;
static int* g_pktAck;            // 1 for ACK, 0 for non-ACK
static int* g_sendflag;          // 1 for need resend, 0 for no need to resend 
static ackTimer_t* ackTimer;
static int g_statsPktRecv = 0;
static int g_statsPktSent = 0;

static void dummy_handler(int ingored) {}

static void handler(union sigval si) {
	int i;
        //fflush(stdout);
     // printf("si.sival_ptr is %d    \n",si.sival_ptr);
	for (i = 0; i < g_windowSize; i++) {
             //   printf("ackTimer[%d].timer_id is %d   \n",i,ackTimer[i].timer_id);
		if (ackTimer[i].timer_id == si.sival_ptr) {
			g_sendflag[i] = 1;
                        g_gbnStat.frameRetrans +=g_statsPktSent - g_statsPktRecv;
			printf("timer %d timeout, set g_sendflag[%d] to 1\n", ackTimer[i].seq_no, i);
			pthread_kill(g_masterThread, SIGALRM);
                       // ackTimer[i].timer_id = NULL;
			return;
		}
	}
}

void dequeue_flag(int* sendflag) {
	int i;
	for (i = 0; i < g_windowSize - 1; i++) {
		sendflag[i] = sendflag[i + 1];
	}
	sendflag[i] = 1;
}

void dequeue_ackTimer(ackTimer_t* ackTimer) {
	int i;
	for (i = 0; i < g_windowSize - 1; i++) {
		ackTimer[i].seq_no = ackTimer[i + 1].seq_no;
		ackTimer[i].timer_id = ackTimer[i + 1].timer_id;
	}
	ackTimer[i].seq_no = -1;
	ackTimer[i].timer_id = NULL;
}

void sr_init(int windowSize, double lossRate, double corruptionRate) {
	int i;
	g_windowSize = windowSize;
	g_masterThread = pthread_self();
	ackTimer = malloc (sizeof(ackTimer_t) * g_windowSize);
	g_timerList = malloc(sizeof(timer_t) * (g_windowSize+1));
	for (i = 0; i < g_windowSize; i++) {
		make_timer(g_timerList, i, handler, 0);
           //   make_timer(ackTimer, i, handler, 0);
                ackTimer[i].seq_no = -1;
	}
        make_timer(g_timerList,g_windowSize,handler,0);
	g_sendflag = malloc (sizeof(int) * g_windowSize);
	for (i = 0; i < g_windowSize; i++) {
		g_sendflag[i] = 1;
	}
	physical_init(lossRate, corruptionRate);
        memset(&g_gbnStat, 0, sizeof(g_gbnStat));
}

size_t sr_send(int sockfd, char* buffer, size_t length) {
	struct sigaction timerAction;	// For setting signal handler 
	int respLen;			// Size of received frame 
	int ackRecv = -1;		// number of ack received 
	int pktSent = -1;		// number of packet sent 
	int nPackets = 0;		// number of packets to send 
	int base = 0;                   // beginning index of slide window
	int bytesSent = 0;

	// Set signal handler for alarm signal
	timerAction.sa_handler = dummy_handler;
	if (sigfillset (&timerAction.sa_mask) < 0) {
		die ("sigfillset() failed");
	}
	timerAction.sa_flags = 0;
	if (sigaction (SIGALRM, &timerAction, 0) < 0) {
		die ("sigaction() failed for SIGALRM");
	}

	nPackets = length / CHUNKSIZE;
	if (length % CHUNKSIZE) {
		nPackets++;			// if it doesn't divide cleanly, need one more odd-sized packet 
	}
	g_gbnStat.actualFrames = nPackets;
	g_gbnStat.filesize = length;

	g_pktAck = malloc (sizeof (int) * nPackets);
	memset(g_pktAck, 0, sizeof(int) * nPackets);
//	clock_t start = clock(), diff;

	while ((ackRecv < nPackets-1)) {
            //    printf("enter while   \n");
		int ctr; /*window size counter */
		for (ctr = 0; ctr < g_windowSize; ctr++) {
//                      printf("g_sendflag[%d] is  %d  \n",ctr, g_sendflag[ctr]);
			if (g_sendflag[ctr] > 0) {
				g_sendflag[ctr] = 0;
				Frame_t currFrame; 
                                printf("g_pktAck[%d] is  %d   \n", base+ctr, g_pktAck[base + ctr]);
				if ((base + ctr) < nPackets && g_pktAck[base + ctr] == 0) {
					memset(&currFrame, 0, sizeof(currFrame));
                                        printf("enter if base+ctr   \n");
					/*convert */
					currFrame.pkt.type = htonl (PACKET);
					currFrame.pkt.seq_no = htonl (base + ctr);

					/*length of last packet is special */
					int currlength;
					if ((length - ((base + ctr) * CHUNKSIZE)) >= CHUNKSIZE) {
						currlength = CHUNKSIZE;
					} 
                                        else {
						currlength = length % CHUNKSIZE;
					}
					bytesSent += currlength;
					currFrame.pkt.length = htonl (currlength);

					//copy buffer data into packet 
					memcpy (currFrame.pkt.data,
							buffer + ((base + ctr) * CHUNKSIZE), currlength);
					currFrame.checksum = htonl (calcChecksum(&(currFrame.pkt)));
					//send  
					int bytes = physical_send(sockfd, &currFrame, FRAME_HEADER + currlength);
					if ( bytes < 0) {
						die ("physical_send() sent a different number of bytes than expected");
					} 
                                        else if (bytes == 0) {
						printf("drop packet %d\n", ntohl(currFrame.pkt.seq_no));
					      }
                                              else {
						printf ("SEND PACKET %d\n", base + ctr);
					      }

					// start a timer to track the packet 
					int i, j;
					for (i = 0; i <= g_windowSize; i++) {
						int found = 0;
						for (j = 0; j < g_windowSize; j++) {
							if (&(g_timerList[i]) == ackTimer[j].timer_id) {
								found = 1;
								break;
							}
						}
						if (!found) {
							break;
						}
					}
	//				if (i == g_windowSize) {            //if cannot found, then 0 will be the one can use
	//					i = 0; 
	//				}
					printf("timer[%d] is free, g_windowSize=%d\n", i, g_windowSize);
					ackTimer[ctr].timer_id = &(g_timerList[i]);
					ackTimer[ctr].seq_no = base + ctr;
					reset_timer(*(ackTimer[ctr].timer_id), TIMEOUT, 0);
					printf ("start timer[%d] for PACKET %d\n", i, base + ctr);
				} 
			} 
		} 

		// Get response 
		Frame_t currAck;
		respLen = physical_recv (sockfd, &currAck, sizeof(currAck));
		if (respLen < 0) {
			if (errno == EINTR) { /* Alarm went off  */
				printf ("timed out, more tries...\n");
			} else {
				die ("physical_recv failed");
			}
		}
		// recv() got something --  cancel the timeout 
		if (respLen) {
			int acktype = ntohl (currAck.pkt.type); // convert to host byte order
			int ackno = ntohl (currAck.pkt.seq_no);
			if (acktype == ACK_MSG) {
				printf ("RECEIVE ACK %d\n", ackno); // receive ack 
				if (g_pktAck[ackno] == 1) {
                                        printf("receive duplicate ack     \n");
					continue; // duplicate ack
				}
				// Set the corresponding Ack vector to 1 
				g_pktAck[ackno] = 1;
				//cancel timer of this packet
				int i;
				for (i = 0; i < g_windowSize; i++) {
                                      //  printf("enter for    \n");
					if (ackTimer[i].seq_no == ackno) {
                                             //   printf("enter if    \n");
						reset_timer(*(ackTimer[i].timer_id), 0, 0);
                                             //   printf("enter reset    \n");
						int j;
						for (j = 0; j < g_windowSize; j++) {
							if (*(ackTimer[i].timer_id) == g_timerList[j]) {
								printf ("cancel timer[%d] for PACKET %d (slide window: %d)\n", j, ackno, base);
                                                          //    printf ("i is %d   \n", i);
							}
						}
					}
				}
                                //printf("leave for    \n");
				if (ackno == base) {
                                     // printf("11111    \n");
					int k;
                                     // printf("22222    \n");
					for (k = base; k < nPackets; k++) {
						if (g_pktAck[k] == 1) {
							printf("g_pktAck[%d]=1  \n", k);
							base++;
                                                     // printf("enter dequeue   \n");
							dequeue_flag(g_sendflag);
							dequeue_ackTimer(ackTimer);
						} else {
							break;
						}
					}
				} else if (ackno > base) {
                                  //    printf("33333    \n");
					g_sendflag[(ackno-base) % g_windowSize] = 0;
					int k;
					for (k = 0; k < g_windowSize; k++) {
						if (ackTimer[k].seq_no == ackno) {
							ackTimer[k].timer_id = NULL;
							ackTimer[k].seq_no = -1;
						}
					}
                                       // printf("leave for  333  \n");

				}
				ackRecv++;
                          //    printf ("ackRecv is %d    \n", ackRecv);
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
		while (1) {
			int bytesSent = physical_send (sockfd, &teardown, sizeof(teardown));
			printf("SEND TEARDOWN\n");
			if (bytesSent == sizeof(teardown)) {
				printf("teardown msg reach\n");
				break;
			}
		}
	}


	return bytesSent;
}

size_t sr_recv(int sockfd, char* buffer) {
	int recvMsgSize = 0;		// Size of received message 
	int packet_rcvd = -1;		// highest packet successfully received 
	struct sigaction myAction;

	while (1) {
		Frame_t currFrame;
		memset(&currFrame, 0, sizeof(currFrame));
		if (physical_recv (sockfd, &currFrame, sizeof (currFrame)) == 0) {
			continue; // corruption detected
		}

		currFrame.pkt.type = ntohl (currFrame.pkt.type);
		currFrame.pkt.length = ntohl (currFrame.pkt.length); // convert 
		currFrame.pkt.seq_no = ntohl (currFrame.pkt.seq_no);
		if (currFrame.pkt.type == TEARDOWN) 
		{
			// assume tear-down message is always received
			printf("RECEIVE TEARDOWN\n");
			return recvMsgSize;
		} else if (currFrame.pkt.type == PACKET) {
			printf ("RECEIVE PACKET %d length %d\n", currFrame.pkt.seq_no, currFrame.pkt.length);
			int buff_offset = CHUNKSIZE * currFrame.pkt.seq_no;
			memcpy (&buffer[buff_offset], currFrame.pkt.data, // copy packet data to buffer
					currFrame.pkt.length);
			recvMsgSize += currFrame.pkt.length;

			Frame_t currAck;
			currAck.pkt.type = htonl (ACK_MSG); //convert to network byte order 
			currAck.pkt.seq_no = htonl (currFrame.pkt.seq_no);
			currAck.pkt.length = htonl(0);
			currAck.checksum = htonl(calcChecksum(&(currAck.pkt)));
			int bytes = physical_send (sockfd, &currAck, sizeof(currAck));
			if ( bytes < 0) {
				die ("physical_send() sent a different number of bytes than expected");
			} else if (bytes == 0) {
				printf("ack msg %d loss\n", currFrame.pkt.seq_no);
			} else {
				printf ("SEND ACK %d\n", currFrame.pkt.seq_no);
			}
		}
                else {
                    printf("wrong type     \n");
                }
	}

	return recvMsgSize;
}
