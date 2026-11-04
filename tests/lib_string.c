// string.h and ctype.h, edge cases included
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

static void check_str(const char *want, const char *got, int line)
{
    if (!got || strcmp(want, got)) {
        printf("\n    строка %d: \"%s\", ожидалось \"%s\"", line, got ? got : "(null)", want);
        g_fail++;
    }
}

#define STR(want, got) check_str(want, got, __LINE__)

int main(void)
{
    char buf[64], tmp[64];
    // length and comparison
    ASSERT(0, strlen(""));
    ASSERT(5, strlen("hello"));
    ASSERT(6, strlen("жук"));                  // bytes of UTF-8
    ASSERT(3, strnlen("hello", 3));
    ASSERT(2, strnlen("hi", 10));
    ASSERT(0, strcmp("abc", "abc"));
    ASSERT(1, strcmp("abc", "abd") < 0);
    ASSERT(1, strcmp("abd", "abc") > 0);
    ASSERT(1, strcmp("ab", "abc") < 0);
    ASSERT(1, strcmp("\xff", "a") > 0);        // compared as unsigned char
    ASSERT(0, strncmp("abcdef", "abcxyz", 3));
    ASSERT(1, strncmp("abcdef", "abcxyz", 4) < 0);
    ASSERT(0, strncmp("a", "b", 0));
    ASSERT(0, strcasecmp("Hello", "hELLO"));
    ASSERT(1, strcasecmp("apple", "Banana") < 0);
    ASSERT(0, strncasecmp("ABCx", "abcy", 3));
    ASSERT(0, strcoll("a", "a"));
    ASSERT(5, strxfrm(tmp, "hello", sizeof(tmp)));
    STR("hello", tmp);
    // copying and joining
    STR("hello", strcpy(buf, "hello"));
    memset(buf, 'x', sizeof(buf));
    strncpy(buf, "ab", 5);
    ASSERT(0, memcmp(buf, "ab\0\0\0x", 6));      // padded with zeros, no more
    strncpy(buf, "abcdef", 3);
    ASSERT('c', buf[2]);
    ASSERT(0, buf[3]);                           // (left from before)
    strcpy(buf, "foo");
    STR("foobar", strcat(buf, "bar"));
    STR("foobarba", strncat(buf, "baz", 2));
    char *end = stpcpy(buf, "abc");
    ASSERT(3, end - buf);
    // searching
    strcpy(buf, "hello world");
    ASSERT(4, strchr(buf, 'o') - buf);
    ASSERT(7, strrchr(buf, 'o') - buf);
    ASSERT(11, strchr(buf, 0) - buf);
    ASSERT(1, strchr(buf, 'z') == 0);
    ASSERT(6, strstr(buf, "world") - buf);
    ASSERT(0, strstr(buf, "") - buf);
    ASSERT(1, strstr(buf, "worlds") == 0);
    ASSERT(6, strspn("aaabbbccc", "ab"));
    ASSERT(0, strspn("xyz", "ab"));
    ASSERT(3, strcspn("abcdef", "dxe"));
    ASSERT(6, strcspn("abcdef", "xyz"));
    const char *letters = "abcdef";
    ASSERT(2, strpbrk(letters, "dc") - letters);
    ASSERT(1, strpbrk("abc", "xyz") == 0);
    // strtok
    strcpy(buf, ",,a,b;;c,");
    STR("a", strtok(buf, ",;"));
    STR("b", strtok(0, ",;"));
    STR("c", strtok(0, ",;"));
    ASSERT(1, strtok(0, ",;") == 0);
    char *save;
    strcpy(buf, "x y  z");
    STR("x", strtok_r(buf, " ", &save));
    STR("y", strtok_r(0, " ", &save));
    STR("z", strtok_r(0, " ", &save));
    ASSERT(1, strtok_r(0, " ", &save) == 0);
    // allocation
    char *d = strdup("copy me");
    STR("copy me", d);
    free(d);
    d = strndup("abcdef", 3);
    STR("abc", d);
    free(d);
    // memory
    char a[10] = "123456789";
    memmove(a + 2, a, 5);
    STR("121234589", a);
    strcpy(a, "123456789");
    memmove(a, a + 2, 5);
    STR("345676789", a);
    memset(a, 0, 10);
    ASSERT(0, a[9]);
    memcpy(a, "xyz", 4);
    STR("xyz", a);
    ASSERT(1, memcmp("abc", "abd", 3) < 0);
    ASSERT(1, memcmp("\x80", "\x01", 1) > 0);
    ASSERT(0, memcmp("abc", "xyz", 0));
    ASSERT(2, (char *)memchr(letters, 'c', 3) - letters);
    ASSERT(1, memchr("abc", 'c', 2) == 0);
    ASSERT(1, memchr("a\0b", 'b', 3) != 0);
    char m[8];
    ASSERT(1, memccpy(m, "ab:cd", ':', 5) == m + 3);
    // errors
    STR("No such file or directory", strerror(ENOENT));
    STR("Result too large", strerror(ERANGE));
    // ctype: the whole ASCII range against the definitions
    int bad = 0;
    for (int c = -1; c < 256; c++) {
        int digit = c >= '0' && c <= '9';
        int upper = c >= 'A' && c <= 'Z', lower = c >= 'a' && c <= 'z';
        int alpha = upper || lower;
        int space = c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
        int print = c >= 32 && c < 127;
        int cntrl = (c >= 0 && c < 32) || c == 127;
        int xdig = digit || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!!isdigit(c) != digit) bad++;
        if (!!isalpha(c) != alpha) bad++;
        if (!!isalnum(c) != (alpha || digit)) bad++;
        if (!!isupper(c) != upper) bad++;
        if (!!islower(c) != lower) bad++;
        if (!!isspace(c) != space) bad++;
        if (!!isblank(c) != (c == ' ' || c == '\t')) bad++;
        if (!!isprint(c) != print) bad++;
        if (!!isgraph(c) != (print && c != ' ')) bad++;
        if (!!iscntrl(c) != cntrl) bad++;
        if (!!ispunct(c) != (print && c != ' ' && !alpha && !digit)) bad++;
        if (!!isxdigit(c) != xdig) bad++;
        if (toupper(c) != (lower ? c - 32 : c)) bad++;
        if (tolower(c) != (upper ? c + 32 : c)) bad++;
    }
    ASSERT(0, bad);
    ASSERT(0, isalpha((char)0xD0));             // a UTF-8 byte is no letter in the C locale
    return g_fail;
}
