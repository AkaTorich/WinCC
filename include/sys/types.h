// sys/types.h: the types of sys/stat.h and io.h
#pragma once
#include <stddef.h>

typedef unsigned int _dev_t;
typedef unsigned short _ino_t;
typedef long _off_t;
typedef long long _off64_t;
typedef long long time_t;
typedef _dev_t dev_t;
typedef _ino_t ino_t;
typedef _off_t off_t;
typedef unsigned short mode_t;
typedef long long ssize_t;
typedef int pid_t;
