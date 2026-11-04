// time.h: mktime/gmtime both ways, normalisation, strftime, asctime, clock
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static char buf[256];

static void check_str(const char *want, const char *got, int line)
{
    if (!got || strcmp(want, got)) {
        printf("\n    строка %d: \"%s\", ожидалось \"%s\"", line, got ? got : "(null)", want);
        g_fail++;
    }
}

#define STR(want, got) check_str(want, got, __LINE__)

static struct tm make(int y, int mon, int d, int h, int mi, int s)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
    return t;
}

static const char *fmt(const char *f, struct tm *t)
{
    strftime(buf, sizeof(buf), f, t);
    return buf;
}

int main(void)
{
    // mktime and localtime are the local time of the machine: the zone's offset counted out
    // (it is 0 under DynastyOS; no summer time in January and February)
    struct tm t = make(1970, 1, 1, 0, 0, 0);
    time_t off = mktime(&t);                     // 0 in UTC, -10800 at +3
    ASSERT(4, t.tm_wday);                        // a Thursday
    t = make(2000, 1, 1, 0, 0, 0);
    ASSERT(946684800, mktime(&t) - off);
    t = make(2024, 2, 29, 13, 5, 9);
    time_t when = mktime(&t);
    ASSERT(1709211909, when - off);
    ASSERT(4, t.tm_wday);
    ASSERT(59, t.tm_yday);
    struct tm *g = gmtime(&when);
    ASSERT(124, g->tm_year);
    ASSERT(1, g->tm_mon);
    ASSERT(29, g->tm_mday);
    ASSERT(13, g->tm_hour - off / 3600);
    ASSERT(5, g->tm_min);
    ASSERT(9, g->tm_sec);
    ASSERT(1, localtime(&when)->tm_mday == 29);
    time_t before = -86400;                      // 1969-12-31
    g = gmtime(&before);
    ASSERT(69, g->tm_year);
    ASSERT(11, g->tm_mon);
    ASSERT(31, g->tm_mday);
    ASSERT(3, g->tm_wday);
    // normalisation: month 13, day 32, minute -1
    t = make(2023, 13, 32, 0, -1, 0);
    mktime(&t);
    ASSERT(124, t.tm_year);                      // 1 February 2024, minus a minute
    ASSERT(0, t.tm_mon);
    ASSERT(31, t.tm_mday);
    ASSERT(23, t.tm_hour);
    ASSERT(59, t.tm_min);
    t = make(2024, 3, 0, 12, 0, 0);              // day 0 = the last day before
    mktime(&t);
    ASSERT(1, t.tm_mon);
    ASSERT(29, t.tm_mday);
    // strftime
    t = make(2024, 2, 29, 13, 5, 9);
    mktime(&t);
    STR("2024-02-29 13:05:09", fmt("%Y-%m-%d %H:%M:%S", &t));
    STR("Thursday February Thu Feb", fmt("%A %B %a %b", &t));
    STR("060 24 29 PM 01 4 4 %", fmt("%j %y %e %p %I %u %w %%", &t));
    STR("Thu Feb 29 13:05:09 2024", fmt("%c", &t));
    STR("02/29/24|2024-02-29|13:05:09|13:05|01:05:09 PM", fmt("%D|%F|%T|%R|%r", &t));
    STR("20 Feb", fmt("%C %h", &t));
    STR("02/29/24 13:05:09", fmt("%x %X", &t));
    STR("09 08 09 2024 24", fmt("%V %U %W %G %g", &t));
    t = make(2024, 1, 1, 0, 0, 0);               // a Monday
    mktime(&t);
    STR("01 00 01 2024", fmt("%V %U %W %G", &t));
    t = make(2021, 1, 1, 0, 0, 0);               // a Friday: ISO week 53 of 2020
    mktime(&t);
    STR("53 2020 5", fmt("%V %G %u", &t));
    t = make(2024, 12, 30, 0, 0, 0);             // a Monday: ISO week 1 of 2025
    mktime(&t);
    STR("01 2025", fmt("%V %G", &t));
    t = make(2024, 7, 4, 0, 0, 0);
    mktime(&t);
    STR(" 4|12 AM", fmt("%e|%I %p", &t));
    STR("a\tb\nc", fmt("a%tb%nc", &t));
    ASSERT(0, strftime(buf, 5, "%Y-%m", &t));   // does not fit
    ASSERT(4, strftime(buf, 5, "%Y", &t));
    // asctime / ctime
    t = make(2024, 2, 29, 13, 5, 9);
    mktime(&t);
    STR("Thu Feb 29 13:05:09 2024\n", asctime(&t));
    STR("Thu Jan  1 00:00:00 1970\n", ctime(&off));    // local midnight
    // the clock
    time_t now = time(0), now2;
    ASSERT(1, now > 1700000000);                 // after 2023
    ASSERT(1, time(&now2) >= now);
    ASSERT(1, now2 >= now);
    ASSERT(1, clock() >= 0);
    ASSERT(1000, CLOCKS_PER_SEC);
    ASSERT(1, difftime(10, 4) == 6.0);
    struct timespec ts;
    ASSERT(TIME_UTC, timespec_get(&ts, TIME_UTC));
    ASSERT(1, ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000);
    struct tm r;
    ASSERT(1, gmtime_r(&when, &r) == &r);
    ASSERT(29, r.tm_mday);
    return g_fail;
}
