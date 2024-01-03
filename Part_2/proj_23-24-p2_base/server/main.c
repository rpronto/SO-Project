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
#include <signal.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

pthread_mutex_t mutex;
pthread_cond_t cond;
int buffer_counter = 0;
int activate_signal = 0;

typedef struct request {
  int fd_req, fd_resp;
  struct request *next;
} request;

typedef struct threadArgs {
  int session_id;
  request **producer_consumer;
} threadArgs;

void sig_handler(int sig) {
  if (sig == SIGUSR1)
    activate_signal = 1;
}

void *threadFunction(void *args) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  threadArgs *thread_args = (threadArgs *)args;
  int fd_req, fd_resp, quit = 0, session_id = thread_args->session_id;
  request **producer_consumer = thread_args->producer_consumer;
  char buffer[BUFFER_SIZE], op_code_str[2], id_str[2];  
  memset(buffer, '\0', sizeof(buffer));
  while(1) {
    pthread_mutex_lock(&mutex);
    while(buffer_counter == 0) {
      pthread_cond_wait(&cond, &mutex);
    }
    fd_req = (*producer_consumer)->fd_req;
    fd_resp = (*producer_consumer)->fd_resp;
    request *aux = *producer_consumer;
    *producer_consumer = (*producer_consumer)->next;
    free(aux);
    buffer_counter--;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    sprintf(id_str, "%d", session_id);
    send_msg(fd_resp, id_str);
    
    char *aux_file_base = "aux_file_";
    size_t aux_file_size = strlen(aux_file_base) + strlen(id_str) + 5; 
    char *aux_file = malloc(aux_file_size * sizeof(char));  
    memset(aux_file, '\0', aux_file_size * sizeof(char));
    strcat(aux_file, aux_file_base);
    strcat(aux_file, id_str);
    strcat(aux_file, ".txt");
    
    while (1) {
      int ret, fd_aux;
      unsigned int event_id = 0;
      size_t rows, cols;
      switch (buffer[0]) {
        case '2':
          quit = 1;
          memset(buffer, '\0', sizeof(buffer));
          close(fd_req);
          close(fd_resp);
          free(aux_file);
          break;
        case '3':
          size_t num_rows = 0, num_col = 0;
          char ret_str[2];
          sscanf(buffer, "%c %u %ld %ld", op_code_str, &event_id, &num_rows, &num_col);
          rows = num_rows;
          cols = num_col;
          ret = ems_create(event_id, num_rows, num_col);
          snprintf(ret_str, sizeof(ret), "%d", ret);
          send_msg(fd_resp, ret_str);
          break;
        case '4':
          int elements_already_read = 0, i = 0;
          const char *ptr = buffer;
          size_t num_seats = 0;
          sscanf(buffer, "%c %u %ld%n", op_code_str, &event_id, &num_seats, &elements_already_read);
          ptr += elements_already_read + 1;
          size_t *xs = (size_t*)malloc(sizeof(size_t) * num_seats);
          if (xs == NULL) {
            fprintf(stderr, "Failed to allocate memory for xs\n");
            free(aux_file);
            pthread_exit(NULL);
          }
          size_t *ys = (size_t*)malloc(sizeof(size_t) * num_seats);
          if (ys == NULL) {
            fprintf(stderr, "Failed to allocate memory for ys\n");
            free(aux_file);
            pthread_exit(NULL);
          }
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
          free(xs);
          free(ys);
          break;
        case '5':
          size_t num_elements = 2 * rows * cols + 100;
          char *msg = (char *)malloc(num_elements);
          if (msg == NULL) {
            fprintf(stderr, "Failed to allocate memory for msg\n");
            free(aux_file);
            pthread_exit(NULL);
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
            free(aux_file);
            pthread_exit(NULL);
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
            free(aux_file);
            pthread_exit(NULL);
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
                free(aux_file);
                pthread_exit(NULL);
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
      if (quit == 1){
        quit = 0;
        break;
      }
      // Read from pipe
      memset(buffer, '\0', sizeof(buffer));
      read_msg(fd_req, buffer, BUFFER_SIZE);
    }
  }
  pthread_exit(NULL);
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

  // Intialize server, create worker threads
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
  char *pipeServer = argv[1];
  request *producer_consumer = NULL;
  int fd_serv, fd_req, fd_resp;
  pthread_t threads[MAX_SESSION_COUNT];
  threadArgs args[MAX_SESSION_COUNT]; 
  char buffer[BUFFER_SIZE], op_code_str[2], req_pipe[41], resp_pipe[41];
  
  unlink(pipeServer);

  if(mkfifo(pipeServer, 0777) < 0) {
      fprintf(stderr, "Failed to create named pipe\n");
      return 1;
  }
  
  if((fd_serv = open(pipeServer, O_RDONLY)) < 0) {
    fprintf(stderr, "Failed to open named pipe\n");
    return 1;
  }

  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    args[i].session_id = i;
    args[i].producer_consumer = &producer_consumer;
    if (pthread_create(&threads[i], NULL, threadFunction, (void *)&args[i]) != 0) {
      fprintf(stderr, "Failed to create thread\n");
      return 1;
    }
  }

  // Write new client to the producer_consumer buffer
  while(1) {
    if (signal(SIGUSR1, sig_handler) == SIG_ERR) {
      return 1;
    }
    memset(buffer, '\0', sizeof(buffer));
    read_msg(fd_serv, buffer, BUFFER_SIZE);
    if ((buffer[0] == '1') && (activate_signal == 0)) {
      memset(op_code_str, '\0', sizeof(op_code_str));
      memset(req_pipe, '\0', sizeof(req_pipe));
      memset(resp_pipe, '\0', sizeof(resp_pipe));
      sscanf(buffer, "%c %s %s", op_code_str, req_pipe, resp_pipe);
      if((fd_req = open(req_pipe, O_RDONLY)) < 0) {
        fprintf(stderr, "Failed to open sender named pipe\n");
        return 1;
      }
      if((fd_resp = open(resp_pipe, O_WRONLY)) < 0) {
        fprintf(stderr, "Failed to open receiver named pipe\n");
        return 1;
      }
      while (buffer_counter == MAX_SESSION_COUNT) {
        pthread_cond_wait(&cond, &mutex);
      }
      request *new_request = (request *)malloc(sizeof(request));
      if (new_request == NULL) {
        fprintf(stderr, "Failed to allocate memory for new_request\n");
        return 1;
      }
      new_request->fd_req = fd_req;
      new_request->fd_resp = fd_resp;
      new_request->next = NULL;
      if (producer_consumer == NULL) {
        producer_consumer = new_request;
      } else {
        request *aux = producer_consumer;
        while (aux->next != NULL) {
          aux = aux->next;
        }
        aux->next = new_request;
      }
      pthread_mutex_lock(&mutex);
      buffer_counter++;
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&mutex);
    }
    if (activate_signal != 0) {
      char *aux_serv_file = "aux_serv_file.txt";
      int fd_serv_aux = open(aux_serv_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
      if (fd_serv_aux < 0) {
        fprintf(stderr, "Failed open aux server file\n");
        return 1;
      }
      int ret = ems_list_events(fd_serv_aux);
      if(ret == 1) {
        close(fd_serv_aux);
        unlink(aux_serv_file);
        return 1;
      }
      unsigned int *ids = NULL, event_id;
      char aux_buffer[BUFFER_SIZE];
      size_t num_events = 0;
      const char *aux_ptr = aux_buffer;
      lseek(fd_serv_aux, 0, SEEK_SET);
      memset(aux_buffer, '\0', sizeof(aux_buffer));
      read_msg(fd_serv_aux, aux_buffer, BUFFER_SIZE);
      close(fd_serv_aux);
      unlink(aux_serv_file);
      while(1) {
        if (sscanf(aux_ptr, "Event: %d", &event_id) == 1) {
          num_events++;
          ids = realloc(ids, num_events * sizeof(unsigned int));
          if (ids == NULL) {
            fprintf(stderr, "Failed to realloc ids array\n");
            free(ids);
            return 1;
          }
          ids[num_events - 1] = event_id;
        }
        aux_ptr = strchr(aux_ptr, '\n');
        if (aux_ptr == NULL)
          break;
        aux_ptr++;
      } 
      for (size_t i = 0; i < num_events; i++) {
        printf("Event: %d\n", ids[i]);
        ems_show(STDOUT_FILENO, ids[i]);
      }
      free(ids);
      activate_signal = 0;
    }
  }

  // Close Server
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Failed to join thread\n");
      return 1;
    }
  }
  
  close(fd_serv);
  unlink(pipeServer);

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);

  ems_terminate();
}