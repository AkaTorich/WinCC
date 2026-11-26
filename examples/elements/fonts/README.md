# fonts

30 **original** fonts drawn by a program — `tools/fontgen`. Regenerate with:

```
python tools/fontgen/build.py            # all 30 (needs fontTools; Pillow for proof sheets)
python tools/fontgen/build.py 24 27      # only some, by number or name
```

How they are made: every letter is described once as stroke axes in real font metrics
(cap height, x-height, ascender, descender; separate lowercase; Cyrillic lowercase as
small caps where that is the norm). A pen turns the axes into TrueType outlines: stroke
width depends on direction like a broad nib (contrast), ends are flat or round, slanted
strokes are cut exactly on the baseline/cap line, bowls are superellipses with overshoot,
serifs are slab / bracketed / hairline / wedge. The 30 families are 30 settings of that pen
and of width, x-height, roundness and slant:

| | | |
|---|---|---|
| Elm Grotesk (Light, Bold, Black, Italic, Condensed, Extended) | Elm Compressed | Elm Geometric (Light, Bold) |
| Elm Humanist (Bold, Italic) | Elm Rounded (Bold) | Elm Techno (Bold) |
| Elm Mono (Bold) | Elm Typewriter | Elm Slab (Bold) |
| Elm Serif (Bold, Italic) | Elm Didone | Elm Oldstyle |
| Elm Hairline | Elm Wide Light | |

Coverage: Latin and Cyrillic (upper and lower case), digits, punctuation, № ₽ € « ».
Proof sheets go to `tools/fontgen/proof/`.

The examples load the fonts from the `ELFONTS` environment variable or from
`<exe folder>\..\fonts`. The embedding mechanism — `el_font_add(name, path)` and
`el_font_add_memory(name, bytes, size)` — works with any `.ttf`/`.otf`.
