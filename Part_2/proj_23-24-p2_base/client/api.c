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
#include "common/io.h"

long int session_id;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  char msg[MAX_PIPE_LENGHT * 2 + 2];
  char buffer[BUFFER_SIZE];
  char *endptr;
  int op_code = 1;
  int fd_serv, fd_req, fd_resp; 
  
  // open server pipe
  fd_serv = open(server_pipe_path, O_WRONLY);
  if (fd_serv < 0) {
    fprintf(stderr, "Failed to open server named pipe.\n");
    return 1;
  }

  // create req_pipe_path
  unlink(req_pipe_path);
  if(mkfifo(req_pipe_path, 0777) < 0) {
      fprintf(stderr, "Failed to create sender named pipe\n");
      return 1;
  }

  // create resp_pipe_path 
  unlink(resp_pipe_path);
  if(mkfifo(resp_pipe_path, 0777) < 0) {
      fprintf(stderr, "Failed to create receiver named pipe\n");
      return 1;
  }
  
  sprintf(msg, "%d %s %s", op_code, req_pipe_path, resp_pipe_path);
  send_msg(fd_serv, msg); //send message to server 
  
  // open req_pipe_path to write
  if((fd_req = open(req_pipe_path, O_WRONLY)) < 0) {
    fprintf(stderr, "Failed to open sender named pipe\n");
    return 1;
  }
  
  // open resp_pipe_path to read
  if((fd_resp = open(resp_pipe_path, O_RDONLY)) < 0) {
      fprintf(stderr, "Failed to open receiver named pipe\n");
      return 1;
  }
  
  memset(buffer, 0, sizeof(buffer));
  read_msg(fd_resp, buffer, BUFFER_SIZE);
  session_id = strtol(buffer, &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "Invalid MAX_PROC value\n");
    return 1;
  }
  
  return 0;
}
/*
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
*/