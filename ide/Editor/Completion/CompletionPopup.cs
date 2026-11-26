using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;

namespace WinCC.Editor.Completion
{
    /// <summary>
    /// Список подсказок под кареткой: имена, которые начинаются с набранного (или содержат его), значок вида слева,
    /// заголовок-источник справа, внизу — объявление выбранного имени. Мышью: щелчок выбирает, двойной — вставляет.
    /// </summary>
    public class CompletionPopup : Popup
    {
        const int MaxShown = 300;

        readonly ListBox list = new ListBox();
        readonly TextBlock detail = new TextBlock { TextWrapping = TextWrapping.Wrap, Margin = new Thickness(8, 5, 8, 6) };
        readonly Border detailBorder = new Border { BorderThickness = new Thickness(0, 1, 0, 0) };
        List<Symbol> all = new List<Symbol>();

        /// <summary>Двойной щелчок по имени — вставить его.</summary>
        public event EventHandler Accepted;

        public CompletionPopup()
        {
            StaysOpen = true;
            AllowsTransparency = true;
            Placement = PlacementMode.Bottom;

            list.Focusable = false;
            list.BorderThickness = new Thickness(0);
            list.Background = Brushes.Transparent;
            list.MaxHeight = 260;
            list.MinWidth = 300;
            list.MaxWidth = 560;
            ScrollViewer.SetHorizontalScrollBarVisibility(list, ScrollBarVisibility.Disabled);
            VirtualizingPanel.SetIsVirtualizing(list, true);
            list.SelectionChanged += (s, e) => ShowDetail();
            list.MouseDoubleClick += (s, e) => { if (list.SelectedItem != null) Accepted?.Invoke(this, EventArgs.Empty); };

            detail.SetResourceReference(TextBlock.ForegroundProperty, "Theme.TextDim");
            detail.SetResourceReference(TextBlock.FontFamilyProperty, "Theme.Font.Mono");
            detail.FontSize = 12;
            detail.MaxWidth = 560;
            detailBorder.SetResourceReference(Border.BorderBrushProperty, "Theme.Popup.Border");
            detailBorder.Child = detail;

            var stack = new DockPanel();
            DockPanel.SetDock(detailBorder, Dock.Bottom);
            stack.Children.Add(detailBorder);
            stack.Children.Add(list);

            var frame = new Border { BorderThickness = new Thickness(1), Padding = new Thickness(2), Child = stack, Margin = new Thickness(0, 2, 8, 8) };
            frame.SetResourceReference(Border.BackgroundProperty, "Theme.Popup.Background");
            frame.SetResourceReference(Border.BorderBrushProperty, "Theme.Popup.Border");
            frame.SetResourceReference(Border.EffectProperty, "Theme.Popup.Effect");
            Child = frame;
        }

        public Symbol Selected { get { var i = list.SelectedItem as ListBoxItem; return i == null ? null : (Symbol)i.Tag; } }

        public bool HasItems { get { return list.Items.Count > 0; } }

        /// <summary>Набор имён, из которого фильтруются подсказки.</summary>
        public void SetSymbols(List<Symbol> symbols) { all = symbols; }

        /// <summary>Оставить имена под набранное: сначала начинающиеся с него (точный регистр выше), потом содержащие.</summary>
        public void Filter(string prefix)
        {
            IEnumerable<Symbol> found;
            if (string.IsNullOrEmpty(prefix)) found = all.OrderBy(s => s.Name, StringComparer.OrdinalIgnoreCase);
            else
                found = all
                    .Select(s => new { s, rank = Rank(s.Name, prefix) })
                    .Where(x => x.rank >= 0)
                    .OrderBy(x => x.rank).ThenBy(x => x.s.Name.Length).ThenBy(x => x.s.Name, StringComparer.Ordinal)
                    .Select(x => x.s);
            list.Items.Clear();
            foreach (var s in found.Take(MaxShown)) list.Items.Add(Row(s, prefix));
            if (list.Items.Count > 0)
            {
                list.SelectedIndex = 0;
                list.ScrollIntoView(list.Items[0]);
            }
            ShowDetail();
        }

        internal static int Rank(string name, string prefix)
        {
            // служебные имена (_Post_invalid_, __AW…) — только когда и набирают с «_»
            if (name.StartsWith("_") && !prefix.StartsWith("_")) return -1;
            if (name == prefix) return 0;
            if (name.StartsWith(prefix, StringComparison.Ordinal)) return 1;
            if (name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) return 2;
            int at = name.IndexOf(prefix, StringComparison.OrdinalIgnoreCase);
            if (at > 0 && prefix.Length >= 2) return 3;
            return -1;
        }

        /// <summary>Стрелки, страницы: сдвинуть выбор.</summary>
        public void Move(int delta)
        {
            if (list.Items.Count == 0) return;
            int i = Math.Max(0, Math.Min(list.Items.Count - 1, list.SelectedIndex + delta));
            list.SelectedIndex = i;
            list.ScrollIntoView(list.Items[i]);
        }

        void ShowDetail()
        {
            var s = Selected;
            string text = s == null ? "" : (s.Detail ?? s.Name) + (string.IsNullOrEmpty(s.Source) ? "" : "   — " + s.Source);
            detail.Text = text;
            detailBorder.Visibility = string.IsNullOrEmpty(text) || s.Kind == SymbolKind.Keyword || s.Kind == SymbolKind.Word
                ? Visibility.Collapsed : Visibility.Visible;
        }

        static ListBoxItem Row(Symbol s, string prefix)
        {
            var badge = new TextBlock
            {
                Text = Badge(s.Kind),
                Width = 18,
                FontWeight = FontWeights.Bold,
                TextAlignment = TextAlignment.Center,
                Foreground = Color(s.Kind),
                VerticalAlignment = VerticalAlignment.Center
            };
            var name = new TextBlock { Text = s.Name, Margin = new Thickness(4, 0, 12, 0), VerticalAlignment = VerticalAlignment.Center };
            name.SetResourceReference(TextBlock.FontFamilyProperty, "Theme.Font.Mono");
            name.SetResourceReference(TextBlock.ForegroundProperty, "Theme.Text");
            var source = new TextBlock { Text = s.Source ?? "", FontSize = 11, VerticalAlignment = VerticalAlignment.Center };
            source.SetResourceReference(TextBlock.ForegroundProperty, "Theme.TextDim");
            var row = new DockPanel();
            DockPanel.SetDock(badge, Dock.Left);
            DockPanel.SetDock(source, Dock.Right);
            row.Children.Add(badge);
            row.Children.Add(source);
            row.Children.Add(name);
            return new ListBoxItem { Content = row, Tag = s, Padding = new Thickness(3, 1, 6, 1) };
        }

        static string Badge(SymbolKind k)
        {
            switch (k)
            {
                case SymbolKind.Function: return "ƒ";
                case SymbolKind.Macro: return "#";
                case SymbolKind.Type: return "T";
                case SymbolKind.Constant: return "c";
                case SymbolKind.Variable: return "v";
                case SymbolKind.Keyword: return "k";
                default: return "·";
            }
        }

        static Brush Color(SymbolKind k)
        {
            switch (k)
            {
                case SymbolKind.Function: return SyntaxColors.For(TokenKind.Function);
                case SymbolKind.Macro: return SyntaxColors.For(TokenKind.Preprocessor);
                case SymbolKind.Type: return SyntaxColors.For(TokenKind.Type);
                case SymbolKind.Constant: return SyntaxColors.For(TokenKind.Constant);
                case SymbolKind.Keyword: return SyntaxColors.For(TokenKind.Keyword);
                case SymbolKind.Variable: return SyntaxColors.Text;
                default: return SyntaxColors.Dim;
            }
        }
    }
}
