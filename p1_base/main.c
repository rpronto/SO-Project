#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "thread.h"

typedef struct dirent dirent;


int processLine(int fd, unsigned int jobsFlag, char *filename) {
  unsigned int event_id, delay = 0;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  
  switch (get_next(fd)) {
    case CMD_CREATE:
      if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (ems_create(event_id, num_rows, num_columns)) 
        fprintf(stderr, "Failed to create event\n");
      break;
    case CMD_RESERVE:
      num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (ems_reserve(event_id, num_coords, xs, ys)) 
        fprintf(stderr, "Failed to reserve seats\n");
      break;
    case CMD_SHOW:
      if (parse_show(fd, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (ems_show(event_id, filename, jobsFlag)) 
        fprintf(stderr, "Failed to show event\n");
      break;
    case CMD_LIST_EVENTS:
      if (ems_list_events(filename, jobsFlag)) 
        fprintf(stderr, "Failed to list events\n");
      break;
    case CMD_WAIT:
      if (parse_wait(fd, &delay, NULL) == -1) {  // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;
      }
      if (delay > 0) {
        printf("Waiting...\n");
        ems_wait(delay);
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
          "  BARRIER\n"                      // Not implemented
          "  HELP\n");
      break;
    case CMD_BARRIER:  // Not implemented
    case CMD_EMPTY:
      break;
    case EOC:
      return 1;
  }
  return 0;
}



int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  unsigned int jobsFlag = 0;
  int fd = STDIN_FILENO;
  int activeProcesses = 0;
  long int MAX_PROC;
  long int MAX_THREADS;
  char extension[6];
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

      fd = open(filename, O_RDONLY);
      if (fd == -1) {
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

      for (int i = 0; i < MAX_THREADS; i++) {

        threadArgs *thread = malloc(sizeof(threadArgs));
        thread->fd = fd;
        thread->jobsFlag = jobsFlag;
        thread->filename = strdup(filename);

        if (pthread_create(&threads[i], NULL, threadFunction, (void *) thread) != 0) {
          fprintf(stderr, "Failed to create thread.\n");
          return 1;
        }
      }

      for (int i = 0; i < MAX_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
          fprintf(stderr, "Failed to wait thread.\n");
          return 1;
        }
      }
      
      close(fd);
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
          printf("Child process with exit status %d.\n", WEXITSTATUS(status));
        
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

      fd = open(filename, O_RDONLY); 
      if (fd == -1) {
        fprintf(stderr, "Failed to open file %s.\n", dp->d_name);
        return 1;
      }
    }  
  }
  
  ems_terminate();
  return 0;
}
