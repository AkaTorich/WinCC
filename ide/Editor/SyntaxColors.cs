using System;
using System.Windows;
using System.Windows.Media;
using ThemeLibrary;

namespace WinCC.Editor
{
    /// <summary>
    /// Цвета подсветки, взятые из текущей темы: ключевые слова — акцент, типы — второй акцент, строки — «успех»,
    /// числа — «предупреждение», препроцессор — «опасность» (смягчённая к тексту), комментарии — приглушённый текст.
    /// Пересчитываются при смене темы.
    /// </summary>
    public static class SyntaxColors
    {
        static readonly Brush[] brushes = new Brush[8];

        public static Brush CurrentLine { get; private set; }
        public static Brush ErrorLine { get; private set; }
        public static Brush WarningLine { get; private set; }
        public static Brush Warning { get; private set; }
        public static Brush Text { get; private set; }
        public static Brush Dim { get; private set; }
        public static Brush Accent { get; private set; }
        public static Brush Danger { get; private set; }

        /// <summary>Вызывается после смены темы — редакторы перерисовываются.</summary>
        public static event EventHandler Changed;

        static SyntaxColors()
        {
            Refresh();
            ThemeManager.ThemeChanged += (s, e) => { Refresh(); Changed?.Invoke(null, EventArgs.Empty); };
        }

        public static Brush For(TokenKind kind) { return brushes[(int)kind]; }

        static void Refresh()
        {
            Color text = Get("Theme.Color.Text", Colors.White);
            Color dim = Get("Theme.Color.TextDim", Colors.Gray);
            Color accent = Get("Theme.Color.Accent", Colors.DeepSkyBlue);
            Color accent2 = Get("Theme.Color.Accent2", Colors.MediumPurple);
            Color success = Get("Theme.Color.Success", Colors.LightGreen);
            Color warning = Get("Theme.Color.Warning", Colors.Orange);
            Color danger = Get("Theme.Color.Danger", Colors.IndianRed);

            brushes[(int)TokenKind.Plain] = Make(text);
            brushes[(int)TokenKind.Keyword] = Make(accent);
            brushes[(int)TokenKind.Type] = Make(accent2);
            brushes[(int)TokenKind.Constant] = Make(warning);
            brushes[(int)TokenKind.String] = Make(success);
            brushes[(int)TokenKind.Comment] = Make(dim);
            brushes[(int)TokenKind.Number] = Make(warning);
            brushes[(int)TokenKind.Preprocessor] = Make(Mix(danger, text, 0.3));

            Text = brushes[(int)TokenKind.Plain];
            Dim = Make(dim);
            Accent = Make(accent);
            Danger = Make(danger);
            CurrentLine = Make(Color.FromArgb(28, accent.R, accent.G, accent.B));
            ErrorLine = Make(Color.FromArgb(52, danger.R, danger.G, danger.B));
            WarningLine = Make(Color.FromArgb(40, warning.R, warning.G, warning.B));
            Warning = Make(warning);
        }

        static Color Get(string key, Color fallback)
        {
            object v = Application.Current != null ? Application.Current.TryFindResource(key) : null;
            return v is Color ? (Color)v : fallback;
        }

        static Color Mix(Color a, Color b, double t)
        {
            return Color.FromRgb((byte)(a.R + (b.R - a.R) * t), (byte)(a.G + (b.G - a.G) * t), (byte)(a.B + (b.B - a.B) * t));
        }

        static Brush Make(Color c)
        {
            var b = new SolidColorBrush(c);
            b.Freeze();
            return b;
        }
    }
}
