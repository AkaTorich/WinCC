// WUI — цвета: смешение, светлее/темнее, HSL. Те же формулы, что ColorMath в ThemeLibrary.
#include "wui_int.h"

double wui_round(double v)
{
    double f = floor(v), d = v - f;
    if (d > 0.5) return f + 1;
    if (d < 0.5) return f;
    return fmod(f, 2.0) == 0 ? f : f + 1;          // ровно половина — к чётному
}

WuiColor wui_alpha(WuiColor c, int alpha)
{
    return (c & 0x00FFFFFFu) | ((WuiColor)(alpha & 255) << 24);
}

static int lerp(int a, int b, double t)
{
    return (int)wui_round(a + (b - a) * t);
}

WuiColor wui_mix(WuiColor a, WuiColor b, double t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return WUI_ARGB(lerp(WUI_A(a), WUI_A(b), t), lerp(WUI_R(a), WUI_R(b), t),
                    lerp(WUI_G(a), WUI_G(b), t), lerp(WUI_B(a), WUI_B(b), t));
}

WuiColor wui_lighten(WuiColor c, double t)
{
    return wui_mix(c, (c & 0xFF000000u) | 0xFFFFFF, t);
}

WuiColor wui_darken(WuiColor c, double t)
{
    return wui_mix(c, c & 0xFF000000u, t);
}

static double hue_channel(double p, double q, double t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

static int to_byte(double v)
{
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return (int)wui_round(v * 255);
}

WuiColor wui_from_hsl(double hue, double sat, double light, int alpha)
{
    double h = fmod(fmod(hue, 360) + 360, 360) / 360.0;
    double s = sat < 0 ? 0 : sat > 1 ? 1 : sat;
    double l = light < 0 ? 0 : light > 1 ? 1 : light;
    double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    double p = 2 * l - q;
    return WUI_ARGB(alpha, to_byte(hue_channel(p, q, h + 1.0 / 3)), to_byte(hue_channel(p, q, h)),
                    to_byte(hue_channel(p, q, h - 1.0 / 3)));
}

void wui_to_hsl(WuiColor c, double *hue, double *sat, double *light)
{
    double r = WUI_R(c) / 255.0, g = WUI_G(c) / 255.0, b = WUI_B(c) / 255.0;
    double max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double d = max - min;
    *light = (max + min) / 2;
    if (d < 1e-9) { *hue = 0; *sat = 0; return; }
    *sat = *light > 0.5 ? d / (2 - max - min) : d / (max + min);
    if (max == r) *hue = ((g - b) / d + (g < b ? 6 : 0)) * 60;
    else if (max == g) *hue = ((b - r) / d + 2) * 60;
    else *hue = ((r - g) / d + 4) * 60;
}

WuiColor wui_with_lightness(WuiColor c, double light)
{
    double h, s, l;
    wui_to_hsl(c, &h, &s, &l);
    return wui_from_hsl(h, s, light, WUI_A(c));
}
