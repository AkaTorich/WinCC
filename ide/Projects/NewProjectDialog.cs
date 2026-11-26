using System;
using System.Windows;
using System.Windows.Controls;
using ThemeLibrary;
using WinCC.Build;

namespace WinCC.Projects
{
    /// <summary>Новый проект: какой вид — консольная программа, оконная (WinMain), DLL или своя библиотека .lib.
    /// От вида зависит заготовка (Templates.cs).</summary>
    public class NewProjectDialog : ThemedDialog
    {
        readonly ListBox list = new ListBox { Width = 460, BorderThickness = new Thickness(0) };

        public ProgramKind Kind { get; private set; }

        public NewProjectDialog(Window owner, ProgramKind current)
        {
            Owner = owner;
            Kind = current;
            Title = Loc.T("NewProjectTitle");
            PrimaryText = Loc.T("NewProjectNext");

            foreach (ProgramKind k in ProgramKinds.All)
            {
                var row = new StackPanel { Margin = new Thickness(4, 6, 4, 6) };
                row.Children.Add(new TextBlock { Text = ProgramKinds.Title(k), FontWeight = FontWeights.SemiBold });
                var hint = new TextBlock { Text = Loc.T("NewKind" + k), TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 2, 0, 0) };
                hint.SetResourceReference(TextBlock.ForegroundProperty, "Theme.TextDim");
                row.Children.Add(hint);
                list.Items.Add(new ListBoxItem { Content = row, Tag = k });
            }
            list.SelectedIndex = Array.IndexOf(ProgramKinds.All, current);
            list.MouseDoubleClick += (s, e) => { if (list.SelectedItem != null) { Take(); DialogResult = true; } };
            Content = list;
            Accepting += (s, e) => Take();
            Loaded += (s, e) => list.Focus();
        }

        void Take()
        {
            var item = list.SelectedItem as ListBoxItem;
            if (item != null) Kind = (ProgramKind)item.Tag;
        }
    }
}
