#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data *data = (struct thread_data *)(thread_param);
    nanosleep(&data->wait_time_ns, NULL);
    pthread_mutex_lock(data->pMutex);
    nanosleep(&data->hold_time_ns, NULL);
    data->thread_complete_success = true;
    pthread_mutex_unlock(data->pMutex);
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *data = (struct thread_data *)malloc(sizeof(struct thread_data));
    if (data == NULL) {
        return false;
    }

    struct timespec wait_time_ns, hold_time_ns;
    wait_time_ns.tv_sec = wait_to_obtain_ms / 1000;
    wait_time_ns.tv_nsec = (wait_to_obtain_ms % 1000) * 1000000;
    hold_time_ns.tv_sec = wait_to_release_ms / 1000;
    hold_time_ns.tv_nsec = (wait_to_release_ms % 1000) * 1000000;
    data->pMutex = mutex;
    data->wait_time_ns = wait_time_ns;
    data->hold_time_ns = hold_time_ns;
    data->thread_complete_success = false;

    int result = pthread_create(thread, NULL, threadfunc, data);
    if (result != 0) {
        free(data);
	return false;
    }
    else {
    	return true;
    }
}

