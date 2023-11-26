#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

typedef struct dirent dirent;

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  unsigned int jobsFlag = 0;
  int fd = STDIN_FILENO;
  char extension[6];
  strcpy(extension, ".jobs");
  DIR* dir;
  dirent* dp;
 
  if (argc > 1) {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }
    if (argc > 2) jobsFlag = 1;

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  while (1) {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    char filename[256];
    
    if(jobsFlag == 1){
      dir = opendir(argv[2]);
      if (dir == NULL) {
        fprintf(stderr, "Failed to open directory %s.\n", argv[2]);
        return 1;
      }
      dp = readdir(dir);
      if (dp == NULL) {
        fprintf(stderr, "Failed to read directory %s.\n", argv[2]);
        return 1;
      }
      while(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strstr(dp->d_name, extension) == NULL) {
        dp = readdir(dir);
        if (dp == NULL) {
          fprintf(stderr, "Failed to read directory %s.\n", argv[2]);
          return 1;
        }
      }
      strcpy(filename, argv[2]);
      strcat(filename, "/");
      strcat(filename, dp->d_name);
      fd = open(filename, O_RDONLY);
      if (fd == -1) {
        fprintf(stderr, "Failed to open file %s.\n", dp->d_name);
        return 1;
      }
      printf("%s\n", dp->d_name);
      jobsFlag = 2;
    }

    if (jobsFlag == 0){
      printf("> ");
      fflush(stdout);
    }

    switch (get_next(fd)) {
      case CMD_CREATE:
        if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }

        continue;

      case CMD_RESERVE:
        num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_reserve(event_id, num_coords, xs, ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }

        continue;

      case CMD_SHOW:
        if (parse_show(fd, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_show(event_id, filename)) {
          fprintf(stderr, "Failed to show event\n");
        }

        continue;

      case CMD_LIST_EVENTS:
        if (ems_list_events(filename)) {
          fprintf(stderr, "Failed to list events\n");
        }

        continue;

      case CMD_WAIT:
        if (parse_wait(fd, &delay, NULL) == -1) {  // thread_id is not implemented
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay > 0) {
          printf("Waiting...\n");
          ems_wait(delay);
        }

        continue;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;

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

        continue;

      case CMD_BARRIER:  // Not implemented
      case CMD_EMPTY:
        continue;

      case EOC:
      printf("\n");
        break;
    }
    close(fd);
    if (jobsFlag == 2) {
      do {
        dp = readdir(dir);
        if (dp == NULL) break;
      } while(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strstr(dp->d_name, extension) == NULL);
      if (dp == NULL) {
        closedir(dir);
        break;
      }
      strcpy(filename, argv[2]);
      strcat(filename, "/");
      strcat(filename, dp->d_name);
      fd = open(filename, O_RDONLY); 
      if (fd == -1) {
        fprintf(stderr, "Failed to open file %s.\n", dp->d_name);
        return 1;
      }
      printf("%s\n", dp->d_name);
    }  
  }
  ems_terminate();
  return 0;
}
