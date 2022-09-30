
#include "aesdsocket.h"

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

int main()
{
    //
    struct addrinfo hints, *sockaddr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, "9000", &hints, &sockaddr)!=0)
    {
        perror("getaddrinfo() error:");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd==-1)
    {
        perror("socket() error");
        return -1;
    }

    if(bind(sockfd, sockaddr->ai_addr, sockaddr->ai_addrlen)!=0)
    {
        perror("bind() error");
        return -1;
    }

    if(listen(sockfd, 1)!=0)
    {
        perror("listen() error");
        return -1;
    }

    struct sockaddr peeraddr;
    socklen_t peer_addr_size = sizeof(peeraddr);
    
    int connectedSock = accept(sockfd, &peeraddr, &peer_addr_size);
    if(connectedSock==-1)
    {
        perror("listen() error");
        return -1;
    }

    openlog("aesdsocket", 0, LOG_USER);

    syslog(LOG_INFO, "Accepted connection from %s", peeraddr.sa_data);

    int fd = creat(OUTPUT_FILEPATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    if(fd==-1)
    {
        perror("creat() error");
        return -1;
    }

    //Receive a single byte at a time
    char recvdByte;
    int totalByteCnt, pktByteCnt = 0;

    while(recv(connectedSock, &recvdByte, 1, 0)!=0)
    {
        
        while(write(fd, &recvdByte, 1)!=1)
            ;

        pktByteCnt++;
        totalByteCnt++;

        //Output contents of output file to
        //the peer for every packet received
        if(recvdByte=='\n')
        {
            pktByteCnt=0;
            //COMPLETE LATER
        }
        
    }

    syslog(LOG_INFO, "Closed connection from %s", peeraddr.sa_data);

    closelog();

    freeaddrinfo(sockaddr);

}
