
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

    int outputFd = open(OUTPUT_FILEPATH, O_TRUNC | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    if(outputFd==-1)
    {
        perror("creat() error");
        return -1;
    }

    //Receive a single byte at a time
    char recvdByte;
    int totalByteCnt = 0;
    int pktByteCnt = 0;

    char retByte;
    ssize_t sendRet;
    ssize_t readRet;

    while(recv(connectedSock, &recvdByte, 1, 0)!=0)
    {
        
        while(write(outputFd, &recvdByte, 1)!=1)
            ;

        pktByteCnt++;
        totalByteCnt++;

        //Output contents of output file to
        //the peer for every packet received
        if(recvdByte=='\n')
        {

            if(lseek(outputFd, 0, SEEK_SET)==-1)
            {
                perror("lseek() error in returning socket input"
                    "to peer");
                    return -1;
            }

            for(int i=0;i<totalByteCnt;i++)
            {
                do
                {
                    readRet=read(outputFd, &retByte, 1);
                    if(readRet==-1)
                    {
                        perror("read() error in returning socket"
                            "input to peer");
                        return -1;
                    }

                } while (readRet!=1);
                
                do
                {
                    sendRet=send(connectedSock, &retByte, 1, 0);
                    if(sendRet==-1)
                    {
                        perror("send() error in returning socket"
                            "input to peer");
                        return -1;
                    }
                } while (sendRet!=1);

            }
            
        }
        
    }

    syslog(LOG_INFO, "Closed connection from %s", peeraddr.sa_data);
    close(outputFd);
    closelog();

    freeaddrinfo(sockaddr);

    return 0;

}
