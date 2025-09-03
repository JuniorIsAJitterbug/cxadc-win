#pragma once

typedef void(servefile_fn)(int fd, int argc, char** argv);

void* http_thread(void* arg);

struct served_file {
  const char* path;
  const char* headers;
  servefile_fn* fn;
};
