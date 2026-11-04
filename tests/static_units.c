// with: static_units.part
// Two sources of one program, each with statics of the same names: every
// source sees its own (internal linkage), a non-static name is shared.
#include <stdio.h>

static int counter = 10;
static int helper(int x) { return x + counter; }
static const char *tag(void) { return "a"; }
int shared_value(void);         // static_units.part
int other_helper(int x);        // static_units.part: its own helper()
const char *other_tag(void);

int main(void)
{
    int bad = 0;
    if (helper(1) != 11) { printf("helper a\n"); bad++; }
    if (other_helper(1) != 101) { printf("helper b\n"); bad++; }
    if (tag()[0] != 'a' || other_tag()[0] != 'b') { printf("tag\n"); bad++; }
    if (shared_value() != 110) { printf("shared\n"); bad++; }
    counter = 20;
    if (helper(1) != 21 || other_helper(1) != 101) { printf("counter\n"); bad++; }
    return bad;
}
