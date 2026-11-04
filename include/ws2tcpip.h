// ws2tcpip.h of WinCC: names to addresses and back for IPv4 and IPv6 (getaddrinfo,
// getnameinfo), addresses as text (inet_pton, inet_ntop).
#pragma once
#ifndef _WS2TCPIP_H_
#define _WS2TCPIP_H_
#include <winsock2.h>

#define AI_PASSIVE 0x00000001
#define AI_CANONNAME 0x00000002
#define AI_NUMERICHOST 0x00000004
#define AI_NUMERICSERV 0x00000008
#define AI_DNS_ONLY 0x00000010
#define AI_ALL 0x00000100
#define AI_ADDRCONFIG 0x00000400
#define AI_V4MAPPED 0x00000800
#define AI_NON_AUTHORITATIVE 0x00004000
#define AI_SECURE 0x00008000
#define AI_RETURN_PREFERRED_NAMES 0x00010000
#define AI_FQDN 0x00020000
#define AI_FILESERVER 0x00040000

#define NI_NOFQDN 0x01
#define NI_NUMERICHOST 0x02
#define NI_NAMEREQD 0x04
#define NI_NUMERICSERV 0x08
#define NI_DGRAM 0x10
#define NI_MAXHOST 1025
#define NI_MAXSERV 32

#define INET_ADDRSTRLEN 22
#define INET6_ADDRSTRLEN 65

#define EAI_AGAIN WSATRY_AGAIN
#define EAI_BADFLAGS WSAEINVAL
#define EAI_FAIL WSANO_RECOVERY
#define EAI_FAMILY WSAEAFNOSUPPORT
#define EAI_MEMORY WSA_NOT_ENOUGH_MEMORY
#define EAI_NOSECURENAME WSA_SECURE_HOST_NOT_FOUND
#define EAI_NODATA EAI_NONAME
#define EAI_NONAME WSAHOST_NOT_FOUND
#define EAI_SERVICE WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE WSAESOCKTNOSUPPORT
#define WSA_SECURE_HOST_NOT_FOUND 11032L

typedef struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;
} ADDRINFOA, *PADDRINFOA;

typedef struct addrinfoW {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    PWSTR ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfoW *ai_next;
} ADDRINFOW, *PADDRINFOW;

// :: and ::1 -- in Windows data exported by ws2_32; here each source's own copy
#define IN6ADDR_ANY_INIT { { { 0 } } }
#define IN6ADDR_LOOPBACK_INIT { { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } } }
static const IN6_ADDR in6addr_any = IN6ADDR_ANY_INIT;
static const IN6_ADDR in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

INT WSAAPI getaddrinfo(PCSTR node, PCSTR service, const ADDRINFOA *hints, PADDRINFOA *result);
void WSAAPI freeaddrinfo(PADDRINFOA info);
INT WSAAPI getnameinfo(const SOCKADDR *addr, socklen_t addrlen, PCHAR host, DWORD hostlen, PCHAR serv,
                       DWORD servlen, INT flags);
INT WSAAPI GetAddrInfoW(PCWSTR node, PCWSTR service, const ADDRINFOW *hints, PADDRINFOW *result);
void WSAAPI FreeAddrInfoW(PADDRINFOW info);
INT WSAAPI GetNameInfoW(const SOCKADDR *addr, socklen_t addrlen, PWCHAR host, DWORD hostlen, PWCHAR serv,
                        DWORD servlen, INT flags);
INT WSAAPI inet_pton(INT family, PCSTR text, PVOID addr);
PCSTR WSAAPI inet_ntop(INT family, const VOID *addr, PSTR text, size_t size);
INT WSAAPI InetPtonW(INT family, PCWSTR text, PVOID addr);
PCWSTR WSAAPI InetNtopW(INT family, const VOID *addr, PWSTR text, size_t size);

#define InetPtonA inet_pton
#define InetNtopA inet_ntop
#define GetAddrInfoA getaddrinfo
#define FreeAddrInfoA freeaddrinfo
#define GetNameInfoA getnameinfo
#ifdef UNICODE
#define GetAddrInfo GetAddrInfoW
#define FreeAddrInfo FreeAddrInfoW
#define GetNameInfo GetNameInfoW
#define ADDRINFOT ADDRINFOW
#else
#define GetAddrInfo getaddrinfo
#define FreeAddrInfo freeaddrinfo
#define GetNameInfo getnameinfo
#define ADDRINFOT ADDRINFOA
#endif
#define InetPton __AW(InetPton)
#define InetNtop __AW(InetNtop)

#endif
