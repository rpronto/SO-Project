#ifndef THREAD_H
#define THREAD_H

typedef struct {
  int fd_jobs;
  int fd_out;
  int barrierFlag;
  unsigned int jobsFlag;
  unsigned long MAX_THREADS;
  unsigned long **delayTable;
} threadArgs;

void *threadFunction(void *);
int processLine(int , int ,unsigned int, int*, unsigned long **);

#endif