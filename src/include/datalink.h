#ifndef DATALINK_H_
#define DATALINK_H_

#include <stdlib.h>
#include <semaphore.h>

typedef struct {
    size_t (*send)(int sockfd, char* buf, size_t length);
    size_t (*recv)(int sockfd, char* buf);
} Object;

void datalink_init(char * protocol, int windowSize, double lossRate, double corruptionRate);
size_t datalink_send(int sockfd, char* buf, size_t length);
size_t datalink_recv(int sockfd, char* buf);

#endif 
