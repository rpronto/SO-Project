#include "io.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

int parse_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    ssize_t read_bytes = read(fd, buf + i, 1);
    if (read_bytes == -1) {
      return 1;
    } else if (read_bytes == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

int print_uint(int fd, unsigned int value) {
  char buffer[16];
  size_t i = 16;

  for (; value > 0; value /= 10) {
    buffer[--i] = '0' + (char)(value % 10);
  }

  if (i == 16) {
    buffer[--i] = '0';
  }

  while (i < 16) {
    ssize_t written = write(fd, buffer + i, 16 - i);
    if (written == -1) {
      return 1;
    }

    i += (size_t)written;
  }

  return 0;
}

int print_str(int fd, const char *str) {
  size_t len = strlen(str);
  while (len > 0) {
    ssize_t written = write(fd, str, len);
    if (written == -1) {
      return 1;
    }

    str += (size_t)written;
    len -= (size_t)written;
  }

  return 0;
}

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

void read_msg (int fd, char *buffer, size_t buffer_size) {
    ssize_t bytes_read = read(fd, buffer, buffer_size - 1);
    buffer[bytes_read] = 0;
    if (bytes_read < 0) {
      fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
}
