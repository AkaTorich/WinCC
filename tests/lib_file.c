// FILE: write -> read -> append -> seek, update modes, fscanf, tmpfile,
// rename, errors. (The test host keeps a program's files in obj\files.)
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char buf[512];

static void check_str(const char *want, const char *got, int line)
{
    if (!got || strcmp(want, got)) {
        printf("\n    строка %d: \"%s\", ожидалось \"%s\"", line, got ? got : "(null)", want);
        g_fail++;
    }
}

#define STR(want, got) check_str(want, got, __LINE__)

// the whole file as a string
static const char *slurp(const char *name)
{
    static char all[4096];
    FILE *f = fopen(name, "rb");
    if (!f) return "(нет файла)";
    size_t n = fread(all, 1, sizeof(all) - 1, f);
    all[n] = 0;
    fclose(f);
    return all;
}

int main(void)
{
#ifdef _WIN32                           // Windows: the current folder
    const char *name = "cctest file.txt";
#else
    const char *name = "desktop/cctest file.txt";
#endif
    // write (bytes as they are: "b"; the text mode of Windows is below)
    FILE *f = fopen(name, "wb");
    ASSERT(1, f != 0);
    ASSERT(7, fprintf(f, "line %d\n", 1));
    ASSERT(1, fputs("second\n", f) >= 0);
    ASSERT('X', fputc('X', f));
    ASSERT('Y', putc('Y', f));
    ASSERT(2, fwrite("Z\n", 1, 2, f));
    ASSERT(18, ftell(f));
    ASSERT(0, fclose(f));
    STR("line 1\nsecond\nXYZ\n", slurp(name));
    // read
    f = fopen(name, "rb");
    ASSERT(1, f != 0);
    STR("line 1\n", fgets(buf, sizeof(buf), f));
    STR("seco", fgets(buf, 5, f));              // n - 1 characters
    STR("nd\n", fgets(buf, sizeof(buf), f));
    ASSERT('X', fgetc(f));
    ASSERT('Y', getc(f));
    ASSERT('Y', ungetc('Y', f));
    ASSERT('Y', fgetc(f));
    ASSERT('Q', ungetc('Q', f));                 // a different character comes back too
    ASSERT('Q', fgetc(f));
    ASSERT(2, fread(buf, 1, 10, f));
    ASSERT(0, memcmp(buf, "Z\n", 2));
    ASSERT(EOF, fgetc(f));
    ASSERT(1, feof(f) != 0);
    ASSERT(0, ferror(f));
    ASSERT(1, fgets(buf, sizeof(buf), f) == 0);
    clearerr(f);
    ASSERT(0, feof(f));
    // seek and tell
    ASSERT(0, fseek(f, 0, SEEK_END));
    ASSERT(18, ftell(f));
    ASSERT(0, fseek(f, -4, SEEK_CUR));
    ASSERT('X', fgetc(f));
    rewind(f);
    ASSERT('l', fgetc(f));
    fpos_t pos;
    ASSERT(0, fgetpos(f, &pos));
    fgetc(f);
    fgetc(f);
    ASSERT(0, fsetpos(f, &pos));
    ASSERT('i', fgetc(f));
    ASSERT(-1, fseek(f, -100, SEEK_SET));
    ASSERT(EOF, fputc('!', f));                  // opened for reading only
    ASSERT(1, ferror(f) != 0);
    ASSERT(0, fclose(f));
    // append
    f = fopen(name, "ab");
    ASSERT(1, f != 0);
    fputs("tail\n", f);
    fclose(f);
    STR("line 1\nsecond\nXYZ\ntail\n", slurp(name));
    f = fopen(name, "a+b");
    fseek(f, 0, SEEK_SET);
    ASSERT('l', fgetc(f));
    fputs("end", f);                             // a+ writes always go to the end
    fclose(f);
    STR("line 1\nsecond\nXYZ\ntail\nend", slurp(name));
    // update in place
    f = fopen(name, "r+b");
    fseek(f, 5, SEEK_SET);
    fputc('#', f);
    fseek(f, 0, SEEK_SET);
    fgets(buf, sizeof(buf), f);
    STR("line #\n", buf);
    fclose(f);
    STR("line #\nsecond\nXYZ\ntail\nend", slurp(name));
    // w+ : write, go back, read
    f = fopen(name, "w+");
    fputs("12 3.5 word -7\n", f);
    rewind(f);
    int i = 0, j = 0;
    double d = 0;
    char word[32];
    ASSERT(4, fscanf(f, "%d %lf %31s %d", &i, &d, word, &j));
    ASSERT(12, i);
    ASSERT(1, d == 3.5);
    STR("word", word);
    ASSERT(-7, j);
    ASSERT(EOF, fscanf(f, "%d", &i));
    fclose(f);
    // fseek past the end: the gap is zeros
    f = fopen(name, "wb");
    fseek(f, 3, SEEK_SET);
    fputc('A', f);
    fclose(f);
    f = fopen(name, "rb");
    ASSERT(4, fread(buf, 1, 10, f));
    ASSERT(0, memcmp(buf, "\0\0\0A", 4));
    fclose(f);
    // binary: every byte value
    unsigned char bytes[256], back[256];
    for (int k = 0; k < 256; k++) bytes[k] = (unsigned char)k;
    f = fopen(name, "wb");
    ASSERT(256, fwrite(bytes, 1, 256, f));
    fclose(f);
    f = fopen(name, "rb");
    ASSERT(64, fread(back, 4, 64, f));
    fclose(f);
    ASSERT(0, memcmp(bytes, back, 256));
    // fflush writes the file while it stays open
    f = fopen(name, "w");
    fputs("flushed", f);
    ASSERT(0, fflush(f));
    STR("flushed", slurp(name));
    fputs(" twice", f);
    fflush(0);
    STR("flushed twice", slurp(name));
    fclose(f);
#ifdef __WINCC__
    // the text mode of Windows: "\n" is "\r\n" in the file and back again, Ctrl+Z ends the text
    f = fopen(name, "w");
    fputs("a\nb\n", f);
    ASSERT(6, ftell(f));
    fclose(f);
    STR("a\r\nb\r\n", slurp(name));
    f = fopen(name, "r");
    STR("a\n", fgets(buf, sizeof(buf), f));
    ASSERT(3, ftell(f));
    STR("b\n", fgets(buf, sizeof(buf), f));
    ASSERT(EOF, fgetc(f));
    fclose(f);
    f = fopen(name, "wb");
    fputs("x\r\ny\x1Az", f);
    fclose(f);
    f = fopen(name, "r");
    ASSERT('x', fgetc(f));
    ASSERT('\n', fgetc(f));
    ASSERT('y', fgetc(f));
    ASSERT(EOF, fgetc(f));
    fclose(f);
    // a big file goes by pieces; places past 2 GB: _fseeki64, _ftelli64
    const char *big = "cctest big.txt";
    f = fopen(big, "wb");
    for (int k = 0; k < 3 * 1024 * 1024 / 16; k++)
        fwrite("0123456789abcdef", 1, 16, f);
    ASSERT(1, _ftelli64(f) == 3 * 1024 * 1024);
    fclose(f);
    f = fopen(big, "rb");
    ASSERT(0, _fseeki64(f, 3 * 1024 * 1024 - 3, SEEK_SET));
    ASSERT('d', fgetc(f));
    ASSERT(1, _ftelli64(f) == 3 * 1024 * 1024 - 2);
    ASSERT(0, fseek(f, -16, SEEK_END));
    ASSERT('0', fgetc(f));
    ASSERT(1, fileno(f) > 2);
    fclose(f);
    ASSERT(0, remove(big));
    // the text mode check above wrote over the file: the next ones want "flushed twice" back
    f = fopen(name, "w");
    fputs("flushed twice", f);
    fclose(f);
#endif
    // a name in Cyrillic
    f = fopen("тест.txt", "w");
    ASSERT(1, f != 0);
    fprintf(f, "%s", "привет");
    fclose(f);
    STR("привет", slurp("тест.txt"));
    // x: only a new file
    errno = 0;
    ASSERT(1, fopen(name, "wx") == 0);
    ASSERT(EEXIST, errno);
    // missing files and bad modes
    errno = 0;
    ASSERT(1, fopen("no such file 123.txt", "r") == 0);
    ASSERT(ENOENT, errno);
    ASSERT(1, fopen(name, "q") == 0);
    ASSERT(-1, remove("no such file 123.txt"));
    // rename there and back
    const char *other = "cctest renamed.txt";
    ASSERT(0, rename(name, other));
    ASSERT(1, fopen(name, "r") == 0);
    STR("flushed twice", slurp(other));
    ASSERT(0, rename(other, name));
    ASSERT(1, fopen(other, "r") == 0);
    STR("flushed twice", slurp(name));
    char fresh[L_tmpnam];
    ASSERT(1, tmpnam(fresh) != 0);               // a name no file has
    ASSERT(1, fopen(fresh, "r") == 0);
    // tmpfile: in memory only
    f = tmpfile();
    ASSERT(1, f != 0);
    fprintf(f, "%d %s", 99, "bottles");
    rewind(f);
    ASSERT(2, fscanf(f, "%d %s", &i, word));
    ASSERT(99, i);
    STR("bottles", word);
    fclose(f);
    // rename over a file that exists: it is replaced (POSIX)
    f = fopen("cctest target.txt", "w");
    fputs("old target", f);
    fclose(f);
    f = fopen("cctest source.txt", "w");
    fputs("new content", f);
    fclose(f);
    ASSERT(0, rename("cctest source.txt", "cctest target.txt"));
    STR("new content", slurp("cctest target.txt"));
    ASSERT(1, fopen("cctest source.txt", "r") == 0);
    ASSERT(-1, rename("cctest missing.txt", "cctest target.txt"));   // no source: the target stays
    STR("new content", slurp("cctest target.txt"));
    // freopen
    f = fopen(name, "r");
    f = freopen(name, "w", f);
    ASSERT(1, f != 0);
    fputs("reopened", f);
    fclose(f);
    STR("reopened", slurp(name));
    // a file left open is written at the end of the program
    f = fopen("cctest left open.txt", "w");
    fputs("written by exit", f);
    ASSERT(1, setvbuf(f, 0, _IOFBF, 0) == 0);
    // stdout and stderr are streams too
    ASSERT(0, fputs("", stdout));
    ASSERT(0, fprintf(stderr, ""));
    ASSERT(-1, ftell(stdout));
    return g_fail;
}
