using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using WinCC.Editor.Completion;

namespace WinCC.Editor
{
    /// <summary>
    /// Подсказки ввода: после двух букв имени (или по Ctrl+Пробел) под кареткой открывается список имён —
    /// функции, макросы, типы и постоянные подключённых заголовков, имена этого и других открытых файлов, слова Си.
    /// Стрелки выбирают, Enter или Tab вставляют, Esc закрывает. После «имя(» над строкой — объявление функции.
    /// </summary>
    public partial class CodeEditor
    {
        /// <summary>Сколько букв имени набрать, чтобы список открылся сам; 0 — только по Ctrl+Пробел (настройки окна).</summary>
        public static int CompletionAfterLetters = 2;

        /// <summary>Тексты других открытых файлов: (имя файла, текст) — их имена тоже в подсказках.</summary>
        public static Func<IEnumerable<KeyValuePair<string, string>>> OpenTexts;

        /// <summary>Папка файла: от неё ищутся «"свои.h"».</summary>
        public string FileFolder { get; set; }

        CompletionPopup completion;
        Popup signature;
        TextBlock signatureText;
        int completionStart = -1;           // где начинается дополняемое имя
        bool typedName;                     // последняя введённая буква — часть имени
        int signatureParen = -1;            // где стоит «(» вызова с подсказкой объявления

        void InitCompletion()
        {
            completion = new CompletionPopup { PlacementTarget = Box };
            completion.Accepted += (s, e) => { AcceptCompletion(); Box.Focus(); };

            signatureText = new TextBlock { Margin = new Thickness(8, 4, 8, 5), FontSize = 12, MaxWidth = 640, TextWrapping = TextWrapping.Wrap };
            signatureText.SetResourceReference(TextBlock.FontFamilyProperty, "Theme.Font.Mono");
            signatureText.SetResourceReference(TextBlock.ForegroundProperty, "Theme.Text");
            var frame = new Border { BorderThickness = new Thickness(1), Child = signatureText, Margin = new Thickness(0, 0, 8, 8) };
            frame.SetResourceReference(Border.BackgroundProperty, "Theme.Popup.Background");
            frame.SetResourceReference(Border.BorderBrushProperty, "Theme.Popup.Border");
            signature = new Popup { PlacementTarget = Box, Placement = PlacementMode.Top, AllowsTransparency = true, StaysOpen = true, Child = frame };

            Box.LostKeyboardFocus += (s, e) => { if (!completion.IsMouseOver) CloseCompletion(); signature.IsOpen = false; };
            Unloaded += (s, e) => { CloseCompletion(); signature.IsOpen = false; };
        }

        /// <summary>Клавиши, пока список открыт; Ctrl+Пробел открывает его. true — клавиша съедена.</summary>
        bool CompletionKey(KeyEventArgs e)
        {
            if (e.Key == Key.Space && Keyboard.Modifiers == ModifierKeys.Control)
            {
                OpenCompletion(true);
                e.Handled = true;
                return true;
            }
            if (e.Key == Key.Escape && signature.IsOpen && !completion.IsOpen)
            {
                signature.IsOpen = false;
                e.Handled = true;
                return true;
            }
            if (!completion.IsOpen) return false;
            switch (e.Key)
            {
                case Key.Down: completion.Move(1); break;
                case Key.Up: completion.Move(-1); break;
                case Key.PageDown: completion.Move(8); break;
                case Key.PageUp: completion.Move(-8); break;
                case Key.Enter:
                case Key.Tab:
                    if (Keyboard.Modifiers != ModifierKeys.None) return false;
                    AcceptCompletion();
                    break;
                case Key.Escape: CloseCompletion(); break;
                default: return false;
            }
            e.Handled = true;
            return true;
        }

        void CompletionTextInput(TextCompositionEventArgs e)
        {
            typedName = e.Text.Length == 1 && (char.IsLetterOrDigit(e.Text[0]) || e.Text[0] == '_');
            if (e.Text == "(")
            {
                if (completion.IsOpen) CloseCompletion();
                Dispatcher.BeginInvoke(new Action(ShowSignature), System.Windows.Threading.DispatcherPriority.Input);
            }
        }

        /// <summary>После правки: список следует за набранным, а после двух букв имени открывается сам.</summary>
        void CompletionAfterEdit()
        {
            if (Box.IsReadOnly) { CloseCompletion(); return; }
            if (completion.IsOpen)
            {
                int start = NameStart(Box.CaretIndex);
                if (start != completionStart || Box.CaretIndex == start) { CloseCompletion(); return; }
                completion.Filter(text.Substring(start, Box.CaretIndex - start));
                if (!completion.HasItems) CloseCompletion();
                return;
            }
            if (typedName)
            {
                typedName = false;
                int start = NameStart(Box.CaretIndex);
                if (CompletionAfterLetters > 0 && Box.CaretIndex - start >= CompletionAfterLetters && !char.IsDigit(text[start]) && InCode(start))
                    OpenCompletion(false);
            }
        }

        /// <summary>Каретка ушла с имени — список закрывается; ушла из скобок вызова — закрывается и объявление.</summary>
        void CompletionAfterCaret()
        {
            if (Box.Text.Length != text.Length) return;        // правка ещё не разобрана — решит CompletionAfterEdit
            if (completion.IsOpen)
            {
                int caret = Box.CaretIndex;
                if (caret < completionStart || NameStart(caret) != completionStart || Box.SelectionLength > 0) CloseCompletion();
            }
            if (signature.IsOpen && !InsideCall()) signature.IsOpen = false;
        }

        void OpenCompletion(bool explicitly)
        {
            if (Box.IsReadOnly) return;
            int caret = Box.CaretIndex;
            int start = NameStart(caret);
            completionStart = start;
            completion.SetSymbols(Gather(start));
            completion.Filter(text.Substring(start, caret - start));
            if (!completion.HasItems) { completionStart = -1; return; }
            Rect at = Box.GetRectFromCharacterIndex(start);
            completion.PlacementRectangle = at.IsEmpty ? Rect.Empty : new Rect(at.X - 22, at.Y, 1, at.Height);
            completion.IsOpen = true;
        }

        void CloseCompletion()
        {
            if (completion != null) completion.IsOpen = false;
            completionStart = -1;
        }

        void AcceptCompletion()
        {
            var s = completion.Selected;
            int start = completionStart;
            CloseCompletion();
            if (s == null || start < 0) return;
            int end = NameEnd(Box.CaretIndex);
            Box.Select(start, end - start);
            Box.SelectedText = s.Name;
            Box.Select(start + s.Name.Length, 0);
        }

        // ---- объявление функции над строкой после «имя(» ----

        void ShowSignature()
        {
            int paren = Box.CaretIndex - 1;
            if (paren < 0 || paren >= text.Length || text[paren] != '(') return;
            int k = paren;
            while (k > 0 && (text[k - 1] == ' ' || text[k - 1] == '\t')) k--;
            int end = k, start = NameStart(end);
            if (start >= end || !InCode(start)) return;
            string name = text.Substring(start, end - start);
            var found = Gather(-1).FirstOrDefault(x => x.Name == name && (x.Kind == SymbolKind.Function || x.Kind == SymbolKind.Macro) && x.Detail != null);
            if (found == null) return;
            signatureParen = paren;
            signatureText.Text = found.Detail + (string.IsNullOrEmpty(found.Source) ? "" : "   — " + found.Source);
            Rect at = Box.GetRectFromCharacterIndex(start);
            signature.PlacementRectangle = at.IsEmpty ? Rect.Empty : new Rect(at.X - 8, at.Y, 1, at.Height);
            signature.IsOpen = true;
        }

        bool InsideCall()
        {
            int caret = Box.CaretIndex;
            if (signatureParen < 0 || signatureParen >= text.Length || caret <= signatureParen || text[signatureParen] != '(') return false;
            int depth = 0;
            for (int i = signatureParen; i < caret; i++)
            {
                char c = text[i];
                if (c == '(') depth++;
                else if (c == ')') { depth--; if (depth == 0) return false; }
                else if (c == ';' || c == '{' || c == '}') return false;
            }
            return depth > 0;
        }

        /// <summary>Снимок для проверки (--window … complete:): каретка после набранного начала имени, список открыт.</summary>
        internal void ShowCompletionAt(int line, int column)
        {
            int index = Math.Min(lineStarts[Math.Max(0, Math.Min(line - 1, lineStarts.Count - 1))] + column - 1, text.Length);
            FocusEditor();
            Box.Select(index, 0);
            Box.ScrollToLine(Math.Max(0, line - 6));
            UpdateLayout();
            OpenCompletion(true);
        }

        /// <summary>Проверка без окна (--completetest): что подсказка предложит на набранное prefix в этом тексте.</summary>
        internal List<Symbol> CandidatesFor(string prefix)
        {
            return Gather(-1)
                .Select(s => new { s, rank = CompletionPopup.Rank(s.Name, prefix) })
                .Where(x => x.rank >= 0)
                .OrderBy(x => x.rank).ThenBy(x => x.s.Name.Length).ThenBy(x => x.s.Name, StringComparer.Ordinal)
                .Select(x => x.s).ToList();
        }

        // ---- откуда имена ----

        /// <summary>Все имена для подсказки: заголовки, этот и другие открытые файлы, слова Си, слова текста (кроме дополняемого).</summary>
        List<Symbol> Gather(int typingAt)
        {
            var byName = new Dictionary<string, Symbol>(StringComparer.Ordinal);
            Action<Symbol> add = s =>
            {
                Symbol had;
                if (string.IsNullOrEmpty(s.Name)) return;
                if (!byName.TryGetValue(s.Name, out had) || Weight(s.Kind) < Weight(had.Kind)) byName[s.Name] = s;
            };
            foreach (var s in Declarations.Parse(text, "")) add(s);
            if (OpenTexts != null)
                foreach (var other in OpenTexts())
                    if (!ReferenceEquals(other.Value, text))
                        foreach (var s in Declarations.Parse(other.Value, other.Key)) add(s);
            foreach (var s in HeaderIndex.For(text, FileFolder)) add(s);
            foreach (string w in CLexer.Keywords) add(new Symbol { Name = w, Kind = SymbolKind.Keyword });
            foreach (string w in CLexer.Types) add(new Symbol { Name = w, Kind = SymbolKind.Type });
            foreach (string w in CLexer.Constants) add(new Symbol { Name = w, Kind = SymbolKind.Constant });
            foreach (Match m in Regex.Matches(text, @"\b[A-Za-z_]\w{2,}\b"))
                if (m.Index != typingAt && !byName.ContainsKey(m.Value))
                    byName[m.Value] = new Symbol { Name = m.Value, Kind = SymbolKind.Word };
            // имя без буквы A/W (#define CreateWindowEx __AW(CreateWindowEx)) — объявление его A-функции:
            // без UNICODE оно и зовётся
            foreach (var s in byName.Values.ToList())
            {
                Symbol a;
                if (s.Detail == null || !s.Detail.EndsWith("__AW(" + s.Name + ")") || !byName.TryGetValue(s.Name + "A", out a) || a.Detail == null) continue;
                byName[s.Name] = new Symbol
                {
                    Name = s.Name,
                    Kind = a.Kind,
                    Detail = Regex.Replace(a.Detail, @"\b" + Regex.Escape(a.Name) + @"\b", s.Name) + "   (" + a.Name + " / " + s.Name + "W)",
                    Source = s.Source
                };
            }
            return byName.Values.ToList();
        }

        static int Weight(SymbolKind k)
        {
            switch (k)
            {
                case SymbolKind.Function: return 0;
                case SymbolKind.Macro: return 1;
                case SymbolKind.Type: return 2;
                case SymbolKind.Variable: return 3;
                case SymbolKind.Constant: return 4;
                case SymbolKind.Keyword: return 5;
                default: return 6;
            }
        }

        static bool IsNameChar(char c) { return char.IsLetterOrDigit(c) || c == '_'; }

        int NameStart(int i)
        {
            while (i > 0 && IsNameChar(text[i - 1])) i--;
            return i;
        }

        int NameEnd(int i)
        {
            while (i < text.Length && IsNameChar(text[i])) i++;
            return i;
        }

        /// <summary>Место — код, а не комментарий, строка, число или имя файла в #include.</summary>
        bool InCode(int index)
        {
            int line = FindLine(index);
            var tokens = new List<Token>();
            CLexer.LexLine(text, lineStarts[line], LineEnd(line), lineStates[line], tokens);
            foreach (var t in tokens)
                if (index >= t.Start && index < t.Start + t.Length)
                    return t.Kind != TokenKind.Comment && t.Kind != TokenKind.String && t.Kind != TokenKind.Number;
            return true;
        }
    }
}
