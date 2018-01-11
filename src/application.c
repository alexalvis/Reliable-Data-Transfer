#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>

#include "application.h"
#include "datalink.h"
#include "control.h"


char* strip(char *s) {
    size_t size;
    char *end;
    size = strlen(s);
    if (!size) return s;
    end = s + size - 1;
    while (end >= s && isspace(*end)) {
    	end--;
    }
    *(end + 1) = '\0';
    while (*s && isspace(*s)) {
    	s++;
    }
    return s;
}

void make_timer(timer_t * head, int index, timerCallback_t callback, int timeout) {
	struct sigevent sev;
	struct itimerspec its;
        //printf("Enter make_timer    \n");
        memset(&sev,0,sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = &(head[index]);
	sev.sigev_notify_function = callback;
        sev.sigev_notify_attributes = 0;
	timer_create(CLOCK_REALTIME, &sev, &(head[index]));

	// Start the timer 
	its.it_value.tv_sec = timeout;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;

	timer_settime(head[index], 0, &its, NULL);
        //timer_gettime(head[index], &its);
        //printf("its.it_value is : %d   \n", its.it_value);
}


void delete_timer(timer_t *head, int index) {
	if (timer_delete(head[index]) == -1) {
		perror("delete_timer");
		exit(1);
	}
}


void reset_timer(timer_t timerid, int timeout, int interval) {
       // printf("start the timer    \n");
	struct itimerspec its;
	// Start the timer 
        
	its.it_value.tv_sec = timeout;  /* Initial expiration */
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = interval;     /* Timer interval */
	its.it_interval.tv_nsec = 0;
       //  printf("reset_timer  aaa   \n");
	if (timer_settime(timerid, CLOCK_REALTIME, &its, NULL) == -1) {
		perror("reset_timer()");
		exit(1);
	}
}

int listen_connection(char * port) {
	struct addrinfo hints, *servinfo, *p;
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	int rv;
	int yes=1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, CLIENT_MAX) == -1) {
		perror("listen");
		exit(1);
	}

	return sockfd;
}


void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int create_connection(char *hostname, char *port) {
	int sockfd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	fd_set fdset;
	struct timeval tv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        printf("failed to connect server\n");
        return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

    freeaddrinfo(servinfo); 
	return sockfd;

}

// return 0 if connection sets up, otherwise return -1 
int accept_connection(int sockfd, int *fdmax, fd_set *master) {
    int new_fd;
	socklen_t addrlen;
	struct sockaddr_storage their_addr; // connector's address information
	char remoteIP[INET6_ADDRSTRLEN];

	addrlen = sizeof their_addr;
	new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
					&addrlen);
	if (new_fd == -1) {
		perror("accept() fails");
		return -1;
	}

	FD_SET(new_fd, master); 
	if (new_fd > *fdmax) {
		*fdmax = new_fd; // keep track of the max
	}
	inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			remoteIP, sizeof remoteIP);
	printf("selectserver: new connection from %s on "
		   "socket %d\n", remoteIP, new_fd);

	return new_fd;
}

FILE* open_file(const char * input_file) {

	int bytesReceived = 0;
	char recvBuff[BUF_MAX + 1];

	char filepath[BUF_MAX];
	//sprintf(filepath, "/recv%s", input_file);
        //printf("file path is: %s",filepath);
        strcpy(filepath,"/home/mhx/4/recv/test.jpg");
	FILE* fp = fopen(filepath, "wb");
	if (fp == NULL) {
		printf("Error opening file  \n");
	}
        printf("open_file is ok  \n");
	return fp;
}

int receive_file(FILE* fp, char * filebuf, int length) {
	// Receive data in chunks of 10000 bytes
        printf("file buff is : %p   \n",filebuf);
	if (filebuf) {
                //printf("receive_file 1 \n");
		fwrite(filebuf, 1, length, fp);
                //printf("receive_file 2 \n");
		fclose(fp);
                //printf("receive_file 3 \n");
	} else {
		printf("\n Read Error \n");
		return -1;
	}
        printf("receive_file is ok  \n");
	return 0;
}

// sends the file given the path name 
int read_file(const char * input_file, char** buffer) {
	struct stat st;
	stat(input_file, &st);
	int size = st.st_size; // size in bytes

	if (size > 100000000) {
                //printf("%s file size = %d \n",input_file,size);
		printf("Size of file > 100 MB. Must send a smaller file.");
		return -1;
	}

	*buffer = malloc (size);
	FILE *fp = fopen(input_file, "rb");
        
	if (fp == NULL) {
                printf("file name is %s , file size = %d \n", input_file, size);
		printf("File open error");
		return -1;
	}
	int nread = fread(*buffer, 1, size, fp);
        printf("client transfer file buff is %p:  \n", buffer);
	fclose(fp);

	return nread;
}

// checksum function
unsigned short calcChecksum(Packet_t *pkt) {
	unsigned short * buffer = (unsigned short *)pkt;
	unsigned long sum = 0;

	int size = sizeof(*pkt);
	while (size > 1) {
		sum += *buffer++;
		size -= sizeof(unsigned short);
	}

	if (size > 0) {
		sum += *(unsigned char *)buffer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (unsigned short)(~sum);
}

void write_sender_stats(const char* path) {
	FILE *fp = fopen(path, "a");
	if (!fp) {
		perror("create stat file fails");
		return;
	}
	fprintf(fp, "The number of frames need to transmitted: %d\n", g_gbnStat.actualFrames);
	fprintf(fp, "The number of data frames transmitted: %d\n", g_gbnStat.frameSent);
	fprintf(fp, "The number of retransmissions: %d\n", g_gbnStat.frameRetrans);
	fprintf(fp, "The number of acknowledgements received: %d\n", g_gbnStat.ackRecv);
	fprintf(fp, "The file size: %d\n", g_gbnStat.filesize);
	fprintf(fp, "The total number of bytes sent: %d\n", g_gbnStat.bytesSent);
	fclose(fp);
}

void write_receiver_stats(const char* path) {
	FILE *fp = fopen(path, "a");
	if (!fp) {
		perror("create stat file fails");
		return;
	}
	fprintf(fp, "The total number of acknowledgements sent: %d\n", g_gbnStat.ackSent);
	fprintf(fp, "The total number of duplicate frames received: %d\n", g_gbnStat.dupFrameRecv);
	fclose(fp);
}



void chat(int sockfd, const char* message) {
	Message_t * msg = malloc(sizeof(Message_t));
	msg->type = CHAT_MSG;
	strncpy(msg->data, message, strlen(message));

	if (datalink_send(sockfd, (char *)msg, MSG_HEADER + strlen(message)) < 0) {
		perror("send chat message fails");
	}
	free(msg);
}

void transfer(int sockfd, const char* filename) {
	char * body;
	int length = read_file(filename, &body);
	if (length == -1) {
		printf("file is not found\n");
		return;
	}
	Message_t * msg = malloc(sizeof(Message_t));
	msg->type = TRANSFER_MSG;
	strncpy(msg->filename, filename, strlen(filename));
	memcpy(msg->data, body, length);
        //printf("msg->data is : %s   \n",msg->data);
        //printf("size of Message_type is : %d  \n", sizeof(Message_type_t));
        //printf("size of Message is : %d   \n", sizeof(Message_t));
        //printf("Length is : %d   \n", length);
	datalink_send(sockfd, (char*)msg, sizeof(Message_type_t)+FILENAME_SIZE+length);
	free(msg);
}

void grace_exit(int sockfd) {
	if (sockfd == -1) {
		exit(1);
	}
	Message_t * msg = malloc(sizeof(Message_t));
	msg->type = REMOTE_SHUTDOWN_MSG;
	if (datalink_send(sockfd, (char*)msg, MSG_HEADER) == -1) {
		perror("notify client fails");
	}
	sleep(1);
	close(sockfd);
	exit(1);
}

void store(Message_t * msg, int bytesRead) {
	//parse the file name 
        //printf("Store can go  \n");
	receive_file (open_file(msg->filename), msg->data, bytesRead - FILENAME_SIZE - sizeof(Message_type_t));
	printf("receive msg successfully\n");
}

// prints all help commands 
void help() {
	printf("%-10s - start server.\n", START);
	printf("%-10s - connect to server.\n", CONNECT);
	printf("%-10s - send file to the server.\n", TRANSFER);
	printf("%-10s - print statistics information.\n", STATS);
	printf("%-10s - print help information.\n", HELP);
	printf("%-10s - exit the program gracefully.\n", EXIT);
}
