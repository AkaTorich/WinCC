#pragma once
// GDI+ flat API for C. The SDK's gdiplus.h is C++ only, so the functions the library
// uses are declared here with their exact signatures (gdiplus.lib, __stdcall).
#include <windows.h>

typedef int   GpStatus;
typedef float REAL;
typedef DWORD ARGB;
typedef UINT  GraphicsState;

typedef struct { REAL X, Y, Width, Height; } GpRectF;
typedef struct { REAL X, Y; } GpPointF;
typedef struct { INT X, Y; } GpPoint;
typedef struct { REAL m[5][5]; } GpColorMatrix;

typedef struct GpGraphics GpGraphics;
typedef struct GpImage GpImage;
typedef struct GpPen GpPen;
typedef struct GpBrush GpBrush;
typedef struct GpPath GpPath;
typedef struct GpFont GpFont;
typedef struct GpFontFamily GpFontFamily;
typedef struct GpFontCollection GpFontCollection;
typedef struct GpStringFormat GpStringFormat;
typedef struct GpImageAttributes GpImageAttributes;

typedef struct {
    UINT32 GdiplusVersion;
    void  *DebugEventCallback;
    BOOL   SuppressBackgroundThread;
    BOOL   SuppressExternalCodecs;
} GdiplusStartupInput;

#define GP_OK 0

// enums (values from GdiplusEnums.h / GdiplusPixelFormats.h)
#define GP_SMOOTH_ANTIALIAS        4
#define GP_TEXT_ANTIALIAS_GRIDFIT  3
#define GP_TEXT_ANTIALIAS          4
#define GP_PIXOFF_HALF             4
#define GP_INTERP_HQ_BICUBIC       7
#define GP_ORDER_PREPEND           0
#define GP_COMBINE_REPLACE         0
#define GP_COMBINE_INTERSECT       1
#define GP_FILL_ALTERNATE          0
#define GP_FILL_WINDING            1
#define GP_LGM_HORIZONTAL          0
#define GP_LGM_VERTICAL            1
#define GP_LGM_FDIAGONAL           2
#define GP_WRAP_TILE               0
#define GP_UNIT_PIXEL              2
#define GP_CAP_FLAT                0
#define GP_CAP_ROUND               2
#define GP_JOIN_ROUND              2
#define GP_PEN_CENTER              0
#define GP_FONT_REGULAR            0
#define GP_FONT_BOLD               1
#define GP_SF_MEASURE_TRAILING     0x0800
#define GP_SF_NOWRAP               0x1000
#define GP_ALIGN_NEAR              0
#define GP_ALIGN_CENTER            1
#define GP_ALIGN_FAR               2
#define GP_TRIM_NONE               0
#define GP_TRIM_ELLIPSIS_CHAR      3
#define GP_FLUSH                   0
#define GP_COLORADJUST_DEFAULT     0
#define GP_COLORMATRIX_DEFAULT     0
#define GP_PIXFMT_32BPP_PARGB      0x000E200B

#define WINGDIPAPI __stdcall

GpStatus WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, void *output);
void     WINAPI GdiplusShutdown(ULONG_PTR token);

// graphics
GpStatus WINGDIPAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics);
GpStatus WINGDIPAPI GdipGetImageGraphicsContext(GpImage *image, GpGraphics **graphics);
GpStatus WINGDIPAPI GdipDeleteGraphics(GpGraphics *graphics);
GpStatus WINGDIPAPI GdipSetSmoothingMode(GpGraphics *graphics, int mode);
GpStatus WINGDIPAPI GdipSetTextRenderingHint(GpGraphics *graphics, int mode);
GpStatus WINGDIPAPI GdipSetPixelOffsetMode(GpGraphics *graphics, int mode);
GpStatus WINGDIPAPI GdipSetInterpolationMode(GpGraphics *graphics, int mode);
GpStatus WINGDIPAPI GdipGraphicsClear(GpGraphics *graphics, ARGB color);
GpStatus WINGDIPAPI GdipFlush(GpGraphics *graphics, int intention);
GpStatus WINGDIPAPI GdipSaveGraphics(GpGraphics *graphics, GraphicsState *state);
GpStatus WINGDIPAPI GdipRestoreGraphics(GpGraphics *graphics, GraphicsState state);
GpStatus WINGDIPAPI GdipTranslateWorldTransform(GpGraphics *graphics, REAL dx, REAL dy, int order);
GpStatus WINGDIPAPI GdipRotateWorldTransform(GpGraphics *graphics, REAL angle, int order);
GpStatus WINGDIPAPI GdipScaleWorldTransform(GpGraphics *graphics, REAL sx, REAL sy, int order);
GpStatus WINGDIPAPI GdipResetWorldTransform(GpGraphics *graphics);
GpStatus WINGDIPAPI GdipSetClipRectI(GpGraphics *graphics, INT x, INT y, INT width, INT height, int combineMode);
GpStatus WINGDIPAPI GdipResetClip(GpGraphics *graphics);

// drawing
GpStatus WINGDIPAPI GdipFillRectangle(GpGraphics *graphics, GpBrush *brush, REAL x, REAL y, REAL width, REAL height);
GpStatus WINGDIPAPI GdipFillPath(GpGraphics *graphics, GpBrush *brush, GpPath *path);
GpStatus WINGDIPAPI GdipDrawPath(GpGraphics *graphics, GpPen *pen, GpPath *path);
GpStatus WINGDIPAPI GdipFillEllipse(GpGraphics *graphics, GpBrush *brush, REAL x, REAL y, REAL width, REAL height);
GpStatus WINGDIPAPI GdipDrawEllipse(GpGraphics *graphics, GpPen *pen, REAL x, REAL y, REAL width, REAL height);
GpStatus WINGDIPAPI GdipDrawLine(GpGraphics *graphics, GpPen *pen, REAL x1, REAL y1, REAL x2, REAL y2);
GpStatus WINGDIPAPI GdipDrawLinesI(GpGraphics *graphics, GpPen *pen, const GpPoint *points, INT count);
GpStatus WINGDIPAPI GdipDrawArc(GpGraphics *graphics, GpPen *pen, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle);
GpStatus WINGDIPAPI GdipDrawCurve2(GpGraphics *graphics, GpPen *pen, const GpPointF *points, INT count, REAL tension);
GpStatus WINGDIPAPI GdipFillClosedCurve(GpGraphics *graphics, GpBrush *brush, const GpPointF *points, INT count);
GpStatus WINGDIPAPI GdipDrawImageRectI(GpGraphics *graphics, GpImage *image, INT x, INT y, INT width, INT height);
GpStatus WINGDIPAPI GdipDrawImageRectRect(GpGraphics *graphics, GpImage *image, REAL dstx, REAL dsty, REAL dstwidth, REAL dstheight,
                                          REAL srcx, REAL srcy, REAL srcwidth, REAL srcheight, int srcUnit,
                                          const GpImageAttributes *imageAttributes, void *callback, void *callbackData);
GpStatus WINGDIPAPI GdipDrawString(GpGraphics *graphics, const WCHAR *string, INT length, const GpFont *font,
                                   const GpRectF *layoutRect, const GpStringFormat *stringFormat, const GpBrush *brush);
GpStatus WINGDIPAPI GdipMeasureString(GpGraphics *graphics, const WCHAR *string, INT length, const GpFont *font,
                                      const GpRectF *layoutRect, const GpStringFormat *stringFormat, GpRectF *boundingBox,
                                      INT *codepointsFitted, INT *linesFilled);

// brushes and pens
GpStatus WINGDIPAPI GdipCreateSolidFill(ARGB color, GpBrush **brush);
GpStatus WINGDIPAPI GdipCreateLineBrushFromRect(const GpRectF *rect, ARGB color1, ARGB color2, int mode, int wrapMode, GpBrush **lineGradient);
GpStatus WINGDIPAPI GdipDeleteBrush(GpBrush *brush);
GpStatus WINGDIPAPI GdipCreatePen1(ARGB color, REAL width, int unit, GpPen **pen);
GpStatus WINGDIPAPI GdipDeletePen(GpPen *pen);
GpStatus WINGDIPAPI GdipSetPenStartCap(GpPen *pen, int startCap);
GpStatus WINGDIPAPI GdipSetPenEndCap(GpPen *pen, int endCap);
GpStatus WINGDIPAPI GdipSetPenLineJoin(GpPen *pen, int lineJoin);
GpStatus WINGDIPAPI GdipSetPenMode(GpPen *pen, int penMode);

// paths
GpStatus WINGDIPAPI GdipCreatePath(int brushMode, GpPath **path);
GpStatus WINGDIPAPI GdipDeletePath(GpPath *path);
GpStatus WINGDIPAPI GdipResetPath(GpPath *path);
GpStatus WINGDIPAPI GdipAddPathArc(GpPath *path, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle);
GpStatus WINGDIPAPI GdipAddPathRectangle(GpPath *path, REAL x, REAL y, REAL width, REAL height);
GpStatus WINGDIPAPI GdipAddPathPolygon(GpPath *path, const GpPointF *points, INT count);
GpStatus WINGDIPAPI GdipClosePathFigure(GpPath *path);

// fonts
GpStatus WINGDIPAPI GdipCreateFontFamilyFromName(const WCHAR *name, GpFontCollection *fontCollection, GpFontFamily **fontFamily);
GpStatus WINGDIPAPI GdipDeleteFontFamily(GpFontFamily *fontFamily);
GpStatus WINGDIPAPI GdipCreateFont(const GpFontFamily *fontFamily, REAL emSize, INT style, int unit, GpFont **font);
GpStatus WINGDIPAPI GdipDeleteFont(GpFont *font);
GpStatus WINGDIPAPI GdipNewPrivateFontCollection(GpFontCollection **fontCollection);
GpStatus WINGDIPAPI GdipDeletePrivateFontCollection(GpFontCollection **fontCollection);
GpStatus WINGDIPAPI GdipPrivateAddFontFile(GpFontCollection *fontCollection, const WCHAR *filename);
GpStatus WINGDIPAPI GdipPrivateAddMemoryFont(GpFontCollection *fontCollection, const void *memory, INT length);
GpStatus WINGDIPAPI GdipGetFontCollectionFamilyCount(GpFontCollection *fontCollection, INT *numFound);
GpStatus WINGDIPAPI GdipGetFontCollectionFamilyList(GpFontCollection *fontCollection, INT numSought, GpFontFamily *gpfamilies[], INT *numFound);
GpStatus WINGDIPAPI GdipGetFamilyName(const GpFontFamily *family, WCHAR name[LF_FACESIZE], LANGID language);

// string formats
GpStatus WINGDIPAPI GdipStringFormatGetGenericTypographic(GpStringFormat **format);
GpStatus WINGDIPAPI GdipCloneStringFormat(const GpStringFormat *format, GpStringFormat **newFormat);
GpStatus WINGDIPAPI GdipDeleteStringFormat(GpStringFormat *format);
GpStatus WINGDIPAPI GdipSetStringFormatFlags(GpStringFormat *format, INT flags);
GpStatus WINGDIPAPI GdipGetStringFormatFlags(const GpStringFormat *format, INT *flags);
GpStatus WINGDIPAPI GdipSetStringFormatAlign(GpStringFormat *format, int align);
GpStatus WINGDIPAPI GdipSetStringFormatLineAlign(GpStringFormat *format, int align);
GpStatus WINGDIPAPI GdipSetStringFormatTrimming(GpStringFormat *format, int trimming);

// images
GpStatus WINGDIPAPI GdipCreateBitmapFromScan0(INT width, INT height, INT stride, INT format, BYTE *scan0, GpImage **bitmap);
GpStatus WINGDIPAPI GdipCreateBitmapFromFile(const WCHAR *filename, GpImage **bitmap);
GpStatus WINGDIPAPI GdipDisposeImage(GpImage *image);
GpStatus WINGDIPAPI GdipGetImageWidth(GpImage *image, UINT *width);
GpStatus WINGDIPAPI GdipGetImageHeight(GpImage *image, UINT *height);
GpStatus WINGDIPAPI GdipCreateImageAttributes(GpImageAttributes **imageattr);
GpStatus WINGDIPAPI GdipDisposeImageAttributes(GpImageAttributes *imageattr);
GpStatus WINGDIPAPI GdipSetImageAttributesColorMatrix(GpImageAttributes *imageattr, int type, BOOL enableFlag,
                                                      const GpColorMatrix *colorMatrix, const GpColorMatrix *grayMatrix, int flags);
