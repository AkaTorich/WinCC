// Ядро Elements: контекст, создание элементов, свойства, вспомогательные
// функции (шрифты, текст, цвета, DPI), главный цикл el_run/el_frame.
#include "el_internal.h"
#include <stdio.h>

Ctx g;

// ---------------------------------------------------------------- utils ----

int px(int d) { return (int)(d * g.dpi + 0.5); }
int dip(int p) { return (int)(p / g.dpi + 0.5); }

u32 mix(u32 a, u32 b, double t) {
    int ar, ag, ab, br, bg2, bb, r, gn, bl;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    ar = (a >> 16) & 255; ag = (a >> 8) & 255; ab = a & 255;
    br = (b >> 16) & 255; bg2 = (b >> 8) & 255; bb = b & 255;
    r = (int)(ar + (br - ar) * t + 0.5);
    gn = (int)(ag + (bg2 - ag) * t + 0.5);
    bl = (int)(ab + (bb - ab) * t + 0.5);
    return (u32)((r << 16) | (gn << 8) | bl);
}

// --- шрифты (кэш) ---
// «лицо» (face): 0 — Segoe UI, 1 — Consolas (mono), 2+ — встроенные через el_font_add
typedef struct { int size, bold, face; GpFont *f; } FontEntry;
typedef struct { char *name; int idx; } FaceName;
static VEC(FontEntry) s_fonts;
static VEC(GpFontFamily *) s_fam;        // 0 Segoe UI, 1 Consolas, 2+ встроенные
static VEC(FaceName) s_faces;            // имя -> индекс лица
// по коллекции на каждый встроенный шрифт: в ней ровно одно семейство. (В общей коллекции GDI+
// отдаёт семейства по алфавиту, а не по порядку добавления — имя привязывалось к чужому шрифту.)
static VEC(GpFontCollection *) s_pfcs;
static int s_nextFace = 2;

static void fam_set(int idx, GpFontFamily *f) {
    if (idx >= s_fam.cap) s_fam.a = vgrow(s_fam.a, &s_fam.cap, idx + 1, sizeof(*s_fam.a));
    if (idx >= s_fam.n) s_fam.n = idx + 1;
    s_fam.a[idx] = f;
}

static GpFontFamily *make_family(const WCHAR *a, const WCHAR *b) {
    GpFontFamily *f = NULL;
    if (GdipCreateFontFamilyFromName(a, NULL, &f) != GP_OK) {
        f = NULL;
        GdipCreateFontFamilyFromName(b, NULL, &f);
    }
    return f;
}

GpFont *get_font(int size, int bold, int face) {
    int i;
    GpFont *f = NULL;
    FontEntry fe;
    if (size <= 0) size = g.def_font;
    if (face < 0) face = 0;
    if (s_fam.n < 2 || !s_fam.a[0]) fam_set(0, make_family(L"Segoe UI", L"Tahoma"));
    if (!s_fam.a[1]) fam_set(1, make_family(L"Consolas", L"Courier New"));
    if (face >= s_fam.n || !s_fam.a[face]) face = 0;
    bold = bold ? 1 : 0;
    for (i = 0; i < s_fonts.n; i++)
        if (s_fonts.a[i].size == size && s_fonts.a[i].bold == bold && s_fonts.a[i].face == face) return s_fonts.a[i].f;
    // размер шрифта задаём в пикселях (мы сами в HiDPI-пикселях рисуем)
    if (GdipCreateFont(s_fam.a[face], (REAL)px(size), bold ? GP_FONT_BOLD : GP_FONT_REGULAR, GP_UNIT_PIXEL, &f) != GP_OK) f = NULL;
    fe.size = size; fe.bold = bold; fe.face = face; fe.f = f;
    VPUSH(s_fonts, fe);
    return f;
}

int font_face_by_name(const char *name) {
    int i;
    if (!name || !*name) return 0;
    if (!strcmp(name, "default")) return 0;
    if (!strcmp(name, "mono")) return 1;
    for (i = 0; i < s_faces.n; i++) if (!strcmp(s_faces.a[i].name, name)) return s_faces.a[i].idx;
    return 0;
}

static int font_take(GpFontCollection *pfc, const char *alias) {
    INT cnt = 0, found = 0;
    GpFontFamily **fams;
    GpFontFamily *ff = NULL;
    WCHAR fam[LF_FACESIZE];
    FaceName fn;
    int i, idx;
    memset(fam, 0, sizeof fam);
    GdipGetFontCollectionFamilyCount(pfc, &cnt);
    if (cnt <= 0) { GdipDeletePrivateFontCollection(&pfc); return 0; }
    fams = (GpFontFamily **)calloc((size_t)cnt, sizeof *fams);
    if (!fams) abort();
    GdipGetFontCollectionFamilyList(pfc, cnt, fams, &found);
    if (found > 0) GdipGetFamilyName(fams[0], fam, 0);
    free(fams);
    if (found <= 0 || GdipCreateFontFamilyFromName(fam, pfc, &ff) != GP_OK) { GdipDeletePrivateFontCollection(&pfc); return 0; }
    VPUSH(s_pfcs, pfc);
    idx = s_nextFace++;
    fam_set(idx, ff);
    for (i = 0; i < s_faces.n; i++) if (!strcmp(s_faces.a[i].name, alias)) { s_faces.a[i].idx = idx; request_redraw(); return 1; }
    fn.name = sdup(alias); fn.idx = idx;
    if (!fn.name) return 1;
    VPUSH(s_faces, fn);
    request_redraw();
    return 1;
}

int el_font_add(const char *name, const char *path) {
    GpFontCollection *pfc = NULL;
    WCHAR *wp;
    GpStatus st;
    if (!name || !*name || !path) return 0;
    if (!g.inited || GdipNewPrivateFontCollection(&pfc) != GP_OK) return 0;
    wp = wdup(path);
    st = GdipPrivateAddFontFile(pfc, wp);
    free(wp);
    if (st != GP_OK) { GdipDeletePrivateFontCollection(&pfc); return 0; }
    return font_take(pfc, name);
}
int el_font_add_memory(const char *name, const void *data, int size) {
    GpFontCollection *pfc = NULL;
    if (!name || !*name || !data || size <= 0) return 0;
    if (!g.inited || GdipNewPrivateFontCollection(&pfc) != GP_OK) return 0;
    if (GdipPrivateAddMemoryFont(pfc, data, size) != GP_OK) { GdipDeletePrivateFontCollection(&pfc); return 0; }
    return font_take(pfc, name);
}
void el_font_face(el e, const char *family) { if (!valid(e)) return; E(e)->mono = font_face_by_name(family); request_redraw(); }

int text_w(const WCHAR *s, int size, int bold, int mono) {
    int w, h;
    text_size(s, size, bold, mono, 0, 0, &w, &h);
    return w;
}

static GpStringFormat *s_fmt[2];       // замер текста: [0] с переносом, [1] в строку

void text_size(const WCHAR *s, int size, int bold, int mono, int wrapW, int maxw, int *w, int *h) {
    GpFont *f = get_font(size, bold, mono);
    int k = wrapW ? 0 : 1;
    REAL lim = wrapW ? (REAL)px(maxw) : 100000.0f;
    GpRectF layout = rectf(0, 0, lim, 100000.0f), bound;
    const WCHAR *t = (s && *s) ? s : L" ";
    memset(&bound, 0, sizeof bound);
    if (!s_fmt[k]) s_fmt[k] = gfx_format(!wrapW, GP_ALIGN_NEAR, GP_ALIGN_NEAR, 0);
    if (f) GdipMeasureString(g.measure_g, t, -1, f, &layout, s_fmt[k], &bound, NULL, NULL);
    // возвращаем в ПИКСЕЛЯХ (вся внутренняя геометрия — в пикселях)
    if (w) *w = (int)(bound.Width + 1.5f);
    if (h) *h = (int)(bound.Height + 1.5f);
    if ((!s || !*s) && w) *w = 0;
}

void text_size_u(const char *s, int size, int bold, int mono, int wrapW, int maxw, int *w, int *h) {
    WCHAR *ws = wdup(s);
    text_size(ws, size, bold, mono, wrapW, maxw, w, h);
    free(ws);
}

int text_w_u(const char *s, int size, int bold, int mono) {
    int w, h;
    text_size_u(s, size, bold, mono, 0, 0, &w, &h);
    return w;
}

static GpImage *load_image(const char *path) {
    GpImage *img = NULL;
    WCHAR *wp;
    if (!path || !*path) return NULL;
    wp = wdup(path);
    if (GdipCreateBitmapFromFile(wp, &img) != GP_OK) img = NULL;
    free(wp);
    return img;
}

// ---------------------------------------------------------------- tree -----

int el_alloc(int kind) {
    int h = -1, i;
    Element *e;
    for (i = 1; i < g.el.n; i++) if (!g.el.a[i].alive) { h = i; break; }
    if (h < 0) {
        Element z;
        memset(&z, 0, sizeof z);
        VPUSH(g.el, z);
        h = g.el.n - 1;
    }
    e = E(h);
    memset(e, 0, sizeof *e);
    e->alive = 1;
    e->kind = kind;
    e->w = e->h = e->minw = e->minh = e->maxw = e->maxh = -1;
    e->ha = e->va = A_STRETCH;
    e->rspan = e->cspan = 1;
    e->bw = -1; e->radius = -1;
    e->gap = -1;
    e->sc = 100; e->op = 255;
    e->styled_gen = -1;
    // отступы по умолчанию (в точках интерфейса), чтобы общая раскладка давала вид
    switch (kind) {
        case K_BUTTON:   e->pl = e->pr = 16; e->pt = e->pb = 7; break;
        case K_TOOL:     e->pl = e->pr = 8;  e->pt = e->pb = 6; break;
        case K_EDIT: case K_PASSWORD: e->pl = e->pr = 8; e->pt = e->pb = 6; break;
        case K_MEMO:     e->pl = e->pr = 6;  e->pt = e->pb = 6; break;
        case K_COMBO:    e->pl = 10; e->pr = 28; e->pt = e->pb = 6; break;
        case K_NUMBER:   e->pl = 8;  e->pr = 30; e->pt = e->pb = 5; break;
        case K_LIST:     e->pl = e->pr = 1; e->pt = e->pb = 4; break;
        case K_GROUP:    e->pl = e->pr = 12; e->pt = 10; e->pb = 12; break;
        case K_TAB:      e->pl = e->pr = 14; e->pt = e->pb = 12; break;
        case K_MENU:     e->pl = e->pr = 12; e->pt = e->pb = 6; break;
        default: break;
    }
    return h;
}

void el_add_child(int parent, int child) {
    Element *p;
    if (!valid(parent)) return;
    p = E(parent);
    E(child)->parent = parent;
    E(child)->next = 0;
    if (p->last) E(p->last)->next = child; else p->first = child;
    p->last = child;
}

static int make(int parent, int kind) {
    int h;
    if (parent == 0) parent = g.window;
    h = el_alloc(kind);
    el_add_child(parent, h);
    request_redraw();
    return h;
}

static void free_subtree(int h) {
    int c;
    Element *e;
    if (!valid(h)) return;
    c = E(h)->first;
    while (c) { int n = E(c)->next; free_subtree(c); c = n; }
    e = E(h);
    free(e->grid); e->grid = NULL;
    if (e->img) { GdipDisposeImage(e->img); e->img = NULL; }
    if (e->icon_img) { GdipDisposeImage(e->icon_img); e->icon_img = NULL; }
    free(e->text); free(e->tip); free(e->name); free(e->cls); free(e->inl); free(e->accel); free(e->icon);
    e->text = e->tip = e->name = e->cls = e->inl = e->accel = e->icon = NULL;
    e->alive = 0;
    e->kind = K_FREE;
    e->first = e->last = e->next = e->parent = 0;
}

// ---------------------------------------------------------------- окно -----

el el_window(const char *title, int w, int h) {
    int wh;
    if (!g.inited) {
        Element z;
        memset(&z, 0, sizeof z);
        g.el.n = 0;
        VPUSH(g.el, z);                     // индекс 0 — «нет»
        g.dpi = 1.0;
        g.def_font = 14;
        g.corner = 5;
        g.dark = 1;
        g.accent = 0x4c8bf5;
        g.win_opacity = 255;
        g.win_minw = g.win_minh = g.win_maxw = g.win_maxh = -1;
        g.aspect_w = g.aspect_h = 0;
        g.win_shape = 0; g.layered = 0; g.frameless = 0;
        g.resizable = 1;
        theme_init();
        gdip_init();
        g.inited = 1;
    }
    wh = el_alloc(K_WINDOW);
    g.window = wh;
    sset(&E(wh)->text, title);
    E(wh)->parent = 0;
    win_create(title, w, h);
    return wh;
}

void el_title(const char *title) {
    if (valid(g.window)) sset(&E(g.window)->text, title);
    win_set_title(title);
}

// ---------------------------------------------------------- панели/контролы

el el_stack(el parent, int horizontal) { int h = make(parent, K_STACK); E(h)->horiz = horizontal != 0; return h; }
el el_dock(el parent) { return make(parent, K_DOCK); }
el el_wrap(el parent) { int h = make(parent, K_WRAP); E(h)->wrap = 1; return h; }
el el_canvas(el parent) { return make(parent, K_CANVAS); }
el el_scroll(el parent) { return make(parent, K_SCROLL); }
el el_border(el parent) { return make(parent, K_BORDER); }

el el_group(el parent, const char *header) { int h = make(parent, K_GROUP); sset(&E(h)->text, header); return h; }

static int grid_parse(const char *spec, u8 *types, int *vals) {
    int n = 0;
    const char *p = spec;
    if (!spec || !*spec) { types[0] = 2; vals[0] = 100; return 1; }
    while (*p && n < GMAX) {
        char tok[32];
        int ti = 0;
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        // определить тип токена
        while (*p && *p != ',' && ti < 31) tok[ti++] = *p++;
        tok[ti] = 0;
        // убрать хвостовые пробелы
        while (ti > 0 && tok[ti - 1] == ' ') tok[--ti] = 0;
        if (strstr(tok, "Auto") || strstr(tok, "auto") || tok[0] == 'A' || tok[0] == 'a') { types[n] = 0; vals[n] = 0; }
        else if (strchr(tok, '*')) { int wgt = atoi(tok); types[n] = 2; vals[n] = (wgt <= 0 ? 1 : wgt) * 100; }
        else { types[n] = 1; vals[n] = atoi(tok); }
        n++;
    }
    if (n == 0) { types[0] = 2; vals[0] = 100; n = 1; }
    return n;
}

el el_grid(el parent, const char *cols, const char *rows) {
    int h = make(parent, K_GRID);
    GridDef *gd = (GridDef *)calloc(1, sizeof(GridDef));
    if (!gd) abort();
    gd->nc = grid_parse(cols, gd->ct, gd->cv);
    gd->nr = grid_parse(rows, gd->rt, gd->rv);
    E(h)->grid = gd;
    return h;
}

el el_tabs(el parent) { int h = make(parent, K_TABS); E(h)->ival = 0; return h; }
el el_tab(el tabs, const char *header) {
    int h;
    if (!valid(tabs)) return 0;
    h = el_alloc(K_TAB);
    sset(&E(h)->text, header);
    el_add_child(tabs, h);
    request_redraw();
    return h;
}

el el_text(el parent, const char *text) { int h = make(parent, K_TEXT); sset(&E(h)->text, text); return h; }
el el_button(el parent, const char *text) { int h = make(parent, K_BUTTON); sset(&E(h)->text, text); return h; }
el el_check(el parent, const char *text, int *value) { int h = make(parent, K_CHECK); sset(&E(h)->text, text); E(h)->iv = value; return h; }
el el_radio(el parent, const char *text, int *group, int value) { int h = make(parent, K_RADIO); sset(&E(h)->text, text); E(h)->iv = group; E(h)->ival = value; return h; }

el el_edit(el parent, char *buf, int cap) { int h = make(parent, K_EDIT); E(h)->buf = buf; E(h)->cap = cap; return h; }
el el_memo(el parent, char *buf, int cap) { int h = make(parent, K_MEMO); E(h)->buf = buf; E(h)->cap = cap; return h; }
el el_password(el parent, char *buf, int cap) { int h = make(parent, K_PASSWORD); E(h)->buf = buf; E(h)->cap = cap; return h; }

el el_combo(el parent, const char *const *items, int count, int *sel) { int h = make(parent, K_COMBO); E(h)->items = items; E(h)->count = count; E(h)->iv = sel; return h; }
el el_listbox(el parent, const char *const *items, int count, int *sel) { int h = make(parent, K_LIST); E(h)->items = items; E(h)->count = count; E(h)->iv = sel; return h; }
void el_items(el e, const char *const *items, int count) { if (!valid(e)) return; E(e)->items = items; E(e)->count = count; request_redraw(); }

el el_slider(el parent, int min, int max, int *value) { int h = make(parent, K_SLIDER); E(h)->vmin = min; E(h)->vmax = max; E(h)->iv = value; return h; }
el el_progress(el parent, int max, int *value) { int h = make(parent, K_PROGRESS); E(h)->vmin = 0; E(h)->vmax = max; E(h)->iv = value; return h; }
el el_separator(el parent) { return make(parent, K_SEP); }

el el_number(el parent, int min, int max, int step, int *value) {
    int h = make(parent, K_NUMBER);
    E(h)->vmin = min; E(h)->vmax = max; E(h)->step = step <= 0 ? 1 : step; E(h)->iv = value;
    if (value) { if (*value < min) *value = min; if (*value > max) *value = max; }
    return h;
}

el el_image(el parent, const char *path) {
    int h = make(parent, K_IMAGE);
    sset(&E(h)->text, path);
    E(h)->img = load_image(path);
    return h;
}

el el_paint(el parent, void (*fn)(el, int, int, void *), void *arg) {
    int h = make(parent, K_PAINT);
    E(h)->paint_fn = fn; E(h)->paint_arg = arg; E(h)->needs_redraw = 1;
    return h;
}
void el_redraw(el e) { if (valid(e)) E(e)->needs_redraw = 1; request_redraw(); }

el el_toolbar(el parent) { int h = make(parent, K_TOOLBAR); E(h)->horiz = 1; E(h)->flat = 1; return h; }
el el_tool(el toolbar, const char *icon, const char *text, const char *tip) {
    int h = make(toolbar, K_TOOL);
    sset(&E(h)->text, text);
    sset(&E(h)->tip, tip);
    E(h)->flat = 1;
    if (icon && *icon) { E(h)->icon_img = load_image(icon); E(h)->icon_size = 20; }
    return h;
}
void el_icon(el button, const char *path, int size) {
    if (!valid(button)) return;
    if (E(button)->icon_img) { GdipDisposeImage(E(button)->icon_img); E(button)->icon_img = NULL; }
    E(button)->icon_img = load_image(path);
    E(button)->icon_size = size <= 0 ? 20 : size;
    request_redraw();
}

// ---------------------------------------------------------- свойства -------

void el_size(el e, int w, int h) { if (!valid(e)) return; E(e)->w = w; E(e)->h = h; request_redraw(); }
void el_min_size(el e, int w, int h) { if (!valid(e)) return; E(e)->minw = w; E(e)->minh = h; request_redraw(); }
void el_max_size(el e, int w, int h) { if (!valid(e)) return; E(e)->maxw = w; E(e)->maxh = h; request_redraw(); }
void el_margin(el e, int l, int t, int r, int b) { Element *x; if (!valid(e)) return; x = E(e); x->ml = l; x->mt = t; x->mr = r; x->mb = b; request_redraw(); }
void el_padding(el e, int l, int t, int r, int b) { Element *x; if (!valid(e)) return; x = E(e); x->pl = l; x->pt = t; x->pr = r; x->pb = b; request_redraw(); }
void el_align(el e, int h, int v) { if (!valid(e)) return; E(e)->ha = h; E(e)->va = v; request_redraw(); }
void el_cell(el e, int row, int col, int rowspan, int colspan) {
    Element *x;
    if (!valid(e)) return;
    x = E(e); x->row = row; x->col = col; x->rspan = rowspan < 1 ? 1 : rowspan; x->cspan = colspan < 1 ? 1 : colspan;
    request_redraw();
}
void el_dock_side(el e, int side) { if (!valid(e)) return; E(e)->dock = side; request_redraw(); }
void el_pos(el e, int x, int y) { if (!valid(e)) return; E(e)->px = x; E(e)->py = y; request_redraw(); }
void el_gap(el e, int p) { if (!valid(e)) return; E(e)->gap = p; request_redraw(); }

void el_set_text(el e, const char *text) {
    if (!valid(e)) return;
    sset(&E(e)->text, text);
    if (E(e)->kind == K_IMAGE) {
        if (E(e)->img) { GdipDisposeImage(E(e)->img); E(e)->img = NULL; }
        E(e)->img = load_image(text);
    }
    request_redraw();
}
const char *el_get_text(el e) { return valid(e) ? S(E(e)->text) : ""; }
void el_text_bind(el e, const char *live) { if (!valid(e)) return; E(e)->live = live; request_redraw(); }
void el_enable(el e, int on) { if (!valid(e)) return; E(e)->disabled = !on; request_redraw(); }
void el_show(el e, int on) { if (!valid(e)) return; E(e)->hidden = !on; request_redraw(); }
void el_tooltip(el e, const char *text) { if (!valid(e)) return; sset(&E(e)->tip, text); }
void el_wrap_text(el e, int on) { if (!valid(e)) return; E(e)->wrap = on != 0; request_redraw(); }
void el_font(el e, int size, int bold) { if (!valid(e)) return; E(e)->fsize = size; E(e)->bold = bold; request_redraw(); }
void el_colors(el e, int fg, int bg) {
    Element *x;
    if (!valid(e)) return;
    x = E(e);
    if (fg >= 0) { x->fg = (u32)fg; x->has_fg = 1; }
    if (bg >= 0) { x->bg = (u32)bg; x->has_bg = 1; }
    request_redraw();
}
void el_frame_style(el e, int color, int width, int radius) {
    Element *x;
    if (!valid(e)) return;
    x = E(e);
    if (color >= 0) { x->bd = (u32)color; x->has_bd = 1; }
    x->bw = width; x->radius = radius;
    request_redraw();
}

void el_name(el e, const char *name) { if (!valid(e)) return; sset(&E(e)->name, name); }
el el_find(const char *name) {
    int i;
    if (!name || !*name) return 0;
    for (i = 1; i < g.el.n; i++) if (g.el.a[i].alive && g.el.a[i].name && !strcmp(g.el.a[i].name, name)) return i;
    return 0;
}
void el_class(el e, const char *classes) { if (!valid(e)) return; sset(&E(e)->cls, classes); E(e)->styled_gen = -1; restyle_all(); request_redraw(); }

void el_select_tab(el tabs, int index) { if (!valid(tabs)) return; E(tabs)->ival = index; request_redraw(); }
int  el_selected_tab(el tabs) { return valid(tabs) ? E(tabs)->ival : 0; }

void el_scroll_to(el e, int pos) { if (!valid(e)) return; E(e)->scroll = pos; request_redraw(); }
void el_focus(el e) { set_focus(valid(e) ? e : 0); request_redraw(); }
el el_focused(void) { return g.focus; }

void el_clear(el e) {
    int c;
    if (!valid(e)) return;
    c = E(e)->first;
    while (c) { int n = E(c)->next; free_subtree(c); c = n; }
    E(e)->first = E(e)->last = 0;
    request_redraw();
}
void el_remove(el e) {
    int p;
    if (!valid(e) || e == g.window) return;
    p = E(e)->parent;
    if (valid(p)) {
        int prev = 0, c = E(p)->first;
        while (c && c != e) { prev = c; c = E(c)->next; }
        if (c == e) {
            if (prev) E(prev)->next = E(e)->next; else E(p)->first = E(e)->next;
            if (E(p)->last == e) E(p)->last = prev;
        }
    }
    free_subtree(e);
    request_redraw();
}

void el_data(el e, void *p) { if (valid(e)) E(e)->data = p; }
void *el_get_data(el e) { return valid(e) ? E(e)->data : NULL; }

// ---------------------------------------------------------- события --------

void el_on_click(el e, el_fn fn, void *arg) { if (!valid(e)) return; E(e)->on_click = fn; E(e)->click_arg = arg; }
void el_on_change(el e, el_fn fn, void *arg) { if (!valid(e)) return; E(e)->on_change = fn; E(e)->change_arg = arg; }
int  el_clicked(el e) { return valid(e) && E(e)->fl_click; }
int  el_changed(el e) { return valid(e) && E(e)->fl_change; }
void fire_click(int h) {
    el_fn fn;
    if (!valid(h)) return;
    E(h)->fl_click = 1;
    fn = E(h)->on_click;
    if (fn) fn(h, E(h)->click_arg);
    request_redraw();
}
void fire_change(int h) {
    el_fn fn;
    if (!valid(h)) return;
    E(h)->fl_change = 1;
    fn = E(h)->on_change;
    if (fn) fn(h, E(h)->change_arg);
    request_redraw();
}

void el_on_close(el_fn fn, void *arg) { g.on_close = fn; g.on_close_arg = arg; g.close_guard = (fn != NULL); }
void el_quit(void) { g.quit = 1; }

// ---------------------------------------------------------- главный цикл ---

static void clear_frame_flags(void) {
    int i;
    for (i = 1; i < g.el.n; i++) if (g.el.a[i].alive) { g.el.a[i].fl_click = 0; g.el.a[i].fl_change = 0; }
}

int el_frame(void) {
    LARGE_INTEGER freq, now;
    if (!g.inited || g.quit) return 0;
    win_pump();
    if (g.quit) return 0;
    if (g.close_req) {
        g.close_req = 0;
        if (g.close_guard && g.on_close) g.on_close(0, g.on_close_arg);
        else return 0;
    }

    // время
    QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&now);
    g.last_ms = g.now_ms;
    g.now_ms = (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
    if (g.last_ms == 0) g.last_ms = g.now_ms;

    clear_frame_flags();

    // раскладка
    layout_root();

    // ввод: оверлеи/меню/списки перехватывают
    if (g.msg_kind) overlay_frame_input();
    else if (g.combo_open) combo_frame_input();
    else if (g.popups.n > 0) menu_frame_input();
    else input_frame();                 // при g.modal ввод ограничен поддеревом окна-диалога

    // анимации/таймеры
    anim_frame();

    // раскладка ещё раз — обработчики могли поменять дерево/значения
    layout_root();

    // рисование
    draw_tree();
    if (g.combo_open) combo_draw();
    if (g.popups.n > 0) menu_draw();
    if (g.msg_kind || g.modal || g.toast_until > g.now_ms) overlay_draw();
    draw_tooltip();
    apply_effects();                    // постобработка кадра (дисперсия, глитч, свечение…)
    present();

    // сброс колеса за кадр
    g.wheel = 0; g.wheel_consumed_by = 0;
    g.pbuttons = g.buttons;
    g.pmx = g.mx; g.pmy = g.my;
    g.keys.n = 0;

    // экономия ЦП: спим тем дольше, чем меньше происходит
    if (anim_busy() || effects_animated()) win_wait(12);
    else if (g.dirty) win_wait(30);
    else win_wait(1000);
    g.dirty = 0;
    return g.quit ? 0 : 1;
}

void el_run(void) { while (el_frame()) {} }

void request_redraw(void) { g.dirty = 1; }
