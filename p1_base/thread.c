#include <stdlib.h>
#include <pthread.h>

#include "thread.h"

void *threadFunction(void *arg) {
  threadArgs *thread = (threadArgs *)arg;
  
  while(processLine(thread->fd, thread->jobsFlag, thread->filename) == 0);

  free(thread->filename);
  free(thread);
  pthread_exit(NULL);
}

