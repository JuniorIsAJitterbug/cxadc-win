#include "http.h"

#include "files.h"

#include <alloca.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void http_serve(int fd, const char* method, char* uri) {
  if (0 != strcmp(method, "GET")) {
    dprintf(fd, "HTTP/1.0 405 Method Not Allowed\r\n\r\n");
    return;
  }

  char** argv = NULL;
  int argc = 0;
  char* args_begin = strchr(uri, '?');
  if (args_begin != NULL) {
    *args_begin++ = 0;
    argc = 1;
    for (const char* p = args_begin; *p; ++p)
      if (*p == '&')
        argc += 1;
    argv = (char**)alloca(sizeof(char*) * argc);
    char** args_it = argv;
    *args_it++ = args_begin;
    for (char* p = args_begin; *p; ++p) {
      if (*p == '&') {
        *p = 0;
        *args_it++ = p + 1;
      }
    }
  }

  for (const struct served_file* file = SERVED_FILES; file->path; ++file) {
    if (0 != strcmp(file->path, uri))
      continue;

    dprintf(fd, "HTTP/1.0 200 OK\r\n%s\r\n", file->headers ? file->headers : "");
    file->fn(fd, argc, argv);
    return;
  }

  dprintf(fd, "HTTP/1.0 404 Not Found\r\n\r\n");
}

void* http_thread(void* arg) {
  const int client_fd = (int)(intptr_t)arg;

  do {
    char buf[0x1000];
    ssize_t len = 0;
    do {
      int cur = read(client_fd, buf + len, sizeof(buf) - len - 1);
      len += cur;
      if (cur <= 0 || len == sizeof(buf) - 1) {
        len = 0;
        break;
      }
      buf[len] = 0;
    } while (NULL == strstr(buf, "\r\n\r\n"));

    if (!len)
      break;

    char method[8];
    int version1, version2;
    char uri[128];
    if (4 != sscanf(buf, "%7s %127s HTTP/%d.%d\r\n", method, uri, &version1, &version2)) {
      dprintf(client_fd, "HTTP/1.0 400 Bad Request\r\n\r\n");
      break;
    }

    http_serve(client_fd, method, uri);
  } while (0);
  close(client_fd);
  return NULL;
}
