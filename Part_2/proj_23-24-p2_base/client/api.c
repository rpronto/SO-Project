#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "api.h"
#include "common/constants.h"

void send_msg (int fd, char const *msg) {
  size_t len = strlen(msg), written = 0;
  while (written < len) {
    ssize_t bytes_written; 
    if ((bytes_written = write(fd, msg + written, len - written)) < 0) {
      fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    written += (size_t)bytes_written;
  }
}

void read_msg (int fd, char *buffer) {
  size_t len = strlen(buffer), already_read = 0;
  while (already_read < len) {
    ssize_t bytes_read;
    if ((bytes_read = read(fd, buffer + already_read, len - already_read)) < 0) {
      fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    already_read += (size_t)bytes_read;
  }
}

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  int fd_serv = open(server_pipe_path, O_WRONLY);
  if (fd_serv < 0) {
    fprintf(stderr, "Failed to open server named pipe.\n");
    return 1;
  }

  // create req_pipe_path pipe
  unlink(req_pipe_path);
  if(mkfifo(req_pipe_path, 0777) < 0) {
      fprintf(stderr, "Failed to create sender named pipe\n");
      return 1;
  }
  int fd_req;
  if((fd_req = open(req_pipe_path, O_WRONLY)) < 0) {
    fprintf(stderr, "Failed to open sender named pipe\n");
    return 1;
  }

  // create resp_pipe_path pipe
  unlink(resp_pipe_path);
  if(mkfifo(resp_pipe_path, 0777) < 0) {
      fprintf(stderr, "Failed to create receiver named pipe\n");
      return 1;
  }
  int fd_resp;
  if((fd_resp = open(resp_pipe_path, O_RDONLY)) < 0) {
      fprintf(stderr, "Failed to open receiver named pipe\n");
      return 1;
  }
  char msg[MAX_PIPE_LENGHT * 2 + 2];
  int op_code = 1;
  sprintf(msg, "%c%s%s", op_code, req_pipe_path, resp_pipe_path);
  send_msg(fd_serv, msg);
  


  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
