// Выгрузка всех тем ThemeLibrary в Си для библиотеки WUI (lib\wui).
//   style.h  — struct WuiStyle: по полю на каждый ключ Theme.* из ThemeBuilder.Build
//   themes.c — таблица тем: поля Theme (палитра, семейство, фактура, рамка) + готовый WuiStyle
//   icons.c  — значки (по объекту на значок), объявления — include\wui_icons.h
// Запуск: bin\ThemesExport.exe <папка lib\wui> <папка include>
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Effects;
using ThemeLibrary;

static class Program
{
    enum Kind { Brush, Color, Number, Thickness, Corner, Effect, Font, Weight, Style, Capitals }

    static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
    static readonly List<string> brushes = new List<string> { "{0}" };      // 0 — пусто (прозрачно)
    static readonly Dictionary<string, int> brushIndex = new Dictionary<string, int> { { "{0}", 0 } };
    static readonly List<string> effects = new List<string> { "{0}" };     // 0 — без эффекта
    static readonly Dictionary<string, int> effectIndex = new Dictionary<string, int> { { "{0}", 0 } };

    [STAThread]
    static int Main(string[] args)
    {
        Console.OutputEncoding = new UTF8Encoding(false);
        if (args.Length < 1) { Console.Error.WriteLine("ThemesExport <папка lib\\wui> | --list"); return 1; }
        // из пергаментов (12 цветов одного вида) берётся один — так решил хозяин
        var themes = Themes.All.Where(t => t.Group != Themes.GroupParchment || t.Id == "Parchment").ToList();
        if (args[0] == "--list")
        {
            foreach (var t in themes)
                Console.WriteLine(t.Group + " | " + t.Id + " | " + t.Family + " | " + t.Texture + " | " + t.Frame + " | " + t.Name);
            return 0;
        }
        string outDir = args[0];
        var dicts = themes.Select(ThemeBuilder.Build).ToList();

        // все ключи Theme.* и их вид
        var kinds = new SortedDictionary<string, Kind>(StringComparer.Ordinal);
        foreach (var d in dicts)
            foreach (var key in d.Keys.OfType<string>())
            {
                if (!key.StartsWith("Theme.") || key == "Theme.Current") continue;
                var k = KindOf(d[key]);
                if (k == null) continue;
                Kind old;
                if (kinds.TryGetValue(key, out old) && old != k.Value)
                    throw new Exception(key + ": " + old + " и " + k.Value);
                kinds[key] = k.Value;
            }
        foreach (var d in dicts)
            foreach (var key in d.Keys.OfType<string>())
                if (key.StartsWith("Theme.") && key != "Theme.Current" && d[key] != null && KindOf(d[key]) == null)
                    Console.Error.WriteLine("пропущен ключ " + key + ": " + d[key].GetType().Name);

        // style.h
        var h = new StringBuilder();
        h.Append("// Создано tools\\themes-export из ThemeLibrary — руками не править.\n");
        h.Append("// Оформление темы: по полю на каждый ключ Theme.* библиотеки ThemeLibrary.\n");
        h.Append("#pragma once\n\n");
        h.Append("typedef struct {\n");
        foreach (var kv in kinds)
            h.Append("    " + CType(kv.Value) + " " + Field(kv.Key) + ";\n");
        h.Append("} WuiStyle;\n");
        File.WriteAllText(Path.Combine(outDir, "style.h"), h.ToString(), new UTF8Encoding(false));

        // themes.c
        var styles = new StringBuilder();
        for (int i = 0; i < themes.Count; i++)
        {
            styles.Append("    { // " + themes[i].Id + "\n");
            foreach (var kv in kinds)
            {
                object v = dicts[i].Contains(kv.Key) ? dicts[i][kv.Key] : null;
                styles.Append("        " + Value(kv.Value, v) + ",\n");
            }
            styles.Append("    },\n");
        }

        var c = new StringBuilder();
        c.Append("// Создано tools\\themes-export из ThemeLibrary — руками не править.\n");
        c.Append("// " + themes.Count + " тем: палитра, семейство, фактура, узоры, рамка и готовое оформление.\n");
        c.Append("#include \"wui_int.h\"\n\n");
        c.Append("const WuiBrushDef wui_brushes[] = {\n");
        foreach (var b in brushes) c.Append("    " + b + ",\n");
        c.Append("};\n\nconst WuiEffectDef wui_effects[] = {\n");
        foreach (var e in effects) c.Append("    " + e + ",\n");
        c.Append("};\n\nstatic const WuiStyle styles[] = {\n");
        c.Append(styles);
        c.Append("};\n\nconst WuiTheme wui_theme_list[] = {\n");
        for (int i = 0; i < themes.Count; i++) c.Append(ThemeRow(themes[i], i));
        c.Append("};\n\nconst int wui_theme_total = " + themes.Count + ";\n");
        File.WriteAllText(Path.Combine(outDir, "themes.c"), c.ToString(), new UTF8Encoding(false));

        int icons = ExportIcons(outDir, args.Length > 1 ? args[1] : null);
        Console.WriteLine("тем: " + themes.Count + ", ключей: " + kinds.Count + ", кистей: " + brushes.Count + ", теней: " + effects.Count
                          + ", значков: " + icons);
        return 0;
    }

    // ---- значки: icons.c (по объекту на значок — в программу попадают только нужные) и wui_icons.h ----

    static int ExportIcons(string outDir, string includeDir)
    {
        var c = new StringBuilder();
        var h = new StringBuilder();
        c.Append("// Создано tools\\themes-export из ThemeLibrary (Icons.cs) — руками не править.\n");
        c.Append("// Значки на сетке 24×24: контур (перо 1.8, круглые концы) и заливка.\n");
        c.Append("// Путь — числа подряд: 0 x y — начало, 1 x y — линия, 2 x1 y1 x2 y2 x y — кривая, 3 — замкнуть.\n");
        c.Append("#include \"wui_int.h\"\n\n");
        h.Append("// Создано tools\\themes-export из ThemeLibrary (Icons.cs) — руками не править.\n");
        h.Append("// Значки ThemeLibrary: wui_icon(…, WUI_ICON_HOME, …). В программу попадают только те, что она зовёт.\n");
        h.Append("#pragma once\n\n");
        int count = 0;
        foreach (IconKind kind in Enum.GetValues(typeof(IconKind)))
        {
            if (kind == IconKind.None) continue;
            if (kind.ToString() == "Hexagram") continue;    // религиозный символ в WinCC не выгружается
            string name = kind.ToString();
            string stroke = PathData(Icons.Get(kind)), fill = PathData(Icons.GetFill(kind));
            int fillRule = 0;
            var fg = Icons.GetFill(kind);
            if (fg != null && PathGeometry.CreateFromGeometry(fg).FillRule == FillRule.Nonzero) fillRule = 1;
            if (stroke != null) c.Append("static const float s_" + name + "[] = { " + stroke + " };\n");
            if (fill != null) c.Append("static const float f_" + name + "[] = { " + fill + " };\n");
            c.Append("const WuiIconData wui_icon_" + name + " = { "
                     + (stroke != null ? "s_" + name + ", " + Count(stroke) : "0, 0") + ", "
                     + (fill != null ? "f_" + name + ", " + Count(fill) : "0, 0") + ", " + fillRule + " };\n\n");
            h.Append("extern const WuiIconData wui_icon_" + name + ";\n");
            h.Append("#define WUI_ICON_" + Upper(name) + " (&wui_icon_" + name + ")\n");
            count++;
        }
        File.WriteAllText(Path.Combine(outDir, "icons.c"), c.ToString(), new UTF8Encoding(false));
        if (includeDir != null) File.WriteAllText(Path.Combine(includeDir, "wui_icons.h"), h.ToString(), new UTF8Encoding(false));
        return count;
    }

    static int Count(string data) { return data.Split(',').Length; }

    static string Upper(string name)
    {
        var sb = new StringBuilder();
        for (int i = 0; i < name.Length; i++)
        {
            if (i > 0 && char.IsUpper(name[i]) && !char.IsUpper(name[i - 1])) sb.Append('_');
            sb.Append(char.ToUpperInvariant(name[i]));
        }
        return sb.ToString();
    }

    static string N(double v)
    {
        string s = ((float)Math.Round(v, 3)).ToString("R", Inv);
        if (!s.Contains(".") && !s.Contains("E")) s += ".0";
        return s + "f";
    }

    // Геометрия WPF → числа пути; дуги — кривыми Безье.
    static string PathData(Geometry g)
    {
        if (g == null || g.IsEmpty()) return null;
        var pg = PathGeometry.CreateFromGeometry(g);
        var o = new List<string>();
        foreach (var f in pg.Figures)
        {
            var cur = f.StartPoint;
            o.Add("0"); o.Add(N(cur.X)); o.Add(N(cur.Y));
            foreach (var s in f.Segments)
            {
                if (s is LineSegment) { cur = ((LineSegment)s).Point; Line(o, cur); }
                else if (s is PolyLineSegment) foreach (var p in ((PolyLineSegment)s).Points) { cur = p; Line(o, p); }
                else if (s is BezierSegment)
                {
                    var b = (BezierSegment)s;
                    Curve(o, b.Point1, b.Point2, b.Point3);
                    cur = b.Point3;
                }
                else if (s is PolyBezierSegment)
                {
                    var pts = ((PolyBezierSegment)s).Points;
                    for (int i = 0; i + 2 < pts.Count; i += 3) Curve(o, pts[i], pts[i + 1], pts[i + 2]);
                    cur = pts[pts.Count - 1];
                }
                else if (s is QuadraticBezierSegment)
                {
                    var q = (QuadraticBezierSegment)s;
                    Curve(o, cur + (q.Point1 - cur) * 2 / 3, q.Point2 + (q.Point1 - q.Point2) * 2 / 3, q.Point2);
                    cur = q.Point2;
                }
                else if (s is PolyQuadraticBezierSegment)
                {
                    var pts = ((PolyQuadraticBezierSegment)s).Points;
                    for (int i = 0; i + 1 < pts.Count; i += 2)
                    {
                        Curve(o, cur + (pts[i] - cur) * 2 / 3, pts[i + 1] + (pts[i] - pts[i + 1]) * 2 / 3, pts[i + 1]);
                        cur = pts[i + 1];
                    }
                }
                else if (s is ArcSegment)
                {
                    var a = (ArcSegment)s;
                    ArcToCurves(o, cur, a);
                    cur = a.Point;
                }
                else throw new Exception("отрезок " + s.GetType().Name);
            }
            if (f.IsClosed) o.Add("3");
        }
        return string.Join(", ", o);
    }

    static void Line(List<string> o, Point p) { o.Add("1"); o.Add(N(p.X)); o.Add(N(p.Y)); }

    static void Curve(List<string> o, Point a, Point b, Point p)
    {
        o.Add("2"); o.Add(N(a.X)); o.Add(N(a.Y)); o.Add(N(b.X)); o.Add(N(b.Y)); o.Add(N(p.X)); o.Add(N(p.Y));
    }

    // Дуга эллипса (как в SVG): центр по концам, потом куски не больше четверти круга.
    static void ArcToCurves(List<string> o, Point p0, ArcSegment a)
    {
        Point p1 = a.Point;
        double rx = Math.Abs(a.Size.Width), ry = Math.Abs(a.Size.Height);
        if (rx < 1e-9 || ry < 1e-9 || (p0 - p1).Length < 1e-9) { Line(o, p1); return; }
        double phi = a.RotationAngle * Math.PI / 180, cos = Math.Cos(phi), sin = Math.Sin(phi);
        double dx = (p0.X - p1.X) / 2, dy = (p0.Y - p1.Y) / 2;
        double x1 = cos * dx + sin * dy, y1 = -sin * dx + cos * dy;
        double lam = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
        if (lam > 1) { rx *= Math.Sqrt(lam); ry *= Math.Sqrt(lam); }
        double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
        double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
        double k = Math.Sqrt(Math.Max(0, num / den));
        bool sweep = a.SweepDirection == SweepDirection.Clockwise;
        if (a.IsLargeArc == sweep) k = -k;
        double cx1 = k * rx * y1 / ry, cy1 = -k * ry * x1 / rx;
        double cx = cos * cx1 - sin * cy1 + (p0.X + p1.X) / 2, cy = sin * cx1 + cos * cy1 + (p0.Y + p1.Y) / 2;
        double t1 = Math.Atan2((y1 - cy1) / ry, (x1 - cx1) / rx);
        double t2 = Math.Atan2((-y1 - cy1) / ry, (-x1 - cx1) / rx);
        double dt = t2 - t1;
        if (sweep && dt < 0) dt += 2 * Math.PI;
        if (!sweep && dt > 0) dt -= 2 * Math.PI;
        int n = (int)Math.Ceiling(Math.Abs(dt) / (Math.PI / 2) - 1e-9);
        if (n < 1) n = 1;
        double step = dt / n, kk = 4.0 / 3 * Math.Tan(step / 4);
        Func<double, Point> at = t => new Point(cx + rx * Math.Cos(t) * cos - ry * Math.Sin(t) * sin,
                                                cy + rx * Math.Cos(t) * sin + ry * Math.Sin(t) * cos);
        Func<double, Vector> d = t => new Vector(-rx * Math.Sin(t) * cos - ry * Math.Cos(t) * sin,
                                                 -rx * Math.Sin(t) * sin + ry * Math.Cos(t) * cos);
        double t0 = t1;
        for (int i = 0; i < n; i++)
        {
            double ta = t0 + step * i, tb = ta + step;
            Point pa = at(ta), pb = i == n - 1 ? p1 : at(tb);
            Curve(o, pa + d(ta) * kk, pb - d(tb) * kk, pb);
        }
    }

    static Kind? KindOf(object v)
    {
        if (v is Brush) return Kind.Brush;
        if (v is Color) return Kind.Color;
        if (v is double) return Kind.Number;
        if (v is Thickness) return Kind.Thickness;
        if (v is CornerRadius) return Kind.Corner;
        if (v is Effect) return Kind.Effect;
        if (v is FontFamily) return Kind.Font;
        if (v is FontWeight) return Kind.Weight;
        if (v is FontStyle) return Kind.Style;
        if (v is FontCapitals) return Kind.Capitals;
        return null;
    }

    static string CType(Kind k)
    {
        switch (k)
        {
            case Kind.Brush: return "unsigned short";       // номер в wui_brushes
            case Kind.Effect: return "unsigned short";      // номер в wui_effects
            case Kind.Color: return "WuiColor";
            case Kind.Number: return "float";
            case Kind.Thickness: return "WuiEdges";
            case Kind.Corner: return "WuiCorners";
            case Kind.Font: return "const char *";
            default: return "int";
        }
    }

    static string Field(string key)
    {
        return key.Substring("Theme.".Length).Replace('.', '_');
    }

    static string F(double v)
    {
        string s = ((float)v).ToString("R", Inv);
        if (!s.Contains(".") && !s.Contains("E")) s += ".0";
        return s + "f";
    }

    static string Hex(Color c)
    {
        return "0x" + c.A.ToString("X2") + c.R.ToString("X2") + c.G.ToString("X2") + c.B.ToString("X2");
    }

    static string Str(string s)
    {
        return s == null ? "0" : "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    }

    static string Value(Kind k, object v)
    {
        switch (k)
        {
            case Kind.Brush: return Pool(BrushDef(v as Brush), brushes, brushIndex).ToString();
            case Kind.Effect: return Pool(EffectDef(v as Effect), effects, effectIndex).ToString();
            case Kind.Color: return v == null ? "0" : Hex((Color)v);
            case Kind.Number: return v == null ? "0" : F((double)v);
            case Kind.Thickness:
                {
                    var t = v == null ? new Thickness(0) : (Thickness)v;
                    return "{ " + F(t.Left) + ", " + F(t.Top) + ", " + F(t.Right) + ", " + F(t.Bottom) + " }";
                }
            case Kind.Corner:
                {
                    var r = v == null ? new CornerRadius(0) : (CornerRadius)v;
                    return "{ " + F(r.TopLeft) + ", " + F(r.TopRight) + ", " + F(r.BottomRight) + ", " + F(r.BottomLeft) + " }";
                }
            case Kind.Font: return v == null ? "0" : Str(((FontFamily)v).Source);
            case Kind.Weight: return v == null ? "400" : ((FontWeight)v).ToOpenTypeWeight().ToString(Inv);
            case Kind.Style: return v != null && ((FontStyle)v) == FontStyles.Italic ? "1" : "0";
            default: return v != null && ((FontCapitals)v) != FontCapitals.Normal ? "1" : "0";
        }
    }

    static int Pool(string def, List<string> list, Dictionary<string, int> index)
    {
        int i;
        if (index.TryGetValue(def, out i)) return i;
        i = list.Count;
        list.Add(def);
        index[def] = i;
        return i;
    }

    // { вид, непрозрачность, x1, y1, x2, y2, (у радиальной: rx, ry), число остановок, { {доля, цвет}… } }
    static string BrushDef(Brush b)
    {
        if (b == null) return "{0}";
        var sb = new StringBuilder("{ ");
        if (b is SolidColorBrush)
        {
            var s = (SolidColorBrush)b;
            if (s.Color.A == 0 && b.Opacity >= 1) return "{0}";
            sb.Append("WUI_BRUSH_SOLID, " + F(b.Opacity) + ", 0, 0, 0, 0, 0, 0, 1, { { 0.0f, " + Hex(s.Color) + " } }");
        }
        else if (b is LinearGradientBrush)
        {
            var g = (LinearGradientBrush)b;
            if (g.MappingMode != BrushMappingMode.RelativeToBoundingBox) throw new Exception("градиент в точках");
            sb.Append("WUI_BRUSH_LINEAR, " + F(b.Opacity) + ", " + F(g.StartPoint.X) + ", " + F(g.StartPoint.Y) + ", "
                      + F(g.EndPoint.X) + ", " + F(g.EndPoint.Y) + ", 0, 0, " + Stops(g.GradientStops));
        }
        else if (b is RadialGradientBrush)
        {
            var g = (RadialGradientBrush)b;
            sb.Append("WUI_BRUSH_RADIAL, " + F(b.Opacity) + ", " + F(g.Center.X) + ", " + F(g.Center.Y) + ", "
                      + F(g.GradientOrigin.X) + ", " + F(g.GradientOrigin.Y) + ", " + F(g.RadiusX) + ", " + F(g.RadiusY) + ", "
                      + Stops(g.GradientStops));
        }
        else if (b is ImageBrush || b is DrawingBrush || b is VisualBrush)
            sb.Append("WUI_BRUSH_TEXTURE, " + F(b.Opacity) + ", 0, 0, 0, 0, 0, 0, 0, { { 0 } }");
        else throw new Exception("кисть " + b.GetType().Name);
        return sb.Append(" }").ToString();
    }

    static string Stops(GradientStopCollection stops)
    {
        if (stops.Count > 6) throw new Exception("больше 6 остановок");
        var sb = new StringBuilder(stops.Count + ", { ");
        foreach (var s in stops.OrderBy(s => s.Offset))
            sb.Append("{ " + F(s.Offset) + ", " + Hex(s.Color) + " }, ");
        return sb.Append("}").ToString();
    }

    // { цвет, размытие, сдвиг, непрозрачность, направление (градусы) }
    static string EffectDef(Effect e)
    {
        if (e == null) return "{0}";
        var s = e as DropShadowEffect;
        if (s == null) throw new Exception("эффект " + e.GetType().Name);
        return "{ " + Hex(s.Color) + ", " + F(s.BlurRadius) + ", " + F(s.ShadowDepth) + ", " + F(s.Opacity) + ", " + F(s.Direction) + " }";
    }

    static string ThemeRow(Theme t, int i)
    {
        var sb = new StringBuilder();
        sb.Append("    { " + Str(t.Id) + ", " + Str(t.Name) + ", " + Str(t.Group) + ", " + Str(t.NameEn) + ",\n");
        sb.Append("      " + (int)t.Family + ", " + (int)t.Texture + ", " + (int)t.Decor + ", " + (int)t.Frame + ", "
                  + (t.IsDark ? 1 : 0) + ",\n");
        Color[] cs =
        {
            t.Background1, t.Background2, t.Background3, t.Text, t.TextDim, t.TextOnAccent,
            t.Accent, t.Accent2, t.AccentStart, t.AccentEnd, t.Glow, t.Success, t.Warning, t.Danger,
            t.Surface, t.Edge, t.Field, t.FieldEdge, t.Popup, t.Selection, t.Lcd, t.LcdText,
            t.Orb1, t.Orb2, t.Orb3, t.Paper
        };
        sb.Append("      { " + string.Join(", ", cs.Select(Hex)) + " },\n");
        sb.Append("      " + Str(t.Font) + ", " + Str(t.HeadingFont) + ", " + Str(t.BodyFont) + ", " + Str(t.MonoFont) + ",\n");
        sb.Append("      " + F(t.FontSize) + ", " + F(t.WindowRadius) + ", " + F(t.CardRadius) + ", " + F(t.ControlRadius)
                  + ", &styles[" + i + "] },\n");
        return sb.ToString();
    }
}
