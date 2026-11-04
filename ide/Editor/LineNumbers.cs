using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace WinCC.Editor
{
    /// <summary>Номера строк слева от поля ввода: текущая — акцентом, строки с ошибками — красным.</summary>
    public class LineNumbers : FrameworkElement
    {
        internal CodeEditor Editor { get; set; }

        Typeface Face()
        {
            TextBox box = Editor.TextBox;
            return new Typeface(box.FontFamily, box.FontStyle, FontWeights.Normal, box.FontStretch);
        }

        FormattedText Number(string s, Brush brush)
        {
            TextBox box = Editor.TextBox;
            return new FormattedText(s, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Face(),
                box.FontSize * 0.9, brush, null, TextOptions.GetTextFormattingMode(box),
                VisualTreeHelper.GetDpi(this).PixelsPerDip);
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            if (Editor == null) return new Size(0, 0);
            int digits = Math.Max(3, Editor.LineCount.ToString(CultureInfo.InvariantCulture).Length);
            double w = Number(new string('8', digits), SyntaxColors.Dim).WidthIncludingTrailingWhitespace;
            return new Size(Math.Ceiling(w + 22), 0);
        }

        protected override void OnRender(DrawingContext dc)
        {
            if (Editor == null) return;
            // Фон для щелчков и тонкая черта справа
            dc.DrawRectangle(Brushes.Transparent, null, new Rect(RenderSize));
            var edge = new Pen(SyntaxColors.Dim, 1) { DashStyle = DashStyles.Solid };
            dc.PushOpacity(0.35);
            dc.DrawLine(edge, new Point(ActualWidth - 0.5, 0), new Point(ActualWidth - 0.5, ActualHeight));
            dc.Pop();

            TextBox box = Editor.TextBox;
            Rect view = Editor.Viewport(this);
            if (view.IsEmpty) return;
            int first = box.GetFirstVisibleLineIndex();
            int last = box.GetLastVisibleLineIndex();
            if (first < 0) return;
            last = Math.Min(last + 1, Editor.LineCount - 1);

            dc.PushClip(new RectangleGeometry(new Rect(0, view.Top, ActualWidth, view.Height)));
            for (int line = first; line <= last; line++)
            {
                Rect at = box.GetRectFromCharacterIndex(Editor.LineStart(line));
                if (at.IsEmpty) continue;
                double y = box.TranslatePoint(at.TopLeft, this).Y;
                Brush brush = Editor.IsErrorLine(line) ? SyntaxColors.Danger
                            : Editor.IsWarningLine(line) ? SyntaxColors.Warning
                            : line == Editor.CaretLineIndex ? SyntaxColors.Accent : SyntaxColors.Dim;
                var ft = Number((line + 1).ToString(CultureInfo.InvariantCulture), brush);
                dc.DrawText(ft, new Point(ActualWidth - 12 - ft.Width, y + (at.Height - ft.Height) / 2));
            }
            dc.Pop();
        }
    }
}
