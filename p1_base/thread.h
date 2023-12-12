#ifndef THREAD_H
#define THREAD_H

typedef struct {
  int fd;
  unsigned int jobsFlag;
  char *filename;
} threadArgs;

void *threadFunction(void *);
int processLine(int , unsigned int , char *);

#endif