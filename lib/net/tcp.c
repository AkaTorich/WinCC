// TCP: connect to a server (with the timeout of net_set_timeout), wait for others
// (IPv6 and IPv4 on one socket), send all, read what came -- by bytes or by lines.
#include "net_int.h"

NetSocket *net_socket_new(SOCKET s)
{
    NetSocket *n = calloc(1, sizeof *n);
    if (!n) {
        closesocket(s);
        net_set_error("Не хватило памяти");
        return 0;
    }
    n->s = s;
    return n;
}

// "1.2.3.4:80", "[::1]:80"; an IPv4 address inside IPv6 (::ffff:1.2.3.4) is written as IPv4
void net_addr_text(const struct sockaddr *a, int len, char *out, int size)
{
    char host[NI_MAXHOST] = "?", serv[NI_MAXSERV] = "?";
    getnameinfo(a, len, host, sizeof host, serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV);
    const char *h = host;
    if (strncmp(h, "::ffff:", 7) == 0 && strchr(h + 7, '.'))
        h += 7;
    if (strchr(h, ':'))
        snprintf(out, (size_t)size, "[%s]:%s", h, serv);
    else
        snprintf(out, (size_t)size, "%s:%s", h, serv);
}

// connect, but wait no longer than ms (0 = as long as Windows waits)
static int connect_within(SOCKET s, const struct sockaddr *a, int len, int ms)
{
    if (!ms)
        return connect(s, a, len) == 0;
    u_long on = 1, off = 0;
    ioctlsocket(s, FIONBIO, &on);
    int ok = connect(s, a, len) == 0;
    if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set w, e;
        FD_ZERO(&w);
        FD_ZERO(&e);
        FD_SET(s, &w);
        FD_SET(s, &e);
        struct timeval tv;
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        int n = select(0, 0, &w, &e, &tv);
        if (n == 0)
            WSASetLastError(WSAETIMEDOUT);
        else if (n > 0) {
            int err = 0, elen = sizeof err;
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &elen);
            if (err)
                WSASetLastError(err);
            ok = err == 0 && FD_ISSET(s, &w);
        }
    }
    int keep = WSAGetLastError();
    ioctlsocket(s, FIONBIO, &off);
    WSASetLastError(keep);
    return ok;
}

NetSocket *net_connect(const char *host, int port)
{
    if (!net_startup())
        return 0;
    char serv[16];
    snprintf(serv, sizeof serv, "%d", port);
    struct addrinfo hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int e = getaddrinfo(host, serv, &hints, &res);
    if (e) {
        net_set_system_error(host, (DWORD)e);
        return 0;
    }
    int err = WSAEHOSTUNREACH;
    for (struct addrinfo *a = res; a; a = a->ai_next) {         // every address of the name, in turn
        SOCKET s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == INVALID_SOCKET) {
            err = WSAGetLastError();
            continue;
        }
        if (connect_within(s, a->ai_addr, (int)a->ai_addrlen, net_timeout_ms())) {
            NetSocket *n = net_socket_new(s);
            if (n)
                net_addr_text(a->ai_addr, (int)a->ai_addrlen, n->peer, sizeof n->peer);
            freeaddrinfo(res);
            if (n)
                net_set_error("");
            return n;
        }
        err = WSAGetLastError();
        closesocket(s);
    }
    freeaddrinfo(res);
    char what[300];
    snprintf(what, sizeof what, "%s:%d", host, port);
    net_set_system_error(what, (DWORD)err);
    return 0;
}

// local: only this computer can connect (127.0.0.1); else every network of it, IPv6 and IPv4
static NetSocket *listen_on(int port, int local)
{
    if (!net_startup())
        return 0;
    // IPv6 with IPv4 inside: one socket hears both; a Windows without IPv6 -- IPv4 alone
    SOCKET s = local ? INVALID_SOCKET : socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    int ok = 0;
    if (s != INVALID_SOCKET) {
        DWORD off = 0;
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, sizeof off);
        struct sockaddr_in6 a;
        memset(&a, 0, sizeof a);
        a.sin6_family = AF_INET6;
        a.sin6_port = htons((u_short)port);
        ok = bind(s, (struct sockaddr *)&a, sizeof a) == 0;
        if (!ok) {
            int err = WSAGetLastError();
            closesocket(s);
            s = INVALID_SOCKET;
            if (err == WSAEADDRINUSE) {
                net_set_errorf("Порт %d занят", port);
                return 0;
            }
        }
    }
    if (s == INVALID_SOCKET) {
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            net_set_system_error("Сокет не создан", (DWORD)WSAGetLastError());
            return 0;
        }
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_port = htons((u_short)port);
        if (local)
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(s, (struct sockaddr *)&a, sizeof a)) {
            int err = WSAGetLastError();
            closesocket(s);
            if (err == WSAEADDRINUSE)
                net_set_errorf("Порт %d занят", port);
            else
                net_set_system_error("Порт не открылся", (DWORD)err);
            return 0;
        }
    }
    if (listen(s, SOMAXCONN)) {
        net_set_system_error("Порт не открылся", (DWORD)WSAGetLastError());
        closesocket(s);
        return 0;
    }
    NetSocket *n = net_socket_new(s);
    if (n) {
        snprintf(n->peer, sizeof n->peer, local ? "127.0.0.1:%d" : "*:%d", net_local_port(n));
        net_set_error("");
    }
    return n;
}

NetSocket *net_listen(int port) { return listen_on(port, 0); }
NetSocket *net_listen_local(int port) { return listen_on(port, 1); }

NetSocket *net_accept(NetSocket *server)
{
    struct sockaddr_storage a;
    int len = sizeof a;
    SOCKET s = accept(server->s, (struct sockaddr *)&a, &len);
    if (s == INVALID_SOCKET) {
        net_set_system_error("Соединение не принято", (DWORD)WSAGetLastError());
        return 0;
    }
    NetSocket *n = net_socket_new(s);
    if (n) {
        net_addr_text((struct sockaddr *)&a, len, n->peer, sizeof n->peer);
        net_set_error("");
    }
    return n;
}

int net_send(NetSocket *s, const void *data, int size)
{
    const char *p = data;
    int left = size;
    while (left > 0) {
        int n = send(s->s, p, left, 0);
        if (n <= 0) {
            net_set_system_error("Не отправилось", (DWORD)WSAGetLastError());
            return -1;
        }
        p += n;
        left -= n;
    }
    net_set_error("");
    return size;
}

int net_send_text(NetSocket *s, const char *text)
{
    return net_send(s, text, (int)strlen(text));
}

int net_printf(NetSocket *s, const char *format, ...)
{
    char small[1024];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(small, sizeof small, format, ap);
    va_end(ap);
    if (n < 0)
        return -1;
    if (n < (int)sizeof small)
        return net_send(s, small, n);
    char *big = malloc((size_t)n + 1);
    if (!big) {
        net_set_error("Не хватило памяти");
        return -1;
    }
    va_start(ap, format);
    vsnprintf(big, (size_t)n + 1, format, ap);
    va_end(ap);
    n = net_send(s, big, n);
    free(big);
    return n;
}

// more bytes into s->buf; >0, 0 = closed, -1 = error
static int fill(NetSocket *s)
{
    if (s->start == s->end)
        s->start = s->end = 0;
    else if (s->end == (int)sizeof s->buf) {
        memmove(s->buf, s->buf + s->start, (size_t)(s->end - s->start));
        s->end -= s->start;
        s->start = 0;
    }
    int n = recv(s->s, s->buf + s->end, (int)sizeof s->buf - s->end, 0);
    if (n < 0) {
        net_set_system_error("Не получилось прочитать", (DWORD)WSAGetLastError());
        return -1;
    }
    s->end += n;
    return n;
}

int net_recv(NetSocket *s, void *buffer, int size)
{
    if (s->start < s->end) {                        // what a line read left over comes first
        int n = s->end - s->start < size ? s->end - s->start : size;
        memcpy(buffer, s->buf + s->start, (size_t)n);
        s->start += n;
        net_set_error("");
        return n;
    }
    int n = recv(s->s, buffer, size, 0);
    if (n < 0)
        net_set_system_error("Не получилось прочитать", (DWORD)WSAGetLastError());
    else
        net_set_error("");
    return n;
}

int net_recv_all(NetSocket *s, void *buffer, int size)
{
    char *p = buffer;
    while (size > 0) {
        int n = net_recv(s, p, size);
        if (n <= 0) {
            if (n == 0)
                net_set_error("Соединение закрыто раньше, чем всё пришло");
            return 0;
        }
        p += n;
        size -= n;
    }
    return 1;
}

char *net_recv_line(NetSocket *s)
{
    NetBuf line = { 0, 0, 0 };
    for (;;) {
        if (s->start == s->end) {
            int n = fill(s);
            if (n <= 0) {
                if (n == 0 && line.size) {          // the last line without "\n"
                    net_set_error("");
                    return line.data;
                }
                free(line.data);
                return 0;
            }
        }
        char *nl = memchr(s->buf + s->start, '\n', (size_t)(s->end - s->start));
        int take = nl ? (int)(nl - (s->buf + s->start)) : s->end - s->start;
        if (!net_buf_add(&line, s->buf + s->start, (size_t)take)) {
            free(line.data);
            net_set_error("Не хватило памяти");
            return 0;
        }
        s->start += take;
        if (nl) {
            s->start++;
            if (line.size && line.data[line.size - 1] == '\r')
                line.data[--line.size] = 0;
            net_set_error("");
            return line.data;
        }
    }
}

int net_ready(NetSocket *s, int ms)
{
    if (s->start < s->end)
        return 1;
    fd_set r;
    FD_ZERO(&r);
    FD_SET(s->s, &r);
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select(0, &r, 0, 0, &tv) > 0;
}

void net_socket_timeout(NetSocket *s, int ms)
{
    DWORD t = ms > 0 ? (DWORD)ms : 0;
    setsockopt(s->s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof t);
    setsockopt(s->s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof t);
}

int net_local_port(NetSocket *s)
{
    struct sockaddr_storage a;
    int len = sizeof a;
    if (getsockname(s->s, (struct sockaddr *)&a, &len))
        return 0;
    return ntohs(((struct sockaddr_in *)&a)->sin_port);    // the port lies at the same place in IPv4 and IPv6
}

const char *net_peer(NetSocket *s)
{
    return s->peer;
}

SOCKET net_handle(NetSocket *s)
{
    return s->s;
}

void net_close(NetSocket *s)
{
    if (!s)
        return;
    if (!s->udp)
        shutdown(s->s, SD_SEND);
    closesocket(s->s);
    free(s);
}
