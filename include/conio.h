// conio.h: the console of Windows without lines -- a key at once, without
// Enter (_getch), is a key waiting (_kbhit), text straight to the screen.
// A letter beyond ASCII comes from _getch byte by byte (UTF-8); an arrow or F1
// comes as two calls: 0 or 0xE0, then the key's scan code (up 72, down 80,
// left 75, right 77, F1 59).
#pragma once

int _getch(void);
int _getche(void);
unsigned short _getwch(void);
int _ungetch(int c);
int _kbhit(void);
int _putch(int c);
unsigned short _putwch(unsigned short c);
int _cputs(const char *s);
int _cprintf(const char *fmt, ...);

// the old names
int getch(void);
int getche(void);
int ungetch(int c);
int kbhit(void);
int putch(int c);
int cputs(const char *s);
int cprintf(const char *fmt, ...);
