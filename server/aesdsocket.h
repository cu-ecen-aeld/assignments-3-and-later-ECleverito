#pragma once

#define BACKLOG     1
#define BUFF_SIZE   256

const char OUTPUT_FILEPATH[] = "/var/tmp/aesdsocketdata";
const char SERVER_PORT[] = "9000";

int createStreamSocket(const char *portNumberStr);
int listenAndLog(int sockfd);
int checkInput(int argc, char *argv[]);
int graceful_exit(int returnVal);