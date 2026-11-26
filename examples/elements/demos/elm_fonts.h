// Список 30 собственных шрифтов Elements (tools/fontgen -> fonts/*.ttf) и их загрузка.
// Папка со шрифтами: переменная ELFONTS или <папка_exe>\..\fonts (пути могут быть с кириллицей).
#pragma once
#include <elements.h>
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

typedef struct ElmFont { const char *file; const char *name; } ElmFont;

static const ElmFont ELM_FONTS[] = {
    { "ElmGrotesk.ttf", "Elm Grotesk" },             { "ElmGroteskLight.ttf", "Elm Grotesk Light" },
    { "ElmGroteskBold.ttf", "Elm Grotesk Bold" },    { "ElmGroteskBlack.ttf", "Elm Grotesk Black" },
    { "ElmGroteskItalic.ttf", "Elm Grotesk Italic" },{ "ElmGroteskCondensed.ttf", "Elm Grotesk Condensed" },
    { "ElmGroteskExtended.ttf", "Elm Grotesk Extended" }, { "ElmCompressed.ttf", "Elm Compressed" },
    { "ElmGeometric.ttf", "Elm Geometric" },         { "ElmGeometricLight.ttf", "Elm Geometric Light" },
    { "ElmGeometricBold.ttf", "Elm Geometric Bold" },{ "ElmHumanist.ttf", "Elm Humanist" },
    { "ElmHumanistBold.ttf", "Elm Humanist Bold" },  { "ElmHumanistItalic.ttf", "Elm Humanist Italic" },
    { "ElmRounded.ttf", "Elm Rounded" },             { "ElmRoundedBold.ttf", "Elm Rounded Bold" },
    { "ElmTechno.ttf", "Elm Techno" },               { "ElmTechnoBold.ttf", "Elm Techno Bold" },
    { "ElmMono.ttf", "Elm Mono" },                   { "ElmMonoBold.ttf", "Elm Mono Bold" },
    { "ElmTypewriter.ttf", "Elm Typewriter" },       { "ElmSlab.ttf", "Elm Slab" },
    { "ElmSlabBold.ttf", "Elm Slab Bold" },          { "ElmSerif.ttf", "Elm Serif" },
    { "ElmSerifBold.ttf", "Elm Serif Bold" },        { "ElmSerifItalic.ttf", "Elm Serif Italic" },
    { "ElmDidone.ttf", "Elm Didone" },               { "ElmOldstyle.ttf", "Elm Oldstyle" },
    { "ElmHairline.ttf", "Elm Hairline" },           { "ElmWideLight.ttf", "Elm Wide Light" },
};
#define ELM_FONT_COUNT ((int)(sizeof(ELM_FONTS) / sizeof(ELM_FONTS[0])))

// папка шрифтов (UTF-16)
static void elm_fonts_dir(wchar_t *out, int cap)
{
    const wchar_t *e = _wgetenv(L"ELFONTS");
    wchar_t *s;
    if (e && *e) { lstrcpynW(out, e, cap); return; }
    GetModuleFileNameW(NULL, out, (DWORD)cap);
    s = wcsrchr(out, L'\\');
    if (s) *s = 0;
    if (lstrlenW(out) + 10 < cap) lstrcatW(out, L"\\..\\fonts");
}

// загрузить все шрифты; ok[i] — загрузился ли i-й; вернёт сколько загрузилось
static int elm_load_fonts(int *ok)
{
    wchar_t dir[700], pw[800];
    char u[2400];
    int n = 0, i;
    elm_fonts_dir(dir, 700);
    for (i = 0; i < ELM_FONT_COUNT; i++) {
        _snwprintf(pw, 799, L"%s\\%S", dir, ELM_FONTS[i].file);
        pw[799] = 0;
        if (!WideCharToMultiByte(CP_UTF8, 0, pw, -1, u, (int)sizeof u, NULL, NULL)) u[0] = 0;
        ok[i] = el_font_add(ELM_FONTS[i].name, u);
        n += ok[i] ? 1 : 0;
    }
    return n;
}
