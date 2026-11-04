// What the files of the net library share: errors per thread, UTF-8 <-> UTF-16, the
// sockets start, a growing byte buffer.
#pragma once
#define NET_INTERNAL
#include <net.h>
#include <mswsock.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

// ---- errors (core.c) ----
void net_set_error(const char *text);               // text as it is ("" clears)
void net_set_errorf(const char *format, ...);
void net_set_system_error(const char *what, DWORD code);    // "what: <the words of Windows>"

// ---- text (core.c) ----
wchar_t *net_wide(const char *utf8);                // malloc; 0 -> 0
char *net_utf8(const wchar_t *wide, int length);    // malloc; length -1 = up to the 0
char *net_strdup(const char *s);
char *net_strndup(const char *s, size_t n);
int net_same(const char *a, const char *b);         // the same letters, big or small (ASCII)
char *net_find_header(const char *headers, const char *name);  // "Name: value\r\n" lines -> value; malloc

// ---- sockets (core.c) ----
int net_startup(void);                              // WSAStartup once; 0 = no sockets
int net_timeout_ms(void);                           // net_set_timeout
NetProgress net_progress_fn(void **user);           // net_on_progress of this thread

// ---- a growing buffer ----
typedef struct {
    char *data;
    size_t size, cap;
} NetBuf;
int net_buf_add(NetBuf *b, const void *data, size_t size);  // 0 = no memory; data stays 0-ended
int net_buf_printf(NetBuf *b, const char *format, ...);

// ---- socket of the library (tcp.c) ----
struct NetSocket {
    SOCKET s;
    int udp;
    char peer[80];
    int start, end;                                 // what recv took but nobody read yet
    char buf[4096];
};
NetSocket *net_socket_new(SOCKET s);
void net_addr_text(const struct sockaddr *a, int len, char *out, int size);     // "ip:port"
