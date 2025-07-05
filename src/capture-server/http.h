/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2024 namazso <admin@namazso.eu>
 */

#pragma once

typedef void(servefile_fn)(int fd, int argc, char** argv);

int http_thread(void* arg);

struct served_file {
    const char* path;
    const char* headers;
    servefile_fn* fn;
};
