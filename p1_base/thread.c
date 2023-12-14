#include <stdlib.h>
#include <pthread.h>

#include "thread.h"

void *threadFunction(void *arg) {
  threadArgs *thread = (threadArgs *)arg;
  thread->barrierFlag = 0;
  while(thread->barrierFlag != 1) {
    if(thread->barrierFlag == 2) {
      int *status = malloc(sizeof(int));
      *status = thread->barrierFlag;
      pthread_exit((void*)status);
    }
    thread->barrierFlag = processLine(thread->fd_jobs, thread->fd_out, thread->jobsFlag);
  }

  pthread_exit(NULL);
}

