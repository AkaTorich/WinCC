# WinCC — a C compiler for Windows

A self-contained C compiler that turns `.c` files into ordinary Windows programs: console `.exe`, GUI `.exe` and `.dll`.
It grew out of the DynastyOS compiler (own parser, own x86-64 assembler, own PE writer) and was taught Windows.
A finished program depends on nothing but Windows' own DLLs: the C library (printf, malloc, files, math…) is linked
into it, and only what the program actually calls ends up inside (hello.exe is about 35 KB).

- `wcc.exe` — the command-line compiler.
- `WinCC.exe` — the editor: projects, tabs, C syntax highlighting, Build and Run buttons, error list.
- `wui.h` — good-looking windows for your own programs: 61 ThemeLibrary themes (glass, instruments, parchment, stone,
  screens…), a custom window with shadow and rounded corners, buttons, fields, lists, tabs, side menu. See "Window styling" below.
- `net.h` — networking: download a page or a file (HTTP and HTTPS), server requests, TCP, UDP, your own web server.
  Underneath are the usual `winsock2.h`, `ws2tcpip.h`, `winhttp.h`, as in Visual Studio. See "Networking" below.

Download the installer from [Releases](https://github.com/AkaTorich/WinCC/releases): it installs into
`%LOCALAPPDATA%\Programs\WinCC`, adds `wcc` to `PATH`, creates shortcuts and "Open with" for `.c`/`.h`, and ships
the manual (in Russian) as a PDF. The installer speaks English and Russian (the list next to "Uninstall"; `/lang=en|ru`
on the command line) and passes the chosen language on to the editor — `setup\Loc.cs`. A Russian version of this file is [README.ru.md](README.ru.md).

## Command line

```
wcc hello.c                      → hello.exe (console)
wcc app.c                        → app.exe (GUI, if it has WinMain and no main)
wcc a.c b.c util.asm -o prog.exe several sources into one program
wcc app.c app.rc                 with resources: icon, version, strings, menus, dialogs… (a ready .res from rc.exe works too)
wcc --gui app.c                  GUI program, no console
wcc --dll lib.c                  → lib.dll (exports: __declspec(dllexport), otherwise all functions)
wcc --lib a.c b.c -o mylib.lib   your own library (see "Your own libraries: DLL and .lib")
wcc main.c mylib.lib -I inc      a program that takes it: only the functions it calls go in
-l user32 / -l mylib             take functions from this DLL (your own DLL is looked up next to the source)
-l ..\mydll\mydll                your own DLL by its path (the program imports it by name: put the DLL next to the .exe)
-I dir  -D NAME=1  -S (assembly to .s)  --stack MB (default 8)  --run (build and run)
-g                               debug info for the WinCC debugger next to the program (prog.wdbg)
-O0                              no register variables: every variable in memory (for debugging)
--manifest                       WinCC's manifest: A-functions take UTF-8, modern controls (also #pragma comment(linker, "/MANIFEST"))
--tiny                           sections and the file aligned by 16 bytes, not 4096/512, when that makes the file smaller
                                 (the file maps as one piece; zero-initialized globals are stored in it); IDE: always on
```

Errors are printed as `file:line: text` (messages are in Russian); warnings (`#warning`, an unknown `\x` escape in a
string) as `file:line: предупреждение: text` and do not stop the build. An assembler error inside an `asm()` insert names
the C source line. A source file may be UTF-8, UTF-16 (with BOM) or code page 1251 — wcc converts it to UTF-8 itself
(`wcc.c`, `read_text`). `#include <x.h>` searches `-I` and `include` first, the current folder last. Version:
`wcc --version` (1.5.1; the same for the editor and the installer).

## What is where

| Folder / file | Contents |
|---|---|
| `compiler\` | the compiler (C, built with MSVC from VS 2026) |
| `compiler\tokenize.c preprocess.c parse.c type.c codegen.c` | parser and code generator from DynastyOS; for Windows: `long` is 4 bytes, `wchar_t` and `L""` are UTF-16, `#pragma pack`, `#pragma comment(lib)`, `asm("…")` outside functions, MSVC keywords (`__stdcall`, `__int64`…); bit-fields laid out as by MSVC (`parse.c`: `struct_decl`, `union_decl`; reads by shifts, writes with a mask — `codegen.c`); `_Static_assert`, `__func__`, `__DATE__`/`__TIME__`/`__COUNTER__`, `#line`, `#warning`; `_Alignas` and `__declspec(align)` on globals, statics, locals (over 16 — stack memory with the required alignment) and fields; `__declspec(naked)`, `selectany`. C11: `typeof`, `_Generic`, variable-length arrays (`parse.c`: `compute_vla_size`, `codegen.c`: `ND_ALLOCA`; the stack does not grow inside a loop), `_alloca`, `_Atomic` (a `lock cmpxchg` loop), `__builtin_atomic_cas/exchange/fetch_add`, K&R (`knr_params`), designated initializers into anonymous members; thread-local variables — `.tls` section (`gs:[0x58]`, `__wcc_tls_index`); `__try`/`__except`/`__finally`/`__leave` (`try_stmt`; the filter and `__finally` are separate funclets, `emit_funclet`); `@name` in `asm()` — a local variable's location |
| `compiler\asm.c` | the assembler; at the end, the Windows `.exe`/`.dll` writer (`cc_assemble_win`): one `.text` section with the code, string literals, import/export tables, the TLS directory, `.pdata` (only with `__try` or `signal`) and resources after it, other sections only when not empty (an empty program is 1 KB: headers and `.text`), imports per DLL, entry point, the manifest on request, ASLR/NX, `.bss` (zeros without file space), `.tls` with a TLS directory, `.pdata` with unwind info (`.seh_proc`, `.seh_scope`, `.seh_endproc` directives; the `__try` handler is `__C_specific_handler` from ntdll); instructions: `cmpxchg xadd bsf bsr`, x87, `popcnt lzcnt tzcnt bswap bt* loop jrcxz movabs syscall rdrand crc32 prefetch*`, SSE2–SSE4.2 (`sse_ex`), AVX with `ymm` (`avx`, VEX encoding — `vex`), `gs:`/`fs:`; numeric labels `1:`/`1b`/`1f`; `.srcfile` — an error in a `.asm` file names its line; `.alias`, `.gc` directives |
| `compiler\strip.c` | drops library functions and data the program never reaches |
| `compiler\winlink.c` | which DLL provides a name: export tables are read straight from the DLL files (System32 and the program folder), no `.lib` needed |
| `compiler\cc.c` | `cc_build_win`: the build, finding main/wmain/WinMain/wWinMain/DllMain and the program entry |
| `compiler\wcc.c` | the `wcc.exe` command line |
| `compiler\peephole.c` | final pass over the assembly text: `push rax … pop rcx` (and `rdx`) pairs become `mov rcx, rax` when nothing in between touches the stack, calls, jumps or uses that register (including implicitly: `rep`, `div`…); a `jmp` to the next line is removed |
| `compiler\rc.c` | resources — see "Resources" below: `.rc` scripts (read the way rc.exe reads them) and ready `.res` files; the `.rsrc` tree is written by `asm.c` (`win_resources`) |
| `compiler\headers.c` | standard headers (stdio.h …), built into the compiler |
| `compiler\preprocess.c` | `#pragma comment(lib, "name")`: if there is a `lib\name` folder next to wcc, its `.c` files are built with the program as a library (unused parts dropped), otherwise it is a DLL; `#pragma comment(linker, "/SUBSYSTEM:WINDOWS")` — a GUI program even with `main`; macros are looked up in a hash table |
| `runtime\runtime.c` | the C library (shared with DynastyOS; Windows parts under `#ifdef __WINCC__`): buffered FILE over a handle (`fh_*`), text mode `\n` ↔ `\r\n`, stdio lock (`LOCKED`, wrappers at the end), per-category locales (`rt_setlocale`, decimal point `g_dp`), per-thread `errno` and `strtok`, unlimited `atexit`, `aligned_alloc` with any alignment; Microsoft functions `strcpy_s`, `strcat_s`, `strncpy_s`, `wcscpy_s`, `wcscat_s`, `memcpy_s`…, `_wcsicmp`, `_wcsnicmp` (`rt_ms_*`: on overflow — empty string and an error code) |
| `runtime\winhost.c` | library ↔ Windows: console (UTF-16, Cyrillic shows correctly), files, time (`clock` from the performance counter), memory, getenv (its own copy per name), system (in UTF-8) |
| `runtime\winfile.c` | buffered file I/O for FILE (`cc_fh_*`), `io.h` descriptors, `_findfirst`, `direct.h` folders, `stat` |
| `runtime\winthread.c` | stdio lock, `threads.h` (C11), `process.h` (`_beginthreadex`, `_spawnv`…) |
| `runtime\winlocale.c` | Windows locales: names ("Russian" → `ru_RU.UTF-8`), month and day names, decimal point, `strcoll`, time zone for `%Z` |
| `runtime\conio.c`, `runtime\cmath.c` | `conio.h` (`_getch`, `_kbhit`…); `complex.h` (Microsoft style) and `fenv.h` |
| `runtime\start.c` | program start — a program gets only what it uses: an entry without parameters (`main(void)`, `WinMain(void)`) is called directly and ends with `ExitProcess` (an empty program imports `ExitProcess` alone); with parameters, the start of its kind (`__wcc_start_main/_wmain/_winmain/_wwinmain`) makes argc/argv in UTF-8 or WinMain's instance and line; the end (atexit, output and files written out) is set by the library on first use (`__wcc_at_end`); CPU faults → C signals only after the program's own `signal(SIGSEGV/SIGFPE/SIGILL)` (last-chance handler: `__try` first); DllMain |
| `compiler\stub.bin` | the DOS stub of every `.exe` and `.dll` (64 bytes, `mov ah, 4Ch; int 21h`); its bytes are built into `asm.c` (`g_dos_stub`), the PE headers follow at `0x80` |
| `runtime\wingdi.c` | TextOutA, DrawTextA… for UTF-8: GDI receives the same text as UTF-16 |
| `runtime\rtasm.asm` | setjmp/longjmp, CPU helpers; `memcpy`, `memmove`, `memset` (`rep movsb`/`stosb`), `strlen` (SSE2) |
| `include\windows.h`, `include\win\*.h` | Windows: types, structures, constants, functions of kernel32, user32, gdi32, comdlg32, shell32, advapi32; `except.h` — exceptions (`EXCEPTION_*`, `CONTEXT`, `RaiseException`) |
| `include\threads.h stdatomic.h complex.h fenv.h tgmath.h` | C11 and math: threads, atomics, complex numbers (Microsoft style), floating-point environment, `_Generic` wrappers |
| `include\io.h fcntl.h direct.h process.h conio.h malloc.h intrin.h sal.h sys\stat.h sys\types.h` | Microsoft C headers: descriptors, folders, threads and processes, console, `_alloca`, intrinsics (macros with `asm`), SAL annotations (empty) |
| `include\windowsx.h tchar.h commctrl.h mmsystem.h commdlg.h shellapi.h shlobj.h` | the rest of the familiar set; `shlobj.h` — choosing a folder (`SHBrowseForFolder`, `SHGetPathFromIDList`, `BROWSEINFO`), item ids, `CoTaskMemFree` (all in `win\shell32.h`); `user32.h` also has `NCCALCSIZE_PARAMS`, `WMSZ_*`, `SetLayeredWindowAttributes`, `MsgWaitForMultipleObjects(Ex)` with `QS_*`/`MWMO_*`, `SetProcessDpiAwarenessContext` |
| Microsoft C with UTF-16 names | `_wfopen`, `_wfreopen`, `_wremove`, `_wrename` (`runtime\winfile.c`), `_wgetenv` (`winhost.c`), `_snwprintf` / `_vsnwprintf` with Microsoft's rules: `%s` wide, `%S` narrow, no NUL when the text fills the buffer, -1 when cut (`runtime.c`) |
| `include\win\resource.h`, `include\winres.h winresrc.h winver.h` | resources from the program: `FindResource`, `LoadResource`, `LoadString`, `LoadMenu`, `LoadAccelerators`, `CreateDialogParam`, `DialogBoxParam`, `LoadBitmap`, enumerating and updating resources, version info (`GetFileVersionInfo`, `VerQueryValue` — `version.dll`); `.rc` constants (`VS_VERSION_INFO`, `VOS_*`, `VFT_*`, `IDC_STATIC`) |
| `include\winsock2.h ws2tcpip.h winhttp.h` (+ `winsock.h`, `ws2ipdef.h` — forward to `winsock2.h`) | `ws2_32` sockets (TCP, UDP, IPv4 and IPv6, select, WSAPoll, window and event notifications, getaddrinfo, inet_pton) and `winhttp` HTTP/HTTPS (requests, headers, proxy, timeouts, WebSocket). 100 functions, all checked against the DLL exports; every constant and structure size checked against MSVC (`tests\winabi\abi.ps1`) |
| `include\net.h` | the networking library — everything a program calls (see "Networking") |
| `lib\net\` | its sources: `core.c` — errors in Windows' own words, UTF-8 ↔ UTF-16, `%XX`, names; `http.c` — HTTP/HTTPS via WinHTTP; `tcp.c` — connections; `udp.c` — datagrams; `server.c` — the web server (a thread per connection, up to 64, handler serialized under a lock); `net_int.h` — internal shared declarations |
| `include\gdiplus.h` | GDI+ for C — the flat functions of `gdiplus.dll` (like `GdiPlusFlat.h` in the SDK): antialiased shapes, brushes and gradients, pens, paths, regions, rotation and scaling, ClearType text, images (load, draw, pixels, save — `GdipEncoderPng`, `…Jpeg`, `…Bmp`, `…Gif`, `…Tiff`), icon from an image. 183 functions, all checked against the `gdiplus.dll` exports; test — `tests\win_gdiplus.c` |
| `include\wui.h`, `include\wui_icons.h` | the WUI styling library — everything a program calls; ThemeLibrary icons (`WUI_ICON_HOME`…) |
| `lib\wui\` | WUI sources — see "Window styling" |
| `tools\themes-export\` | C# exporter of themes and icons from `ThemeLibrary.dll` into `lib\wui\themes.c`, `style.h`, `icons.c` and `include\wui_icons.h` |
| `examples\wui\` | **WUI showcase** — `wcc main.c pages.c -o "Показ WUI.exe"` (or the `Показ WUI.wccproj` project in the editor): every control on five pages, a side menu up to the very top of the window, a theme picker in the title bar; `--страница=N` opens page N (0…4) directly |
| `examples\` | `hello.c`, `window.c` (window, menu, button, painting, file dialog), `dll\mathlib.c` + `dll\usemath.c` |
| `examples\net\` | `get.c` — `get url` prints the server response, `get url file` downloads with a progress bar; `server.c` — a small site at `http://localhost:8080`: a page with a form, the time, `/stop` |
| `examples\calc\` | **Calculator** — `wcc main.c engine.c draw.c -o Калькулятор.exe` (or the `Калькулятор.wccproj` project): draws everything itself with GDI+ (antialiasing, ClearType, gradients, shadows, smooth hover), dark title bar, an icon drawn by the program, sharp at any display scale. Operator precedence (2 + 3 × 4 = 14), percentages as on a phone (200 + 10 % = 220), large numbers as `×10¹⁷`, a history pane in a wide window (click — the number goes back to the display, wheel — scroll), keyboard, Esc, Delete, F9 (±), Ctrl+C / Ctrl+V. `--демо=12+3*4=` presses the buttons itself (for screenshots). Parts: `engine.c` — arithmetic and number formatting, `draw.c` — painting, `main.c` — window and input; GDI+ from WinCC's `<gdiplus.h>` |
| `tests\` | tests: `run.ps1` builds every `tests\*.c` into a real .exe and runs it; `win_api.c` — Windows API; `win_wui.c` — WUI; `win_net.c` — networking without internet (the program is its own server: HTTP, TCP, UDP, a slow client); `pp_c11.c` — preprocessor and number literals; `c11_lang.c` — C11 and Microsoft C (VLA, `_Generic`, `__try`…); `win_lib.c` — the newer library headers; `win_res.c` — resources; `wui\click.ps1` — WUI driven by mouse and keyboard (window messages, a screenshot after each step); `winabi\abi.ps1` — headers against the real ones (MSVC): `sizes.c` — windows.h, `net.c` — network structures, `consts.c` is generated from every `#define` of the network headers, `bits.c` — which bytes and bits each bit-field occupies in 27 structures; `res\compare.ps1` — resources byte for byte against rc.exe; `snap.ps1` — a screenshot of a program window |
| `compiler\host.c`, `build-cctest.cmd` | cctest — the same compiler tested in DynastyOS mode |
| `ide\` | the WinCC.exe editor (section below) |
| `setup\` | the installer (`make-setup.ps1`, `Installer.cs`, `SetupWindow.xaml`) |
| `docs\руководство\` | **WinCC Manual** (in Russian) — a book for people writing programs with WinCC: chapters as text, `python сборка.py` builds the PDF, `python "сборка chm.py"` builds the Windows help file `Руководство WinCC.chm` (needs HTML Help Workshop), `python "проверка примеров.py"` compiles every example of the book |

**Unicode.** Sources are UTF-8. The console, files, `argv` and GDI text get UTF-8 through the library's own W-calls.
For the A-functions of Windows a program asks for WinCC's manifest — `#pragma comment(linker, "/MANIFEST")` or
`wcc --manifest` (none by default: an empty program is 1 KB) — which sets its code page to UTF-8, so
`MessageBoxA`, `CreateWindowExA`, `SetWindowTextA` and the other A-functions take the program's text as is. GDI ignores
the manifest, so `TextOutA`, `DrawTextA`, `ExtTextOutA`, `GetTextExtentPoint32A`, `CreateFontA` go through
`runtime\wingdi.c`. With `#define UNICODE` before `windows.h` the plain names are the W-functions (UTF-16, `L"…"`,
`TEXT("…")`). The same manifest turns on modern control visuals (Common Controls 6).

## Building

- `build.cmd` — builds `wcc.exe` into the root (MSVC, Release flags `/O2`).
- `ide\` — `dotnet build -c Release`; `WinCC.exe` is copied into the root.
- `compiler\build-cctest.cmd` — cctest for DynastyOS mode.
- **Installer:** `powershell -ExecutionPolicy Bypass -File setup\make-setup.ps1` — builds `wcc.exe` and `WinCC.exe`,
  packs the payload into `setup\payload.zip` (both programs, `runtime\`, `include\`, `lib\`, `examples\` sources, README,
  "Руководство WinCC.pdf" — the installer will not build without it — and "Руководство WinCC.chm" when built), and builds `WinCCSetup.exe` into the root with a
  copy on the desktop.

## Testing

- `powershell -File tests\run.ps1` — 48 tests as real .exe files (all pass; an exe that Windows refuses to start is a
  failure); `x87.c` — x87 instructions; `bitfields.c` — bit-fields at work (signed reads, writes, `+=`, `++`,
  initializers, unions).
- `powershell -File tests\wui\click.ps1 showcase.exe folder` — the WUI showcase driven by mouse and keyboard, screenshots into the folder.
- `powershell -File tests\winabi\abi.ps1` — 1018 lines (sizes, offsets, constants, bit-field bytes) match MSVC.
- `powershell -File tests\res\compare.ps1` — 22 resources identical to rc.exe output.
- `compiler\cctest.exe --tests ..\tests` — the same tests in DynastyOS mode (all pass).

## Limitations

- The atomic `InterlockedIncrement` family lives in `runtime\rtasm.asm` (64-bit Windows exports none of them from any DLL).
- `long double` is the same as `double`; there is no language-level `_Complex` (`complex.h` uses structures, as Microsoft does).
- Optimization is simple: variables in registers, constant folding, one-instruction element addressing; no loop
  unrolling, no inlining, no auto-vectorization. Assembler labels (except numeric ones) are global to the whole
  program; no SSE intrinsics with `__m128`.
- `return` inside `__finally`, and `__try` inside `__finally` or inside an `__except` filter, are not supported.
- Library time: `time()` is UTC, `localtime`/`mktime`/`ctime` use the Windows time zone and daylight saving
  (`winhost.c`: `cc_utc_seconds`, `cc_utc_offset`).
- `mbrtowc`/`mbtowc` cannot return a character above U+FFFF (it does not fit into one `wchar_t`); whole-string
  functions (`mbstowcs`, `%ls`, `fputws`, `fgetwc`…) handle UTF-16 surrogate pairs.

## Window library: `elements.h` (Elements)

Elements — a native window library in C in the spirit of WPF on GDI+ (the project `Code\Libraries\Elements`), built
into WinCC like WUI: `#include <elements.h>`, and wcc builds `lib\elements` with the program and drops what it does not
call; the program is a windowed one even with `main`. Nothing else is needed: no `.lib`, no `-I`.

```c
#include <elements.h>
char name[64] = "World";
int main(void) {
    el w = el_window("Hello", 420, 230);
    el s = el_stack(w, 0);
    el_edit(s, name, sizeof(name));
    el_button(s, "Say hello");
    el_run();
}
```

| Where | What |
|---|---|
| `include\elements.h` | its header, plus `#pragma comment(lib, "elements")` and `/SUBSYSTEM:WINDOWS` (under `__WINCC__`) |
| `lib\elements\` | its sources (`el_*.c`, `el_internal.h`, `el_gdip.h` — GDI+ for C) |
| `examples\elements\demos\` | its 11 examples, a `.wccproj` for each: hello, form, cyberpunk, design, curves, effects, shaped, custom_window, triangle, fonts, fontshow |
| `examples\elements\fonts\` | its 30 fonts (the font examples look in `..\fonts` next to the `.exe`) |
| `examples\elements\README.md`, `DOCUMENTATION.md` | its description and every function |
| `tools\sync-elements.ps1` | takes a fresh Elements from `..\Libraries\Elements` (does not change it) and puts it in these places |

What WinCC got for it: `shlobj.h`, the user32 additions above, `_wfopen`, `_wgetenv`, `_snwprintf`.
After changes in Elements itself: `powershell -ExecutionPolicy Bypass -File tools\sync-elements.ps1`.

## Window styling: `wui.h`

WUI brings the look of ThemeLibrary themes to C programs. One line, `#include <wui.h>`, is enough: wcc builds the
`lib\wui` folder together with the program and drops whatever the program does not call. The program becomes a GUI
program even with `main` (no console). The result needs only Windows DLLs (GDI+ included).

```c
#include <wui.h>
int main(void) {
    WuiControl *win = wui_window("Hello", 800, 520, "Hello");     // key: where the theme choice is remembered
    wui_theme_picker(wui_titlebar(win));                         // pick any theme from the title bar
    WuiControl *card = wui_card(wui_content(win), "Name");
    wui_textbox(card, "What is your name?");
    wui_button(card, "Say hello", WUI_BUTTON_ACCENT);
    return wui_run();
}
```

- **Themes:** 61 ThemeLibrary themes — all except eleven parchment colours (one parchment is kept).
  Switching is live in all windows; the choice is remembered as ThemeLibrary does: `%APPDATA%\ThemeLibrary\<key>.theme`.
- **Window:** custom, per-pixel transparent — rounded corners, shadow, border, the theme background (gradient or texture,
  light spots, stars, patterns, frame), a title bar with buttons, dragging, resizing by the edges, maximizing to the
  work area of its own monitor, per-monitor scaling. `wui_window_extend` — content up to the very top of the window.
- **Layout:** column, row, wrap, scroll, card; size by content, fixed (`wui_size`) or a share of the remaining space
  (`wui_stretch`); margins, padding, alignment.
- **Controls:** labels (headings, book text, small text, a large glowing number), buttons in five styles and with icons,
  side-menu items with icons, text and password fields (selection, words, Ctrl+A/C/X/V/Z), checkbox, radio button,
  segmented buttons, slider, progress bar (and indeterminate), combo box, list, tabs, ThemeLibrary icons (123),
  a canvas for your own GDI+ painting, tooltip, message box. Tab and Shift+Tab move between controls.

| File in `lib\wui\` | Contents |
|---|---|
| `wui_int.h` | internal shared declarations: theme, brushes, controls, window |
| `themes.c`, `style.h`, `icons.c` | generated by `tools\themes-export` from `ThemeLibrary.dll` — do not edit by hand: palettes, 203 style values per theme (brushes, shadows, fonts, spacing), icons |
| `core.c` | GDI+ and DPI startup, current theme and remembering it, UTF-8 ↔ UTF-16 strings |
| `color.c`, `random.c` | colours as in ColorMath; .NET's System.Random number for number (textures and stars match WPF) |
| `paint.c` | rounded shapes, theme brushes (WPF gradients in fractions of the bounds), borders, blurred shadows and glow |
| `text.c` | theme fonts, measuring, wrapping, text glow |
| `texture.c` | 17 textures (parchment, lapis, marble, wood, leather, brass…) — pixel for pixel as in ThemeLibrary, multithreaded; sin/cos as in .NET (x87) |
| `backdrop.c`, `patterns.c` | background layer: spots, stars, vignette, grids, scan lines; patterns (honeycomb, flower of life, circuit board, waves, contour lines) |
| `frames.c`, `frames2.c`, `frames.h` | 13 window frames (triple line, gold, art nouveau, Egyptian, engraving, seal, screws, meander, art deco, stitches, Celtic, arabesque, runes) and a WPF path parser |
| `window.c` | window, title bar, mouse, keyboard, focus, painting with a cached background |
| `layout.c` | control tree, layout, scrolling, card, paint traversal |
| `controls.c`, `textbox.c`, `combo.c`, `popup.c`, `values.c`, `dialog.c` | controls; text field; lists and theme picker; popup window; tabs and values; message box |

**Updating themes** after ThemeLibrary changes: `dotnet build -c Release` in `tools\themes-export`, then
`tools\themes-export\bin\ThemesExport.exe lib\wui include` (theme list — `ThemesExport.exe --list`).

**Headless checks:** `WUI_SNAPSHOT=file.png` — the first window is saved as an image and the program exits;
`WUI_THEME=Id` — theme, `WUI_SIZE=width,height`, `WUI_SCALE=2.25` — display scale; `WUI_SNAPSHOT_WAIT=1` — the window
stays alive and saves `file-N.png` on a `WM_APP+3` message (this is how `tests\wui\click.ps1` works).

## Networking: `net.h`

One line, `#include <net.h>`: wcc builds the `lib\net` folder together with the program and drops what is unused.
The program needs only Windows DLLs (`ws2_32`, `winhttp`). Strings and paths are UTF-8.

```c
#include <net.h>
#include <stdio.h>
int main(void) {
    NetResponse *r = net_get("https://example.com");
    if (r->error) printf("Failed: %s\n", r->error);
    else printf("%d, %zu bytes:\n%s\n", r->status, r->size, r->body);
    net_free(r);
}
```

- **HTTP and HTTPS** (WinHTTP): `net_get`, `net_post`, `net_post_text`, `net_request` (any method and custom headers),
  `net_header`, `net_download` (into a file via "name.part": the old file stays intact until the new one is complete),
  `net_on_progress` — download progress. Proxy as configured in Windows, compressed responses are decompressed,
  redirects are followed, TLS 1.2 and 1.3. `net_set_timeout`, `net_set_agent`, `net_url_encode` / `net_url_decode`.
- **TCP:** `net_connect` (by name or address, IPv4 and IPv6, with a timeout), `net_listen` + `net_accept`,
  `net_listen_local` — this computer only (one socket hears both IPv4 and IPv6), `net_send`, `net_send_text`,
  `net_printf`, `net_recv`, `net_recv_all`, `net_recv_line`, `net_ready`, `net_socket_timeout`, `net_peer`,
  `net_handle` — the raw socket for `winsock2.h` functions.
- **UDP:** `net_udp`, `net_udp_send` (broadcast too: `255.255.255.255`), `net_udp_recv` — with the sender's address.
- **Names:** `net_resolve`, `net_hostname`.
- **Web server:** `net_http_start` (in its own thread), `net_http_start_local` (this computer only) or `net_http_run`
  (blocks); paths with `..` are rejected (400), chunked bodies are reassembled, the handler gets the method, the path
  (already `%XX`-decoded), the query string, headers and body; `net_http_param` — a value from the URL or a form,
  `net_http_header`, replies `net_http_reply`, `_text`, `_html`, `_file` (type by extension), `net_http_add_header`.
  No reply means 404. Stop with `net_http_stop` (also from a handler: `net_http_server(r)`).
- **Errors:** functions return 0 or -1, `net_error()` says why — in Windows' own words, in the system language
  ("The server name or address could not be resolved (12007)"). Each thread has its own error.

## Your own libraries: DLL and `.lib`

Two kinds, both written in C like a program, both without `main`:

| | DLL (`--dll`) | Library `.lib` (`--lib`) |
|---|---|---|
| What it is | a separate file the program loads while running | your functions built into every program that takes them |
| Build | `wcc --dll mydll.c` → `mydll.dll` | `wcc --lib mylib.c util.c -o mylib.lib` |
| What is visible | functions marked `__declspec(dllexport)` (none marked — all non-`static`) | every non-`static` function |
| A program takes it | `wcc main.c -l mydll` or `-l ..\mydll\mydll` (by path); `mydll.dll` must lie next to the `.exe` | `wcc main.c ..\mylib\mylib.lib` — the `.lib` named on the command line |
| The program declares | the functions, e.g. from `mydll.h` (`-I` its folder) | the same, from `mylib.h` |

You name the libraries a program takes yourself (on the command line or as project files); wcc looks for nothing by
itself apart from `-l name` next to the source and in System32.

**`.lib`** — WinCC's own format, not Microsoft's (Visual Studio `.lib` files are not read). Inside (`cc.c`:
`cc_build_lib`; `wcc.c`: `read_wcclib`) a text "WCCLIB 1": every C file after the preprocessor (its headers and macros
already in, `#line` keeps the original file and line for errors and the debugger), `.asm` files as they are, `uses NAME` —
its `#pragma comment(lib)` names. The library is compiled once when it is made, so its own errors show there. A program
compiles it in as library code: functions nobody calls stay out (`strip.c`). Its functions are debugged like the
program's (steps go in, variables are shown) while its sources stay where they were.

**In the editor:** "New project…" asks the kind first (`ide\Projects\NewProjectDialog.cs`) and writes its template
(`ide\Projects\Templates.cs`):

| Kind | Template |
|---|---|
| Console | `main.c` with `main()` |
| Window | `main.c` with `WinMain()` |
| DLL | `name.h` with the declarations, `name.c` with `__declspec(dllexport)` functions and `DllMain` |
| Library | `name.h` and `name.c` with functions, no `main`; F7 builds `name.lib` |

"New" (Ctrl+N) writes a file of the chosen kind too; a new `.c` in a library project starts with `#include "name.h"`.
A program's project takes a `.lib` as a file ("Add files…" → `*.lib`; the panel shows it under **Libraries**) and its
own DLLs in Properties → DLLs (`-l`); the library's header folder goes into `#include` folders. A DLL or a library does
not run by itself.

## Optimization (1.3.0)

Program and library code is always compiled to be fast and compact; there are no flags for it. Compared with 1.2.0 on
the same benchmark (recursion, sorting, structures, strings, memory) it is 8.6 times faster (1217 ms → 142 ms), and
programs are half the size:

| Program | 1.2.0 | 1.3.0 |
|---|---|---|
| hello.exe | 69 KB | 35 KB |
| window.exe | 80 KB | 41 KB |
| `net\get.c` | 92 KB | 48 KB |
| Calculator | 190 KB | 97 KB |
| WUI showcase | 1023 KB | 748 KB |

The same LayoutSwitcher program built by MSVC is 113 KB; built by WinCC it is 34 KB and needs no Visual Studio runtime.

- Variables in registers (`codegen.c`: `alloc_regs`, `ra_walk`): integers and pointers whose address is never taken go
  into rbx, rsi, rdi, r12–r15 (callee-saved), the most used first (inside a loop — weight ×8). The prologue is
  `push rbp; push regs; [sub rsp, 8]; mov rbp, rsp` — parameters then sit 8 bytes higher per saved register; their
  unwind info (`.seh_proc 0, PAD, R...` → `asm.c`: `write_unwind`, PUSH_NONVOL codes) restores the registers on `__except`.
  Functions with `__try`, `asm()` (an insert may read `[rbp+16]` and use any register), `setjmp`, `...` or naked
  are left as before.
- Locations without extra code (`place_of`): a local is `[rbp-N]` or a register, a global of this file is
  `[rip+@name+N]`; an element through a pointer is a single `[base+index*scale+disp]` operand (`elem_addr`): `a[i]`,
  `p->x`, `a[i].y`, `a[j+1]` (the addend goes into the displacement).
- Constants are folded at compile time (`const_int`); a constant or variable right-hand side becomes an operand directly,
  without the stack (`int_operands`, `leaf_operand`); multiplication by a power of two is a shift, by 1 — nothing;
  unsigned division by a power of two — a shift and a mask; no-op casts (`int` to `int`) are removed before code
  generation (`drop_casts`).
- Comparisons in `if`, loops, `?:`, `&&`, `||` produce a conditional jump directly (`gen_branch`); the condition of
  `for`/`while` is at the bottom, one jump per iteration.
- Statements without a value (`gen_void`): `x = 5`, `i++`, `x += y`, `x = y + 3` — one instruction into a register or
  memory (`assign_direct`); in the parser, `x op= y` on a plain variable becomes `x = x op y` (`parse.c`: `plain_lvalue`).
- Calls: only the registers for the actual arguments are loaded (`xmm` — only for floating-point ones); register
  parameters come straight from `rcx`…`r9`.
- Library: `memcpy`, `memmove`, `memset` use `rep movsb`/`rep stosb`, `strlen` uses SSE2 16 bytes at a time
  (`runtime\rtasm.asm`; the C versions remain for DynastyOS): `memset` of 100 MB takes 4 ms instead of 391.
- Verified by all tests (`tests\run.ps1`, `compiler\cctest.exe --tests ..\tests`, `tests\winabi\abi.ps1`,
  `tests\res\compare.ps1`) and all examples of the manual (`проверка примеров.py`).

## Resources (`.rc`, `.res`)

`wcc app.c app.rc` puts resources into the `.exe` / `.dll` (`.rsrc` section). The `.rc` script is read the way
Microsoft's rc.exe reads it and produces the same bytes: `tests\res\compare.ps1` builds `tests\win_res.rc` with both and
compares every resource.

- C preprocessor: `#include`, `#define`, `#if`; `RC_INVOKED` is defined; from included `.h` and `.c` files only macros
  are taken (`#define IDI_APP 101` from `resource.h`). `#pragma code_page` is not needed: UTF-8, UTF-16 or 1251 files
  are detected automatically.
- `ICON`, `CURSOR` (with hotspot), `BITMAP`, `RCDATA`, `HTML`, `FONT`, `MESSAGETABLE`, `ANICURSOR`, `ANIICON`, manifest
  (`1 24 "file"` or `RT_MANIFEST`), custom types by number or name — from a file or a
  `BEGIN 1, 2L, "string", L"string" END` block; `STRINGTABLE`, `VERSIONINFO`, `MENU`, `MENUEX`, `ACCELERATORS`, `DIALOG`,
  `DIALOGEX` (all controls: `CONTROL`, `LTEXT`/`RTEXT`/`CTEXT`, `PUSHBUTTON`, `DEFPUSHBUTTON`, `EDITTEXT`, checkboxes,
  radio buttons, `GROUPBOX`, `LISTBOX`, `COMBOBOX`, `SCROLLBAR`, `ICON`; styles with `NOT`); `LANGUAGE` — for everything
  below or for a single resource.
- File paths are relative to the `.rc` folder, then the `-I` folders; both `"res\\app.ico"` and `"res\app.ico"` work.
- As in rc.exe: icon and cursor images share one counter; a quoted name (`"NAME" RCDATA …`) is stored with its quotes;
  a resource with the same type, name and language — the last one wins.
- The WinCC manifest (UTF-8 code page, Common Controls 6) is added when the program asks for it (`--manifest`,
  `#pragma comment(linker, "/MANIFEST")`); your own `1 24 "x.manifest"` replaces it.
- A ready `.res` (rc.exe, windres) — `wcc app.c app.res`: resources are taken as they are.
- Tests: `tests\win_res.c` — Windows itself loads every resource (16 and 32 px icons as BMP and PNG, cursor, bitmap,
  data, strings in two languages, version, both menu kinds, accelerators, both dialog kinds); `tests\res\compare.ps1` —
  the same with a `.res` from rc.exe and a byte-for-byte comparison (`tests\res\resdump.c` lists an exe's resources;
  the test files are generated by `tests\res\make.py`).

## The WinCC.exe editor (`ide\`)

An editor for the compiler: projects with a side panel of project files, file tabs, C highlighting, building with
`wcc.exe`, an error list, running. C# 7.3, WPF, .NET Framework 4.8, no NuGet; ThemeLibrary is compiled in from source —
the program is a single exe. There is one theme, Cyberpunk (`ide\App.xaml`, `App.xaml.cs`); no theme picker.

**Interface language:** English or Russian — the gear button on the right of the toolbar opens Settings
(`ide\SettingsDialog.cs`); the window switches at once, no restart. The first run follows the Windows language. All
interface strings of both languages sit side by side in `ide\Loc.cs`: XAML takes them as `{DynamicResource S.key}`,
code as `Loc.T("key")` / `Loc.F("key", args)`; ThemeLibrary's own texts (message box buttons) follow the same choice.
Compiler messages come from wcc and stay in Russian.

**Build:** `dotnet build -c Release` in `ide\`; `WinCC.exe` is copied into the `WinCC\` root next to `wcc.exe`.
The icon is drawn by `ide\tools\make-icon.ps1` (Windows PowerShell 5.1) into `ide\icon.ico`.

**Features and where they live:**
- Window, buttons, status bar, shortcuts, dropping files onto the window — `ide\MainWindow.xaml`, `MainWindow.xaml.cs`.
  Keys: Ctrl+N new, Ctrl+O open, Ctrl+S save, Ctrl+Shift+S save as, Ctrl+W close tab, F7 or Ctrl+B build,
  F5 debug, Ctrl+F5 run without the debugger, F9 breakpoint, F10 / F11 / Shift+F11 steps, Shift+F5 stop, F1 — the manual (book button on the right of the toolbar), Ctrl+Space — code hints, Ctrl+Shift+E — show / hide the project
  panel (the project button on the left of the toolbar).
- A project is a `name.wccproj` file (`ide\Projects\Project.cs`): UTF-8 text, `key=value` lines, paths relative to the
  project folder: `kind=` Console / Window / Dll / Library, `config=` Debug / Release, `output=` output file name (default — the project name), `file=` a project
  file (one per line, in build order), `define=` a macro (`-D`), `include=` an `#include` folder (`-I`), `lib=` a DLL
  (`-l`, a name or a path), `stack=` stack in MB, `options=` any other wcc arguments as is; lines starting with `#` are
  comments. `file=` takes `.c`, `.h`, `.asm`, `.rc`, `.res` and your own `.lib`.
  `.c`, `.asm` and `.rc`/`.res` resources go into the program; headers and other files (icons, images) are only listed.
  Sample projects: `examples\calc\Калькулятор.wccproj`, `examples\wui\Показ WUI.wccproj`.
- The project side panel — `ide\Projects\ProjectPanel.xaml(.cs)`: project name, files grouped as Sources / Headers /
  Resources / Other (icons, images and `.res` open in their Windows program, not in the editor), the front tab's file in
  bold, a missing file gets a warning icon; at the bottom — the program kind and output name. Inside each group, files in
  subfolders sit under their folders (`src\util\str.c` → src › util › str.c); a file outside the project folder sits
  under its full folder path. Double-click or Enter —
  open, Delete — remove from the project (the file stays on disk), right click — open, remove, show in folder; on a
  folder: Delete or right click — remove the whole folder from the project, show in folder. Files and folders dropped
  onto the panel join the project (a folder with all its subfolders), a dropped `.wccproj` is opened. Buttons on top:
  add files, add folder, new file in the project, properties, close project. A folder brings in its `.c`, `.h`, `.asm`,
  `.rc`, `.res`, `.ico`, `.cur`, `.bmp` files, skipping `bin`, `obj`, `.git`, `.vs`, `node_modules` and hidden folders
  (`Project.ScanFolder`). Without a project: "New project…", "Open project…" and recent projects (up to 8).
- Project properties — `ide\Projects\ProjectDialog.cs`: kind, output file, macros, `#include` folders, DLLs (lists
  separated by `;`), stack, other arguments.
- New, open, close project, add and remove files, recent projects — `MainWindow.Project.cs`. A new project in a folder
  that already has C files (subfolders included) offers to include them; in an empty folder it writes the template of
  the chosen kind (see "Your own libraries: DLL and `.lib`"). A `.wccproj`
  also opens via Open (Ctrl+O), by dropping it onto the window and from the command line. While a project is open,
  the "program kind" selector writes into the project.
- File tabs (new, open, save, close with an unsaved-changes prompt) — `MainWindow.Files.cs`; one open file —
  `Documents\SourceDocument.cs`: reads UTF-8 (with and without BOM), UTF-16 and 1251, saves in the same encoding, CRLF lines.
- The editor — `Editor\CodeEditor.xaml(.cs)`: a standard text box with transparent glyphs (editing, selection, undo are
  its own), Tab/Shift+Tab indent lines, Enter keeps the indent (deeper after `{`), `}` outdents the line,
  Ctrl+wheel — font size.
  - code hints — `Editor\CodeEditor.Completion.cs`: after N letters of a name (window settings: 1–4, or Ctrl+Space only)
    a list opens under the caret; arrows, Enter/Tab insert, Esc closes; after `name(` the function's declaration is shown
    above the line. Names — `Editor\Completion\Symbols.cs`: declarations (functions with parameters, `#define`, `typedef`,
    `enum`, globals) from the included headers — built into wcc (the text of `compiler\headers.c` is embedded in
    WinCC.exe), `include\` and your own `"…"` next to the file, following nested `#include`; from this and the other
    open files; C keywords. The list — `Editor\Completion\CompletionPopup.cs`. Check without a window:
    `WinCC.exe --completetest report.txt file.c prefix…`; a picture with the list: `--window shot.png complete:file.c:line:col`.
  - highlighting: a line-by-line C lexer — `Editor\CLexer.cs` (keywords, types, constants, strings and characters,
    comments including multi-line ones, numbers, preprocessor with `\` line continuation); visible lines are painted over
    the text box — `Editor\HighlightLayer.cs`;
  - colours come from the theme (accent, second accent, warning, danger, muted text); function calls are bright green
    (`#00E640`), function declarations and definitions and function-like macros are purple (`#B06CFF`): a name before
    `(` preceded by a type, `static`/`extern`…, a `*` after a type, the start of an unindented line or `#define` —
    `Editor\SyntaxColors.cs`, `Editor\CLexer.cs`;
  - line numbers, current line, lines with errors (red) and warnings (yellow) — `Editor\LineNumbers.cs`.
- Building — `MainWindow.Build.cs` + `Build\Compiler.cs`: modified files are saved first, then
  `wcc file.c -o file.exe` (kind Window — `--gui`, kind DLL — `--dll` and `-o file.dll`), working folder — the source
  folder. While a project is open, Build and Run build the whole project regardless of the front tab:
  `wcc a.c b.c … -o name.exe [kind] [-D… -I… -l… --stack …]`, working folder — the project folder; missing files are
  reported. Compiler output (UTF-8) goes to the Output tab. If there is no `wcc.exe` next to `WinCC.exe`, the panel says
  the compiler was not found and a warning lights up on the right of the toolbar. If the output file is locked by a
  running program, that is reported too.
- Errors: lines like `file:line: text` (and `file:line:column: text`; "предупреждение: …" is a warning) —
  `Build\CompilerMessage.cs`; listed in the Errors tab, a click opens the file at that line; double-clicking such a line
  in Output does the same.
- Running — `Build\Runner.cs`: a console program runs in its own console window (`cmd /c`), which prints the exit code
  at the end and waits for a key; a GUI program runs as is; a DLL is not run.
- Program kind (Console / Window / DLL) — `Build\ProgramKind.cs`.
- Remembered: interface language, program kind, open files, front tab, open project, recent projects, project panel (shown, width), bottom
  panel height — `Settings.cs` (`%APPDATA%\WinCC\settings.txt`).

**Build modes** (the list on the toolbar; a project keeps its own in `config=`): **Debug** — `wcc -g -O0`: debug info
next to the program (`prog.wdbg`), every variable in memory; **Release** — optimized code, no debug info (an old `.wdbg`
is deleted). F5 in Release builds with `-g`: the debugger then also shows variables kept in registers.
Both modes add `--tiny` (the 16-byte layout, only when it makes the file smaller).

**Debugger** (`ide\Debugger\`, `MainWindow.Debug.cs`):
- F9 or a click left of the line numbers — a breakpoint (red dot); breakpoints follow the text when lines are added or
  removed above them and are remembered between runs. A line without code moves the breakpoint to the next line with code.
- F5 — build and start under the debugger (Debug button), then "Continue"; Ctrl+F5 — run without the debugger;
  Shift+F5 — stop; F10 — step over (calls run through), F11 — step into (into the program's own functions; library and
  Windows code is run through), Shift+F11 — step out (to the end of the function). While the program is stopped the
  current line is marked with a yellow arrow and the editors are read-only.
- **Variables** tab: the stopped function's parameters and locals declared so far, then the program's globals; structs,
  arrays and pointers expand (a `char *` shows its string, a struct pointer — the struct); a value changed since the
  last stop is red. **Call stack** tab: the functions down to `main`; a click shows that function's line and variables.
- An unhandled exception (access violation, division by zero, stack overflow…) stops the program at the faulting line
  of the program (also when it happened inside the library, e.g. `strlen(NULL)`), with the reason in Output; Continue
  or Stop then ends the program. `__debugbreak()` stops it too; `OutputDebugString` goes to Output.
- How it works: `DebugEngine.cs` — the Windows debug API (`CreateProcess` with `DEBUG_ONLY_THIS_PROCESS`,
  `WaitForDebugEvent` in its own thread), breakpoints are `int3` bytes, steps use the trap flag and a temporary breakpoint
  at the return address; the call stack follows `rbp` (with the registers each function saved). `DebugInfo.cs` reads
  `.wdbg`, `Values.cs` turns memory into C values, `Native.cs` — the kernel32 calls.
- `.wdbg` is text (`compiler\asm.c`, `dbg_text`): `file N name`, `func START END name` (RVAs, hex), after it `frame NSAVE PAD
  regs…` and `var LOC LINE TYPE name` (LOC `s-16` = `[rbp-16]`, `S-16` — the address is there, `rrbx` — in a register;
  LINE 0 — a parameter), `line RVA FILE LINE`, `global RVA LINE TYPE name`, `struct ID SIZE s|u` + `field OFF BITOFF BITS
  TYPE name`. TYPE: `c C b i2 i4 i8 u2 u4 u8 f4 f8 e4 v F`, `*T`, `[N]T`, `sID`.
- Headless check: `WinCC.exe --debugtest report.txt prog.exe file.c:line[,line] commands` (c continue, n step over,
  s step into, o step out, x stop) writes every stop with its stack and variables; `--window shot.png … debug:file.c:line`
  — a screenshot of the window stopped there.

**Headless check:** `WinCC.exe --window shot.png [WxH] [empty] [project:file.wccproj] [lang:en|ru]` — a screenshot of
the window with sample code, output and errors (`Samples.cs`), nothing is remembered; `empty` — a window without files;
`project:` — with a project open; `lang:` — interface language.
