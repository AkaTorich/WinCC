using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace WinCC.Editor
{
    /// <summary>
    /// Рисует цветной текст поверх поля ввода — только видимые строки. Каждый кусок строки между табуляциями
    /// ставится туда, где поле ввода держит его первую букву, поэтому буквы совпадают с кареткой и выделением.
    /// </summary>
    public class HighlightLayer : FrameworkElement
    {
        readonly List<Token> tokens = new List<Token>();

        internal CodeEditor Editor { get; set; }

        protected override void OnRender(DrawingContext dc)
        {
            if (Editor == null) return;
            TextBox box = Editor.TextBox;
            Rect view = Editor.Viewport(this);
            if (view.IsEmpty) return;

            int first = box.GetFirstVisibleLineIndex();
            int last = box.GetLastVisibleLineIndex();
            if (first < 0) return;
            last = System.Math.Min(last + 1, Editor.LineCount - 1);

            string text = Editor.Source;
            var typeface = new Typeface(box.FontFamily, box.FontStyle, box.FontWeight, box.FontStretch);
            double dip = VisualTreeHelper.GetDpi(this).PixelsPerDip;
            TextFormattingMode mode = TextOptions.GetTextFormattingMode(box);

            dc.PushClip(new RectangleGeometry(view));
            for (int line = first; line <= last; line++)
            {
                int start = Editor.LineStart(line), end = Editor.LineEnd(line);
                Rect at = box.GetRectFromCharacterIndex(start);
                if (at.IsEmpty) continue;
                Point origin = box.TranslatePoint(at.TopLeft, this);
                if (origin.Y > view.Bottom) break;
                if (origin.Y + at.Height < view.Top) continue;

                var band = new Rect(view.Left, origin.Y, view.Width, at.Height);
                if (line == Editor.ExecLineIndex) dc.DrawRectangle(SyntaxColors.ExecLine, null, band);
                else if (Editor.IsBreakpointLine(line)) dc.DrawRectangle(SyntaxColors.BreakpointLine, null, band);
                else if (Editor.IsErrorLine(line)) dc.DrawRectangle(SyntaxColors.ErrorLine, null, band);
                else if (Editor.IsWarningLine(line)) dc.DrawRectangle(SyntaxColors.WarningLine, null, band);
                else if (line == Editor.CaretLineIndex) dc.DrawRectangle(SyntaxColors.CurrentLine, null, band);

                if (end <= start) continue;
                tokens.Clear();
                CLexer.LexLine(text, start, end, Editor.LineState(line), tokens);

                // Куски без табуляций
                int seg = start;
                while (seg < end)
                {
                    while (seg < end && text[seg] == '\t') seg++;
                    if (seg >= end) break;
                    int segEnd = text.IndexOf('\t', seg, end - seg);
                    if (segEnd < 0) segEnd = end;
                    DrawSegment(dc, box, text, seg, segEnd, typeface, dip, mode);
                    seg = segEnd;
                }
            }
            dc.Pop();
        }

        void DrawSegment(DrawingContext dc, TextBox box, string text, int from, int to,
                         Typeface typeface, double dip, TextFormattingMode mode)
        {
            Rect at = box.GetRectFromCharacterIndex(from);
            if (at.IsEmpty) return;
            Point origin = box.TranslatePoint(at.TopLeft, this);

            var ft = new FormattedText(text.Substring(from, to - from), CultureInfo.CurrentUICulture,
                FlowDirection.LeftToRight, typeface, box.FontSize, SyntaxColors.Text, null, mode, dip);
            foreach (Token t in tokens)
            {
                if (t.Kind == TokenKind.Plain) continue;
                int s = System.Math.Max(t.Start, from), e = System.Math.Min(t.Start + t.Length, to);
                if (e > s) ft.SetForegroundBrush(SyntaxColors.For(t.Kind), s - from, e - s);
            }
            dc.DrawText(ft, origin);
        }
    }
}
