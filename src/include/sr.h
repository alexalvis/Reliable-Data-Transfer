#ifndef SR_H_
#define SR_H_

#include <stdlib.h>

typedef struct ackTimer_t {
	int seq_no;
	timer_t *timer_id;
} ackTimer_t;

void sr_init(int windowSize, double lossRate, double corruptionRate);
size_t sr_send(int sockfd, char* buf, size_t length);
size_t sr_recv(int sockfd, char* buf);

#endif 
