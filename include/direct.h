// direct.h: folders -- make, remove, go into, the current one. Names are UTF-8.
#pragma once

int _mkdir(const char *name);
int _rmdir(const char *name);
int _chdir(const char *name);
char *_getcwd(char *buf, int size);     // buf 0: a new one from malloc (free it)
int _getdrive(void);                    // 1 = A, 3 = C
int _chdrive(int drive);

int mkdir(const char *name);
int rmdir(const char *name);
int chdir(const char *name);
char *getcwd(char *buf, int size);
