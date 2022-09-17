#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    thread_func_args->thread_complete_success = false;

    usleep(thread_func_args->wait_to_obtain_ms*1000);
    
    int lockReturnVal = 0;

    lockReturnVal = pthread_mutex_lock(thread_func_args->mutex);

    if(lockReturnVal!=0)
    {
        perror(strerror(errno));
        pthread_exit(thread_param);
    }

    usleep(thread_func_args->wait_to_release_ms*1000);

    lockReturnVal = pthread_mutex_unlock(thread_func_args->mutex);

    if(lockReturnVal!=0)
    {
        perror(strerror(errno));
        pthread_exit(thread_param);
    }

    thread_func_args->thread_complete_success = true;

    pthread_exit(thread_param);
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *my_thread_data = (struct thread_data *) malloc(sizeof(struct thread_data));

    //Check if memory is still available for allocation
    if(my_thread_data == NULL)
    {
        return false;
    }

    my_thread_data->mutex = mutex;
    my_thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    my_thread_data->wait_to_release_ms = wait_to_release_ms;

    if(pthread_create(thread, NULL, threadfunc, my_thread_data) != 0)
    {
        ERROR_LOG("Thread could not be created");
        return false;
    }
    else
    {
        // pthread_detach(*thread);
        return true;
    }

}

