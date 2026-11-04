// windows.h: gdi32 -- drawing: pens, brushes, fonts, text, bitmaps.
#pragma once

#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH 2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define HOLLOW_BRUSH NULL_BRUSH
#define WHITE_PEN 6
#define BLACK_PEN 7
#define NULL_PEN 8
#define OEM_FIXED_FONT 10
#define ANSI_FIXED_FONT 11
#define ANSI_VAR_FONT 12
#define SYSTEM_FONT 13
#define DEVICE_DEFAULT_FONT 14
#define DEFAULT_PALETTE 15
#define SYSTEM_FIXED_FONT 16
#define DEFAULT_GUI_FONT 17
#define DC_BRUSH 18
#define DC_PEN 19

#define TRANSPARENT 1
#define OPAQUE 2
#define R2_BLACK 1
#define R2_NOT 6
#define R2_XORPEN 7
#define R2_COPYPEN 13
#define R2_WHITE 16
#define SRCCOPY 0x00CC0020
#define SRCPAINT 0x00EE0086
#define SRCAND 0x008800C6
#define SRCINVERT 0x00660046
#define SRCERASE 0x00440328
#define NOTSRCCOPY 0x00330008
#define MERGECOPY 0x00C000CA
#define PATCOPY 0x00F00021
#define PATINVERT 0x005A0049
#define DSTINVERT 0x00550009
#define BLACKNESS 0x00000042
#define WHITENESS 0x00FF0062
#define CAPTUREBLT 0x40000000

#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_DASHDOT 3
#define PS_DASHDOTDOT 4
#define PS_NULL 5
#define PS_INSIDEFRAME 6
#define PS_ENDCAP_ROUND 0x00000000
#define PS_ENDCAP_SQUARE 0x00000100
#define PS_ENDCAP_FLAT 0x00000200
#define PS_JOIN_ROUND 0x00000000
#define PS_JOIN_BEVEL 0x00001000
#define PS_JOIN_MITER 0x00002000
#define PS_GEOMETRIC 0x00010000
#define HS_HORIZONTAL 0
#define HS_VERTICAL 1
#define HS_FDIAGONAL 2
#define HS_BDIAGONAL 3
#define HS_CROSS 4
#define HS_DIAGCROSS 5
#define BS_SOLID 0
#define BS_NULL 1
#define BS_HATCHED 2

#define FW_DONTCARE 0
#define FW_THIN 100
#define FW_EXTRALIGHT 200
#define FW_LIGHT 300
#define FW_NORMAL 400
#define FW_REGULAR 400
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_BOLD 700
#define FW_EXTRABOLD 800
#define FW_HEAVY 900
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SYMBOL_CHARSET 2
#define RUSSIAN_CHARSET 204
#define OEM_CHARSET 255
#define OUT_DEFAULT_PRECIS 0
#define OUT_TT_PRECIS 4
#define OUT_TT_ONLY_PRECIS 7
#define OUT_OUTLINE_PRECIS 8
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define CLEARTYPE_QUALITY 5
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE (0 << 4)
#define FF_ROMAN (1 << 4)
#define FF_SWISS (2 << 4)
#define FF_MODERN (3 << 4)
#define FF_SCRIPT (4 << 4)
#define FF_DECORATIVE (5 << 4)
#define LF_FACESIZE 32

#define TA_NOUPDATECP 0
#define TA_UPDATECP 1
#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_CENTER 6
#define TA_TOP 0
#define TA_BOTTOM 8
#define TA_BASELINE 24
#define ETO_OPAQUE 0x0002
#define ETO_CLIPPED 0x0004

#define HORZSIZE 4
#define VERTSIZE 6
#define HORZRES 8
#define VERTRES 10
#define BITSPIXEL 12
#define PLANES 14
#define LOGPIXELSX 88
#define LOGPIXELSY 90
#define DESKTOPHORZRES 118
#define DESKTOPVERTRES 117
#define BLACKONWHITE 1
#define WHITEONBLACK 2
#define COLORONCOLOR 3
#define HALFTONE 4
#define STRETCH_DELETESCANS COLORONCOLOR
#define STRETCH_HALFTONE HALFTONE

#define BI_RGB 0L
#define BI_RLE8 1L
#define BI_RLE4 2L
#define BI_BITFIELDS 3L
#define BI_JPEG 4L
#define BI_PNG 5L
#define DIB_RGB_COLORS 0
#define DIB_PAL_COLORS 1
#define RGN_AND 1
#define RGN_OR 2
#define RGN_XOR 3
#define RGN_DIFF 4
#define RGN_COPY 5
#define ALTERNATE 1
#define WINDING 2
#define CLR_INVALID 0xFFFFFFFF
#define GDI_ERROR 0xFFFFFFFFL
#define HGDI_ERROR ((HANDLE)-1)

typedef struct tagLOGFONTA {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;
typedef struct tagLOGFONTW {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    WCHAR lfFaceName[LF_FACESIZE];
} LOGFONTW, *PLOGFONTW, *LPLOGFONTW;

typedef struct tagTEXTMETRICA {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth,
         tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    BYTE tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
    BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRICA, *PTEXTMETRICA, *LPTEXTMETRICA;
typedef struct tagTEXTMETRICW {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth,
         tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    WCHAR tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
    BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRICW, *PTEXTMETRICW, *LPTEXTMETRICW;

typedef struct tagLOGBRUSH { UINT lbStyle; COLORREF lbColor; ULONG_PTR lbHatch; } LOGBRUSH, *PLOGBRUSH;
typedef struct tagLOGPEN { UINT lopnStyle; POINT lopnWidth; COLORREF lopnColor; } LOGPEN, *PLOGPEN;
typedef struct tagBITMAP {
    LONG bmType, bmWidth, bmHeight, bmWidthBytes;
    WORD bmPlanes, bmBitsPixel;
    LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;
typedef struct tagRGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD, *LPRGBQUAD;
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount; DWORD biCompression, biSizeImage;
    LONG biXPelsPerMeter, biYPelsPerMeter; DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
    WORD bfType; DWORD bfSize; WORD bfReserved1, bfReserved2; DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#pragma pack(pop)
typedef struct _BLENDFUNCTION { BYTE BlendOp, BlendFlags, SourceConstantAlpha, AlphaFormat; } BLENDFUNCTION;
#define AC_SRC_OVER 0x00
#define AC_SRC_ALPHA 0x01
// окно с прозрачностью по точкам (WS_EX_LAYERED), функция из user32
BOOL UpdateLayeredWindow(HWND hwnd, HDC dst, POINT *pos, SIZE *size, HDC src, POINT *src_pos, COLORREF key,
                         BLENDFUNCTION *blend, DWORD flags);

HGDIOBJ GetStockObject(int index);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ obj);
BOOL DeleteObject(HGDIOBJ obj);
int GetObjectA(HANDLE obj, int size, LPVOID out);
int GetObjectW(HANDLE obj, int size, LPVOID out);
HBRUSH CreateSolidBrush(COLORREF color);
HBRUSH CreateHatchBrush(int style, COLORREF color);
HBRUSH CreatePatternBrush(HBITMAP bmp);
HBRUSH CreateBrushIndirect(const LOGBRUSH *lb);
HPEN CreatePen(int style, int width, COLORREF color);
HPEN ExtCreatePen(DWORD style, DWORD width, const LOGBRUSH *lb, DWORD n, const DWORD *dashes);
HFONT CreateFontA(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike,
                  DWORD charset, DWORD out_prec, DWORD clip_prec, DWORD quality, DWORD pitch, LPCSTR face);
HFONT CreateFontW(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike,
                  DWORD charset, DWORD out_prec, DWORD clip_prec, DWORD quality, DWORD pitch, LPCWSTR face);
HFONT CreateFontIndirectA(const LOGFONTA *lf);
HFONT CreateFontIndirectW(const LOGFONTW *lf);
COLORREF SetTextColor(HDC dc, COLORREF color);
COLORREF GetTextColor(HDC dc);
COLORREF SetBkColor(HDC dc, COLORREF color);
COLORREF GetBkColor(HDC dc);
int SetBkMode(HDC dc, int mode);
COLORREF SetDCBrushColor(HDC dc, COLORREF color);
COLORREF SetDCPenColor(HDC dc, COLORREF color);
int SetROP2(HDC dc, int mode);
UINT SetTextAlign(HDC dc, UINT align);
BOOL TextOutA(HDC dc, int x, int y, LPCSTR text, int n);
BOOL TextOutW(HDC dc, int x, int y, LPCWSTR text, int n);
BOOL ExtTextOutA(HDC dc, int x, int y, UINT opts, const RECT *r, LPCSTR text, UINT n, const INT *dx);
BOOL ExtTextOutW(HDC dc, int x, int y, UINT opts, const RECT *r, LPCWSTR text, UINT n, const INT *dx);
BOOL GetTextExtentPoint32A(HDC dc, LPCSTR text, int n, LPSIZE size);
BOOL GetTextExtentPoint32W(HDC dc, LPCWSTR text, int n, LPSIZE size);
BOOL GetTextMetricsA(HDC dc, LPTEXTMETRICA tm);
BOOL GetTextMetricsW(HDC dc, LPTEXTMETRICW tm);
BOOL MoveToEx(HDC dc, int x, int y, LPPOINT old);
BOOL LineTo(HDC dc, int x, int y);
BOOL Polyline(HDC dc, const POINT *pts, int n);
BOOL PolylineTo(HDC dc, const POINT *pts, DWORD n);
BOOL Polygon(HDC dc, const POINT *pts, int n);
BOOL PolyBezier(HDC dc, const POINT *pts, DWORD n);
BOOL Rectangle(HDC dc, int left, int top, int right, int bottom);
BOOL RoundRect(HDC dc, int left, int top, int right, int bottom, int w, int h);
BOOL Ellipse(HDC dc, int left, int top, int right, int bottom);
BOOL Arc(HDC dc, int l, int t, int r, int b, int x1, int y1, int x2, int y2);
BOOL Pie(HDC dc, int l, int t, int r, int b, int x1, int y1, int x2, int y2);
BOOL Chord(HDC dc, int l, int t, int r, int b, int x1, int y1, int x2, int y2);
COLORREF SetPixel(HDC dc, int x, int y, COLORREF color);
BOOL SetPixelV(HDC dc, int x, int y, COLORREF color);
COLORREF GetPixel(HDC dc, int x, int y);
BOOL FloodFill(HDC dc, int x, int y, COLORREF border);
BOOL ExtFloodFill(HDC dc, int x, int y, COLORREF color, UINT type);
BOOL PatBlt(HDC dc, int x, int y, int w, int h, DWORD rop);
BOOL BitBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD rop);
BOOL StretchBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, int sw, int sh, DWORD rop);
BOOL MaskBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, HBITMAP mask, int mx, int my, DWORD rop);
int SetStretchBltMode(HDC dc, int mode);
BOOL SetBrushOrgEx(HDC dc, int x, int y, LPPOINT old);
HDC CreateCompatibleDC(HDC dc);
HDC CreateDCA(LPCSTR driver, LPCSTR device, LPCSTR port, const void *mode);
HDC CreateDCW(LPCWSTR driver, LPCWSTR device, LPCWSTR port, const void *mode);
BOOL DeleteDC(HDC dc);
int SaveDC(HDC dc);
BOOL RestoreDC(HDC dc, int saved);
int GetDeviceCaps(HDC dc, int index);
HBITMAP CreateCompatibleBitmap(HDC dc, int w, int h);
HBITMAP CreateBitmap(int w, int h, UINT planes, UINT bits, const void *data);
HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO *bi, UINT usage, void **bits, HANDLE section, DWORD offset);
HBITMAP CreateDIBitmap(HDC dc, const BITMAPINFOHEADER *bih, DWORD init, const void *bits, const BITMAPINFO *bi, UINT usage);
int StretchDIBits(HDC dc, int x, int y, int w, int h, int sx, int sy, int sw, int sh, const void *bits,
                  const BITMAPINFO *bi, UINT usage, DWORD rop);
int SetDIBitsToDevice(HDC dc, int x, int y, DWORD w, DWORD h, int sx, int sy, UINT start, UINT lines,
                      const void *bits, const BITMAPINFO *bi, UINT usage);
int GetDIBits(HDC dc, HBITMAP bmp, UINT start, UINT lines, LPVOID bits, LPBITMAPINFO bi, UINT usage);
int SetDIBits(HDC dc, HBITMAP bmp, UINT start, UINT lines, const void *bits, const BITMAPINFO *bi, UINT usage);
HRGN CreateRectRgn(int left, int top, int right, int bottom);
HRGN CreateRoundRectRgn(int left, int top, int right, int bottom, int w, int h);
HRGN CreateEllipticRgn(int left, int top, int right, int bottom);
HRGN CreatePolygonRgn(const POINT *pts, int n, int mode);
int CombineRgn(HRGN dst, HRGN a, HRGN b, int mode);
int SelectClipRgn(HDC dc, HRGN rgn);
int IntersectClipRect(HDC dc, int left, int top, int right, int bottom);
BOOL FillRgn(HDC dc, HRGN rgn, HBRUSH brush);
BOOL PtInRegion(HRGN rgn, int x, int y);
int SetWindowRgn(HWND hwnd, HRGN rgn, BOOL redraw);
BOOL GdiFlush(void);
int SetMapMode(HDC dc, int mode);
BOOL SetViewportOrgEx(HDC dc, int x, int y, LPPOINT old);
BOOL SetWindowOrgEx(HDC dc, int x, int y, LPPOINT old);
BOOL BeginPath(HDC dc);
BOOL EndPath(HDC dc);
BOOL StrokePath(HDC dc);
BOOL FillPath(HDC dc);
BOOL StrokeAndFillPath(HDC dc);

#define LOGFONT __AW(LOGFONT)
#define PLOGFONT __AW(PLOGFONT)
#define LPLOGFONT __AW(LPLOGFONT)
#define TEXTMETRIC __AW(TEXTMETRIC)
#define LPTEXTMETRIC __AW(LPTEXTMETRIC)
#define GetObject __AW(GetObject)
#define CreateFont __AW(CreateFont)
#define CreateFontIndirect __AW(CreateFontIndirect)
#define TextOut __AW(TextOut)
#define ExtTextOut __AW(ExtTextOut)
#define GetTextExtentPoint32 __AW(GetTextExtentPoint32)
#define GetTextMetrics __AW(GetTextMetrics)
#define CreateDC __AW(CreateDC)

// GDI reads the text of ...A functions in the system's code page, not in the
// program's UTF-8: these take it through the library (runtime\wingdi.c), which
// gives GDI the same text in UTF-16.
int DrawTextExA(HDC dc, LPSTR text, int n, LPRECT r, UINT format, void *params);
int DrawTextExW(HDC dc, LPWSTR text, int n, LPRECT r, UINT format, void *params);
LONG TabbedTextOutA(HDC dc, int x, int y, LPCSTR text, int n, int ntabs, const INT *tabs, int origin);
LONG TabbedTextOutW(HDC dc, int x, int y, LPCWSTR text, int n, int ntabs, const INT *tabs, int origin);
#define DrawTextEx __AW(DrawTextEx)
#define TabbedTextOut __AW(TabbedTextOut)
#ifndef WINCC_GDI_ANSI
BOOL wcc_TextOutA(HDC dc, int x, int y, LPCSTR text, int n);
BOOL wcc_ExtTextOutA(HDC dc, int x, int y, UINT opts, const RECT *r, LPCSTR text, UINT n, const INT *dx);
int wcc_DrawTextA(HDC dc, LPCSTR text, int n, LPRECT r, UINT format);
int wcc_DrawTextExA(HDC dc, LPSTR text, int n, LPRECT r, UINT format, void *params);
BOOL wcc_GetTextExtentPoint32A(HDC dc, LPCSTR text, int n, LPSIZE size);
LONG wcc_TabbedTextOutA(HDC dc, int x, int y, LPCSTR text, int n, int ntabs, const INT *tabs, int origin);
HFONT wcc_CreateFontA(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike,
                      DWORD charset, DWORD out_prec, DWORD clip_prec, DWORD quality, DWORD pitch, LPCSTR face);
#define TextOutA wcc_TextOutA
#define ExtTextOutA wcc_ExtTextOutA
#define DrawTextA wcc_DrawTextA
#define DrawTextExA wcc_DrawTextExA
#define GetTextExtentPoint32A wcc_GetTextExtentPoint32A
#define TabbedTextOutA wcc_TabbedTextOutA
#define CreateFontA wcc_CreateFontA
HFONT wcc_CreateFontIndirectA(const LOGFONTA *lf);
int wcc_GetTextFaceA(HDC dc, int cap, LPSTR face);
BOOL wcc_GetTextExtentExPointA(HDC dc, LPCSTR text, int n, int max, LPINT fit, LPINT dx, LPSIZE size);
BOOL wcc_GetTextExtentPointA(HDC dc, LPCSTR text, int n, LPSIZE size);
#define CreateFontIndirectA wcc_CreateFontIndirectA
#define GetTextFaceA wcc_GetTextFaceA
#define GetTextExtentExPointA wcc_GetTextExtentExPointA
#define GetTextExtentPointA wcc_GetTextExtentPointA
#endif
