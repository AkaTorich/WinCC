// gdiplus.h of WinCC: GDI+ for C -- the flat functions of gdiplus.dll (as GdiPlusFlat.h of the
// Windows SDK has them): smooth shapes, gradients, pens, paths, ClearType text, pictures
// (load and save PNG, JPEG, BMP, GIF), transforms, icons from pictures.
//
//   ULONG_PTR token;
//   GdiplusStartupInput in = { 1 };
//   GdiplusStartup(&token, &in, NULL);      ... once, before any other call
//   GdipCreateFromHDC(dc, &g);  GdipSetSmoothingMode(g, SmoothingModeAntiAlias); ...
//   GdiplusShutdown(token);                 ... at the end
//
// Every function returns a GpStatus: Ok (0) or what went wrong. Objects are opaque
// pointers; in C one brush is any brush (GpSolidFill, GpLineGradient... are GpBrush).
#pragma once
#include <windows.h>
#pragma comment(lib, "gdiplus")

typedef void GpGraphics, GpImage, GpBitmap, GpMetafile, GpBrush, GpSolidFill, GpLineGradient,
             GpPathGradient, GpHatch, GpTexture, GpPen, GpPath, GpRegion, GpMatrix, GpFontFamily,
             GpFont, GpFontCollection, GpStringFormat, GpImageAttributes, GpCachedBitmap;
typedef float REAL;
typedef DWORD ARGB;
typedef int GpStatus;

// ---- status ----
enum {
    Ok = 0, GenericError = 1, InvalidParameter = 2, OutOfMemory = 3, ObjectBusy = 4, InsufficientBuffer = 5,
    NotImplemented = 6, Win32Error = 7, WrongState = 8, Aborted = 9, FileNotFound = 10, ValueOverflow = 11,
    AccessDenied = 12, UnknownImageFormat = 13, FontFamilyNotFound = 14, FontStyleNotFound = 15,
    NotTrueTypeFont = 16, UnsupportedGdiplusVersion = 17, GdiplusNotInitialized = 18, PropertyNotFound = 19,
    PropertyNotSupported = 20
};

// ---- geometry ----
typedef struct { REAL X, Y; } PointF, GpPointF;
typedef struct { INT X, Y; } Point, GpPoint;
typedef struct { REAL Width, Height; } SizeF;
typedef struct { REAL X, Y, Width, Height; } RectF, GpRectF;
typedef struct { INT X, Y, Width, Height; } Rect, GpRect;

// ---- colors: 0xAARRGGBB ----
#define MakeARGB(a, r, g, b) ((ARGB)(((DWORD)(BYTE)(a) << 24) | ((DWORD)(BYTE)(r) << 16) | ((DWORD)(BYTE)(g) << 8) | (DWORD)(BYTE)(b)))
#define MakeRGB(r, g, b) MakeARGB(255, r, g, b)

// ---- start and stop ----
typedef struct {
    UINT32 GdiplusVersion;              // 1
    void *DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;
GpStatus GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, void *output);
void GdiplusShutdown(ULONG_PTR token);

// ---- modes ----
enum { QualityModeInvalid = -1, QualityModeDefault = 0, QualityModeLow = 1, QualityModeHigh = 2 };
enum { SmoothingModeInvalid = -1, SmoothingModeDefault = 0, SmoothingModeHighSpeed = 1, SmoothingModeHighQuality = 2,
       SmoothingModeNone = 3, SmoothingModeAntiAlias = 4, SmoothingModeAntiAlias8x4 = 4, SmoothingModeAntiAlias8x8 = 5 };
enum { PixelOffsetModeInvalid = -1, PixelOffsetModeDefault = 0, PixelOffsetModeHighSpeed = 1, PixelOffsetModeHighQuality = 2,
       PixelOffsetModeNone = 3, PixelOffsetModeHalf = 4 };
enum { TextRenderingHintSystemDefault = 0, TextRenderingHintSingleBitPerPixelGridFit = 1, TextRenderingHintSingleBitPerPixel = 2,
       TextRenderingHintAntiAliasGridFit = 3, TextRenderingHintAntiAlias = 4, TextRenderingHintClearTypeGridFit = 5 };
enum { InterpolationModeInvalid = -1, InterpolationModeDefault = 0, InterpolationModeLowQuality = 1,
       InterpolationModeHighQuality = 2, InterpolationModeBilinear = 3, InterpolationModeBicubic = 4,
       InterpolationModeNearestNeighbor = 5, InterpolationModeHighQualityBilinear = 6, InterpolationModeHighQualityBicubic = 7 };
enum { CompositingModeSourceOver = 0, CompositingModeSourceCopy = 1 };
enum { CompositingQualityDefault = 0, CompositingQualityHighSpeed = 1, CompositingQualityHighQuality = 2,
       CompositingQualityGammaCorrected = 3, CompositingQualityAssumeLinear = 4 };
enum { UnitWorld = 0, UnitDisplay = 1, UnitPixel = 2, UnitPoint = 3, UnitInch = 4, UnitDocument = 5, UnitMillimeter = 6 };
enum { MatrixOrderPrepend = 0, MatrixOrderAppend = 1 };
enum { FillModeAlternate = 0, FillModeWinding = 1 };
enum { WrapModeTile = 0, WrapModeTileFlipX = 1, WrapModeTileFlipY = 2, WrapModeTileFlipXY = 3, WrapModeClamp = 4 };
enum { LinearGradientModeHorizontal = 0, LinearGradientModeVertical = 1, LinearGradientModeForwardDiagonal = 2,
       LinearGradientModeBackwardDiagonal = 3 };
enum { DashStyleSolid = 0, DashStyleDash = 1, DashStyleDot = 2, DashStyleDashDot = 3, DashStyleDashDotDot = 4, DashStyleCustom = 5 };
enum { LineCapFlat = 0, LineCapSquare = 1, LineCapRound = 2, LineCapTriangle = 3, LineCapNoAnchor = 0x10,
       LineCapSquareAnchor = 0x11, LineCapRoundAnchor = 0x12, LineCapDiamondAnchor = 0x13, LineCapArrowAnchor = 0x14 };
enum { LineJoinMiter = 0, LineJoinBevel = 1, LineJoinRound = 2, LineJoinMiterClipped = 3 };
enum { CombineModeReplace = 0, CombineModeIntersect = 1, CombineModeUnion = 2, CombineModeXor = 3, CombineModeExclude = 4,
       CombineModeComplement = 5 };
enum { HatchStyleHorizontal = 0, HatchStyleVertical = 1, HatchStyleForwardDiagonal = 2, HatchStyleBackwardDiagonal = 3,
       HatchStyleCross = 4, HatchStyleDiagonalCross = 5 };

// ---- text ----
enum { FontStyleRegular = 0, FontStyleBold = 1, FontStyleItalic = 2, FontStyleBoldItalic = 3, FontStyleUnderline = 4,
       FontStyleStrikeout = 8 };
enum { StringAlignmentNear = 0, StringAlignmentCenter = 1, StringAlignmentFar = 2 };
enum { StringFormatFlagsDirectionRightToLeft = 0x0001, StringFormatFlagsDirectionVertical = 0x0002,
       StringFormatFlagsNoFitBlackBox = 0x0004, StringFormatFlagsDisplayFormatControl = 0x0020,
       StringFormatFlagsNoFontFallback = 0x0400, StringFormatFlagsMeasureTrailingSpaces = 0x0800,
       StringFormatFlagsNoWrap = 0x1000, StringFormatFlagsLineLimit = 0x2000, StringFormatFlagsNoClip = 0x4000 };
enum { StringTrimmingNone = 0, StringTrimmingCharacter = 1, StringTrimmingWord = 2, StringTrimmingEllipsisCharacter = 3,
       StringTrimmingEllipsisWord = 4, StringTrimmingEllipsisPath = 5 };

// ---- pictures ----
typedef INT PixelFormat;
#define PixelFormat24bppRGB 0x00021808
#define PixelFormat32bppRGB 0x00022009
#define PixelFormat32bppARGB 0x0026200A
#define PixelFormat32bppPARGB 0x000E200B
enum { ImageLockModeRead = 1, ImageLockModeWrite = 2, ImageLockModeUserInputBuf = 4 };
enum { RotateNoneFlipNone = 0, Rotate90FlipNone = 1, Rotate180FlipNone = 2, Rotate270FlipNone = 3,
       RotateNoneFlipX = 4, RotateNoneFlipY = 6 };
typedef struct {
    UINT Width, Height;
    INT Stride;
    PixelFormat PixelFormat;
    void *Scan0;
    UINT_PTR Reserved;
} BitmapData;
typedef struct { GUID Guid; ULONG NumberOfValues; ULONG Type; void *Value; } EncoderParameter;
typedef struct { UINT Count; EncoderParameter Parameter[1]; } EncoderParameters;
// the encoders GdipSaveImageToFile takes (the CLSIDs Windows gives them)
static const CLSID GdipEncoderBmp  = { 0x557cf400, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
static const CLSID GdipEncoderJpeg = { 0x557cf401, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
static const CLSID GdipEncoderGif  = { 0x557cf402, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
static const CLSID GdipEncoderTiff = { 0x557cf405, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
static const CLSID GdipEncoderPng  = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };

// ---- graphics: where it draws ----
GpStatus GdipCreateFromHDC(HDC dc, GpGraphics **g);
GpStatus GdipCreateFromHWND(HWND hwnd, GpGraphics **g);
GpStatus GdipDeleteGraphics(GpGraphics *g);
GpStatus GdipGetDC(GpGraphics *g, HDC *dc);
GpStatus GdipReleaseDC(GpGraphics *g, HDC dc);
GpStatus GdipGraphicsClear(GpGraphics *g, ARGB color);
GpStatus GdipFlush(GpGraphics *g, int intention);
GpStatus GdipSetSmoothingMode(GpGraphics *g, int mode);
GpStatus GdipGetSmoothingMode(GpGraphics *g, int *mode);
GpStatus GdipSetPixelOffsetMode(GpGraphics *g, int mode);
GpStatus GdipSetTextRenderingHint(GpGraphics *g, int mode);
GpStatus GdipSetTextContrast(GpGraphics *g, UINT contrast);
GpStatus GdipSetInterpolationMode(GpGraphics *g, int mode);
GpStatus GdipSetCompositingMode(GpGraphics *g, int mode);
GpStatus GdipSetCompositingQuality(GpGraphics *g, int quality);
GpStatus GdipSetPageUnit(GpGraphics *g, int unit);
GpStatus GdipSetPageScale(GpGraphics *g, REAL scale);
GpStatus GdipGetDpiX(GpGraphics *g, REAL *dpi);
GpStatus GdipGetDpiY(GpGraphics *g, REAL *dpi);
GpStatus GdipSaveGraphics(GpGraphics *g, UINT *state);
GpStatus GdipRestoreGraphics(GpGraphics *g, UINT state);
GpStatus GdipResetWorldTransform(GpGraphics *g);
GpStatus GdipSetWorldTransform(GpGraphics *g, GpMatrix *m);
GpStatus GdipGetWorldTransform(GpGraphics *g, GpMatrix *m);
GpStatus GdipTranslateWorldTransform(GpGraphics *g, REAL dx, REAL dy, int order);
GpStatus GdipScaleWorldTransform(GpGraphics *g, REAL sx, REAL sy, int order);
GpStatus GdipRotateWorldTransform(GpGraphics *g, REAL angle, int order);
GpStatus GdipSetClipRect(GpGraphics *g, REAL x, REAL y, REAL w, REAL h, int mode);
GpStatus GdipSetClipPath(GpGraphics *g, GpPath *path, int mode);
GpStatus GdipSetClipRegion(GpGraphics *g, GpRegion *region, int mode);
GpStatus GdipResetClip(GpGraphics *g);

// ---- lines and outlines ----
GpStatus GdipDrawLine(GpGraphics *g, GpPen *pen, REAL x1, REAL y1, REAL x2, REAL y2);
GpStatus GdipDrawLineI(GpGraphics *g, GpPen *pen, INT x1, INT y1, INT x2, INT y2);
GpStatus GdipDrawLines(GpGraphics *g, GpPen *pen, const PointF *points, INT count);
GpStatus GdipDrawRectangle(GpGraphics *g, GpPen *pen, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipDrawRectangleI(GpGraphics *g, GpPen *pen, INT x, INT y, INT w, INT h);
GpStatus GdipDrawEllipse(GpGraphics *g, GpPen *pen, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipDrawArc(GpGraphics *g, GpPen *pen, REAL x, REAL y, REAL w, REAL h, REAL start, REAL sweep);
GpStatus GdipDrawPie(GpGraphics *g, GpPen *pen, REAL x, REAL y, REAL w, REAL h, REAL start, REAL sweep);
GpStatus GdipDrawBezier(GpGraphics *g, GpPen *pen, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4);
GpStatus GdipDrawBeziers(GpGraphics *g, GpPen *pen, const PointF *points, INT count);
GpStatus GdipDrawPolygon(GpGraphics *g, GpPen *pen, const PointF *points, INT count);
GpStatus GdipDrawCurve(GpGraphics *g, GpPen *pen, const PointF *points, INT count);
GpStatus GdipDrawClosedCurve(GpGraphics *g, GpPen *pen, const PointF *points, INT count);
GpStatus GdipDrawPath(GpGraphics *g, GpPen *pen, GpPath *path);

// ---- filled shapes ----
GpStatus GdipFillRectangle(GpGraphics *g, GpBrush *brush, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipFillRectangleI(GpGraphics *g, GpBrush *brush, INT x, INT y, INT w, INT h);
GpStatus GdipFillEllipse(GpGraphics *g, GpBrush *brush, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipFillPie(GpGraphics *g, GpBrush *brush, REAL x, REAL y, REAL w, REAL h, REAL start, REAL sweep);
GpStatus GdipFillPolygon(GpGraphics *g, GpBrush *brush, const PointF *points, INT count, int fill_mode);
GpStatus GdipFillClosedCurve(GpGraphics *g, GpBrush *brush, const PointF *points, INT count);
GpStatus GdipFillPath(GpGraphics *g, GpBrush *brush, GpPath *path);
GpStatus GdipFillRegion(GpGraphics *g, GpBrush *brush, GpRegion *region);

// ---- brushes ----
GpStatus GdipCreateSolidFill(ARGB color, GpSolidFill **brush);
GpStatus GdipSetSolidFillColor(GpSolidFill *brush, ARGB color);
GpStatus GdipGetSolidFillColor(GpSolidFill *brush, ARGB *color);
GpStatus GdipCreateLineBrush(const PointF *p1, const PointF *p2, ARGB c1, ARGB c2, int wrap, GpLineGradient **brush);
GpStatus GdipCreateLineBrushFromRect(const RectF *r, ARGB c1, ARGB c2, int mode, int wrap, GpLineGradient **brush);
GpStatus GdipCreateLineBrushFromRectWithAngle(const RectF *r, ARGB c1, ARGB c2, REAL angle, BOOL scalable, int wrap,
                                              GpLineGradient **brush);
GpStatus GdipSetLinePresetBlend(GpLineGradient *brush, const ARGB *colors, const REAL *positions, INT count);
GpStatus GdipSetLineGammaCorrection(GpLineGradient *brush, BOOL on);
GpStatus GdipCreatePathGradient(const PointF *points, INT count, int wrap, GpPathGradient **brush);
GpStatus GdipCreatePathGradientFromPath(const GpPath *path, GpPathGradient **brush);
GpStatus GdipSetPathGradientCenterColor(GpPathGradient *brush, ARGB color);
GpStatus GdipSetPathGradientSurroundColorsWithCount(GpPathGradient *brush, const ARGB *colors, INT *count);
GpStatus GdipSetPathGradientCenterPoint(GpPathGradient *brush, const PointF *point);
GpStatus GdipCreateHatchBrush(int style, ARGB fore, ARGB back, GpHatch **brush);
GpStatus GdipCreateTexture(GpImage *image, int wrap, GpTexture **brush);
GpStatus GdipDeleteBrush(GpBrush *brush);

// ---- pens ----
GpStatus GdipCreatePen1(ARGB color, REAL width, int unit, GpPen **pen);
GpStatus GdipCreatePen2(GpBrush *brush, REAL width, int unit, GpPen **pen);
GpStatus GdipDeletePen(GpPen *pen);
GpStatus GdipSetPenColor(GpPen *pen, ARGB color);
GpStatus GdipSetPenWidth(GpPen *pen, REAL width);
GpStatus GdipSetPenDashStyle(GpPen *pen, int style);
GpStatus GdipSetPenDashArray(GpPen *pen, const REAL *dashes, INT count);
GpStatus GdipSetPenStartCap(GpPen *pen, int cap);
GpStatus GdipSetPenEndCap(GpPen *pen, int cap);
GpStatus GdipSetPenLineCap197819(GpPen *pen, int start, int end, int dash_cap);
GpStatus GdipSetPenLineJoin(GpPen *pen, int join);

// ---- paths ----
GpStatus GdipCreatePath(int fill_mode, GpPath **path);
GpStatus GdipDeletePath(GpPath *path);
GpStatus GdipResetPath(GpPath *path);
GpStatus GdipStartPathFigure(GpPath *path);
GpStatus GdipClosePathFigure(GpPath *path);
GpStatus GdipAddPathLine(GpPath *path, REAL x1, REAL y1, REAL x2, REAL y2);
GpStatus GdipAddPathLine2(GpPath *path, const PointF *points, INT count);
GpStatus GdipAddPathArc(GpPath *path, REAL x, REAL y, REAL w, REAL h, REAL start, REAL sweep);
GpStatus GdipAddPathBezier(GpPath *path, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4);
GpStatus GdipAddPathRectangle(GpPath *path, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipAddPathEllipse(GpPath *path, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipAddPathPie(GpPath *path, REAL x, REAL y, REAL w, REAL h, REAL start, REAL sweep);
GpStatus GdipAddPathPolygon(GpPath *path, const PointF *points, INT count);
GpStatus GdipAddPathCurve(GpPath *path, const PointF *points, INT count);
GpStatus GdipAddPathClosedCurve(GpPath *path, const PointF *points, INT count);
GpStatus GdipAddPathString(GpPath *path, const WCHAR *text, INT len, const GpFontFamily *family, INT style, REAL size,
                           const RectF *layout, const GpStringFormat *format);
GpStatus GdipTransformPath(GpPath *path, GpMatrix *m);
GpStatus GdipGetPathWorldBounds(GpPath *path, RectF *bounds, const GpMatrix *m, const GpPen *pen);
GpStatus GdipIsVisiblePathPoint(GpPath *path, REAL x, REAL y, GpGraphics *g, BOOL *result);

// ---- regions ----
GpStatus GdipCreateRegion(GpRegion **region);
GpStatus GdipCreateRegionRect(const RectF *r, GpRegion **region);
GpStatus GdipCreateRegionPath(GpPath *path, GpRegion **region);
GpStatus GdipCombineRegionRect(GpRegion *region, const RectF *r, int mode);
GpStatus GdipCombineRegionPath(GpRegion *region, GpPath *path, int mode);
GpStatus GdipIsVisibleRegionPoint(GpRegion *region, REAL x, REAL y, GpGraphics *g, BOOL *result);
GpStatus GdipGetRegionHRgn(GpRegion *region, GpGraphics *g, HRGN *rgn);
GpStatus GdipDeleteRegion(GpRegion *region);

// ---- transforms ----
GpStatus GdipCreateMatrix(GpMatrix **m);
GpStatus GdipCreateMatrix2(REAL m11, REAL m12, REAL m21, REAL m22, REAL dx, REAL dy, GpMatrix **m);
GpStatus GdipDeleteMatrix(GpMatrix *m);
GpStatus GdipSetMatrixElements(GpMatrix *m, REAL m11, REAL m12, REAL m21, REAL m22, REAL dx, REAL dy);
GpStatus GdipTranslateMatrix(GpMatrix *m, REAL dx, REAL dy, int order);
GpStatus GdipScaleMatrix(GpMatrix *m, REAL sx, REAL sy, int order);
GpStatus GdipRotateMatrix(GpMatrix *m, REAL angle, int order);
GpStatus GdipInvertMatrix(GpMatrix *m);
GpStatus GdipTransformMatrixPoints(GpMatrix *m, PointF *points, INT count);

// ---- fonts and text ----
GpStatus GdipCreateFontFamilyFromName(const WCHAR *name, GpFontCollection *collection, GpFontFamily **family);
GpStatus GdipGetGenericFontFamilySansSerif(GpFontFamily **family);
GpStatus GdipGetGenericFontFamilySerif(GpFontFamily **family);
GpStatus GdipGetGenericFontFamilyMonospace(GpFontFamily **family);
GpStatus GdipDeleteFontFamily(GpFontFamily *family);
GpStatus GdipCreateFont(const GpFontFamily *family, REAL size, INT style, int unit, GpFont **font);
GpStatus GdipCreateFontFromDC(HDC dc, GpFont **font);
GpStatus GdipCreateFontFromLogfontW(HDC dc, const LOGFONTW *lf, GpFont **font);
GpStatus GdipDeleteFont(GpFont *font);
GpStatus GdipGetFontHeight(const GpFont *font, const GpGraphics *g, REAL *height);
GpStatus GdipCreateStringFormat(INT flags, LANGID language, GpStringFormat **format);
GpStatus GdipStringFormatGetGenericDefault(GpStringFormat **format);
GpStatus GdipStringFormatGetGenericTypographic(GpStringFormat **format);
GpStatus GdipDeleteStringFormat(GpStringFormat *format);
GpStatus GdipSetStringFormatAlign(GpStringFormat *format, int align);
GpStatus GdipSetStringFormatLineAlign(GpStringFormat *format, int align);
GpStatus GdipSetStringFormatFlags(GpStringFormat *format, INT flags);
GpStatus GdipSetStringFormatTrimming(GpStringFormat *format, int trimming);
GpStatus GdipDrawString(GpGraphics *g, const WCHAR *text, INT len, const GpFont *font, const RectF *layout,
                        const GpStringFormat *format, const GpBrush *brush);
GpStatus GdipMeasureString(GpGraphics *g, const WCHAR *text, INT len, const GpFont *font, const RectF *layout,
                           const GpStringFormat *format, RectF *bounds, INT *chars_fitted, INT *lines_filled);

// ---- pictures: load, draw, save, pixels ----
GpStatus GdipLoadImageFromFile(const WCHAR *file, GpImage **image);
GpStatus GdipCreateBitmapFromFile(const WCHAR *file, GpBitmap **bitmap);
GpStatus GdipCreateBitmapFromScan0(INT w, INT h, INT stride, PixelFormat format, BYTE *scan0, GpBitmap **bitmap);
GpStatus GdipCreateBitmapFromHBITMAP(HBITMAP hbm, HPALETTE pal, GpBitmap **bitmap);
GpStatus GdipCreateBitmapFromHICON(HICON icon, GpBitmap **bitmap);
GpStatus GdipCreateBitmapFromGraphics(INT w, INT h, GpGraphics *target, GpBitmap **bitmap);
GpStatus GdipCreateHBITMAPFromBitmap(GpBitmap *bitmap, HBITMAP *hbm, ARGB background);
GpStatus GdipCreateHICONFromBitmap(GpBitmap *bitmap, HICON *icon);
GpStatus GdipCloneImage(GpImage *image, GpImage **clone);
GpStatus GdipDisposeImage(GpImage *image);
GpStatus GdipGetImageWidth(GpImage *image, UINT *w);
GpStatus GdipGetImageHeight(GpImage *image, UINT *h);
GpStatus GdipGetImagePixelFormat(GpImage *image, PixelFormat *format);
GpStatus GdipGetImageGraphicsContext(GpImage *image, GpGraphics **g);
GpStatus GdipImageRotateFlip(GpImage *image, int how);
GpStatus GdipSaveImageToFile(GpImage *image, const WCHAR *file, const CLSID *encoder, const EncoderParameters *params);
GpStatus GdipDrawImage(GpGraphics *g, GpImage *image, REAL x, REAL y);
GpStatus GdipDrawImageI(GpGraphics *g, GpImage *image, INT x, INT y);
GpStatus GdipDrawImageRect(GpGraphics *g, GpImage *image, REAL x, REAL y, REAL w, REAL h);
GpStatus GdipDrawImageRectI(GpGraphics *g, GpImage *image, INT x, INT y, INT w, INT h);
GpStatus GdipDrawImageRectRect(GpGraphics *g, GpImage *image, REAL dx, REAL dy, REAL dw, REAL dh,
                               REAL sx, REAL sy, REAL sw, REAL sh, int unit, const GpImageAttributes *attrs,
                               void *callback, void *callback_data);
GpStatus GdipBitmapGetPixel(GpBitmap *bitmap, INT x, INT y, ARGB *color);
GpStatus GdipBitmapSetPixel(GpBitmap *bitmap, INT x, INT y, ARGB color);
GpStatus GdipBitmapLockBits(GpBitmap *bitmap, const Rect *r, UINT mode, PixelFormat format, BitmapData *data);
GpStatus GdipBitmapUnlockBits(GpBitmap *bitmap, BitmapData *data);
GpStatus GdipCreateImageAttributes(GpImageAttributes **attrs);
GpStatus GdipDisposeImageAttributes(GpImageAttributes *attrs);
// type: 1 = ColorAdjustTypeBitmap; matrix: 5x5 (rows R, G, B, A, shift); flags 0 = ColorMatrixFlagsDefault
GpStatus GdipSetImageAttributesColorMatrix(GpImageAttributes *attrs, int type, BOOL enable, const REAL *matrix,
                                           const REAL *gray, int flags);

// ---- more: gradients, textures, paths, pens, fonts (used by lib\wui) ----
GpStatus GdipSetLineTransform(GpLineGradient *brush, const GpMatrix *matrix);
GpStatus GdipSetPathGradientPresetBlend(GpPathGradient *brush, const ARGB *colors, const REAL *positions, INT count);
GpStatus GdipSetTextureTransform(GpTexture *brush, const GpMatrix *matrix);
GpStatus GdipScaleTextureTransform(GpTexture *brush, REAL sx, REAL sy, int order);
GpStatus GdipMultiplyWorldTransform(GpGraphics *g, GpMatrix *matrix, int order);
GpStatus GdipGetMatrixElements(const GpMatrix *matrix, REAL *elements);
GpStatus GdipClonePath(GpPath *path, GpPath **clone);
GpStatus GdipFlattenPath(GpPath *path, GpMatrix *matrix, REAL flatness);
GpStatus GdipWidenPath(GpPath *path, GpPen *pen, GpMatrix *matrix, REAL flatness);
GpStatus GdipSetPathFillMode(GpPath *path, int mode);
GpStatus GdipGetPenWidth(GpPen *pen, REAL *width);
GpStatus GdipClonePen(GpPen *pen, GpPen **clone);
GpStatus GdipGetPenBrushFill(GpPen *pen, GpBrush **brush);
GpStatus GdipCloneStringFormat(const GpStringFormat *format, GpStringFormat **clone);
GpStatus GdipGetCellAscent(const GpFontFamily *family, INT style, UINT16 *ascent);
GpStatus GdipGetEmHeight(const GpFontFamily *family, INT style, UINT16 *em);
GpStatus GdipGetLineSpacing(const GpFontFamily *family, INT style, UINT16 *spacing);
