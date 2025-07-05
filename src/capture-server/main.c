/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 * Copyright (C) 2024 namazso <admin@namazso.eu>
 */

#include "common.h"
#include "network.h"

#include <signal.h>

#include "http.h"
#include "version.h"

static void usage(const char* name) {
    fprintf(stderr, "Usage: %s version|<port>|unix:<socket>\n", name);
}

int main(int argc, char* argv[]) {
    char* path = NULL;

    if (argc != 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    SOCKET server_fd = INVALID_SOCKET;
    struct sockaddr* server_addr = NULL;
    struct sockaddr_un server_addr_un;
    struct sockaddr_in server_addr_in;
    int server_addr_len = 0;

#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    if (0 == strcmp(argv[1], "version")) {
        puts(CXADC_VHS_SERVER_VERSION);
        exit(EXIT_SUCCESS);
    }
    else if (0 == strncmp(argv[1], "unix:", 5)) {
        path = argv[1] + 5;
        const size_t length = strlen(path);
        if (length == 0 || length >= 108) {
            errno = EINVAL;
            perror(NULL);
            exit(EXIT_FAILURE);
        }

        server_addr_un.sun_family = AF_UNIX;
        strcpy(server_addr_un.sun_path, path);
        server_addr = (struct sockaddr*)&server_addr_un;
        server_addr_len = sizeof(server_addr_un);
        remove(path);
    }
    else {
        long lport;
        if ((lport = atol(argv[1])) <= 0 || lport > 0xffff) {
            errno = EINVAL;
            perror(NULL);
            exit(EXIT_FAILURE);
        }
        server_addr_in.sin_family = AF_INET;
        server_addr_in.sin_addr.s_addr = INADDR_ANY;
        server_addr_in.sin_port = htons((uint16_t)lport);
        server_addr = (struct sockaddr*)&server_addr_in;
        server_addr_len = sizeof(server_addr_in);
    }

    if (_network_init() != 0) {
        fprintf(stderr, "network init failed: %s\n", _get_network_error());
        goto exit;
    }

    // create server socket
    server_fd = socket(server_addr->sa_family, SOCK_STREAM, 0);

    if (!IS_SOCKET_VALID(server_fd)) {
        fprintf(stderr, "socket failed: %s\n", _get_network_error());
        goto exit;
    }

    if (server_addr->sa_family == AF_INET) {
        int reuseaddr = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuseaddr, sizeof(reuseaddr))) {
            fprintf(stderr, "setsockopt failed: %s\n", _get_network_error());
            goto exit;
        }
    }

    // config socket
    if (bind(server_fd, server_addr, server_addr_len) < 0) {
        fprintf(stderr, "bind failed: %s\n", _get_network_error());
        goto exit;
    }

    // listen for connections
    if (listen(server_fd, 10) < 0) {
        fprintf(stderr, "listen failed: %s\n", _get_network_error());
        goto exit;
    }

    printf("server listening on %s\n", argv[1]);
    while (1) {
        // client info
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // accept client connection
        const SOCKET client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);

        if (!IS_SOCKET_VALID(client_fd)) {
            fprintf(stderr, "accept failed: %s\n", _get_network_error());
            continue;
        }

        // create a new thread to handle client request
        thrd_t thread_id;
        int err = 0;
        if ((err = thrd_create(&thread_id, http_thread, (void*)(intptr_t)client_fd)) != 0) {
            fprintf(stderr, "can't create http thread: %d\n", err);
            continue;
        }
        if ((err = thrd_detach(thread_id)) != 0) {
            fprintf(stderr, "can't detach http thread: %d\n", err);
            continue;
        }
    }

exit:
    _socket_close(server_fd);
    _network_cleanup();

    if (path != NULL) {
        remove(path);
    }

    return 0;
}
