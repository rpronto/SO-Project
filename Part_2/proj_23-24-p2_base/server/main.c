#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

typedef struct threadArgs {
  int *session_id_status;
  int session_id, fd_req, fd_resp;
} threadArgs;


int findNextAvailableSessionID (int *session_id_status) {
  for (int id = 0 ; id < MAX_SESSION_COUNT ; id++) {
    if (session_id_status[id] == 0)
      return id;
  }
  return -1;
}

void *threadFunction(void *args) {
  threadArgs *thread_args = (threadArgs *)args;
  int *session_id_status = thread_args->session_id_status;
  int session_id = thread_args->session_id;
  int fd_req = thread_args->fd_req;
  int fd_resp = thread_args->fd_resp;
  char buffer[BUFFER_SIZE];  
  char op_code_str[2];
  char id_str[2];
  sprintf(id_str, "%d", session_id);
  send_msg(fd_resp, id_str);
  memset(buffer, '\0', sizeof(buffer));
  read_msg(fd_req, buffer, BUFFER_SIZE);
  while (1) {
    int ret, fd_aux;
    size_t num_seats = 0;
    unsigned int event_id = 0;
    size_t xs[num_seats];
    size_t ys[num_seats];
    size_t rows;
    size_t cols;
    const char *aux_file = "aux_file.txt";
    switch (buffer[0]) {
      case '2':
        session_id_status[session_id] = 0;
        pthread_exit(NULL);
      case '3':
        size_t num_rows = 0;
        size_t num_col = 0;
        char ret_str[2];
        sscanf(buffer, "%c %u %ld %ld", op_code_str, &event_id, &num_rows, &num_col);
        rows = num_rows;
        cols = num_col;
        ret = ems_create(event_id, num_rows, num_col);
        snprintf(ret_str, sizeof(ret), "%d", ret);
        send_msg(fd_resp, ret_str);
        break;
      case '4':
        int elements_already_read = 0;
        int i = 0;
        const char *ptr = buffer;
        sscanf(buffer, "%c %u %ld%n", op_code_str, &event_id, &num_seats, &elements_already_read);
        ptr += elements_already_read;
        while (sscanf(ptr, "%zu", &xs[i]) == 1) {
          ptr = strchr(ptr, ' ');
          if(ptr == NULL)
            break;
          ptr++;
          sscanf(ptr, "%zu", &ys[i]);
          ptr = strchr(ptr, ' ');
          if(ptr == NULL)
            break;
          ptr++;
          i++;
        }
        ret = ems_reserve(event_id, num_seats, xs, ys);
        snprintf(ret_str, sizeof(ret), "%d", ret);
        send_msg(fd_resp, ret_str);
        break;
      case '5':
        size_t num_elements = 2 * rows * cols + 100;
        char *msg = (char *)malloc(num_elements);
        if (msg == NULL) {
            fprintf(stderr, "Failed to allocate memory for msg\n");
            exit(1);
        }
        const char *msg_ptr = msg;
        memset(msg, 0, num_elements);
        sscanf(buffer, "%c %u", op_code_str, &event_id);
        fd_aux = open(aux_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd_aux < 0) {
          fprintf(stderr, "Failed open aux file\n");
          free(msg);
          close(fd_aux);
          unlink(aux_file);
          exit(1);
        }
        ret = ems_show(fd_aux, event_id);
        if(ret == 1) {
          send_msg(fd_resp, "1");
          close(fd_aux);
          unlink(aux_file);
          free(msg);
          break;
        }
        sprintf(msg, "%d %zu %zu\n", ret, rows, cols);
        msg_ptr += strlen(msg);
        lseek(fd_aux, 0, SEEK_SET);
        read(fd_aux, msg + strlen(msg), num_elements); 
        send_msg(fd_resp, msg);
        close(fd_aux);
        unlink(aux_file);
        free(msg);
        break;
      case '6':
        fd_aux = open(aux_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd_aux < 0) {
          fprintf(stderr, "Failed open aux file\n");
          exit(1);
        }

        ret = ems_list_events(fd_aux);
        if(ret == 1) {
          send_msg(fd_resp, "1");
          close(fd_aux);
          unlink(aux_file);
          break;
        }
        unsigned int *ids = NULL;
        char aux_buffer[BUFFER_SIZE];
        size_t num_events = 0;
        const char *aux_ptr = aux_buffer;

        lseek(fd_aux, 0, SEEK_SET);
        memset(aux_buffer, '\0', sizeof(aux_buffer));
        read_msg(fd_aux, aux_buffer, BUFFER_SIZE);
        
        while(1) {
          if (sscanf(aux_ptr, "Event: %d", &event_id) == 1) {
            num_events++;
            ids = realloc(ids, num_events * sizeof(unsigned int));
            if (ids == NULL) {
              fprintf(stderr, "Failed to realloc ids array\n");
              close(fd_aux);
              unlink(aux_file);
              free(ids);
              exit(1);
            }
            ids[num_events - 1] = event_id;
          }
          aux_ptr = strchr(aux_ptr, '\n');
          if (aux_ptr == NULL)
            break;
          aux_ptr++;
        }
        
        char *list_events_msg = (char *)malloc(sizeof(ret) + sizeof(num_events) + (num_events * sizeof(unsigned int)) + 2);
        sprintf(list_events_msg, "%d %zu", ret, num_events);
        for (size_t j = 0; j < num_events; ++j) 
          sprintf(list_events_msg + strlen(list_events_msg), " %u", ids[j]);
        send_msg(fd_resp, list_events_msg);
        close(fd_aux);
        unlink(aux_file);
        free(ids);
        free(list_events_msg);
        break;
    }
    //TODO: Read from pipe
    memset(buffer, '\0', sizeof(buffer));
    read_msg(fd_req, buffer, BUFFER_SIZE);
    //TODO: Write new client to the producer-consumer buffer
  }
}


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
  int session_id_status[MAX_SESSION_COUNT] = {0};
  pthread_t threads[MAX_SESSION_COUNT];
  threadArgs args[MAX_SESSION_COUNT]; 
  char buffer[BUFFER_SIZE];
  char op_code_str[2];
  char req_pipe[41];
  char resp_pipe[41];
  int fd_req;
  int fd_resp;
  
  unlink(pipeServer);

  if(mkfifo(pipeServer, 0777) < 0) {
      fprintf(stderr, "Failed to create named pipe\n");
      return 1;
  }
  
  if((fd_serv = open(pipeServer, O_RDONLY)) < 0) {
    fprintf(stderr, "Failed to open named pipe\n");
    return 1;
  }

  while(1){
    int id = -1;
    memset(buffer, '\0', sizeof(buffer));
    read_msg(fd_serv, buffer, BUFFER_SIZE);
    if (buffer[0] == '1') {
      while((id = findNextAvailableSessionID(session_id_status)) == -1)
        continue;
      sscanf(buffer, "%c %s %s", op_code_str, req_pipe, resp_pipe);
      if((fd_req = open(req_pipe, O_RDONLY)) < 0) {
        fprintf(stderr, "Failed to open sender named pipe\n");
        return 1;
      }

      if((fd_resp = open(resp_pipe, O_WRONLY)) < 0) {
        fprintf(stderr, "Failed to open receiver named pipe\n");
        return 1;
      }
      args[id].session_id_status = session_id_status;
      args[id].session_id = id;
      args[id].fd_req = fd_req;
      args[id].fd_resp = fd_resp;
      pthread_create(&threads[id], NULL, threadFunction, (void *)&args[id]);
    }
  }
 
  //TODO: Close Server
  close(fd_serv);

  ems_terminate();
}