// A header of units.c and its .part sources, with #pragma once.
#pragma once
static int once_id(void)
{
    static int n;
    return ++n;
}
