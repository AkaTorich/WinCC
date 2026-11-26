# Elements — documentation

A window library for Windows in plain C (C17), in the spirit of WPF. Rendering is GDI+,
with no third-party dependencies. A single header `elements.h`, usable from C and C++; the
implementation lives in `src/`.

Contents:
1. [How it works](#1-how-it-works)
2. [Building and linking](#2-building-and-linking)
3. [Window and loop](#3-window-and-loop)
4. [Tree and layout](#4-tree-and-layout)
5. [Controls](#5-controls)
6. [Element properties](#6-element-properties)
7. [Themes](#7-themes)
8. [Inline and table styling (CSS)](#8-inline-and-table-styling-css)
9. [Events](#9-events)
10. [Menu](#10-menu)
11. [Animations and timers](#11-animations-and-timers)
12. [Ready-made windows and dialogs](#12-ready-made-windows-and-dialogs)
13. [Non-standard windows and shapes](#13-non-standard-windows-and-shapes)
14. [Custom drawing](#14-custom-drawing)
15. [Window effects](#15-window-effects)
16. [Embeddable fonts](#16-embeddable-fonts)
17. [Hotkeys and data](#17-hotkeys-and-data)

---

## 1. How it works

A program builds a **tree of elements** once and runs `el_run()`. Each frame the library
gathers input, dispatches events, lays out (measure → arrange, as in WPF) and draws. When
nothing is happening the window sleeps — no CPU load.

- An **element** is an integer (`el`), 0 means "none". `parent = 0` means the window itself.
- **Sizes** are given in interface points; the library applies the HiDPI scale itself.
- **Strings** are UTF-8. Compile with `/utf-8`.

A minimal program:

```c
#include <elements.h>
char name[64];
void hi(el e, void *a) { el_set_text(el_find("out"), name); }
int main(void) {
    el w = el_window("Example", 420, 220);
    el s = el_stack(w, 0); el_padding(s, 16, 16, 16, 16);
    el_edit(s, name, sizeof name);
    el_on_click(el_button(s, "Hello"), hi, 0);
    el_name(el_text(s, ""), "out");
    el_run();
}
```

## 2. Building and linking

You need Visual Studio with the C/C++ tools (MSVC, x64/x86). The library is C17 and builds
without warnings at `/W3`. From PowerShell:

```powershell
& ".\build.cmd"        # x64: static lib + all examples in bin\
& ".\build_all.ps1"    # x64 and x86: static libs and DLLs in dist\
```

To use it in your program: add `include\` to the search path, link the library plus
`gdiplus.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib`, build as a windowed
application: `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`, sources with `/utf-8`.

Library variants (`dist\<arch>\`): `static\Elements.lib` — static; `dll\Elements.dll` +
`dll\Elements.lib` — dynamic (DLL and import lib). For the DLL, copy `Elements.dll` next to
your program.

Your program can be C or C++. The static library exports only the `el_*` API; its internal
functions and data carry the `eli_` prefix (see `src/el_internal.h`), so they never clash
with names in your program. GDI+ is called through its flat API, declared for C in
`src/el_gdip.h`.

## 3. Window and loop

| Function | Purpose |
|---|---|
| `el el_window(const char *title, int w, int h)` | create a window; returns its `el` |
| `void el_title(const char *title)` | change the title |
| `int el_frame(void)` | one frame; 0 — window closed (for your own loop `while (el_frame()){}`) |
| `void el_run(void)` | run frames until closed |
| `void el_quit(void)` | close the window (the next `el_frame` returns 0) |
| `void el_on_close(el_fn fn, void *arg)` | intercept closing: call `fn` instead of exiting (then close with `el_quit`) |

## 4. Tree and layout

Panels lay out their own children. `parent = 0` is the window.

| Panel | Behavior |
|---|---|
| `el_stack(parent, horizontal)` | children in sequence (0 — down, 1 — right), spacing `el_gap` |
| `el_dock(parent)` | children get `el_dock_side`; the last one takes the rest |
| `el_wrap(parent)` | a row with wrapping |
| `el_grid(parent, cols, rows)` | a grid; spec `"Auto,*,2*,120"` — by content, fractions of the rest, points |
| `el_scroll(parent)` | one child, scroll by wheel and thumb |
| `el_canvas(parent)` | children at `el_pos(x,y)` |
| `el_border(parent)` | a frame/background around one child |
| `el_group(parent, header)` | a frame with a header |
| `el_tabs(parent)` / `el_tab(tabs, header)` | tabs |

The window itself is a DockPanel. A grid cell — `el_cell(e, row, col, rowspan, colspan)`.
Cells in columns of known width (pixel columns, and star columns once the grid's width is
known) are measured with that width, so wrapped text and wrap panels inside a grid break into
lines and the auto rows grow to fit. `el_border` holds one child — put a stack inside it for
several.

## 5. Controls

`el_text` (TextBlock; `el_wrap_text` — wrapping), `el_button`, `el_check(text,&v)`,
`el_radio(text,&group,value)`, `el_edit(buf,cap)`, `el_memo(buf,cap)` (multiline),
`el_password(buf,cap)`, `el_combo(items,count,&sel)`, `el_listbox(items,count,&sel)`,
`el_slider(min,max,&v)`, `el_progress(max,&v)`, `el_number(min,max,step,&v)`,
`el_image(path)` (png/jpg/…), `el_separator`, `el_toolbar` + `el_tool(icon,text,tip)`.

Text fields: mouse and Shift+arrow selection, Ctrl+A/C/X/V, Home/End; in a memo — arrows
and wrapping. `el_items(e, items, count)` — replace the list.

## 6. Element properties

`el_size(w,h)` (-1 — by content), `el_min_size`, `el_max_size`, `el_margin(l,t,r,b)`,
`el_padding(l,t,r,b)`, `el_align(horizontal, vertical)` (`EL_STRETCH`/`START`/`CENTER`/`END`),
`el_cell`, `el_dock_side` (`EL_LEFT/TOP/RIGHT/BOTTOM`), `el_pos`, `el_gap`,
`el_set_text`/`el_get_text`, `el_text_bind(live)` (show the program's string each frame),
`el_enable`, `el_show`, `el_tooltip`, `el_font(size,bold)`, `el_colors(fg,bg)` (-1 — theme),
`el_frame_style(color,width,radius)`, `el_name`/`el_find`, `el_select_tab`/`el_selected_tab`,
`el_scroll_to`, `el_focus`/`el_focused`, `el_clear`, `el_remove`.

## 7. Themes

`el_theme(dark)` (1 — dark, 0 — light), `el_accent(rgb)`, `el_default_font(size)`,
`el_corner_radius(r)`, `el_theme_color(slot, rgb)`. Slots: 0 window, 1 panel, 2 button,
3 hover, 4 press, 5 border, 6 text, 7 bright text, 8 dim, 9 accent, 10 text-on-accent,
11 field, 12 selection, 13 menu.

Built-in named themes: `el_theme_named("Ночь")` — the names are Russian: Ночь (Night),
День (Day), Аметист, Изумруд, Сапфир, Рубин, Янтарь, Обсидиан, Пергамент, Океан, Лес,
Сакура, Графит, Неон, Золото, Лёд; `el_theme_name(i)` returns the name by index.

## 8. Inline and table styling (CSS)

`el_style(e, "...")` — properties as a string, like CSS:

```
background:#223  color:#fff  border:#456  border-width:1  radius:6
padding:4 8   margin:0 0 8 0   width:120  height:32
gradient:#1a2a6c #b21f1f vertical|horizontal|diagonal
background-alpha:160            (glass)
shadow:#000 18 0 6 120          (color blur x y strength; shadow:none)
hover-background  press-background  hover-color  hover-border
font-size:16  font-weight:bold  font-family:mono|<font name>  text-align:center|right
```

Style table: `el_stylesheet("button{radius:10} .card{background:#fff;background-alpha:28} #title{font-size:28}")`,
`el_stylesheet_file(path)`, element class — `el_class(e, "card big")`. Selectors: tag,
`.class`, `#name`, `tag.class`. An element's own settings win over table rules.

## 9. Events

`el_on_click(e, fn, arg)` and `el_on_change(e, fn, arg)` — handlers; or poll after the
frame: `el_clicked(e)`, `el_changed(e)`. `on_click` works for a button, a menu item, a
checkbox, Enter in a field, and also for text/image/panel (a click on them).

## 10. Menu

`el_menubar(parent)` — the bar (at the top of the window). `el_menu(parent, "Item\tCtrl+N")`
— a bar item or submenu (keys on the right after `\t`). `el_menu_check(item, &v)` — an item
with a check mark. `el_context_menu(target)` — a right-click menu. `el_popup_menu()` +
`el_menu_popup(menu, at, x, y)` — open a menu at a point.

## 11. Animations and timers

`el_animate(e, prop, from, to, ms, easing, loop)` — `prop`: `"opacity"` 0..255, `"x"`/`"y"`
(shift), `"scale"` (%), `"rotate"` (°). Easings: `EL_LINEAR`, `EL_EASE_IN`, `EL_EASE_OUT`,
`EL_EASE_IN_OUT`, `EL_OVERSHOOT`, `EL_BOUNCE`. `loop`: 0 once, 1 looping, 2 back-and-forth.
`el_appear(e, "fade|slide|left|right|zoom|pop|spin", ms)` — an entrance. `el_transform(e,x,y,scale,rotate,opacity)`
— immediately. `el_animating`, `el_stop`. Timers: `el_every(ms, fn, arg)` → id, `el_timer_stop(id)`.

## 12. Ready-made windows and dialogs

`el_message(title, text)`, `el_ask(title, text, fn(yes,arg), arg)`, `el_toast(text, ms)`,
`el_open_file(title, folder, filter, fn(path,arg), arg)` (filter `".txt;.c"` or 0),
`el_save_file(...)`, `el_pick_folder(...)`, `el_dialog(title, width)` + `el_dialog_close(d)`
— your own modal window (put elements into it).

## 13. Non-standard windows and shapes

A window can be any shape, size and proportion, and draw itself.

| Function | Purpose |
|---|---|
| `el_frameless(on)` | remove the system frame: your own title bar/buttons/background |
| `el_window_round(r)` | round the window corners |
| `el_window_shape("round:24"\|"ellipse"\|"pixels")` | window shape; **`"pixels"`** — shape from the opacity of what is drawn (smooth custom outlines) |
| `el_window_region(xy, n)` | an arbitrary polygonal outline (x,y pairs) |
| `el_window_aspect(w, h)` | keep the aspect ratio while resizing (0,0 — free) |
| `el_window_minmax(minw,minh,maxw,maxh)` | size limits (-1 — no limit) |
| `el_window_resizable(on)` | whether the window can be resized (frameless: drag by the opaque edges) |
| `el_window_opacity(a)` | opacity of the whole window 0..255 |
| `el_drag_region(e)` | drag the window by this element |
| `el_window_canvas(fn, arg)` | draw the whole window background with `el_g_*` |

**Can windows be resized?** Yes. Ordinary windows drag by their edges and the layout
rebuilds live. Frameless and shaped windows too (`el_window_resizable`, on by default); for
shaped windows the shape is recomputed from the new size.

Triangular window with triangular buttons — `examples/triangle.c`; a shaped (curved) one
— `examples/curves.c`; an angular panel in the Cyberpunk spirit — `examples/cyberpunk.c`.

## 14. Custom drawing

`el_paint(parent, fn(e,w,h,arg), arg)` — a surface; inside `fn` draw with primitives in its
points ((0,0) is the corner). `el_redraw(e)` — redraw. Input: `el_mouse(e,&x,&y,&btn)`
(1 — over e), `el_wheel(e,&mods)`.

Primitives: `el_g_fill`, `el_g_round`, `el_g_frame`, `el_g_line`, `el_g_disc`, `el_g_ring`,
`el_g_shade` (semi-transparent), `el_g_text` + `el_g_text_w`, and for any shapes:
`el_g_poly(xy,n,rgb)` / `el_g_poly_frame` (polygon), `el_g_spline(xy,n,rgb,width)` (smooth
stroked curve), `el_g_blob(xy,n,rgb)` (filled smooth closed curve — any "curved" shape),
`el_g_arc(cx,cy,r,start,sweep,rgb,width)` (arc).

## 15. Window effects

Post-processing of the finished frame. Several can be on at once; `amount` is the strength, 0 removes it.

```c
el_effect("chromatic", 3);   // dispersion (channel split)
el_effect("scanlines", 30);  // CRT scanlines
el_effect("bloom", 55);      // glow on bright areas
el_effect("vignette", 24);   // darkened edges
el_effect("glitch", 16);     // glitch (animated)
el_effect("noise", 18);      // noise
el_effect("pixelate", 8);    // pixelation
el_effect("grayscale", 100); // grayscale
el_effect("sepia", 100);     // sepia
el_effect("invert", 100);    // invert
el_effects_clear();          // remove all
```

An effect applies to the **whole** window, including text. For windows with small text keep
dispersion low (1–3) or use effects only on decorative/shaped windows. Showcase —
`examples/effects.c`.

## 16. Embeddable fonts

Any `.ttf`/`.otf` can be loaded and named, then applied to elements or in a style.

```c
el_font_add("MyFont", "fonts/arial.ttf");          // from a file
el_font_add_memory("MyFont", bytes, size);         // from memory (embed into the exe as a byte array)
el_font_face(elem, "MyFont");                        // element font by name
el_style(elem, "font-family:MyFont");                // or via a style
```

The names `"default"` (Segoe UI) and `"mono"` (Consolas) are built in; in a style the name may
have several words: `font-family:Elm Serif Bold`. Each embedded font gets its own private
collection, so a name always maps to exactly the file it was loaded from.

**30 original fonts** ship in `fonts\` — drawn by the library's generator `tools/fontgen`
(stroke axes in real metrics → a broad-nib pen with contrast, flat/round ends, serifs → TrueType;
`python tools/fontgen/build.py`). Families: Elm Grotesk (Light, Bold, Black, Italic, Condensed,
Extended), Compressed, Geometric (Light, Bold), Humanist (Bold, Italic), Rounded (Bold), Techno
(Bold), Mono (Bold), Typewriter, Slab (Bold), Serif (Bold, Italic), Didone, Oldstyle, Hairline,
Wide Light. Showcases: `examples/fontshow.c` (non-standard window), `examples/fonts.c`.
The examples share the font list in `examples/elm_fonts.h`.

## 17. Hotkeys and data

`el_hotkey("Ctrl+S", fn, arg)` — window keys ("F5", "Esc", "Alt+Z"…), `el_mods()` — which of
Shift/Ctrl/Alt are held (`EL_SHIFT|EL_CTRL|EL_ALT`). `el_data(e, p)` / `el_get_data(e)` —
your own pointer attached to an element.
