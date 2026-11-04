// gdiplus.h of WinCC: drawing into a picture with GDI+, pixels, text, transforms, PNG out and back.
#include "test.h"
#ifndef _WIN32                          // DynastyOS (cctest): no Windows there
int main(void) { return 0; }
#else
#include <gdiplus.h>

int main(void)
{
    ULONG_PTR token;
    GdiplusStartupInput in = { 1 };
    ASSERT(Ok, GdiplusStartup(&token, &in, NULL));

    GpBitmap *bmp;
    ASSERT(Ok, GdipCreateBitmapFromScan0(64, 48, 0, PixelFormat32bppARGB, NULL, &bmp));
    GpGraphics *g;
    ASSERT(Ok, GdipGetImageGraphicsContext(bmp, &g));
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipGraphicsClear(g, MakeARGB(255, 10, 20, 30));

    GpSolidFill *red;
    GdipCreateSolidFill(MakeRGB(255, 0, 0), &red);
    ASSERT(Ok, GdipFillEllipse(g, red, 20, 10, 24, 24));            // круг в середине

    GpPen *pen;
    GdipCreatePen1(MakeRGB(0, 255, 0), 2, UnitPixel, &pen);
    GdipDrawLine(g, pen, 0, 46, 63, 46);                              // зелёная линия внизу

    GpPath *path;
    GdipCreatePath(FillModeAlternate, &path);
    GdipAddPathRectangle(path, 2, 2, 8, 8);
    GpSolidFill *blue;
    GdipCreateSolidFill(MakeRGB(0, 0, 255), &blue);
    GdipFillPath(g, blue, path);                                      // синий квадрат в углу

    GpFontFamily *ff;
    ASSERT(Ok, GdipGetGenericFontFamilySansSerif(&ff));
    GpFont *font;
    ASSERT(Ok, GdipCreateFont(ff, 12, FontStyleRegular, UnitPixel, &font));
    RectF lay = { 0, 0, 1000, 100 }, box = { 0 };
    ASSERT(Ok, GdipMeasureString(g, L"Привет", -1, font, &lay, NULL, &box, NULL, NULL));
    ASSERT(1, box.Width > 20 && box.Height > 8);

    GpMatrix *m;
    GdipCreateMatrix(&m);
    GdipRotateMatrix(m, 90, MatrixOrderAppend);
    PointF pt = { 10, 0 };
    GdipTransformMatrixPoints(m, &pt, 1);
    ASSERT(1, pt.X > -0.01f && pt.X < 0.01f && pt.Y > 9.99f && pt.Y < 10.01f);

    GdipDeleteGraphics(g);
    ARGB c;
    GdipBitmapGetPixel(bmp, 32, 22, &c);
    ASSERT((int)MakeRGB(255, 0, 0), (int)c);
    GdipBitmapGetPixel(bmp, 5, 5, &c);
    ASSERT((int)MakeRGB(0, 0, 255), (int)c);
    GdipBitmapGetPixel(bmp, 1, 1, &c);
    ASSERT((int)MakeARGB(255, 10, 20, 30), (int)c);

    // PNG: сохранить и прочитать обратно
    ASSERT(Ok, GdipSaveImageToFile(bmp, L"проба gdiplus.png", &GdipEncoderPng, NULL));
    GpBitmap *back;
    ASSERT(Ok, GdipCreateBitmapFromFile(L"проба gdiplus.png", &back));
    UINT w = 0, h = 0;
    GdipGetImageWidth(back, &w);
    GdipGetImageHeight(back, &h);
    ASSERT(64, (int)w);
    ASSERT(48, (int)h);
    GdipBitmapGetPixel(back, 32, 22, &c);
    ASSERT((int)MakeRGB(255, 0, 0), (int)c);
    GdipDisposeImage(back);
    DeleteFileW(L"проба gdiplus.png");

    GdipDeleteMatrix(m);
    GdipDeleteFont(font);
    GdipDeleteFontFamily(ff);
    GdipDeletePath(path);
    GdipDeletePen(pen);
    GdipDeleteBrush(red);
    GdipDeleteBrush(blue);
    GdipDisposeImage(bmp);
    GdiplusShutdown(token);
    return g_fail;
}
#endif
