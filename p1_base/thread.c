#include <stdlib.h>
#include <pthread.h>

#include "thread.h"

void *threadFunction(void *arg) {
  threadArgs *thread = (threadArgs *)arg;
  
  while(processLine(thread->fd_jobs, thread->fd_out, thread->jobsFlag) == 0);

  pthread_exit(NULL);
}

