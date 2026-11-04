// The structures of winsock2.h, ws2tcpip.h and winhttp.h: built by MSVC with the real
// headers and by WinCC with its own, the two lists must be the same (abi.ps1).
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define S(t) printf("%-44s %4d\n", #t, (int)sizeof(t))
#define O(t, m) printf("%-44s %4d\n", #t "." #m, (int)offsetof(t, m))
#define V(e) printf("%-44s %4d\n", #e, (int)(e))

int main(void)
{
    S(WSADATA); O(WSADATA, iMaxSockets); O(WSADATA, lpVendorInfo); O(WSADATA, szDescription); O(WSADATA, szSystemStatus);
    S(IN_ADDR); S(IN6_ADDR); S(SOCKADDR); S(SOCKADDR_IN); O(SOCKADDR_IN, sin_addr); O(SOCKADDR_IN, sin_zero);
    S(SOCKADDR_IN6); O(SOCKADDR_IN6, sin6_flowinfo); O(SOCKADDR_IN6, sin6_addr); O(SOCKADDR_IN6, sin6_scope_id);
    S(SOCKADDR_STORAGE); O(SOCKADDR_STORAGE, __ss_align); S(SCOPE_ID);
    S(ADDRINFOA); O(ADDRINFOA, ai_addrlen); O(ADDRINFOA, ai_canonname); O(ADDRINFOA, ai_addr); O(ADDRINFOA, ai_next);
    S(ADDRINFOW); O(ADDRINFOW, ai_next);
    S(fd_set); O(fd_set, fd_array); S(TIMEVAL); S(WSAPOLLFD); O(WSAPOLLFD, events); O(WSAPOLLFD, revents);
    S(LINGER); S(WSABUF); O(WSABUF, buf); S(WSAOVERLAPPED); S(WSANETWORKEVENTS); O(WSANETWORKEVENTS, iErrorCode);
    S(HOSTENT); O(HOSTENT, h_addrtype); O(HOSTENT, h_length); O(HOSTENT, h_addr_list);
    S(SERVENT); O(SERVENT, s_proto); O(SERVENT, s_port); S(PROTOENT); O(PROTOENT, p_proto);
    S(IP_MREQ); S(IPV6_MREQ); O(IPV6_MREQ, ipv6mr_interface);
    S(SOCKET); S(u_long); S(socklen_t); S(ADDRESS_FAMILY);

    S(HINTERNET); S(INTERNET_PORT); S(INTERNET_SCHEME);
    S(URL_COMPONENTS); O(URL_COMPONENTS, nScheme); O(URL_COMPONENTS, lpszHostName); O(URL_COMPONENTS, nPort);
    O(URL_COMPONENTS, lpszUserName); O(URL_COMPONENTS, lpszUrlPath); O(URL_COMPONENTS, dwExtraInfoLength);
    S(WINHTTP_PROXY_INFO); O(WINHTTP_PROXY_INFO, lpszProxy);
    S(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG); O(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG, lpszAutoConfigUrl);
    S(WINHTTP_AUTOPROXY_OPTIONS); O(WINHTTP_AUTOPROXY_OPTIONS, lpvReserved); O(WINHTTP_AUTOPROXY_OPTIONS, fAutoLogonIfChallenged);
    S(WINHTTP_ASYNC_RESULT); S(WINHTTP_WEB_SOCKET_ASYNC_RESULT); O(WINHTTP_WEB_SOCKET_ASYNC_RESULT, Operation);
    S(WINHTTP_WEB_SOCKET_STATUS); S(WINHTTP_WEB_SOCKET_BUFFER_TYPE);

    V(WINHTTP_WEB_SOCKET_SHUTDOWN_OPERATION); V(WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE);
    V(WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE); V(WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS);
    V(WINHTTP_WEB_SOCKET_EMPTY_CLOSE_STATUS); V(WINHTTP_WEB_SOCKET_SECURE_HANDSHAKE_ERROR_CLOSE_STATUS);

    // bit fields and macros at work
    SCOPE_ID id;
    id.Value = 0xA0000005;
    V(id.Zone); V(id.Level);
    IN_ADDR a;
    a.s_addr = 0x0100007f;
    V(a.S_un.S_un_b.s_b1); V(a.s_impno); V(a.S_un.S_un_w.s_w2);
    IN6_ADDR a6;
    memset(&a6, 0, sizeof a6);
    a6.s6_addr[15] = 1;
    V(a6.u.Word[7]);
    fd_set set;
    FD_ZERO(&set);
    FD_SET((SOCKET)5, &set); FD_SET((SOCKET)7, &set); FD_SET((SOCKET)5, &set); FD_SET((SOCKET)9, &set);
    V(set.fd_count);
    FD_CLR((SOCKET)7, &set);
    V(set.fd_count); V(set.fd_array[1]);
    V(htons(0x1234)); V(ntohl(0x01020304) == 0x04030201);
    V(inet_addr("10.20.30.40") == htonl(0x0a141e28));
    return 0;
}
