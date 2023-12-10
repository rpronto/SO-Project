#include <stdlib.h>
#include <pthread.h>

#include "thread.h"

void *threadFunction(void *arg) {
  threadArgs *thread = (threadArgs *)arg;

  processFile(thread->fd, thread->jobsFlag, thread->filename);

  free(thread->filename);
  free(thread);
  pthread_exit(NULL);
}

