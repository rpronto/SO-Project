#include <stdlib.h>
#include <pthread.h>

#include "thread.h"

pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER; // mutex for barrier

void *threadFunction(void *arg) {
  
  threadArgs *thread = (threadArgs *)arg;
  long *status = malloc(sizeof(long));
  
  while(1) {
    pthread_mutex_lock(&mutex_b);
    if(thread->barrierFlag == 2) {
      *status = thread->barrierFlag;
      pthread_mutex_unlock(&mutex_b);
      pthread_exit((void*)status);
    }
    pthread_mutex_unlock(&mutex_b);
    thread->barrierFlag = processLine(thread->fd_jobs, thread->fd_out, thread->jobsFlag, &thread->barrierFlag);
    
    if (thread->barrierFlag == 1) {
      pthread_mutex_unlock(&mutex_b);
      break;
    }
    pthread_mutex_unlock(&mutex_b);
  }
  pthread_mutex_lock(&mutex_b);
  *status = thread->barrierFlag;
  pthread_mutex_unlock(&mutex_b);
  pthread_exit((void*) status); 
}

