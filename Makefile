SERVER_SRC := server.c \
                                      datalink.c \
                                      gbn.c \
                                      sr.c \
                                      application.c \
                                      physical.c

CLIENT_SRC := client.c  \
                                  datalink.c \
                                  gbn.c \
                                  sr.c \
                                  application.c \
                                  physical.c                         


PWD := $(shell pwd;cd)
TOPDIR := $(PWD)
SRC_DIR := $(TOPDIR)/src
OBJ_DIR := $(TOPDIR)/obj
LOG_DIR := $(TOPDIR)/log
RECV_DIR := $(TOPDIR)/recv
INCLUDE_DIR := $(SRC_DIR)/include

CLIENT_OBJ := $(patsubst %.c, $(OBJ_DIR)/%.o, $(CLIENT_SRC))
SERVER_OBJ := $(patsubst %.c, $(OBJ_DIR)/%.o, $(SERVER_SRC))


CLIENT_TARGET := client
SERVER_TARGET := server

CFLAGS := -g -I$(INCLUDE_DIR) 
LDFLAGS := -pthread -lrt

all: clean dir client server

dir:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(LOG_DIR)
	@mkdir -p $(RECV_DIR)
	

client: $(CLIENT_OBJ)

	$(CC) $(CFLAGS) -o $(TOPDIR)/$(CLIENT_TARGET) $(CLIENT_OBJ) $(LDFLAGS)

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $(TOPDIR)/$(SERVER_TARGET) $(SERVER_OBJ) $(LDFLAGS)
	
clean:
	rm -rf $(OBJ_DIR) $(LOG_DIR) $(RECV_DIR) $(TOPDIR)/$(CLIENT_TARGET) $(TOPDIR)/$(SERVER_TARGET) $(CLIENT_WRAPPER_OBJ)   $(TOPDIR)/client_wrapper

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
