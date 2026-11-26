using System;
using System.Windows;
using System.Windows.Controls;
using ThemeLibrary;

namespace WinCC
{
    /// <summary>Editor settings: interface language (each language is named in itself). OK returns the choice in Language.</summary>
    public class SettingsDialog : ThemedDialog
    {
        readonly ComboBox languageBox = new ComboBox { Width = 180, HorizontalAlignment = HorizontalAlignment.Left, VerticalAlignment = VerticalAlignment.Center };

        public UiLanguage Language { get; private set; }

        public SettingsDialog(Window owner, UiLanguage current)
        {
            Owner = owner;
            Language = current;
            Title = Loc.T("SettingsTitle");
            PrimaryText = Loc.T("Save");

            foreach (UiLanguage lang in Loc.All)
                languageBox.Items.Add(new ComboBoxItem { Content = Loc.NativeName(lang), Tag = lang });
            languageBox.SelectedIndex = Array.IndexOf(Loc.All, current);

            var grid = new Grid { Width = 360, Margin = new Thickness(4, 4, 4, 8) };
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            var label = new TextBlock { Text = Loc.T("Language"), VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 14, 0) };
            grid.Children.Add(label);
            Grid.SetColumn(languageBox, 1);
            grid.Children.Add(languageBox);
            Content = grid;

            Accepting += (s, e) =>
            {
                var item = languageBox.SelectedItem as ComboBoxItem;
                if (item != null) Language = (UiLanguage)item.Tag;
            };
            Loaded += (s, e) => languageBox.Focus();
        }
    }
}
