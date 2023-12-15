#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#include "thread.h"
#include "operations.h"

pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER; // mutex for barrier

void *threadFunction(void *arg) {
  
  threadArgs *thread = (threadArgs *)arg;
  long *status = malloc(sizeof(long));
  unsigned long *thread_id = (unsigned long *) malloc(sizeof(unsigned long));
  *thread_id = 0;
  
  while(1) {
    pthread_mutex_lock(&mutex_b);
    if (*thread_id == 0) {
      for (unsigned int i = 0; i < thread->MAX_THREADS; i++) {
        if (thread->delayTable[i][0] == pthread_self()) { //data race com main.c:231
          *thread_id = (unsigned long) i;
          break;
        }
        fprintf(stderr, "Failed to find thread id.\n");
      }
    }
    if(thread->delayTable[*thread_id][1] > 0) {
      ems_wait((unsigned int) thread->delayTable[*thread_id][1]);
      thread->delayTable[*thread_id][1] = 0; 
    }
    if(thread->barrierFlag == 2) {
      *status = thread->barrierFlag;
      pthread_mutex_unlock(&mutex_b);
      pthread_exit((void*)status);
    }
    pthread_mutex_unlock(&mutex_b);
    thread->barrierFlag = processLine(thread->fd_jobs, thread->fd_out, thread->jobsFlag, &thread->barrierFlag, thread->delayTable);
    
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

