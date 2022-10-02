#pragma once

#define BACKLOG     1
#define BUFF_SIZE   256

const char OUTPUT_FILEPATH[] = "/var/tmp/aesdsocketdata";
const char SERVER_PORT[] = "9000";

/**
 * @brief Create a Stream Socket object
 * 
 * @param portNumberStr The desired port number to have stream socket server listen to. Input as string
 * @return int 
 * @retval -1 Error
 * @retval  0 Success
 */
int createStreamSocket(const char *portNumberStr);

/**
 * @brief Listen to the bound stream socket object for incoming connections, then output
 *  the incoming data to OUTPUT_FILEPATH shown above and echo entirety of file to
 *  the client
 * 
 * @param sockfd 
 * @return int
 * @retval -1 Error
 * @retval  0 Success 
 */
int listenAndLog(int sockfd);

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