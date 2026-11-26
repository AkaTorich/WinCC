// GDI+ helpers: one call creates the brush/pen/path, draws and frees it.
#include "el_internal.h"

void gfx_fill_rect(GpGraphics *gr, ARGB c, float x, float y, float w, float h)
{
    GpBrush *b = NULL;
    if (GdipCreateSolidFill(c, &b) != GP_OK) return;
    GdipFillRectangle(gr, b, x, y, w, h);
    GdipDeleteBrush(b);
}

void gfx_fill_path(GpGraphics *gr, ARGB c, GpPath *p)
{
    GpBrush *b = NULL;
    if (GdipCreateSolidFill(c, &b) != GP_OK) return;
    GdipFillPath(gr, b, p);
    GdipDeleteBrush(b);
}

void gfx_draw_path(GpGraphics *gr, ARGB c, float width, GpPath *p)
{
    GpPen *pen = NULL;
    if (GdipCreatePen1(c, width, GP_UNIT_PIXEL, &pen) != GP_OK) return;
    GdipSetPenMode(pen, GP_PEN_CENTER);
    GdipDrawPath(gr, pen, p);
    GdipDeletePen(pen);
}

void gfx_fill_ellipse(GpGraphics *gr, ARGB c, float x, float y, float w, float h)
{
    GpBrush *b = NULL;
    if (GdipCreateSolidFill(c, &b) != GP_OK) return;
    GdipFillEllipse(gr, b, x, y, w, h);
    GdipDeleteBrush(b);
}

void gfx_draw_ellipse(GpGraphics *gr, ARGB c, float width, float x, float y, float w, float h)
{
    GpPen *pen = NULL;
    if (GdipCreatePen1(c, width, GP_UNIT_PIXEL, &pen) != GP_OK) return;
    GdipDrawEllipse(gr, pen, x, y, w, h);
    GdipDeletePen(pen);
}

void gfx_line(GpGraphics *gr, ARGB c, float width, float x0, float y0, float x1, float y1, int round)
{
    GpPen *pen = NULL;
    if (GdipCreatePen1(c, width, GP_UNIT_PIXEL, &pen) != GP_OK) return;
    if (round) { GdipSetPenStartCap(pen, GP_CAP_ROUND); GdipSetPenEndCap(pen, GP_CAP_ROUND); }
    GdipDrawLine(gr, pen, x0, y0, x1, y1);
    GdipDeletePen(pen);
}

void gfx_lines(GpGraphics *gr, ARGB c, float width, const GpPoint *pts, int n)
{
    GpPen *pen = NULL;
    if (GdipCreatePen1(c, width, GP_UNIT_PIXEL, &pen) != GP_OK) return;
    GdipDrawLinesI(gr, pen, pts, n);
    GdipDeletePen(pen);
}

void gfx_fill_poly(GpGraphics *gr, ARGB c, const GpPointF *pts, int n)
{
    GpPath *p = NULL;
    if (n < 3 || GdipCreatePath(GP_FILL_ALTERNATE, &p) != GP_OK) return;
    GdipAddPathPolygon(p, pts, n);
    gfx_fill_path(gr, c, p);
    GdipDeletePath(p);
}

void round_rect_path(GpPath *p, GpRectF r, float rad)
{
    float d;
    GdipResetPath(p);
    if (rad <= 0.5f) { GdipAddPathRectangle(p, r.X, r.Y, r.Width, r.Height); return; }
    d = rad * 2;
    if (d > r.Width) d = r.Width;
    if (d > r.Height) d = r.Height;
    GdipAddPathArc(p, r.X, r.Y, d, d, 180, 90);
    GdipAddPathArc(p, r.X + r.Width - d, r.Y, d, d, 270, 90);
    GdipAddPathArc(p, r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    GdipAddPathArc(p, r.X, r.Y + r.Height - d, d, d, 90, 90);
    GdipClosePathFigure(p);
}

GpPath *gfx_rrect(GpRectF r, float rad)
{
    GpPath *p = NULL;
    if (GdipCreatePath(GP_FILL_ALTERNATE, &p) != GP_OK) return NULL;
    round_rect_path(p, r, rad);
    return p;
}

GpStringFormat *gfx_format(int nowrap, int align, int valign, int ellipsis)
{
    GpStringFormat *gen = NULL, *sf = NULL;
    INT flags = 0;
    GdipStringFormatGetGenericTypographic(&gen);
    if (GdipCloneStringFormat(gen, &sf) != GP_OK) return NULL;
    GdipGetStringFormatFlags(sf, &flags);
    flags |= GP_SF_MEASURE_TRAILING;
    if (nowrap) flags |= GP_SF_NOWRAP;
    GdipSetStringFormatFlags(sf, flags);
    GdipSetStringFormatAlign(sf, align);
    GdipSetStringFormatLineAlign(sf, valign);
    GdipSetStringFormatTrimming(sf, ellipsis ? GP_TRIM_ELLIPSIS_CHAR : GP_TRIM_NONE);
    return sf;
}

void gfx_text(GpGraphics *gr, const WCHAR *s, GpFont *f, ARGB c, GpRectF r, const GpStringFormat *sf)
{
    GpBrush *b = NULL;
    if (!s || !*s || !f) return;
    if (GdipCreateSolidFill(c, &b) != GP_OK) return;
    GdipDrawString(gr, s, -1, f, &r, sf, b);
    GdipDeleteBrush(b);
}

void gfx_text_at(GpGraphics *gr, const WCHAR *s, GpFont *f, ARGB c, float x, float y)
{
    GpStringFormat *sf = gfx_format(1, GP_ALIGN_NEAR, GP_ALIGN_NEAR, 0);
    gfx_text(gr, s, f, c, rectf(x, y, 0, 0), sf);
    if (sf) GdipDeleteStringFormat(sf);
}
