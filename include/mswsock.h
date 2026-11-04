// mswsock.h of WinCC: the Microsoft additions to sockets that programs use most.
#pragma once
#include <winsock2.h>

#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)       // off: a packet to nobody does not break recvfrom
#define SIO_UDP_NETRESET _WSAIOW(IOC_VENDOR, 15)
#define SO_CONNDATA 0x7000
#define SO_CONNOPT 0x7001
#define SO_DISCDATA 0x7002
#define SO_DISCOPT 0x7003
#define SO_OPENTYPE 0x7008
#define SO_MAXDG 0x7009
#define SO_MAXPATHDG 0x700A
#define SO_UPDATE_ACCEPT_CONTEXT 0x700B
#define SO_CONNECT_TIME 0x700C
#define SO_UPDATE_CONNECT_CONTEXT 0x7010
#define TCP_BSDURGENT 0x7000
