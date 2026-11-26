// Темы Elements: тёмная/светлая, 16 именованных тем, 14 цветовых слотов.
#include "el_internal.h"

// Слоты: 0 окно,1 панель,2 кнопка,3 наведение,4 нажатие,5 рамка,6 текст,
// 7 яркий текст,8 тусклый,9 акцент,10 текст на акценте,11 поле,12 выделение,13 меню
static void set_dark(u32 *t, u32 accent) {
    t[0] = 0x1e1e1e; t[1] = 0x252526; t[2] = 0x333337; t[3] = 0x3f3f46; t[4] = 0x505059;
    t[5] = 0x3f3f46; t[6] = 0xe6e6e6; t[7] = 0xffffff; t[8] = 0x9a9a9a; t[9] = accent;
    t[10] = 0xffffff; t[11] = 0x2b2b2e; t[12] = mix(accent, 0x000000, 0.35); t[13] = 0x2a2a2d;
}
static void set_light(u32 *t, u32 accent) {
    t[0] = 0xf3f3f3; t[1] = 0xffffff; t[2] = 0xe8e8e8; t[3] = 0xdcdcdc; t[4] = 0xc8c8c8;
    t[5] = 0xc0c0c0; t[6] = 0x1b1b1b; t[7] = 0x000000; t[8] = 0x6a6a6a; t[9] = accent;
    t[10] = 0xffffff; t[11] = 0xffffff; t[12] = mix(accent, 0xffffff, 0.6); t[13] = 0xffffff;
}

void theme_init(void) {
    if (g.dark) set_dark(g.th, g.accent); else set_light(g.th, g.accent);
    g.th[14] = 0; g.th[15] = 0;
}

u32 th(int slot) { if (slot < 0 || slot > 13) return 0x808080; return g.th[slot]; }

void el_theme(int dark) { g.dark = dark ? 1 : 0; g.theme_idx = dark ? 0 : 1; theme_init(); request_redraw(); }
void el_accent(int rgb) { if (rgb >= 0) g.accent = (u32)rgb; theme_init(); request_redraw(); }
void el_default_font(int size) { if (size > 0) g.def_font = size; request_redraw(); }
void el_corner_radius(int r) { g.corner = r; request_redraw(); }
void el_theme_color(int slot, int rgb) { if (slot >= 0 && slot <= 15 && rgb >= 0) g.th[slot] = (u32)rgb; request_redraw(); }

// --- именованные темы ---
// name is the English name; alias is the DyOS name, so programs written for DyOS keep working
typedef struct { const char *name; const char *alias; int dark; u32 accent; u32 win, panel, button, border, text, input, menu; } Named;
static const Named NAMED[] = {
    { "Night",     "Ночь",      1, 0x4c8bf5, 0x1e1e1e, 0x252526, 0x333337, 0x3f3f46, 0xe6e6e6, 0x2b2b2e, 0x2a2a2d },
    { "Day",       "День",      0, 0x2f6fde, 0xf3f3f3, 0xffffff, 0xe8e8e8, 0xc0c0c0, 0x1b1b1b, 0xffffff, 0xffffff },
    { "Amethyst",  "Аметист",   1, 0xb085f5, 0x1a1626, 0x241b33, 0x2f2340, 0x473363, 0xece6f7, 0x251c35, 0x2a2040 },
    { "Emerald",   "Изумруд",   1, 0x3ddc84, 0x0f1c17, 0x15271f, 0x1c3429, 0x2a4e3c, 0xdef5ea, 0x15271f, 0x1a3028 },
    { "Sapphire",  "Сапфир",    1, 0x4d9fff, 0x0e1626, 0x152034, 0x1b2c49, 0x2a4169, 0xdbe8fb, 0x152034, 0x1a2a46 },
    { "Ruby",      "Рубин",     1, 0xff5d6c, 0x24121a, 0x331a24, 0x45222f, 0x63303f, 0xf7dee4, 0x331a24, 0x3a2029 },
    { "Amber",     "Янтарь",    1, 0xffb347, 0x241c0f, 0x332814, 0x45371c, 0x63512a, 0xf7ecd9, 0x332814, 0x3a3018 },
    { "Obsidian",  "Обсидиан",  1, 0x8ab4ff, 0x111214, 0x17181b, 0x1f2024, 0x303136, 0xdfe2e8, 0x17181b, 0x1b1c20 },
    { "Parchment", "Пергамент", 0, 0xa6703a, 0xf0e6d2, 0xfbf5e6, 0xeaddc2, 0xcbb59a, 0x3a2f1e, 0xfffdf4, 0xfbf5e6 },
    { "Ocean",     "Океан",     1, 0x33c4d8, 0x0d1b20, 0x12262d, 0x18333c, 0x244c58, 0xd6f0f5, 0x12262d, 0x163038 },
    { "Forest",    "Лес",       1, 0x7bc043, 0x16200f, 0x202e15, 0x2b3d1c, 0x3f5a2a, 0xe6f2d9, 0x202e15, 0x26361a },
    { "Sakura",    "Сакура",    0, 0xe56b9a, 0xfdeef4, 0xffffff, 0xf7d6e3, 0xe9afc5, 0x4a2535, 0xffffff, 0xffffff },
    { "Graphite",  "Графит",    1, 0x9aa0a6, 0x202124, 0x282a2d, 0x35383c, 0x4a4e54, 0xe8eaed, 0x282a2d, 0x2c2e31 },
    { "Neon",      "Неон",      1, 0x00e5ff, 0x0a0f14, 0x10171f, 0x16202b, 0x203445, 0xd7f6ff, 0x10171f, 0x121c26 },
    { "Gold",      "Золото",    1, 0xd4af37, 0x1c1810, 0x282214, 0x362e1b, 0x4e4328, 0xf3ecd6, 0x282214, 0x2e2718 },
    { "Ice",       "Лёд",       0, 0x5aa0d0, 0xeaf2f8, 0xffffff, 0xd2e3ef, 0xb0cce0, 0x203440, 0xffffff, 0xffffff },
};
#define NAMED_N ((int)(sizeof(NAMED) / sizeof(NAMED[0])))

int el_theme_named(const char *name) {
    int i;
    if (!name) return 0;
    for (i = 0; i < NAMED_N; i++) {
        const Named *n = &NAMED[i];
        if (strcmp(n->name, name) == 0 || strcmp(n->alias, name) == 0) {
            g.dark = n->dark; g.accent = n->accent;
            if (n->dark) set_dark(g.th, n->accent); else set_light(g.th, n->accent);
            g.th[0] = n->win; g.th[1] = n->panel; g.th[2] = n->button; g.th[5] = n->border;
            g.th[6] = n->text; g.th[11] = n->input; g.th[13] = n->menu;
            g.th[3] = mix(n->button, n->dark ? 0xffffff : 0x000000, 0.12);
            g.th[4] = mix(n->button, n->dark ? 0xffffff : 0x000000, 0.22);
            g.th[9] = n->accent; g.th[12] = mix(n->accent, n->dark ? 0x000000 : 0xffffff, n->dark ? 0.35 : 0.55);
            g.theme_idx = i;
            request_redraw();
            return 1;
        }
    }
    return 0;
}
const char *el_theme_name(int index) { if (index < 0 || index >= NAMED_N) return ""; return NAMED[index].name; }
