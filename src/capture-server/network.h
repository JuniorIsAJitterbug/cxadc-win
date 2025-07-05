/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

#ifdef _WIN32
    #include "common.h"
    
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <afunix.h>  
    
    #pragma comment(lib, "ws2_32.lib")
    
    #define IS_SOCKET_VALID(socket)           ((socket) != INVALID_SOCKET)
    
    #define _get_network_error()              win32_get_err_msg(WSAGetLastError())
    #define _network_init                     win32_init_winsock
    #define _network_cleanup                  win32_deinit_winsock
    
    #define _socket_read(s, buf, len)         recv((s), (buf), (int)(len), 0)
    #define _socket_write(s, buf, len)        send((s), (buf), (int)(len), 0)
    #define _socket_printf                    win32_socket_printf
    #define _socket_close                     closesocket
    
    
    static void win32_socket_printf(SOCKET sock, char* msg, ...) {
        va_list va = NULL;
        va_start(va, msg);
    
        const int buf_sz = _vscprintf(msg, va) + 1;
        char* buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buf_sz);
    
        if (buf != NULL) {
            _vsnprintf_s(buf, buf_sz, _TRUNCATE, msg, va);
            send(sock, buf, (int)strlen(buf), 0);
            HeapFree(GetProcessHeap(), 0, buf);
        }
    
        va_end(msg);
    }
    
    __forceinline static int win32_init_winsock() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    
    __forceinline static int win32_deinit_winsock() {
        return WSACleanup();
    }
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>

    #define IS_SOCKET_VALID(socket)           ((socket) >= 0)
    #define SOCKET                            int
    #define INVALID_SOCKET                    -1

    #define _get_network_error()              strerror(errno)
    #define _network_init()                   (0)
    #define _network_cleanup()                ((void)0)

    #define _socket_read                      read
    #define _socket_write                     write
    #define _socket_printf                    dprintf
    #define _socket_close                     close
#endif
