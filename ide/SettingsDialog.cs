using System;
using System.Windows;
using System.Windows.Controls;
using ThemeLibrary;

namespace WinCC
{
    /// <summary>
    /// Настройки окна: язык (каждый назван на нём самом) и когда открываются подсказки ввода.
    /// «Сохранить» — выбор уходит в Language и HintLetters.
    /// </summary>
    public class SettingsDialog : ThemedDialog
    {
        readonly ComboBox languageBox = Box();
        readonly ComboBox hintsBox = Box();

        public new UiLanguage Language { get; private set; }
        /// <summary>После скольких букв открываются подсказки; 0 — только по Ctrl+Пробел.</summary>
        public int HintLetters { get; private set; }

        static ComboBox Box() { return new ComboBox { Width = 200, HorizontalAlignment = HorizontalAlignment.Left, VerticalAlignment = VerticalAlignment.Center }; }

        public SettingsDialog(Window owner, UiLanguage current, int hintLetters)
        {
            Owner = owner;
            Language = current;
            HintLetters = hintLetters;
            Title = Loc.T("SettingsTitle");
            PrimaryText = Loc.T("Save");

            foreach (UiLanguage lang in Loc.All)
                languageBox.Items.Add(new ComboBoxItem { Content = Loc.NativeName(lang), Tag = lang });
            languageBox.SelectedIndex = Array.IndexOf(Loc.All, current);

            for (int n = 0; n <= 4; n++)
                hintsBox.Items.Add(new ComboBoxItem
                {
                    Content = n == 0 ? Loc.T("HintsManual") : n == 1 ? Loc.T("HintsOne") : Loc.F("HintsMany", n),
                    Tag = n
                });
            hintsBox.SelectedIndex = Math.Max(0, Math.Min(4, hintLetters));

            var grid = new Grid { Width = 380, Margin = new Thickness(4, 4, 4, 8) };
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            AddRow(grid, 0, Loc.T("Language"), languageBox);
            AddRow(grid, 1, Loc.T("HintsAfter"), hintsBox);
            Content = grid;

            Accepting += (s, e) =>
            {
                var item = languageBox.SelectedItem as ComboBoxItem;
                if (item != null) Language = (UiLanguage)item.Tag;
                var hints = hintsBox.SelectedItem as ComboBoxItem;
                if (hints != null) HintLetters = (int)hints.Tag;
            };
            Loaded += (s, e) => languageBox.Focus();
        }

        static void AddRow(Grid grid, int row, string text, FrameworkElement field)
        {
            var label = new TextBlock { Text = text, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, row > 0 ? 10 : 0, 14, 0) };
            Grid.SetRow(label, row);
            grid.Children.Add(label);
            field.Margin = new Thickness(0, row > 0 ? 10 : 0, 0, 0);
            Grid.SetRow(field, row);
            Grid.SetColumn(field, 1);
            grid.Children.Add(field);
        }
    }
}
