
#include "aesdsocket.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

struct addrinfo *sockaddr = NULL;

void SIG_handler(int SIG_val)
{
    if(unlink(OUTPUT_FILEPATH)==-1)
    {
        if(errno==ENOENT)
        {
            syslog(LOG_INFO, "Output file had not been created yet");
        }
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    graceful_exit(0);
    exit(0);

}

int main(int argc, char *argv[])
{
    openlog("aesdsocket", 0, LOG_USER);

    bool daemonFlag = false;
    //Check for correct number of arguments and provide usage information
    if(argc > 1)
    {
        if(checkInput(argc,argv)!=0)
        {
            return -1;
        }
        daemonFlag = true;
    }

    //Set up signal-handling
    struct sigaction SIGS_action;
    SIGS_action.sa_handler = &SIG_handler;
    sigemptyset(&SIGS_action.sa_mask);
    SIGS_action.sa_flags = 0;
    sigaction(SIGTERM, &SIGS_action, NULL);
    sigaction(SIGINT, &SIGS_action, NULL);

    int sockfd = createStreamSocket(SERVER_PORT);
    if(sockfd==-1)
    {
        return graceful_exit(-1);
    }

    if(daemonFlag)
    {
        int pid = fork();
        if(pid==-1)
        {
            perror("fork() error:");
            return graceful_exit(-1);
        }
        //End parent process
        if(pid!=0)
        {
            return graceful_exit(0);
        }
    }

    while(listenAndLog(sockfd)==0)
        ;

    return graceful_exit(-1);

}

//Remember to perform freeaddrinfo() on sockaddr after calling this
int createStreamSocket(const char *portNumberStr)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, portNumberStr, &hints, &sockaddr)!=0)
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

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))==-1)
    {
        perror("setsockopt() error");
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

    return sockfd;

}

int listenAndLog(int sockfd)
{

    struct sockaddr peeraddr;
    socklen_t peer_addr_size = sizeof(peeraddr);
    
    int connectedSock = accept(sockfd, &peeraddr, &peer_addr_size);
    if(connectedSock==-1)
    {
        perror("listen() error");
        return -1;
    }

    struct sockaddr_in *peeraddr_in;
    peeraddr_in = (struct sockaddr_in *)(&peeraddr);
    const char *ipv4Addr = (char *)(&peeraddr_in->sin_addr.s_addr);

    syslog(LOG_INFO, "Accepted connection from %d.%d.%d.%d", ipv4Addr[0], ipv4Addr[1], ipv4Addr[2], ipv4Addr[3]);

    int outputFd = open(OUTPUT_FILEPATH, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    if(outputFd==-1)
    {
        perror("creat() error");
        return -1;
    }

    //Receive a single byte at a time
    char recvdByte;
    static int totalByteCnt = 0;
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

    syslog(LOG_INFO, "Closed connection from %d.%d.%d.%d", ipv4Addr[0], ipv4Addr[1], ipv4Addr[2], ipv4Addr[3]);
    close(outputFd);

    return 0;
}

int checkInput(int argc, char *argv[])
{

    if(argc > 2)
	{
		const char* usageErrStr = "Invalid number of options provided.\n\n";

		const char* correctUsageStr ="USAGE: The only available option is -d, "
                        "which runs aesdsocket as a daemon\n\n";

		fprintf(stderr, "%s", usageErrStr);
		printf("%s", correctUsageStr);

		syslog(LOG_ERR, "%s", usageErrStr);
		syslog(LOG_INFO, "%s", correctUsageStr);

		closelog();

		return -1;
	}
    else if(argc == 2 && (strcmp(argv[1],"-d")!=0))
    {
        const char* usageErrStr = "Invalid option provided.\n\n";

        const char* correctUsageStr ="USAGE: The only available option is -d, "
                        "which runs aesdsocket as a daemon\n\n";

        fprintf(stderr, "%s", usageErrStr);
        printf("%s", correctUsageStr);

        syslog(LOG_ERR, "%s", usageErrStr);
        syslog(LOG_INFO, "%s", correctUsageStr);

        closelog();

        return -1;
    }	

    return 0;

}

int graceful_exit(int returnVal)
{
    freeaddrinfo(sockaddr);
    closelog();
    return returnVal;
}