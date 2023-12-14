#include <stdlib.h>
#include <pthread.h>

#include "thread.h"

pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER; // mutex for barrier

void *threadFunction(void *arg) {
  
  threadArgs *thread = (threadArgs *)arg;
  thread->barrierFlag = 0;
  long *status = malloc(sizeof(long));
  while(thread->barrierFlag != 1) {
    if(thread->barrierFlag == 2) {
      *status = thread->barrierFlag;
      pthread_exit((void*)status);
    }
    thread->barrierFlag = processLine(thread->fd_jobs, thread->fd_out, thread->jobsFlag);
    pthread_mutex_unlock(&mutex_b);
  }
  *status = 0;
  pthread_exit((void*) status); 
}

