using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using ThemeLibrary;
using WinCC.Build;

namespace WinCC.Projects
{
    /// <summary>Свойства проекта: вид программы, имя готового файла, макросы, папки #include, DLL, стек, другие параметры wcc.
    /// Списки пишутся через «;». ОК — значения уходят в проект (сохраняет окно).</summary>
    public class ProjectDialog : ThemedDialog
    {
        readonly Project project;
        readonly ComboBox kindBox = new ComboBox();
        readonly TextBox outputBox = Field();
        readonly TextBox definesBox = Field();
        readonly TextBox includesBox = Field();
        readonly TextBox libsBox = Field();
        readonly NumericUpDown stackBox = new NumericUpDown { Minimum = 0, Maximum = 1024, Width = 120, HorizontalAlignment = HorizontalAlignment.Left };
        readonly TextBox optionsBox = Field();

        public ProjectDialog(Window owner, Project project)
        {
            this.project = project;
            Owner = owner;
            Title = "Свойства проекта «" + project.Name + "»";
            PrimaryText = "Сохранить";

            foreach (ProgramKind k in ProgramKinds.All)
                kindBox.Items.Add(new ComboBoxItem { Content = ProgramKinds.Title(k), Tag = k });
            kindBox.SelectedIndex = Array.IndexOf(ProgramKinds.All, project.Kind);
            kindBox.Width = 160;
            kindBox.HorizontalAlignment = HorizontalAlignment.Left;
            outputBox.Text = project.Output;
            definesBox.Text = Join(project.Defines);
            includesBox.Text = Join(project.Includes);
            libsBox.Text = Join(project.Libs);
            stackBox.Value = project.StackMb;
            optionsBox.Text = project.Options;

            var grid = new Grid { Width = 520, Margin = new Thickness(4, 4, 4, 8) };
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            AddRow(grid, "Вид программы", kindBox);
            AddRow(grid, "Готовый файл", outputBox, "Пусто — " + project.Name + ".exe / .dll");
            AddRow(grid, "Макросы (-D)", definesBox, "DEBUG; VERSION=2");
            AddRow(grid, "Папки #include (-I)", includesBox);
            AddRow(grid, "DLL (-l)", libsBox, "dwmapi; mylib");
            AddRow(grid, "Стек, МБ", stackBox, "0 — 8 МБ");
            AddRow(grid, "Другие параметры", optionsBox, "-S");
            Content = grid;

            Accepting += (s, e) => Apply();
            Loaded += (s, e) => outputBox.Focus();
        }

        void Apply()
        {
            var item = kindBox.SelectedItem as ComboBoxItem;
            if (item != null) project.Kind = (ProgramKind)item.Tag;
            project.Output = outputBox.Text.Trim();
            Fill(project.Defines, definesBox.Text);
            Fill(project.Includes, includesBox.Text);
            Fill(project.Libs, libsBox.Text);
            project.StackMb = (int)stackBox.Value;
            project.Options = optionsBox.Text.Trim();
        }

        static TextBox Field() { return new TextBox { MinWidth = 300 }; }

        static string Join(IEnumerable<string> items) { return string.Join("; ", items); }

        static void Fill(List<string> list, string text)
        {
            list.Clear();
            list.AddRange(text.Split(';').Select(x => x.Trim()).Where(x => x.Length > 0));
        }

        static void AddRow(Grid grid, string label, FrameworkElement field, string hint = null)
        {
            int row = grid.RowDefinitions.Count;
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            var text = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 14, 8) };
            Grid.SetRow(text, row);
            grid.Children.Add(text);

            field.Margin = new Thickness(0, 0, 0, 8);
            field.VerticalAlignment = VerticalAlignment.Center;
            if (hint != null)
            {
                if (field is TextBox) ThemeAssist.SetPlaceholder(field, hint);
                else field.ToolTip = hint;
            }
            Grid.SetRow(field, row);
            Grid.SetColumn(field, 1);
            grid.Children.Add(field);
        }
    }
}
