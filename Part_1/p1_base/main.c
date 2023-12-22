/*  Grupo: 1
    Guilherme Lobo Rodrigues da Silva Gomes  ist1103206
    Rafael Alexandre Proenca Pronto          ist1105672
*/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "thread.h"

typedef struct dirent dirent;


int processLine(int fd_jobs, int fd_out, unsigned int jobsFlag, int *barrierFlag, unsigned long **delayTable) {
  int thread_id = -1;
  unsigned int event_id;
  unsigned long delay;
  extern pthread_mutex_t mutex_b;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  
  switch (get_next(fd_jobs)) {
    case CMD_CREATE:
      if (parse_create(fd_jobs, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (ems_create(event_id, num_rows, num_columns)) 
        fprintf(stderr, "Failed to create event\n");
      break;
    case CMD_RESERVE:
      num_coords = parse_reserve(fd_jobs, MAX_RESERVATION_SIZE, &event_id, xs, ys);
      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (ems_reserve(event_id, num_coords, xs, ys)) 
        fprintf(stderr, "Failed to reserve seats\n");
      break;
    case CMD_SHOW:
      if (parse_show(fd_jobs, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (ems_show(event_id, jobsFlag, fd_out)) 
        fprintf(stderr, "Failed to show event\n");
      break;
    case CMD_LIST_EVENTS:
      if (ems_list_events(jobsFlag, fd_out)) 
        fprintf(stderr, "Failed to list events\n");
      break;
    case CMD_WAIT:
      if (parse_wait(fd_jobs, (unsigned int*)&delay, &thread_id) == -1) {  
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (delay > 0) {
        if(thread_id == -1) {
          printf("Waiting...\n");
          ems_wait((unsigned int) delay);
        } else {
          pthread_mutex_lock(&mutex_b);
          delayTable[thread_id][1] = delay;
          pthread_mutex_unlock(&mutex_b);
        }
      }
      break;
    case CMD_INVALID:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;
    case CMD_HELP:
      printf(
          "Available commands:\n"
          "  CREATE <event_id> <num_rows> <num_columns>\n"
          "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
          "  SHOW <event_id>\n"
          "  LIST\n"
          "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
          "  BARRIER\n"                      
          "  HELP\n");
      break;
    case CMD_BARRIER:
      pthread_mutex_lock(&mutex_b);  
      return 2;
    case CMD_EMPTY:
      break;
    case EOC:
      pthread_mutex_lock(&mutex_b);
      return 1;
  }
  pthread_mutex_lock(&mutex_b);
  if(*barrierFlag == 2) {
    return 2;
  }
  return 0;
}


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  unsigned int jobsFlag = 0;
  int fd_jobs = STDIN_FILENO;
  int fd_out = STDOUT_FILENO;
  int activeProcesses = 0;
  long int MAX_PROC;
  long int MAX_THREADS;
  char extension[6];
  extern pthread_mutex_t mutex_b;
  pid_t pid;
  DIR* dir;
  dirent* dp;

  strcpy(extension, ".jobs");
  
  if (argc > 1) 
      jobsFlag = 1;

  if (argc > 2) {
    char *endptr;
    
    MAX_PROC = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "Invalid MAX_PROC value\n");
      return 1;
    }

    if (argc > 3) {
      MAX_THREADS = strtol(argv[3], &endptr, 10);
      if (*endptr != '\0') {
        fprintf(stderr, "Invalid MAX_THREADS value\n");
        return 1;
      }
    }

    if(argc > 4) {
      unsigned long int delay = strtoul(argv[4], &endptr, 10);
      if (*endptr != '\0' || delay > UINT_MAX) {
        fprintf(stderr, "Invalid delay value or value too large\n");
        return 1;
      }
      state_access_delay_ms = (unsigned int)delay;
    }  
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  while (1) {
    char filename[256];
    
    if (jobsFlag == 0) {
      printf("> ");
      fflush(stdout);
    } else if (jobsFlag == 1) {
      dir = opendir(argv[1]);

      if (dir == NULL) {
        fprintf(stderr, "Failed to open directory %s.\n", argv[1]);
        return 1;
      }

      dp = readdir(dir);
      if (dp == NULL) {
        fprintf(stderr, "Failed to read directory %s.\n", argv[1]);
        return 1;
      }

      while(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strstr(dp->d_name, extension) == NULL) {
        dp = readdir(dir);
        if (dp == NULL) {
          fprintf(stderr, "Failed to read directory %s.\n", argv[1]);
          return 1;
        }
      }

      strcpy(filename, argv[1]);
      strcat(filename, "/");
      strcat(filename, dp->d_name);

      if(activeProcesses <= MAX_PROC) {
        activeProcesses++;
        pid = fork();
      }

      fd_jobs = open(filename, O_RDONLY);
      if (fd_jobs == -1) {
        fprintf(stderr, "Failed to open file %s.\n", dp->d_name);
        return 1;
      }
      jobsFlag = 2;
    }

    if (pid == -1) {
      fprintf(stderr, "Failed to fork.\n");
      activeProcesses--;
    } else if (pid == 0) {
      pthread_t threads[MAX_THREADS];
      char new_filename[256];
      long threadResult = 2;

      strcpy(new_filename, filename);
      char *extension_ptr = strstr(new_filename, ".jobs");
      if(extension_ptr != NULL) {
        strcpy(extension_ptr, ".out");
      }
      
      fd_out = open(new_filename, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);
      if(fd_out == -1) {
        fprintf(stderr, "Failed to open file %s.\n", new_filename);
        return 1;
      }
      
      threadArgs *thread = malloc(sizeof(threadArgs));
      thread->fd_jobs = fd_jobs;
      thread->fd_out = fd_out;
      thread->jobsFlag = jobsFlag;
      thread->delayTable = (unsigned long **) malloc(sizeof(unsigned long *) * (unsigned long)MAX_THREADS);
      thread->MAX_THREADS = (unsigned long)MAX_THREADS;

      for (int i = 0; i < MAX_THREADS; i++) {
        thread->delayTable[i] = (unsigned long *) malloc(sizeof(unsigned long) * 2);
        thread->delayTable[i][1] = 0;
      }

      while(threadResult == 2) {
        thread->barrierFlag = 0;
        for (int i = 0; i < MAX_THREADS; i++) {
          pthread_mutex_lock(&mutex_b);
          if (pthread_create(&threads[i], NULL, threadFunction, (void *) thread) != 0) {
            fprintf(stderr, "Failed to create thread.\n");
            pthread_mutex_unlock(&mutex_b);
            return 1;
          } 
          thread->delayTable[i][0] = threads[i]; 
          pthread_mutex_unlock(&mutex_b);
        }

        for (int i = 0; i < MAX_THREADS; i++) {
          void *status;
          if (pthread_join(threads[i], &status) != 0) {
            fprintf(stderr, "Failed to wait thread.\n");
            return 1;
          }
          threadResult = *((long *) status);
          free(status);
        }
      }

      for (int i = 0; i < MAX_THREADS; i++) {
        free(thread->delayTable[i]);
      }
      free(thread->delayTable);
      free(thread);
      close(fd_out);
      close(fd_jobs);
      exit(0);
    } else {
      int status;

      while (activeProcesses > 0) {
        pid = wait(&status);
        if (pid == -1) {
          fprintf(stderr, "Failed to wait the child process.\n");
          activeProcesses--;
          return 1;
        }

        if (WIFEXITED(status)) 
          printf("Child %d ID exit with status %d.\n", pid, WEXITSTATUS(status));
        
        activeProcesses--;
      }
    }

    if (jobsFlag == 2) {
      do {
        dp = readdir(dir);
        if (dp == NULL) break;
      } while(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strstr(dp->d_name, extension) == NULL);
      
      if (dp == NULL) {
        closedir(dir);
        break;
      }

      strcpy(filename, argv[1]);
      strcat(filename, "/");
      strcat(filename, dp->d_name);

      if(activeProcesses <= MAX_PROC) {
        activeProcesses++;
        pid = fork();
      }

      fd_jobs = open(filename, O_RDONLY); 
      if (fd_jobs == -1) {
        fprintf(stderr, "Failed to open file %s.\n", dp->d_name);
        return 1;
      }
    }  
  }
  
  ems_terminate();
  return 0;
}
