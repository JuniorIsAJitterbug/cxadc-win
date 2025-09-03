#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "version.h"

static void usage(const char* name) {
  fprintf(stderr, "Usage: %s version|<port>|unix:<socket>\n", name);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  signal(SIGPIPE, SIG_IGN);

  int server_fd;

  if (0 == strcmp(argv[1], "version")) {
    puts(CXADC_VHS_SERVER_VERSION);
    exit(EXIT_SUCCESS);
  } else if (0 == strncmp(argv[1], "unix:", 5)) {
    const char* path = argv[1] + 5;
    const int length = strlen(path);
    if (length == 0 || length >= 108) {
      errno = EINVAL;
      perror(NULL);
      exit(EXIT_FAILURE);
    }

    // create server socket
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
      perror("socket failed");
      exit(EXIT_FAILURE);
    }

    int reuseaddr = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
      perror("setsockopt failed");
      exit(EXIT_FAILURE);
    }

    // config socket
    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, path);

    // bind socket to port
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }
  } else {
    long lport;
    if ((lport = atol(argv[1])) <= 0 || lport > 0xffff) {
      errno = EINVAL;
      perror(NULL);
      exit(EXIT_FAILURE);
    }

    const uint16_t port = lport;

    // create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      perror("socket failed");
      exit(EXIT_FAILURE);
    }

    int reuseaddr = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
      perror("setsockopt failed");
      exit(EXIT_FAILURE);
    }

    // config socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind socket to port
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }
  }

  // listen for connections
  if (listen(server_fd, 10) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("server listening on %s\n", argv[1]);
  while (1) {
    // client info
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // accept client connection
    const int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);

    if (client_fd < 0) {
      perror("accept failed");
      continue;
    }

    // create a new thread to handle client request
    pthread_t thread_id;
    int err = 0;
    if ((err = pthread_create(&thread_id, NULL, http_thread, (void*)(intptr_t)client_fd)) != 0) {
      fprintf(stderr, "can't create http thread: %d\n", err);
      continue;
    }
    if ((err = pthread_detach(thread_id)) != 0) {
      fprintf(stderr, "can't detach http thread: %d\n", err);
      continue;
    }
  }

  close(server_fd);
  return 0;
}
