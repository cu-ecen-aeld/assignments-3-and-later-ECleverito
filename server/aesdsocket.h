#pragma once

#include "./queue.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#define BACKLOG     1
#define BUFF_SIZE   256

const char OUTPUT_FILEPATH[] = "/dev/aeschar";
const char SERVER_PORT[] = "9000";

typedef struct socket_data_s socket_data_t;

struct socket_data_s{
    pthread_t threadHandle;
    int connectedSock;
    struct sockaddr peeraddr;
    bool threadCompleteFlag;
    SLIST_ENTRY(socket_data_s) entries;
};

/**
 * @brief Create a Stream Socket object
 * 
 * @param portNumberStr The desired port number to have stream socket server listen to. Input as string
 * @return int 
 * @retval -1 Error
 * @retval  0 Success
 */
int createStreamSocket(const char *portNumberStr);

int listenForConnections(int sockfd, socket_data_t **newListElement);

void* recvAndSendAndLog(void* socket_data_arg);

/**
 * @brief Check input to the application for validity
 * 
 * @param argc Number of arguments
 * @param argv Vector of arguments
 * @return int 
 * @retval -1 Error
 * @retval  0 Success
 */
int checkInput(int argc, char *argv[]);

/**
 * @brief Exit gracefully, freeing the sockaddr structs generated by getaddrinfo()
 *  and closing the log
 * 
 * @param returnVal Value to be returned by this function (allows it to be used as 
 *  return graceful_exit() directly in main)
 * @return int 
 * @retval Whatever input returnVal is
 */
int graceful_exit(int returnVal);