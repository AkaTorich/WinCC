// WUI — рамки окна: общие помощники frames.c и frames2.c (Backdrop.cs, BackdropArt.cs, BackdropFrames.cs).
#pragma once
#include "wui_int.h"

typedef struct { double x, y; } FrPt;

// ---- кисти и перья ----
GpBrush *fr_solid(WuiColor c);                                       // ArtSolid
GpBrush *fr_gilded(WuiColor c, double w, double h, double opacity);  // Gilded: позолота наискосок
GpPen   *fr_pen(GpBrush *b, double th);                              // ArtPen: плоские концы, острые углы
GpPen   *fr_round_pen(GpBrush *b, double th);                        // RoundPen: круглые концы и углы
WuiColor fr_opacity(WuiColor c, double k);                           // c.WithAlpha((byte)(c.A * k))
WuiColor fr_ground(const WuiTheme *t);                               // Ground: цвет «фона» под рамкой

// ---- фигуры: заливка, затем обводка; NULL — без неё ----
void fr_rect(GpGraphics *g, GpBrush *fill, GpPen *pen, double x, double y, double w, double h);
void fr_inset(GpGraphics *g, GpPen *pen, double w, double h, double inset);   // DrawRectangle(Inset(...))
void fr_ellipse(GpGraphics *g, GpBrush *fill, GpPen *pen, double cx, double cy, double rx, double ry);
void fr_line(GpGraphics *g, GpPen *pen, double x1, double y1, double x2, double y2);
void fr_geo(GpGraphics *g, GpBrush *fill, GpPen *pen, GpPath *p);             // DrawGeometry
void fr_geo_free(GpGraphics *g, GpBrush *fill, GpPen *pen, GpPath *p);        // то же и удалить путь

// ---- пути ----
GpPath *fr_diamond(double cx, double cy, double r);
GpPath *fr_polyline(const FrPt *pts, int n, int closed);
GpPath *fr_parse(const char *s);           // строка языка путей WPF: M L H V C Q A Z
void fr_arc_to(GpPath *p, double x0, double y0, double x1, double y1, double rx, double ry, int large, int clockwise);

// ---- преобразования: PushTransform / Pop ----
UINT fr_push(GpGraphics *g, double m11, double m12, double m21, double m22, double dx, double dy);
UINT fr_push_rotate(GpGraphics *g, double angle, double cx, double cy);
UINT fr_mirror(GpGraphics *g, int i, double w, double h);     // Mirrors(w, h)[i]
void fr_pop(GpGraphics *g, UINT state);
void fr_toward(int corner, double w, double h, double inset, double *x, double *y);   // Toward(Corners[i])

// ---- знаки шрифтом ----
GpFontFamily *fr_family(const WCHAR *const *names, int count);                // первое найденное имя
GpPath *fr_text_path(GpFontFamily *f, const WCHAR *text, int len, double size, int bold, double x, double y);
// HieroglyphShapes / RuneShapes: знаки по 20, общий масштаб до высоты height, сплющены до 0.05.
void fr_glyph_shapes(const WCHAR *const *names, int nnames, const unsigned *codes, int n, double height, GpPath **out);
void fr_free_paths(GpPath **paths, int n);
// Знак в клетку (cx, cy): сдвиг на целые DIP, как Math.Round в C#.
void fr_place(GpGraphics *g, GpBrush *fill, GpPath *shape, double cx, double cy);

// ---- рамки из BackdropFrames (frames2.c) ----
void fr_meander(GpGraphics *g, const WuiTheme *t, double w, double h);
void fr_artdeco(GpGraphics *g, const WuiTheme *t, double w, double h);
void fr_stitched(GpGraphics *g, const WuiTheme *t, double w, double h);
void fr_celtic(GpGraphics *g, const WuiTheme *t, double w, double h);
void fr_arabesque(GpGraphics *g, const WuiTheme *t, double w, double h);
void fr_runic(GpGraphics *g, const WuiTheme *t, double w, double h);
