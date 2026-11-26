// Leaves out of a Windows program what it never reaches (WinCC).
//
// The library (runtime\) is compiled into every program whole; the code
// generator marks each of its functions and variables, and every string
// literal, with a ".gc" line before it. Here the assembly text is cut into
// pieces: one piece is one function or one variable (with its .align, .local,
// .gc lines). A marked piece stays only if something that stays names one of
// its labels -- directly, or through an ".alias name, target" line (printf ->
// rt_printf). Everything not marked (the program's own code and data, .asm
// files, the entry) stays and is where the search starts.
#include "cc.h"

typedef struct {
    const char *s;
    int len;
} Line;

typedef struct {
    int first, last;            // its lines [first, last)
    int removable;              // marked .gc
    int keep;
} Piece;

typedef struct {
    const char *name;
    int len;
    int piece;                  // the piece that defines it, or -1 for an alias
    const char *target;         // an alias: the label it stands for
    int tlen;
} Name;

static Name *g_names;
static int g_cap;

static int id1(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || c == '$' || c == '@' || c >= 0x80; }
static int id2(int c) { return id1(c) || (c >= '0' && c <= '9'); }

static u32 hash(const char *s, int n)
{
    u32 h = 2166136261u;
    for (int i = 0; i < n; i++)
        h = (h ^ (u8)s[i]) * 16777619u;
    return h;
}

static Name *lookup(const char *s, int n, int add)
{
    u32 k = hash(s, n) & (u32)(g_cap - 1);
    while (g_names[k].name) {
        if (g_names[k].len == n && cc_strneq(g_names[k].name, s, n))
            return &g_names[k];
        k = (k + 1) & (u32)(g_cap - 1);
    }
    if (!add)
        return 0;
    g_names[k].name = s;
    g_names[k].len = n;
    g_names[k].piece = -1;
    return &g_names[k];
}

static const char *skip(const char *p, const char *e)
{
    while (p < e && (*p == ' ' || *p == '\t' || *p == '\r'))
        p++;
    return p;
}

// the word at the start of a line (after blanks), its length
static int first_word(const Line *l, const char **w)
{
    const char *p = skip(l->s, l->s + l->len), *e = l->s + l->len, *q = p;
    while (q < e && id2((u8)*q))
        q++;
    *w = p;
    return (int)(q - p);
}

static int is_word(const Line *l, const char *word)
{
    const char *w;
    int n = first_word(l, &w);
    return n == cc_strlen(word) && cc_strneq(w, word, n);
}

// "name:" at the start of the line: the name's length, else 0
static int label_of(const Line *l, const char **name)
{
    const char *w;
    int n = first_word(l, &w);
    if (n && id1((u8)w[0]) && w + n < l->s + l->len && w[n] == ':') {
        *name = w;
        return n;
    }
    return 0;
}

static void keep_name(Piece *pc, int *stack, int *sp, const char *s, int n, int depth)
{
    Name *nm = lookup(s, n, 0);
    if (!nm)
        return;                 // a DLL's function, a register, an instruction...
    if (nm->piece < 0) {        // an alias: the label it stands for
        if (nm->target && depth < 4)
            keep_name(pc, stack, sp, nm->target, nm->tlen, depth + 1);
        return;
    }
    Piece *p = &pc[nm->piece];
    if (!p->keep) {
        p->keep = 1;
        stack[(*sp)++] = nm->piece;
    }
}

// every name a line mentions keeps the piece that defines it
static void scan_line(const Line *l, Piece *pc, int *stack, int *sp)
{
    const char *p = l->s, *e = l->s + l->len;
    while (p < e && (*p == ' ' || *p == '\t'))
        p++;
    if (e - p > 4 && p[0] == '.' && p[1] == 'd' && p[2] == 'b' && p[3] == 'g')
        return;                 // .dbgvar & co (wcc -g): a variable's name is not a use of a function
    p = l->s;
    while (p < e) {
        if (*p == ';' || (*p == '/' && p + 1 < e && p[1] == '/'))
            return;             // a comment
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (p < e && *p != q) p++;
            p++;
            continue;
        }
        if (id1((u8)*p) && !(p > l->s && id2((u8)p[-1]))) {
            const char *s = p;
            while (p < e && id2((u8)*p)) p++;
            if (*s == '@') s++;     // @name: a C name inside asm
            if (p > s)
                keep_name(pc, stack, sp, s, (int)(p - s), 0);
            continue;
        }
        p++;
    }
}

char *cc_strip_unused(const char *text)
{
    // lines
    int nl = 1;
    for (const char *p = text; *p; p++)
        if (*p == '\n') nl++;
    Line *ln = cc_alloc(sizeof(Line) * (size_t)nl);
    nl = 0;
    for (const char *p = text;;) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        ln[nl].s = p;
        ln[nl].len = (int)(e - p);
        nl++;
        if (!*e) break;
        p = e + 1;
    }

    g_cap = 1;
    while (g_cap < nl * 2 + 64) g_cap *= 2;
    g_names = cc_alloc(sizeof(Name) * (size_t)g_cap);

    // pieces: a piece starts at its first .gc/.align/.local/.export line or its
    // label; in .text only a label without a dot (or a string literal's .L.str.N) starts one (.L... are inside)
    Piece *pc = cc_alloc(sizeof(Piece) * (size_t)(nl + 1));
    int *piece_of = cc_alloc(sizeof(int) * (size_t)nl);     // -1: outside any piece (kept)
    int np = 0, cur = -1, pending = -1, text_sec = 1;
    for (int i = 0; i < nl; i++) {
        const char *name;
        int n;
        if (is_word(&ln[i], ".text") || is_word(&ln[i], ".data") || is_word(&ln[i], ".bss") || is_word(&ln[i], ".tls")) {
            text_sec = is_word(&ln[i], ".text");
            if (cur >= 0) pc[cur].last = i;
            cur = -1;
            pending = -1;
            piece_of[i] = -1;
            continue;
        }
        if (is_word(&ln[i], ".alias")) {
            const char *w;
            first_word(&ln[i], &w);
            const char *p = skip(w + 6, ln[i].s + ln[i].len), *e = ln[i].s + ln[i].len, *q = p;
            while (q < e && id2((u8)*q)) q++;
            const char *t = skip(q, e);
            if (t < e && *t == ',') t = skip(t + 1, e);
            if (t < e && *t == '&') t = skip(t + 1, e);
            const char *te = t;
            while (te < e && id2((u8)*te)) te++;
            Name *nm = lookup(p, (int)(q - p), 1);
            if (nm->piece < 0 && !nm->target) {     // the first alias of a name is the one
                nm->target = t;
                nm->tlen = (int)(te - t);
            }
            piece_of[i] = -2;                       // not scanned for names
            continue;
        }
        if (is_word(&ln[i], ".dbgfile")) {
            piece_of[i] = -2;           // a file's number stays even if the function that named it first goes
            continue;
        }
        int starter = is_word(&ln[i], ".gc") || is_word(&ln[i], ".local") || is_word(&ln[i], ".export") ||
                      (!text_sec && is_word(&ln[i], ".align"));
        n = label_of(&ln[i], &name);
        if (starter && pending < 0) {
            if (cur >= 0) pc[cur].last = i;
            cur = np++;
            pc[cur].first = i;
            pc[cur].removable = 0;
            pending = cur;
        }
        // in .text a string literal (.L.str.N, Windows: literals lie next to the code) starts a piece too;
        // goto labels inside functions (.L..N) do not
        int literal = n > 7 && name[0] == '.' && name[1] == 'L' && name[2] == '.' && name[3] == 's' &&
                      name[4] == 't' && name[5] == 'r' && name[6] == '.';
        if (n && (!text_sec || name[0] != '.' || literal)) {
            if (pending < 0) {                      // a label with nothing before it starts a piece
                if (cur >= 0) pc[cur].last = i;
                cur = np++;
                pc[cur].first = i;
                pc[cur].removable = 0;
            }
            pending = -1;
        }
        if (is_word(&ln[i], ".gc") && cur >= 0)
            pc[cur].removable = 1;
        if (n && cur >= 0) {
            Name *nm = lookup(name, n, 1);
            nm->piece = cur;                        // a label: its piece (it wins over an alias)
        }
        piece_of[i] = cur;
    }
    if (cur >= 0) pc[cur].last = nl;

    // what stays: everything not marked, and what it reaches
    int *stack = cc_alloc(sizeof(int) * (size_t)(np + 1));
    int sp = 0;
    for (int i = 0; i < np; i++)
        if (!pc[i].removable) {
            pc[i].keep = 1;
            stack[sp++] = i;
        }
    for (int i = 0; i < nl; i++)
        if (piece_of[i] == -1)
            scan_line(&ln[i], pc, stack, &sp);
    while (sp > 0) {
        Piece *p = &pc[stack[--sp]];
        for (int i = p->first; i < p->last; i++)
            if (piece_of[i] != -2)          // an .alias line inside a piece names nothing it needs
                scan_line(&ln[i], pc, stack, &sp);
    }

    // the text again, without what nothing reaches
    size_t total = 0;
    for (int i = 0; i < nl; i++)
        if (piece_of[i] < 0 || pc[piece_of[i]].keep)
            total += (size_t)ln[i].len + 1;
    char *out = cc_alloc(total + 1);
    size_t k = 0;
    for (int i = 0; i < nl; i++) {
        if (piece_of[i] >= 0 && !pc[piece_of[i]].keep)
            continue;
        for (int j = 0; j < ln[i].len; j++)
            out[k++] = ln[i].s[j];
        out[k++] = '\n';
    }
    out[k] = 0;
    return out;
}
