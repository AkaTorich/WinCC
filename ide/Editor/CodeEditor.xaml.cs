using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace WinCC.Editor
{
    /// <summary>
    /// Редактор Си: обычное поле ввода (правка, выделение, отмена — его) с прозрачными буквами,
    /// поверх — слой подсветки, слева — номера строк. Разбивка на строки и состояние разбора
    /// пересчитываются при каждой правке.
    /// </summary>
    public partial class CodeEditor : UserControl
    {
        const string Indent = "    ";

        string text = "";
        readonly List<int> lineStarts = new List<int> { 0 };
        readonly List<int> lineStates = new List<int> { 0 };
        readonly HashSet<int> errorLines = new HashSet<int>();
        readonly HashSet<int> warningLines = new HashSet<int>();
        ScrollViewer scrollHost;
        int caretLine = -1;
        bool redrawPending;

        /// <summary>Текст изменён правкой (не загрузкой).</summary>
        public event EventHandler TextEdited;
        /// <summary>Каретка перешла на другое место.</summary>
        public event EventHandler CaretMoved;

        public CodeEditor()
        {
            InitializeComponent();
            Gutter.Editor = this;
            Layer.Editor = this;

            Box.TextChanged += OnTextChanged;
            Box.SelectionChanged += OnSelectionChanged;
            Box.PreviewKeyDown += OnPreviewKeyDown;
            Box.PreviewTextInput += OnPreviewTextInput;
            Box.PreviewMouseWheel += OnPreviewMouseWheel;
            Box.LayoutUpdated += (s, e) => { if (redrawPending) { redrawPending = false; Redraw(); } };
            Box.Loaded += (s, e) => HookScroll();
            Box.SizeChanged += (s, e) => Redraw();
            SyntaxColors.Changed += OnColorsChanged;
            Unloaded += (s, e) => SyntaxColors.Changed -= OnColorsChanged;
            Loaded += (s, e) => { SyntaxColors.Changed -= OnColorsChanged; SyntaxColors.Changed += OnColorsChanged; Redraw(); };
        }

        internal TextBox TextBox { get { return Box; } }
        internal string Source { get { return text; } }
        internal int LineCount { get { return lineStarts.Count; } }
        internal int LineStart(int line) { return lineStarts[line]; }
        internal int LineState(int line) { return lineStates[line]; }
        internal int CaretLineIndex { get { return caretLine; } }
        internal bool IsErrorLine(int line) { return errorLines.Contains(line); }
        internal bool IsWarningLine(int line) { return warningLines.Contains(line); }

        /// <summary>Конец строки без знаков перевода.</summary>
        internal int LineEnd(int line)
        {
            int end = line + 1 < lineStarts.Count ? lineStarts[line + 1] : text.Length;
            while (end > lineStarts[line] && (text[end - 1] == '\n' || text[end - 1] == '\r')) end--;
            return end;
        }

        /// <summary>Видимая часть поля ввода (без полос прокрутки) в координатах элемента target.</summary>
        internal Rect Viewport(Visual target)
        {
            HookScroll();
            var presenter = scrollHost != null ? FindPresenter(scrollHost) : null;
            if (presenter == null || presenter.ActualWidth <= 0) return Rect.Empty;
            try
            {
                var p = presenter.TransformToVisual(target).Transform(new Point(0, 0));
                return new Rect(p, new Size(presenter.ActualWidth, presenter.ActualHeight));
            }
            catch (InvalidOperationException) { return Rect.Empty; }
        }

        /// <summary>Весь текст. Установка — как загрузка файла: отмена начинается заново.</summary>
        public string Text
        {
            get { return text; }
            set
            {
                Box.TextChanged -= OnTextChanged;
                Box.Text = value ?? "";
                Box.TextChanged += OnTextChanged;
                Box.IsUndoEnabled = false;
                Box.IsUndoEnabled = true;
                Box.CaretIndex = 0;
                Reparse();
                RequestRedraw();
            }
        }

        /// <summary>Строка и столбец каретки, с единицы.</summary>
        public int CaretRow { get { return Math.Max(caretLine, 0) + 1; } }
        public int CaretColumn
        {
            get
            {
                int line = Math.Max(caretLine, 0);
                return line < lineStarts.Count ? Box.CaretIndex - lineStarts[line] + 1 : 1;
            }
        }

        public void FocusEditor() { Box.Focus(); Keyboard.Focus(Box); }

        /// <summary>Перейти на строку (с единицы) и, если известен, столбец; строка выделяется.</summary>
        public void GoToLine(int line, int column = 0)
        {
            int index = Math.Max(0, Math.Min(line - 1, lineStarts.Count - 1));
            int start = lineStarts[index], end = LineEnd(index);
            FocusEditor();
            if (column > 0) Box.Select(Math.Min(start + column - 1, end), 0);
            else Box.Select(start, end - start);
            int first = Math.Max(0, index - 5);
            Box.ScrollToLine(Math.Min(lineStarts.Count - 1, index + 5));
            Box.ScrollToLine(first);
            RequestRedraw();
        }

        /// <summary>Строки с ошибками и предупреждениями (с единицы) — отмечаются, пока текст не правят.</summary>
        public void SetErrorLines(IEnumerable<int> errors, IEnumerable<int> warnings = null)
        {
            errorLines.Clear();
            warningLines.Clear();
            if (errors != null) foreach (int l in errors) errorLines.Add(l - 1);
            if (warnings != null) foreach (int l in warnings) warningLines.Add(l - 1);
            Redraw();
        }

        void OnColorsChanged(object sender, EventArgs e) { Redraw(); }

        void OnTextChanged(object sender, TextChangedEventArgs e)
        {
            Reparse();
            errorLines.Clear();
            warningLines.Clear();
            RequestRedraw();
            TextEdited?.Invoke(this, EventArgs.Empty);
        }

        void OnSelectionChanged(object sender, RoutedEventArgs e)
        {
            int line = FindLine(Box.CaretIndex);
            if (line != caretLine)
            {
                caretLine = line;
                RequestRedraw();
            }
            CaretMoved?.Invoke(this, EventArgs.Empty);
        }

        void Reparse()
        {
            text = Box.Text;
            lineStarts.Clear();
            lineStates.Clear();
            lineStarts.Add(0);
            for (int i = 0; i < text.Length; i++)
            {
                char c = text[i];
                if (c == '\r')
                {
                    if (i + 1 < text.Length && text[i + 1] == '\n') i++;
                    lineStarts.Add(i + 1);
                }
                else if (c == '\n') lineStarts.Add(i + 1);
            }
            int state = 0;
            for (int l = 0; l < lineStarts.Count; l++)
            {
                lineStates.Add(state);
                state = CLexer.LexLine(text, lineStarts[l], LineEnd(l), state, null);
            }
            caretLine = FindLine(Box.CaretIndex);
            Gutter.InvalidateMeasure();
        }

        int FindLine(int index)
        {
            int lo = 0, hi = lineStarts.Count - 1;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) / 2;
                if (lineStarts[mid] <= index) lo = mid; else hi = mid - 1;
            }
            return lo;
        }

        void RequestRedraw()
        {
            redrawPending = true;
            Box.InvalidateArrange();
        }

        void Redraw()
        {
            Layer.InvalidateVisual();
            Gutter.InvalidateVisual();
        }

        void HookScroll()
        {
            if (scrollHost != null) return;
            Box.ApplyTemplate();
            scrollHost = Box.Template.FindName("PART_ContentHost", Box) as ScrollViewer;
            if (scrollHost != null) scrollHost.ScrollChanged += (s, e) => Redraw();
        }

        static ScrollContentPresenter FindPresenter(DependencyObject root)
        {
            int n = VisualTreeHelper.GetChildrenCount(root);
            for (int i = 0; i < n; i++)
            {
                var child = VisualTreeHelper.GetChild(root, i);
                var p = child as ScrollContentPresenter ?? FindPresenter(child);
                if (p != null) return p;
            }
            return null;
        }

        // ---- Клавиши: отступы табуляцией, отступ новой строки, «}» сдвигает строку назад, Ctrl+колесо — размер ----

        void OnPreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Tab && (Keyboard.Modifiers & (ModifierKeys.Control | ModifierKeys.Alt)) == 0)
            {
                bool back = (Keyboard.Modifiers & ModifierKeys.Shift) != 0;
                int first = FindLine(Box.SelectionStart);
                int last = FindLine(Box.SelectionStart + Box.SelectionLength);
                if (back || first != last) ShiftLines(first, last, back);
                else Insert(Indent);
                e.Handled = true;
            }
            else if (e.Key == Key.Enter && Keyboard.Modifiers == ModifierKeys.None)
            {
                int line = FindLine(Box.SelectionStart);
                int start = lineStarts[line];
                int k = start;
                while (k < text.Length && (text[k] == ' ' || text[k] == '\t') && k < Box.SelectionStart) k++;
                string indent = text.Substring(start, k - start);
                string before = text.Substring(start, Box.SelectionStart - start).TrimEnd();
                if (before.EndsWith("{")) indent += Indent;
                Insert("\r\n" + indent);
                e.Handled = true;
            }
        }

        void OnPreviewTextInput(object sender, TextCompositionEventArgs e)
        {
            if (e.Text != "}" || Box.SelectionLength > 0) return;
            int line = FindLine(Box.CaretIndex);
            int start = lineStarts[line];
            string before = text.Substring(start, Box.CaretIndex - start);
            if (before.Length >= Indent.Length && before.Trim().Length == 0)
            {
                Box.Select(Box.CaretIndex - Indent.Length, Indent.Length);
                Insert("}");
                e.Handled = true;
            }
        }

        void OnPreviewMouseWheel(object sender, MouseWheelEventArgs e)
        {
            if ((Keyboard.Modifiers & ModifierKeys.Control) == 0) return;
            Box.FontSize = Math.Max(9, Math.Min(32, Box.FontSize + (e.Delta > 0 ? 1 : -1)));
            Gutter.InvalidateMeasure();
            RequestRedraw();
            e.Handled = true;
        }

        void Insert(string s)
        {
            Box.SelectedText = s;
            Box.Select(Box.SelectionStart + s.Length, 0);
        }

        void ShiftLines(int first, int last, bool back)
        {
            // Строка, в начале которой кончается выделение, не сдвигается
            if (last > first && lineStarts[last] == Box.SelectionStart + Box.SelectionLength) last--;
            int from = lineStarts[first], to = LineEnd(last);
            var sb = new System.Text.StringBuilder();
            for (int l = first; l <= last; l++)
            {
                int s = lineStarts[l], e = LineEnd(l);
                string body = text.Substring(s, e - s);
                if (back)
                {
                    int cut = 0;
                    while (cut < Indent.Length && cut < body.Length && body[cut] == ' ') cut++;
                    if (cut == 0 && body.StartsWith("\t")) cut = 1;
                    body = body.Substring(cut);
                }
                else if (body.Length > 0) body = Indent + body;
                sb.Append(body);
                if (l < last) sb.Append(text, e, lineStarts[l + 1] - e);
            }
            Box.BeginChange();
            Box.Select(from, to - from);
            Box.SelectedText = sb.ToString();
            Box.EndChange();
            Box.Select(from, sb.Length);
        }
    }
}
