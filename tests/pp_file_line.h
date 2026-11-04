// for pp_file_line.c: macros of a header that name the place of their use
#pragma once
#define HERE_FILE __FILE__
#define HERE_LINE __LINE__
#define HERE_CALL(f) f(__FILE__, __LINE__)
#define HERE_DEEP() HERE_CALL(note)
static const char *header_own_file(void) { return __FILE__; }   // a function: its own file
