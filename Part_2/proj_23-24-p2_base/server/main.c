#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  //TODO: Intialize server, create worker threads
  char *pipeServer = argv[1];
  int fd_serv;
  int session_id = 0;

  unlink(pipeServer);

  if(mkfifo(pipeServer, 0777) < 0) {
      fprintf(stderr, "Failed to create named pipe\n");
      return 1;
  }

  if((fd_serv = open(pipeServer, O_RDONLY)) < 0) {
    fprintf(stderr, "Failed to open named pipe\n");
    return 1;
  }
  


  while (1) {
    char buffer[128];
    char op_code_str[2];
    char req_pipe[41];
    char resp_pipe[41];
    int fd_req;
    int fd_resp;

    read_msg(fd_serv, buffer);

    printf("chega aqui\n");
    switch (buffer[0]) {
    case '1':
      sscanf(buffer, "%c %s %s", op_code_str, req_pipe, resp_pipe);
      if((fd_req = open(req_pipe, O_RDONLY)) < 0) {
        fprintf(stderr, "Failed to open sender named pipe\n");
        return 1;
      }

      if((fd_resp = open(resp_pipe, O_WRONLY)) < 0) {
        fprintf(stderr, "Failed to open receiver named pipe\n");
        return 1;
      }

      session_id++;
      char session_id_str[2];
      snprintf(session_id_str, sizeof(session_id_str), "%d", session_id);
      send_msg(fd_resp, session_id_str);
      break;
    
    default:
      break;
    }
    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer
  }

  //TODO: Close Server
  close(fd_serv);

  ems_terminate();
}