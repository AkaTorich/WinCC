// UDP: single packets to an address (or to everyone of the network: 255.255.255.255)
// and from anyone, IPv4.
#include "net_int.h"

NetSocket *net_udp(int port)
{
    if (!net_startup())
        return 0;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        net_set_system_error("Сокет не создан", (DWORD)WSAGetLastError());
        return 0;
    }
    BOOL on = 1, off = 0;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof on);
    // a packet sent where nobody listens must not break the next recvfrom (error 10054)
    DWORD got;
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof off, 0, 0, &got, 0, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    if (bind(s, (struct sockaddr *)&a, sizeof a)) {
        int err = WSAGetLastError();
        closesocket(s);
        if (err == WSAEADDRINUSE)
            net_set_errorf("Порт %d занят", port);
        else
            net_set_system_error("Порт не открылся", (DWORD)err);
        return 0;
    }
    NetSocket *n = net_socket_new(s);
    if (n) {
        n->udp = 1;
        snprintf(n->peer, sizeof n->peer, "*:%d", net_local_port(n));
        net_set_error("");
    }
    return n;
}

int net_udp_send(NetSocket *s, const char *host, int port, const void *data, int size)
{
    char serv[16];
    snprintf(serv, sizeof serv, "%d", port);
    struct addrinfo hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    int e = getaddrinfo(host, serv, &hints, &res);
    if (e) {
        net_set_system_error(host, (DWORD)e);
        return -1;
    }
    int n = sendto(s->s, data, size, 0, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    if (n < 0) {
        net_set_system_error("Не отправилось", (DWORD)WSAGetLastError());
        return -1;
    }
    net_set_error("");
    return n;
}

int net_udp_recv(NetSocket *s, void *buffer, int size, char *from, int from_size)
{
    struct sockaddr_storage a;
    int len = sizeof a;
    int n = recvfrom(s->s, buffer, size, 0, (struct sockaddr *)&a, &len);
    if (n < 0) {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT)
            net_set_error("Ничего не пришло");
        else
            net_set_system_error("Не получилось прочитать", (DWORD)err);
        return -1;
    }
    if (from && from_size > 0)
        net_addr_text((struct sockaddr *)&a, len, from, from_size);
    net_set_error("");
    return n;
}
