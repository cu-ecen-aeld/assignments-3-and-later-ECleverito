
#include "aesdsocket.h"

struct addrinfo *sockaddr = NULL;
SLIST_HEAD(slisthead, socket_data_s) head;
pthread_mutex_t mutex;

void SIGTERM_handler(int SIG_val)
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

void SIGINT_handler(int SIG_val)
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

void alarm_handler(int signo)
{
    int lockRet = pthread_mutex_lock(&mutex);

    if(lockRet!=0)
    {
        perror("pthread_mutex_lock() error");
        graceful_exit(-1);
        exit(-1);
    }

    //Open output file to append to or create if it does not already exist
    int outputFd = open(OUTPUT_FILEPATH, O_RDWR | O_CREAT | O_APPEND,\
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    if(outputFd==-1)
    {
        perror("open() error in timestamping");
        graceful_exit(-1);
        exit(-1);
    }

    time_t t;
    struct tm *tmp;
    char timeStr[200];
    t = time(NULL);
    tmp = localtime(&t);
    if(tmp==NULL)
    {
        perror("localtime() error");
        graceful_exit(-1);
        exit(-1);
    }

    int bytesWritten;
    bytesWritten = strftime(timeStr,sizeof(timeStr)/sizeof(timeStr[0]),\
                "timestamp:%a, %d %b %Y %T %z\n",tmp);
    
    if(bytesWritten==0)
    {
        fprintf(stderr, "strftime returned 0");
        graceful_exit(-1);
        exit(-1);
    }

    int bytesOutFile = 0;

    do
    {
        bytesOutFile = write(outputFd, &timeStr[bytesOutFile],\
                                     bytesWritten);
        bytesWritten-=bytesOutFile;
    } while (bytesWritten>0);

    close(outputFd);

    pthread_mutex_unlock(&mutex);

    return;

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
    struct sigaction SIGINT_action;
    struct sigaction SIGTERM_action;
    SIGINT_action.sa_handler = &SIGINT_handler;
    SIGTERM_action.sa_handler = &SIGTERM_handler;
    sigemptyset(&SIGINT_action.sa_mask);
    sigemptyset(&SIGTERM_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    SIGTERM_action.sa_flags = 0;
    sigaction(SIGTERM, &SIGTERM_action, NULL);
    sigaction(SIGINT, &SIGINT_action, NULL);

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

    //Timer must be set up after daemon has been created,
    //as child processes do not inherit timers
    if(setupTimer()!=0)
    {
        fprintf(stderr,"Timer setup failed\n");
        return -1;
    }

    SLIST_INIT(&head);

    socket_data_t *newListElement = NULL;
    socket_data_t *listSearchp = NULL;

    socket_data_t *tmpItem = NULL;

    int retVal;

    do
    {
        retVal = listenForConnections(sockfd, &newListElement);

        if(retVal==0)
        {
            SLIST_INSERT_HEAD(&head, newListElement, entries);

            //Check list for completed threads
            SLIST_FOREACH_SAFE(listSearchp, &head, entries, tmpItem)
            {
                if(listSearchp->threadCompleteFlag)
                {
                    pthread_join(listSearchp->threadHandle, NULL);
                    SLIST_REMOVE(&head, listSearchp, socket_data_s, entries);
                    free(listSearchp);                    
                }
            }
        }
        
    }while(retVal!=-1);

    return graceful_exit(-1);

}

int createStreamSocket(const char *portNumberStr)
{
    //Create addrinfo struct for creating TCP stream
    //socket
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, portNumberStr, &hints, &sockaddr)!=0)
    {
        perror("getaddrinfo() error:");
        return -1;
    }

    int sockfd = socket(sockaddr->ai_family, sockaddr->ai_socktype, sockaddr->ai_protocol);
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

int listenForConnections(int sockfd, socket_data_t **newListElement)
{
    struct sockaddr peeraddr;
    socklen_t peer_addr_size = sizeof(peeraddr);
    
    int connectedSock = accept(sockfd, &peeraddr, &peer_addr_size);
    if(connectedSock==-1)
    {
        if(errno==EINTR)
        {
            //Allow this to keep executing even
            //on interruptions from interval alarm
            return -2;
        }

        perror("accept() error");
        return -1;
    }

    *newListElement = (socket_data_t *)malloc(sizeof(socket_data_t));

    if(*newListElement==NULL)
    {
        //If cannot malloc anymore space
        return -1;
    }

    //Initialize arguments to be used by thread
    (*newListElement)->connectedSock = connectedSock;
    (*newListElement)->peeraddr = peeraddr;
    (*newListElement)->threadCompleteFlag = false;

    if(pthread_create((&(*newListElement)->threadHandle), NULL,\
                    recvAndSendAndLog, *newListElement) != 0)
    {
        perror("pthread_create() error");
        free(*newListElement);
        return -1;
    }

    return 0;
}

void* recvAndSendAndLog(void* socket_data_arg)
{
    socket_data_t *socket_data = (socket_data_t *) socket_data_arg;  

    //Determine IP address of client for logging
    struct sockaddr_in *peeraddr_in;
    peeraddr_in = (struct sockaddr_in *)(&(socket_data->peeraddr));
    const char *ipv4Addr = (char *)(&peeraddr_in->sin_addr.s_addr);

    syslog(LOG_INFO, "Accepted connection from %d.%d.%d.%d",\
            ipv4Addr[0], ipv4Addr[1], ipv4Addr[2], ipv4Addr[3]);

    //Open output file to append to or create if it does not already exist
    int outputFd = open(OUTPUT_FILEPATH, O_RDWR | O_CREAT | O_APPEND,\
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    if(outputFd==-1)
    {
        perror("open() error");
        socket_data->threadCompleteFlag = true;
        pthread_exit(socket_data);
    }

    //Receive a single byte at a time
    char recvdByte;

    char retByte;
    ssize_t sendRet;
    ssize_t readRet;
    int lockRet;

    bool mutexLocked=false;

    //Append each byte received to the output file
    while(recv(socket_data->connectedSock, &recvdByte, 1, 0)!=0)
    {
        if(!mutexLocked)
        {

            lockRet = pthread_mutex_lock(&mutex);

            if(lockRet!=0)
            {
                perror("pthread_mutex_lock() error");
                socket_data->threadCompleteFlag = true;
                pthread_exit(socket_data);
            }

            mutexLocked=true;

        }

        while(write(outputFd, &recvdByte, 1)!=1)
            ;

        //Output contents of output file to
        //the peer for every packet received
        if(recvdByte=='\n')
        {
            //Set file pointer to 0
            if(lseek(outputFd, 0, SEEK_SET)==-1)
            {
                perror("lseek() error in returning socket input"
                    "to peer");
                    close(outputFd);
                    //Include error element in socket_data_t?
                    socket_data->threadCompleteFlag = true;
                    pthread_exit(socket_data);
            }
            
            readRet=1;
            //Read until EOF
            while(readRet!=0)
            {
                do
                {
                    readRet=read(outputFd, &retByte, 1);
                    if(readRet==-1)
                    {
                        if(errno!=EAGAIN)
                        {
                            perror("read() error in returning socket"
                            "input to peer");
                            close(outputFd);
                            socket_data->threadCompleteFlag = true;
                            pthread_exit(socket_data);
                        }
                        
                    }

                } while (readRet<0);
                
                do
                {
                    sendRet=send(socket_data->connectedSock, &retByte, readRet, 0);
                    if(sendRet==-1)
                    {
                        perror("send() error in returning socket"
                            "input to peer");
                        close(outputFd);
                        socket_data->threadCompleteFlag = true;
                        pthread_exit(socket_data);
                    }
                } while (sendRet<0);

            }

            if(mutexLocked)
            {
                lockRet = pthread_mutex_unlock(&mutex);

                if(lockRet!=0)
                {
                    perror("pthread_mutex_lock() error");
                    socket_data->threadCompleteFlag = true;
                    pthread_exit(socket_data);
                }

            }
            
        }
        
    }

    syslog(LOG_INFO, "Closed connection from %d.%d.%d.%d",\
            ipv4Addr[0], ipv4Addr[1], ipv4Addr[2], ipv4Addr[3]);
    close(outputFd);

    socket_data->threadCompleteFlag = true;

    pthread_exit(socket_data);
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

int setupTimer()
{
    //Alarm signal-handling
    struct sigaction alrm_action;
    alrm_action.sa_handler = &alarm_handler;
    alrm_action.sa_flags = 0;
    sigemptyset(&alrm_action.sa_mask);
    sigaction(SIGALRM, &alrm_action, NULL);
    struct itimerval delay;
    delay.it_value.tv_sec=10;
    delay.it_value.tv_usec=0;
    delay.it_interval.tv_sec=10;
    delay.it_interval.tv_usec=0;

    if(setitimer(ITIMER_REAL, &delay, NULL)!=0)
    {
        perror("setitimer() error:");
        return -1;
    }

    return 0;

}

int graceful_exit(int returnVal)
{
    socket_data_t *listSearchp = NULL;
    socket_data_t *tmpItem = NULL;

    while(!SLIST_EMPTY(&head))
    {
        //Check list for completed threads
        SLIST_FOREACH_SAFE(listSearchp, &head, entries, tmpItem){
            if(listSearchp->threadCompleteFlag)
            {
                pthread_join(listSearchp->threadHandle, NULL);
                SLIST_REMOVE(&head, listSearchp, socket_data_s, entries);
                free(listSearchp);                
            }
        }
    }

    freeaddrinfo(sockaddr);
    closelog();
    return returnVal;
}