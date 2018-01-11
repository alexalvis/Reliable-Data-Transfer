#ifndef __application_H__
#define __application_H__

#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>

typedef enum { INIT, CONNECTING, CHATTING, TRANSFERING } client_state_t;
typedef enum { SERVER_INIT, SERVER_RUNNING,  GRACE_PERIOD } server_state_t;

#define PORT_1                "8080"   // the port client will be connecting to
#define PORT_2                "8081"   // the port server will be connecting to
#define SERV_HOST             "localhost"
#define BUF_MAX                256    // max size for client data
#define CLIENT_MAX             20     
#define GRACE_PERIOD_SECONDS   10     // grace period seconds for stopping the server

#define CHUNKSIZE              10000
#define PACKET_HEADER	       sizeof(int) * 3
#define FRAME_HEADER           sizeof(unsigned int) + PACKET_HEADER
#define TIMEOUT                1      // default timeout value for timer
#define MAXTRIES	       100
#define DL_BUFFER_SIZE         8000
#define FILENAME_SIZE          40
#define MSG_SIZE	       10*1024*1024
#define MSG_HEADER     	       FILENAME_SIZE + sizeof(int)


typedef enum PacketType {
	PACKET = 1,
	ACK_MSG = 2,
	TEARDOWN = 4,
} PacketType_t;

typedef enum Message_type {
	REMOTE_SHUTDOWN_MSG = 1,
	CHAT_MSG = 2,
	TRANSFER_MSG = 4,
} Message_type_t;

typedef struct Message {
	Message_type_t type;
	char filename[FILENAME_SIZE];
	char data[MSG_SIZE];
} Message_t;

typedef struct Packet
{
	int type;
	int seq_no;
	int length;
	char data[CHUNKSIZE];
} Packet_t;

typedef struct Frame {
	unsigned int checksum;
	Packet_t pkt;
} Frame_t;

typedef struct Statistics {
	unsigned int actualFrames; /* sender */
	unsigned int frameSent;   /* sender */
	unsigned int frameRetrans;  /* sender */
	unsigned int ackRecv;     /* sender */
	unsigned int bytesSent;		/*sender*/
	unsigned int filesize;     /* sender */
	unsigned int timeTaken;		/*sender*/
	unsigned int ackSent;     /* receiver */
	unsigned int dupFrameRecv;   /*receiver*/
} Stats_t;

typedef void (*timerCallback_t)(union sigval si);

char* strip(char *s);
void make_timer(timer_t * timers_head, int index, timerCallback_t callback, int timeout);
void delete_timer(timer_t *head, int index);
void reset_timer(timer_t timerid, int timeout, int interval);
int listen_connection(char * port);
int create_connection(char *hostname, char *port);
int accept_connection(int sockfd, int *fdmax, fd_set *master);
FILE* open_file(const char * input_file);
int receive_file(FILE* fp, char * filebuf, int length);
int read_file(const char * input_file, char** buffer);
unsigned short calcChecksum(Packet_t *pkt);
void write_sender_stats(const char* path);
void write_receiver_stats(const char* path);
void chat(int sockfd, const char* message);
void transfer(int sockfd, const char* filename);
void store(Message_t * msg, int bytesRead);
void help();

Stats_t g_gbnStat;


#endif 
