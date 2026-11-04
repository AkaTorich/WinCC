// fcntl.h: how _open opens a file (io.h)
#pragma once

#define _O_RDONLY       0x0000
#define _O_WRONLY       0x0001
#define _O_RDWR         0x0002
#define _O_APPEND       0x0008
#define _O_CREAT        0x0100
#define _O_TRUNC        0x0200
#define _O_EXCL         0x0400
#define _O_TEXT         0x4000      // "\n" is "\r\n" in the file
#define _O_BINARY       0x8000      // bytes as they are
#define _O_WTEXT        0x10000
#define _O_U16TEXT      0x20000
#define _O_U8TEXT       0x40000
#define _O_RAW          _O_BINARY
#define _O_NOINHERIT    0x0080
#define _O_TEMPORARY    0x0040
#define _O_SEQUENTIAL   0x0020
#define _O_RANDOM       0x0010

#define O_RDONLY    _O_RDONLY
#define O_WRONLY    _O_WRONLY
#define O_RDWR      _O_RDWR
#define O_APPEND    _O_APPEND
#define O_CREAT     _O_CREAT
#define O_TRUNC     _O_TRUNC
#define O_EXCL      _O_EXCL
#define O_TEXT      _O_TEXT
#define O_BINARY    _O_BINARY
#define O_RAW       _O_BINARY
