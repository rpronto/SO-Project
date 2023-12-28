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

sessionID session_ID;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  char msg[MAX_PIPE_NAME_LENGHT * 2 + 2];
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
  
  memset(buffer, '\0', sizeof(buffer));
  read_msg(fd_resp, buffer, BUFFER_SIZE);
  session_ID.session_id = strtol(buffer, &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "Invalid sessionID value\n");
    return 1;
  }
  session_ID.fd_serv = fd_serv;
  session_ID.fd_req = fd_req;
  session_ID.fd_resp = fd_resp;
  strcpy(session_ID.req_pipe_path, req_pipe_path);
  strcpy(session_ID.resp_pipe_path, resp_pipe_path);
  session_ID.status = 1;
  
  return 0;
}

int ems_quit(void) { 
  char msg[2];
  memset(msg, '\0', sizeof(msg));
  msg[0] = '2';
  send_msg(session_ID.fd_req, msg);
  if (close(session_ID.fd_req) < 0) {
    fprintf(stderr, "Failed to close fd_req\n");
    return 1;
  }
  if (close(session_ID.fd_resp) < 0) {
    fprintf(stderr, "Failed to close fd_req\n");
    return 1;
  }
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  int op_code = 3, ret;
  char msg[BUFFER_SIZE];
  memset(msg, '\0', sizeof(msg));
  sprintf(msg, "%d %d %ld %ld", op_code, event_id, num_rows, num_cols);
  send_msg(session_ID.fd_req, msg);

  memset(msg, '\0', sizeof(msg));
  while (msg[0] == '\0')
    read_msg(session_ID.fd_resp, msg, BUFFER_SIZE);
  sscanf(msg, "%d", &ret);
  if (ret != 0)
    return 1;
  return 0;
}


int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  int op_code = 4, ret;
  char msg[BUFFER_SIZE + (num_seats * 2)];
  memset(msg, '\0', sizeof(msg));
  sprintf(msg, "%d %u %zu", op_code, event_id, num_seats);
  

  size_t i;
  for (i = 0; i < num_seats; ++i) {
      sprintf(msg + strlen(msg), " %zu %zu", xs[i], ys[i]);
  }
  send_msg(session_ID.fd_req, msg);

  memset(msg, '\0', sizeof(msg));
  while (msg[0] == '\0')
    read_msg(session_ID.fd_resp, msg, sizeof(msg));
  sscanf(msg, "%d", &ret);
  if (ret != 0)
    return 1;
  return 0;
}


int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  int op_code = 5, ret;
  size_t num_rows = 0, num_cols = 0;
  char msg[BUFFER_SIZE];
  memset(msg, '\0', sizeof(msg));
  sprintf(msg, "%d %u", op_code, event_id);
  send_msg(session_ID.fd_req, msg);
  memset(msg, '\0', sizeof(msg));
  while (msg[0] == '\0')
    read_msg(session_ID.fd_resp, msg, sizeof(msg));
  send_msg(out_fd, msg);
  sscanf(msg, "%d %lu %lu", &ret, &num_rows, &num_cols);
  char out[num_rows * num_cols];
  
  
  if (ret != 0)
    return 1;
  return 0;
}

/*
int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
*/