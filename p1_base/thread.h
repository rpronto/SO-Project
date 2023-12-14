#ifndef THREAD_H
#define THREAD_H

typedef struct {
  int fd_jobs;
  int fd_out;
  unsigned int jobsFlag;
} threadArgs;

void *threadFunction(void *);
int processLine(int , int ,unsigned int);

#endif