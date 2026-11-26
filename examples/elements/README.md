# Elements — a window library for Windows (C)

A native UI library in the spirit of WPF, a port of the "design library" prototype
from DynastyOS (`<elements.h>`) to real Windows. Rendering is done with **GDI+**
(no third-party dependencies). A program builds a tree of elements once and runs
`el_run()`; panels lay out their own children (measure/arrange, as in WPF).

The library is written in **plain C (C17)** and is used the same way from C and C++
(`elements.h` has `extern "C"`). The API is the same as in DyOS: any code written
against `elements.h` compiles there and here.

## Layout of the project

```
Elements/
  include/elements.h     — the single header a program includes (C-compatible API)
  src/                   — implementation, split by layer:
    el_internal.h          shared internal header: element struct, context, helpers; internal names get the eli_ prefix
    el_gdip.h              GDI+ flat API declarations for C (the SDK's gdiplus.h is C++ only)
    el_util.c              growable arrays, UTF-8 <-> UTF-16 strings
    el_gfx.c               GDI+ drawing helpers (brush/pen/path in one call)
    el_core.c              context, element creation, properties, fonts/text, el_run/el_frame loop
    el_theme.c             themes: dark/light, 16 named, 14 color slots
    el_layout.c            layout: stack/dock/wrap/grid/scroll/canvas/border/group/tabs, align, "Auto,*,2*,120" grid
    el_controls.c          input: hit-testing, hover, clicks, text editor, combo, tooltips
    el_paint.c             drawing of every element on GDI+; the el_g_* custom-drawing primitives
    el_menu.c              menu bar, drop-downs and submenus, context menu
    el_style.c             inline styling (CSS-like) el_style and the el_stylesheet table
    el_anim.c              animations (translate/scale/rotate/opacity), easings, timers
    el_dialog.c            message, ask, toast, file/folder pickers, modal dialog
    el_win32.c             window, message loop, input, frame buffer, NON-STANDARD windows
    el_effects.c           window effects (post-processing the finished frame)
  examples/              — examples in C (see below); elm_fonts.h loads the 30 fonts
  fonts/                 — 30 original fonts (.ttf) drawn by tools/fontgen
  tools/fontgen/         — font generator: stroke axes -> pen -> TrueType (Python + fontTools)
  build.cmd              — build (Release, static CRT)
  build_all.ps1          — build every variant (static + DLL for x64 and x86)
  README.md, DOCUMENTATION.md
```

## Building

You need Visual Studio with the C/C++ tools (MSVC, C17). From PowerShell in the project folder:

```powershell
& ".\build.cmd"        # x64: static lib + all examples in bin\
& ".\build_all.ps1"    # every variant: x64 and x86, static and dynamic (DLL) in dist\
```

`build.cmd` builds `lib\Elements.lib` and the examples in `bin\`. `build_all.ps1` builds:

```
dist\x64\static\Elements.lib        dist\x86\static\Elements.lib        (static)
dist\x64\dll\Elements.dll + .lib    dist\x86\dll\Elements.dll + .lib    (dynamic: DLL + import lib)
```

To use it in your program: add `include\` to the header search path, link the right
`Elements.lib` + `gdiplus.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib`,
build as a windowed application (`/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`). Compile
sources with `/utf-8` — strings in the program are UTF-8. For the DLL, put `Elements.dll`
next to your program. The program itself can be C or C++; the static library exports
only `el_*` names (internal ones are `eli_*`), so it does not clash with your own names.

## A quick tour of the API

- **Window and loop:** `el_window`, `el_run` (or your own `while (el_frame())`), `el_quit`.
- **Panels:** `el_stack`, `el_dock`, `el_wrap`, `el_grid("Auto,*,2*,120", "...")`, `el_scroll`, `el_canvas`, `el_border`, `el_group`, `el_tabs`/`el_tab`.
- **Controls:** `el_text`, `el_button`, `el_check`, `el_radio`, `el_edit`, `el_memo`, `el_password`, `el_combo`, `el_listbox`, `el_slider`, `el_progress`, `el_number`, `el_image`, `el_separator`, `el_toolbar`/`el_tool`.
- **Menu:** `el_menubar`, `el_menu` ("Item\tCtrl+N"), `el_menu_check`, `el_context_menu`.
- **Properties:** `el_size/min/max`, `el_margin`, `el_padding`, `el_align`, `el_cell`, `el_dock_side`, `el_gap`, `el_set_text`, `el_text_bind`, `el_tooltip`, `el_font`, `el_colors`, `el_frame_style`.
- **Theme:** `el_theme(dark)`, `el_accent`, `el_theme_color(slot, rgb)`, `el_corner_radius`, `el_theme_named("Night"…)`.
- **Inline styling (CSS-like):** `el_style(e, "gradient:#1a2a6c #b21f1f diagonal; radius:12; background-alpha:160; shadow:#000 18 0 6 120; hover-background:#fff; font-weight:bold; text-align:center")`; the table form is `el_stylesheet`.
- **Events:** `el_on_click`, `el_on_change`, `el_clicked`, `el_changed`.
- **Animations:** `el_animate(e,"opacity",0,255,300,EL_EASE_OUT,0)`, `el_appear(e,"pop",400)`, `el_transform`, timers `el_every`.
- **Ready-made windows:** `el_message`, `el_ask`, `el_toast`, `el_open_file`, `el_save_file`, `el_pick_folder`, `el_dialog`.
- **Custom drawing:** `el_paint` (a surface) + primitives `el_g_fill/round/frame/line/disc/ring/shade/text/poly/poly_frame/spline/blob/arc`.
- **Window effects:** `el_effect("chromatic"|"scanlines"|"noise"|"vignette"|"glitch"|"bloom"|"pixelate"|"grayscale"|"sepia"|"invert", strength)`, `el_effects_clear()` — post-processing of the whole frame, several at once.
- **Embeddable fonts:** `el_font_add(name, "file.ttf")` / `el_font_add_memory(name, bytes, size)`, then `el_font_face(e, name)` or the style `font-family:name`.
- **Window resizing:** ordinary windows resize by their edges with live re-layout; `el_window_resizable(on)` applies to frameless and shaped windows too.

## Non-standard windows and shapes

A window can be any shape, size and proportion, and draw itself.

| Function | What it does |
|---|---|
| `el_frameless(on)` | remove the system frame: your own title bar/buttons/background |
| `el_window_round(r)` | round the window corners |
| `el_window_shape("round:24"\|"ellipse"\|"pixels")` | window shape; **`"pixels"`** takes the shape from the opacity of what you draw (smooth custom outlines) |
| `el_window_region(xy, n)` | an arbitrary polygonal outline (x,y pairs) |
| `el_window_aspect(w, h)` | keep the aspect ratio while resizing (0,0 — free) |
| `el_window_minmax(minw,minh,maxw,maxh)` | size limits (-1 — no limit) |
| `el_window_resizable(on)` | whether the window can be resized (frameless: drag by the opaque edges) |
| `el_window_opacity(a)` | opacity of the whole window 0..255 |
| `el_drag_region(e)` | drag the window by this element |
| `el_window_canvas(fn, arg)` | draw the whole window background with `el_g_*` |

Arbitrary shapes, curves included, are drawn with `el_g_poly/poly_frame` (polygon),
`el_g_spline` (smooth stroked curve), `el_g_blob` (filled smooth closed curve — any
"curved" shape) and `el_g_arc` (arc).

## Window effects

Post-processing of the finished frame; several can be on at once, `amount` is the strength, 0 removes it.

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

An effect applies to the whole window, including text, so keep dispersion low (1–3) on
windows with small text, or use effects only on decorative/shaped windows.

## Embeddable fonts

Any `.ttf`/`.otf` can be loaded and named, then applied to elements or in a style.

```c
el_font_add("MyFont", "fonts/arial.ttf");          // from a file
el_font_add_memory("MyFont", bytes, size);         // from memory (embed into the exe as a byte array)
el_font_face(elem, "MyFont");                        // element font by name
el_style(elem, "font-family:MyFont");                // or via a style
```

The names `"default"` (Segoe UI) and `"mono"` (Consolas) are built in; a style also accepts
multi-word names: `font-family:Elm Serif Bold`.

The library comes with **30 original fonts** in `fonts\`, drawn by its own generator
`tools/fontgen` (`python tools/fontgen/build.py`): grotesques (light → black, italic, condensed,
extended, compressed), geometric, humanist, rounded, techno, mono, typewriter, slab, serif
(with italic), didone, oldstyle, hairline, wide. Latin + Cyrillic, upper and lower case, digits,
punctuation. See `fonts/README.md`. Showcases: `examples/fontshow.c` (a non-standard window)
and `examples/fonts.c` (a plain list).

## Examples (`bin\`)

| Example | What it shows |
|---|---|
| `hello` | the minimum: a field, a button, a label |
| `form` | menu, tabs, a form grid, a list and a multiline field, tooltips |
| `design` | inline styling: gradients, glass, shadows, smooth hover |
| `custom_window` | a frameless window: own corners, title bar, background, 16:10 proportions |
| `shaped` | a "pill" window: shape from the opacity of the drawing |
| `triangle` | **a triangular window with triangular buttons** |
| `curves` | a "blob" window with smooth curved edges and round buttons, a sine curve and an arc |
| `cyberpunk` | an angular panel in the spirit of Cyberpunk 2077: dispersion, scanline and glow effects; yellow/cyan/magenta neon |
| `effects` | a window-effects showcase: 10 effects on toggles |
| `fontshow` | **the 30 original fonts in a non-standard window**: own shape, gradient, own title bar, glass cards |
| `fonts` | the same 30 fonts in a plain window |

## What is implemented and what isn't

Done: everything above — layout, controls, themes, inline and table styling, menus,
animations, dialogs, custom drawing, non-standard windows and shapes, window effects,
embeddable fonts, resizing with live re-layout, HiDPI, idle sleep (the window sleeps when
nothing happens). Grid cells in columns of known width (pixel or star) are measured with
that width, so wrapped text and wrap panels inside a grid break into lines. Full
reference — `DOCUMENTATION.md`.

Simplified / not yet implemented (declarations exist in `elements.h` unless noted):
loading a window from XML markup, `TreeView`/`DataGrid`, a code editor with highlighting,
a Find/Replace bar, color/date pickers, `background-image` in a style (parsed, not drawn).
They can be added the same way — as another layer in `src/`.
