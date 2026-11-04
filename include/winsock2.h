// winsock2.h of WinCC: sockets of ws2_32 -- TCP and UDP, IPv4 and IPv6, select/poll,
// window and event notifications, names and addresses (getaddrinfo in ws2tcpip.h).
// Before the first socket a program calls WSAStartup(MAKEWORD(2, 2), &wsa).
#pragma once
#ifndef _WINSOCK2API_
#define _WINSOCK2API_
#define _WINSOCKAPI_
#include <windows.h>
#pragma comment(lib, "ws2_32")

#define WSAAPI

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned long long u_int64;
typedef UINT_PTR SOCKET;
typedef USHORT ADDRESS_FAMILY;
typedef int socklen_t;
typedef unsigned int GROUP;
typedef HANDLE WSAEVENT, *LPWSAEVENT;

#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)

// ---- families, types, protocols ----
#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_IPX 6
#define AF_APPLETALK 16
#define AF_NETBIOS 17
#define AF_INET6 23
#define AF_IRDA 26
#define AF_BTH 32
#define AF_MAX 35
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define PF_MAX AF_MAX

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5

#define IPPROTO_IP 0
#define IPPROTO_HOPOPTS 0
#define IPPROTO_ICMP 1
#define IPPROTO_IGMP 2
#define IPPROTO_GGP 3
#define IPPROTO_IPV4 4
#define IPPROTO_TCP 6
#define IPPROTO_PUP 12
#define IPPROTO_UDP 17
#define IPPROTO_IDP 22
#define IPPROTO_IPV6 41
#define IPPROTO_ROUTING 43
#define IPPROTO_FRAGMENT 44
#define IPPROTO_ESP 50
#define IPPROTO_AH 51
#define IPPROTO_ICMPV6 58
#define IPPROTO_NONE 59
#define IPPROTO_DSTOPTS 60
#define IPPROTO_ND 77
#define IPPROTO_ICLFXBM 78
#define IPPROTO_RAW 255
#define IPPROTO_MAX 256

#define IPPORT_ECHO 7
#define IPPORT_FTP 21
#define IPPORT_TELNET 23
#define IPPORT_SMTP 25
#define IPPORT_RESERVED 1024

// ---- addresses ----
#define INADDR_ANY (ULONG)0x00000000
#define INADDR_LOOPBACK 0x7f000001
#define INADDR_BROADCAST (ULONG)0xffffffff
#define INADDR_NONE 0xffffffff
#define ADDR_ANY INADDR_ANY

typedef struct in_addr {
    union {
        struct { UCHAR s_b1, s_b2, s_b3, s_b4; } S_un_b;
        struct { USHORT s_w1, s_w2; } S_un_w;
        ULONG S_addr;
    } S_un;
} IN_ADDR, *PIN_ADDR, *LPIN_ADDR;
#define s_addr S_un.S_addr
#define s_host S_un.S_un_b.s_b2
#define s_net S_un.S_un_b.s_b1
#define s_imp S_un.S_un_w.s_w2
#define s_impno S_un.S_un_b.s_b4
#define s_lh S_un.S_un_b.s_b3

typedef struct in6_addr {
    union {
        UCHAR Byte[16];
        USHORT Word[8];
    } u;
} IN6_ADDR, *PIN6_ADDR, *LPIN6_ADDR;
#define in_addr6 in6_addr
#define _S6_un u
#define _S6_u8 Byte
#define s6_addr _S6_un._S6_u8
#define s6_bytes u.Byte
#define s6_words u.Word

typedef struct sockaddr {
    ADDRESS_FAMILY sa_family;
    CHAR sa_data[14];
} SOCKADDR, *PSOCKADDR, *LPSOCKADDR;

typedef struct sockaddr_in {
    ADDRESS_FAMILY sin_family;
    USHORT sin_port;
    IN_ADDR sin_addr;
    CHAR sin_zero[8];
} SOCKADDR_IN, *PSOCKADDR_IN, *LPSOCKADDR_IN;

typedef struct {
    union {
        struct {
            ULONG Zone : 28;
            ULONG Level : 4;
        };
        ULONG Value;
    };
} SCOPE_ID, *PSCOPE_ID;

typedef struct sockaddr_in6 {
    ADDRESS_FAMILY sin6_family;
    USHORT sin6_port;
    ULONG sin6_flowinfo;
    IN6_ADDR sin6_addr;
    union {
        ULONG sin6_scope_id;
        SCOPE_ID sin6_scope_struct;
    };
} SOCKADDR_IN6, *PSOCKADDR_IN6, *LPSOCKADDR_IN6;

#define _SS_MAXSIZE 128
#define _SS_ALIGNSIZE 8
#define _SS_PAD1SIZE 6
#define _SS_PAD2SIZE 112
typedef struct sockaddr_storage {
    ADDRESS_FAMILY ss_family;
    CHAR __ss_pad1[_SS_PAD1SIZE];
    long long __ss_align;
    CHAR __ss_pad2[_SS_PAD2SIZE];
} SOCKADDR_STORAGE, *PSOCKADDR_STORAGE, *LPSOCKADDR_STORAGE;

typedef struct ip_mreq {
    IN_ADDR imr_multiaddr;
    IN_ADDR imr_interface;
} IP_MREQ, *PIP_MREQ;

typedef struct ipv6_mreq {
    IN6_ADDR ipv6mr_multiaddr;
    ULONG ipv6mr_interface;
} IPV6_MREQ, *PIPV6_MREQ;

// ---- options (setsockopt / getsockopt) ----
#define SOL_SOCKET 0xffff
#define SO_DEBUG 0x0001
#define SO_ACCEPTCONN 0x0002
#define SO_REUSEADDR 0x0004
#define SO_KEEPALIVE 0x0008
#define SO_DONTROUTE 0x0010
#define SO_BROADCAST 0x0020
#define SO_USELOOPBACK 0x0040
#define SO_LINGER 0x0080
#define SO_OOBINLINE 0x0100
#define SO_DONTLINGER (int)(~SO_LINGER)
#define SO_EXCLUSIVEADDRUSE ((int)(~SO_REUSEADDR))
#define SO_SNDBUF 0x1001
#define SO_RCVBUF 0x1002
#define SO_SNDLOWAT 0x1003
#define SO_RCVLOWAT 0x1004
#define SO_SNDTIMEO 0x1005
#define SO_RCVTIMEO 0x1006
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008
#define SO_CONDITIONAL_ACCEPT 0x3002

#define TCP_NODELAY 0x0001
#define TCP_EXPEDITED_1122 0x0002
#define TCP_KEEPALIVE 3
#define TCP_MAXSEG 4
#define TCP_MAXRT 5
#define TCP_KEEPCNT 16
#define TCP_KEEPIDLE TCP_KEEPALIVE
#define TCP_KEEPINTVL 17
#define TCP_FASTOPEN 15

#define IP_OPTIONS 1
#define IP_HDRINCL 2
#define IP_TOS 3
#define IP_TTL 4
#define IP_MULTICAST_IF 9
#define IP_MULTICAST_TTL 10
#define IP_MULTICAST_LOOP 11
#define IP_ADD_MEMBERSHIP 12
#define IP_DROP_MEMBERSHIP 13
#define IP_DONTFRAGMENT 14
#define IP_PKTINFO 19
#define IPV6_HOPLIMIT 21
#define IPV6_UNICAST_HOPS 4
#define IPV6_MULTICAST_IF 9
#define IPV6_MULTICAST_HOPS 10
#define IPV6_MULTICAST_LOOP 11
#define IPV6_ADD_MEMBERSHIP 12
#define IPV6_DROP_MEMBERSHIP 13
#define IPV6_JOIN_GROUP IPV6_ADD_MEMBERSHIP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP
#define IPV6_V6ONLY 27

typedef struct linger {
    u_short l_onoff;
    u_short l_linger;
} LINGER, *PLINGER, *LPLINGER;

// ---- ioctlsocket ----
#define IOCPARM_MASK 0x7f
#define IOC_VOID 0x20000000
#define IOC_OUT 0x40000000
#define IOC_IN 0x80000000
#define IOC_INOUT (IOC_IN | IOC_OUT)
#define _IO(x, y) (IOC_VOID | ((x) << 8) | (y))
#define _IOR(x, y, t) (IOC_OUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define _IOW(x, y, t) (IOC_IN | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define IOC_UNIX 0x00000000
#define IOC_WS2 0x08000000
#define IOC_PROTOCOL 0x10000000
#define IOC_VENDOR 0x18000000
#define _WSAIO(x, y) (IOC_VOID | (x) | (y))
#define _WSAIOR(x, y) (IOC_OUT | (x) | (y))
#define _WSAIOW(x, y) (IOC_IN | (x) | (y))
#define _WSAIORW(x, y) (IOC_INOUT | (x) | (y))
#define FIONREAD _IOR('f', 127, u_long)
#define FIONBIO _IOW('f', 126, u_long)
#define FIOASYNC _IOW('f', 125, u_long)
#define SIOCATMARK _IOR('s', 7, u_long)

// ---- send / recv / shutdown ----
#define MSG_OOB 0x1
#define MSG_PEEK 0x2
#define MSG_DONTROUTE 0x4
#define MSG_WAITALL 0x8
#define MSG_PUSH_IMMEDIATE 0x20
#define MSG_PARTIAL 0x8000
#define MSG_MAXIOVLEN 16
#define SD_RECEIVE 0x00
#define SD_SEND 0x01
#define SD_BOTH 0x02
#define SOMAXCONN 0x7fffffff
#define SOMAXCONN_HINT(b) (-(b))

// ---- select ----
#ifndef FD_SETSIZE                  // a program may ask for more: #define FD_SETSIZE 256 before this header
#define FD_SETSIZE 64
#endif
typedef struct fd_set {
    u_int fd_count;
    SOCKET fd_array[FD_SETSIZE];
} fd_set, FD_SET, *PFD_SET, *LPFD_SET;

typedef struct timeval {
    long tv_sec;
    long tv_usec;
} TIMEVAL, *PTIMEVAL, *LPTIMEVAL;

int WSAAPI __WSAFDIsSet(SOCKET fd, fd_set *set);
#define FD_CLR(fd, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_set *)(set))->fd_count; __i++) { \
        if (((fd_set *)(set))->fd_array[__i] == (fd)) { \
            while (__i < ((fd_set *)(set))->fd_count - 1) { \
                ((fd_set *)(set))->fd_array[__i] = ((fd_set *)(set))->fd_array[__i + 1]; \
                __i++; \
            } \
            ((fd_set *)(set))->fd_count--; \
            break; \
        } \
    } \
} while (0)
#define FD_SET(fd, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_set *)(set))->fd_count; __i++) \
        if (((fd_set *)(set))->fd_array[__i] == (fd)) break; \
    if (__i == ((fd_set *)(set))->fd_count && ((fd_set *)(set))->fd_count < FD_SETSIZE) { \
        ((fd_set *)(set))->fd_array[__i] = (fd); \
        ((fd_set *)(set))->fd_count++; \
    } \
} while (0)
#define FD_ZERO(set) (((fd_set *)(set))->fd_count = 0)
#define FD_ISSET(fd, set) __WSAFDIsSet((SOCKET)(fd), (fd_set *)(set))

#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)

// ---- poll ----
#define POLLRDNORM 0x0100
#define POLLRDBAND 0x0200
#define POLLIN (POLLRDNORM | POLLRDBAND)
#define POLLPRI 0x0400
#define POLLWRNORM 0x0010
#define POLLOUT (POLLWRNORM)
#define POLLWRBAND 0x0020
#define POLLERR 0x0001
#define POLLHUP 0x0002
#define POLLNVAL 0x0004
typedef struct pollfd {
    SOCKET fd;
    SHORT events;
    SHORT revents;
} WSAPOLLFD, *PWSAPOLLFD, *LPWSAPOLLFD;

// ---- names of the old kind (getaddrinfo in ws2tcpip.h is the new one) ----
struct hostent {
    char *h_name;
    char **h_aliases;
    short h_addrtype;
    short h_length;
    char **h_addr_list;
};
#define h_addr h_addr_list[0]
struct servent {
    char *s_name;
    char **s_aliases;
    char *s_proto;
    short s_port;
};
struct protoent {
    char *p_name;
    char **p_aliases;
    short p_proto;
};
typedef struct hostent HOSTENT, *PHOSTENT, *LPHOSTENT;
typedef struct servent SERVENT, *PSERVENT, *LPSERVENT;
typedef struct protoent PROTOENT, *PPROTOENT, *LPPROTOENT;

// ---- WSAStartup ----
#define WSADESCRIPTION_LEN 256
#define WSASYS_STATUS_LEN 128
typedef struct WSAData {
    WORD wVersion;
    WORD wHighVersion;
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char *lpVendorInfo;
    char szDescription[WSADESCRIPTION_LEN + 1];
    char szSystemStatus[WSASYS_STATUS_LEN + 1];
} WSADATA, *LPWSADATA;

// ---- events: window messages (WSAAsyncSelect) and event objects (WSAEventSelect) ----
#define FD_READ_BIT 0
#define FD_READ (1 << FD_READ_BIT)
#define FD_WRITE_BIT 1
#define FD_WRITE (1 << FD_WRITE_BIT)
#define FD_OOB_BIT 2
#define FD_OOB (1 << FD_OOB_BIT)
#define FD_ACCEPT_BIT 3
#define FD_ACCEPT (1 << FD_ACCEPT_BIT)
#define FD_CONNECT_BIT 4
#define FD_CONNECT (1 << FD_CONNECT_BIT)
#define FD_CLOSE_BIT 5
#define FD_CLOSE (1 << FD_CLOSE_BIT)
#define FD_QOS_BIT 6
#define FD_QOS (1 << FD_QOS_BIT)
#define FD_GROUP_QOS_BIT 7
#define FD_GROUP_QOS (1 << FD_GROUP_QOS_BIT)
#define FD_ROUTING_INTERFACE_CHANGE_BIT 8
#define FD_ROUTING_INTERFACE_CHANGE (1 << FD_ROUTING_INTERFACE_CHANGE_BIT)
#define FD_ADDRESS_LIST_CHANGE_BIT 9
#define FD_ADDRESS_LIST_CHANGE (1 << FD_ADDRESS_LIST_CHANGE_BIT)
#define FD_MAX_EVENTS 10
#define FD_ALL_EVENTS ((1 << FD_MAX_EVENTS) - 1)

#define WSAMAKEASYNCREPLY(len, err) MAKELONG(len, err)
#define WSAMAKESELECTREPLY(event, err) MAKELONG(event, err)
#define WSAGETASYNCBUFLEN(l) LOWORD(l)
#define WSAGETASYNCERROR(l) HIWORD(l)
#define WSAGETSELECTEVENT(l) LOWORD(l)
#define WSAGETSELECTERROR(l) HIWORD(l)

typedef struct _WSANETWORKEVENTS {
    long lNetworkEvents;
    int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS, *LPWSANETWORKEVENTS;

#define WSA_IO_PENDING (ERROR_IO_PENDING)
#define WSA_IO_INCOMPLETE (ERROR_IO_INCOMPLETE)
#define WSA_INVALID_HANDLE (ERROR_INVALID_HANDLE)
#define WSA_INVALID_PARAMETER (ERROR_INVALID_PARAMETER)
#define WSA_NOT_ENOUGH_MEMORY (ERROR_NOT_ENOUGH_MEMORY)
#define WSA_OPERATION_ABORTED (ERROR_OPERATION_ABORTED)
#define WSA_INVALID_EVENT ((WSAEVENT)NULL)
#define WSA_MAXIMUM_WAIT_EVENTS (MAXIMUM_WAIT_OBJECTS)
#define WSA_WAIT_FAILED (WAIT_FAILED)
#define WSA_WAIT_EVENT_0 (WAIT_OBJECT_0)
#define WSA_WAIT_IO_COMPLETION (WAIT_IO_COMPLETION)
#define WSA_WAIT_TIMEOUT (WAIT_TIMEOUT)
#define WSA_INFINITE (INFINITE)

// ---- buffers and overlapped calls ----
typedef struct _WSABUF {
    ULONG len;
    CHAR *buf;
} WSABUF, *LPWSABUF;
typedef OVERLAPPED WSAOVERLAPPED, *LPWSAOVERLAPPED;
typedef void (CALLBACK *LPWSAOVERLAPPED_COMPLETION_ROUTINE)(DWORD error, DWORD transferred, LPWSAOVERLAPPED ov, DWORD flags);
typedef struct _WSAPROTOCOL_INFOA WSAPROTOCOL_INFOA, *LPWSAPROTOCOL_INFOA;
typedef struct _WSAPROTOCOL_INFOW WSAPROTOCOL_INFOW, *LPWSAPROTOCOL_INFOW;

#define WSA_FLAG_OVERLAPPED 0x01
#define WSA_FLAG_MULTIPOINT_C_ROOT 0x02
#define WSA_FLAG_MULTIPOINT_C_LEAF 0x04
#define WSA_FLAG_MULTIPOINT_D_ROOT 0x08
#define WSA_FLAG_MULTIPOINT_D_LEAF 0x10
#define WSA_FLAG_ACCESS_SYSTEM_SECURITY 0x40
#define WSA_FLAG_NO_HANDLE_INHERIT 0x80

// ---- errors (WSAGetLastError) ----
#define WSABASEERR 10000
#define WSAEINTR (WSABASEERR + 4)
#define WSAEBADF (WSABASEERR + 9)
#define WSAEACCES (WSABASEERR + 13)
#define WSAEFAULT (WSABASEERR + 14)
#define WSAEINVAL (WSABASEERR + 22)
#define WSAEMFILE (WSABASEERR + 24)
#define WSAEWOULDBLOCK (WSABASEERR + 35)
#define WSAEINPROGRESS (WSABASEERR + 36)
#define WSAEALREADY (WSABASEERR + 37)
#define WSAENOTSOCK (WSABASEERR + 38)
#define WSAEDESTADDRREQ (WSABASEERR + 39)
#define WSAEMSGSIZE (WSABASEERR + 40)
#define WSAEPROTOTYPE (WSABASEERR + 41)
#define WSAENOPROTOOPT (WSABASEERR + 42)
#define WSAEPROTONOSUPPORT (WSABASEERR + 43)
#define WSAESOCKTNOSUPPORT (WSABASEERR + 44)
#define WSAEOPNOTSUPP (WSABASEERR + 45)
#define WSAEPFNOSUPPORT (WSABASEERR + 46)
#define WSAEAFNOSUPPORT (WSABASEERR + 47)
#define WSAEADDRINUSE (WSABASEERR + 48)
#define WSAEADDRNOTAVAIL (WSABASEERR + 49)
#define WSAENETDOWN (WSABASEERR + 50)
#define WSAENETUNREACH (WSABASEERR + 51)
#define WSAENETRESET (WSABASEERR + 52)
#define WSAECONNABORTED (WSABASEERR + 53)
#define WSAECONNRESET (WSABASEERR + 54)
#define WSAENOBUFS (WSABASEERR + 55)
#define WSAEISCONN (WSABASEERR + 56)
#define WSAENOTCONN (WSABASEERR + 57)
#define WSAESHUTDOWN (WSABASEERR + 58)
#define WSAETOOMANYREFS (WSABASEERR + 59)
#define WSAETIMEDOUT (WSABASEERR + 60)
#define WSAECONNREFUSED (WSABASEERR + 61)
#define WSAELOOP (WSABASEERR + 62)
#define WSAENAMETOOLONG (WSABASEERR + 63)
#define WSAEHOSTDOWN (WSABASEERR + 64)
#define WSAEHOSTUNREACH (WSABASEERR + 65)
#define WSAENOTEMPTY (WSABASEERR + 66)
#define WSAEPROCLIM (WSABASEERR + 67)
#define WSAEUSERS (WSABASEERR + 68)
#define WSAEDQUOT (WSABASEERR + 69)
#define WSAESTALE (WSABASEERR + 70)
#define WSAEREMOTE (WSABASEERR + 71)
#define WSASYSNOTREADY (WSABASEERR + 91)
#define WSAVERNOTSUPPORTED (WSABASEERR + 92)
#define WSANOTINITIALISED (WSABASEERR + 93)
#define WSAEDISCON (WSABASEERR + 101)
#define WSAENOMORE (WSABASEERR + 102)
#define WSAECANCELLED (WSABASEERR + 103)
#define WSAEINVALIDPROCTABLE (WSABASEERR + 104)
#define WSAEINVALIDPROVIDER (WSABASEERR + 105)
#define WSAEPROVIDERFAILEDINIT (WSABASEERR + 106)
#define WSASYSCALLFAILURE (WSABASEERR + 107)
#define WSASERVICE_NOT_FOUND (WSABASEERR + 108)
#define WSATYPE_NOT_FOUND (WSABASEERR + 109)
#define WSA_E_NO_MORE (WSABASEERR + 110)
#define WSA_E_CANCELLED (WSABASEERR + 111)
#define WSAEREFUSED (WSABASEERR + 112)
#define WSAHOST_NOT_FOUND (WSABASEERR + 1001)
#define WSATRY_AGAIN (WSABASEERR + 1002)
#define WSANO_RECOVERY (WSABASEERR + 1003)
#define WSANO_DATA (WSABASEERR + 1004)
#define WSANO_ADDRESS WSANO_DATA
#define HOST_NOT_FOUND WSAHOST_NOT_FOUND
#define TRY_AGAIN WSATRY_AGAIN
#define NO_RECOVERY WSANO_RECOVERY
#define NO_DATA WSANO_DATA
#define NO_ADDRESS WSANO_ADDRESS
#define h_errno WSAGetLastError()

// ---- functions ----
SOCKET WSAAPI accept(SOCKET s, struct sockaddr *addr, int *addrlen);
int WSAAPI bind(SOCKET s, const struct sockaddr *name, int namelen);
int WSAAPI closesocket(SOCKET s);
int WSAAPI connect(SOCKET s, const struct sockaddr *name, int namelen);
int WSAAPI ioctlsocket(SOCKET s, long cmd, u_long *argp);
int WSAAPI getpeername(SOCKET s, struct sockaddr *name, int *namelen);
int WSAAPI getsockname(SOCKET s, struct sockaddr *name, int *namelen);
int WSAAPI getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen);
u_long WSAAPI htonl(u_long hostlong);
u_short WSAAPI htons(u_short hostshort);
unsigned long WSAAPI inet_addr(const char *cp);
char *WSAAPI inet_ntoa(struct in_addr in);
int WSAAPI listen(SOCKET s, int backlog);
u_long WSAAPI ntohl(u_long netlong);
u_short WSAAPI ntohs(u_short netshort);
int WSAAPI recv(SOCKET s, char *buf, int len, int flags);
int WSAAPI recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
int WSAAPI select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
int WSAAPI send(SOCKET s, const char *buf, int len, int flags);
int WSAAPI sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
int WSAAPI setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen);
int WSAAPI shutdown(SOCKET s, int how);
SOCKET WSAAPI socket(int af, int type, int protocol);

struct hostent *WSAAPI gethostbyaddr(const char *addr, int len, int type);
struct hostent *WSAAPI gethostbyname(const char *name);
int WSAAPI gethostname(char *name, int namelen);
int WSAAPI GetHostNameW(PWSTR name, int namelen);
struct servent *WSAAPI getservbyport(int port, const char *proto);
struct servent *WSAAPI getservbyname(const char *name, const char *proto);
struct protoent *WSAAPI getprotobynumber(int number);
struct protoent *WSAAPI getprotobyname(const char *name);

int WSAAPI WSAStartup(WORD version, LPWSADATA data);
int WSAAPI WSACleanup(void);
void WSAAPI WSASetLastError(int error);
int WSAAPI WSAGetLastError(void);
int WSAAPI WSAPoll(LPWSAPOLLFD fds, ULONG count, INT timeout);

int WSAAPI WSAAsyncSelect(SOCKET s, HWND wnd, u_int msg, long events);
int WSAAPI WSAEventSelect(SOCKET s, WSAEVENT event, long events);
WSAEVENT WSAAPI WSACreateEvent(void);
BOOL WSAAPI WSACloseEvent(WSAEVENT event);
BOOL WSAAPI WSAResetEvent(WSAEVENT event);
BOOL WSAAPI WSASetEvent(WSAEVENT event);
DWORD WSAAPI WSAWaitForMultipleEvents(DWORD count, const WSAEVENT *events, BOOL all, DWORD timeout, BOOL alertable);
int WSAAPI WSAEnumNetworkEvents(SOCKET s, WSAEVENT event, LPWSANETWORKEVENTS events);

SOCKET WSAAPI WSASocketA(int af, int type, int protocol, LPWSAPROTOCOL_INFOA info, GROUP g, DWORD flags);
SOCKET WSAAPI WSASocketW(int af, int type, int protocol, LPWSAPROTOCOL_INFOW info, GROUP g, DWORD flags);
int WSAAPI WSASend(SOCKET s, LPWSABUF bufs, DWORD count, LPDWORD sent, DWORD flags, LPWSAOVERLAPPED ov,
                   LPWSAOVERLAPPED_COMPLETION_ROUTINE done);
int WSAAPI WSASendTo(SOCKET s, LPWSABUF bufs, DWORD count, LPDWORD sent, DWORD flags, const struct sockaddr *to,
                     int tolen, LPWSAOVERLAPPED ov, LPWSAOVERLAPPED_COMPLETION_ROUTINE done);
int WSAAPI WSARecv(SOCKET s, LPWSABUF bufs, DWORD count, LPDWORD received, LPDWORD flags, LPWSAOVERLAPPED ov,
                   LPWSAOVERLAPPED_COMPLETION_ROUTINE done);
int WSAAPI WSARecvFrom(SOCKET s, LPWSABUF bufs, DWORD count, LPDWORD received, LPDWORD flags, struct sockaddr *from,
                       LPINT fromlen, LPWSAOVERLAPPED ov, LPWSAOVERLAPPED_COMPLETION_ROUTINE done);
BOOL WSAAPI WSAGetOverlappedResult(SOCKET s, LPWSAOVERLAPPED ov, LPDWORD transferred, BOOL wait, LPDWORD flags);
int WSAAPI WSAIoctl(SOCKET s, DWORD code, LPVOID in, DWORD inlen, LPVOID out, DWORD outlen, LPDWORD returned,
                    LPWSAOVERLAPPED ov, LPWSAOVERLAPPED_COMPLETION_ROUTINE done);
INT WSAAPI WSAAddressToStringA(LPSOCKADDR addr, DWORD addrlen, LPWSAPROTOCOL_INFOA info, LPSTR out, LPDWORD outlen);
INT WSAAPI WSAAddressToStringW(LPSOCKADDR addr, DWORD addrlen, LPWSAPROTOCOL_INFOW info, LPWSTR out, LPDWORD outlen);
INT WSAAPI WSAStringToAddressA(LPSTR text, INT family, LPWSAPROTOCOL_INFOA info, LPSOCKADDR addr, LPINT addrlen);
INT WSAAPI WSAStringToAddressW(LPWSTR text, INT family, LPWSAPROTOCOL_INFOW info, LPSOCKADDR addr, LPINT addrlen);

#define WSASocket __AW(WSASocket)
#define WSAAddressToString __AW(WSAAddressToString)
#define WSAStringToAddress __AW(WSAStringToAddress)

#endif
