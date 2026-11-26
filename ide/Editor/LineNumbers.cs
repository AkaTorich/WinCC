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
            return new Size(Math.Ceiling(w + 22 + MarkWidth), 0);
        }

        /// <summary>Поле слева от номеров: кружки точек останова и стрелка строки, где стоит программа.</summary>
        const double MarkWidth = 16;

        protected override void OnMouseLeftButtonDown(System.Windows.Input.MouseButtonEventArgs e)
        {
            base.OnMouseLeftButtonDown(e);
            if (Editor == null) return;
            int line = LineAtY(e.GetPosition(this).Y);
            if (line >= 0) { Editor.ToggleBreakpoint(line + 1); e.Handled = true; }
        }

        int LineAtY(double y)
        {
            TextBox box = Editor.TextBox;
            int first = box.GetFirstVisibleLineIndex(), last = box.GetLastVisibleLineIndex();
            if (first < 0) return -1;
            last = Math.Min(last + 1, Editor.LineCount - 1);
            for (int line = first; line <= last; line++)
            {
                Rect at = box.GetRectFromCharacterIndex(Editor.LineStart(line));
                if (at.IsEmpty) continue;
                double top = box.TranslatePoint(at.TopLeft, this).Y;
                if (y >= top && y < top + at.Height) return line;
            }
            return -1;
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
                double cy = y + at.Height / 2, r = Math.Min(5.5, at.Height * 0.32);
                if (Editor.IsBreakpointLine(line))
                    dc.DrawEllipse(SyntaxColors.Breakpoint, null, new Point(9, cy), r, r);
                if (line == Editor.ExecLineIndex)
                {
                    var arrow = new StreamGeometry();
                    using (var g = arrow.Open())
                    {
                        g.BeginFigure(new Point(3, cy - r), true, true);
                        g.LineTo(new Point(9, cy - r), true, false);
                        g.LineTo(new Point(15, cy), true, false);
                        g.LineTo(new Point(9, cy + r), true, false);
                        g.LineTo(new Point(3, cy + r), true, false);
                    }
                    arrow.Freeze();
                    dc.DrawGeometry(SyntaxColors.ExecArrow, new Pen(Brushes.Black, 0.6), arrow);
                }
            }
            dc.Pop();
        }
    }
}
