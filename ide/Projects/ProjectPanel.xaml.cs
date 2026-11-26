using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using ThemeLibrary;
using WinCC.Build;

namespace WinCC.Projects
{
    /// <summary>
    /// Панель проекта сбоку окна: имя проекта, файлы по группам (исходники, заголовки, ресурсы, прочие), что получится при сборке.
    /// Без проекта — кнопки «Новый проект», «Открыть проект» и недавние проекты.
    /// Сама ничего не меняет: просьбы уходят окну событиями (MainWindow.Project.cs).
    /// </summary>
    public partial class ProjectPanel : UserControl
    {
        Project project;
        string activeFile;

        public event Action<string> OpenFileRequested;
        public event Action<string> RemoveFileRequested;
        public event Action<string[]> FilesDropped;
        public event Action AddFilesRequested;
        public event Action AddFolderRequested;
        /// <summary>Убрать из проекта все файлы папки (полный путь папки).</summary>
        public event Action<string> RemoveFolderRequested;
        public event Action NewFileRequested;
        public event Action PropertiesRequested;
        public event Action CloseProjectRequested;
        public event Action NewProjectRequested;
        /// <summary>Открыть проект: путь из недавних или null — спросить файл.</summary>
        public event Action<string> OpenProjectRequested;

        public ProjectPanel()
        {
            InitializeComponent();
            AddButton.Click += (s, e) => AddFilesRequested?.Invoke();
            AddFolderButton.Click += (s, e) => AddFolderRequested?.Invoke();
            NoFilesAddButton.Click += (s, e) => AddFilesRequested?.Invoke();
            NewFileButton.Click += (s, e) => NewFileRequested?.Invoke();
            PropertiesButton.Click += (s, e) => PropertiesRequested?.Invoke();
            CloseButton.Click += (s, e) => CloseProjectRequested?.Invoke();
            NewProjectButton.Click += (s, e) => NewProjectRequested?.Invoke();
            OpenProjectButton.Click += (s, e) => OpenProjectRequested?.Invoke(null);

            Tree.MouseDoubleClick += (s, e) =>
            {
                string path = FileOf(e.OriginalSource as DependencyObject);
                if (path != null) { e.Handled = true; OpenFileRequested?.Invoke(path); }
            };
            Tree.KeyDown += Tree_KeyDown;
            Tree.PreviewMouseRightButtonDown += (s, e) =>
            {
                var item = ItemOf(e.OriginalSource as DependencyObject);
                if (item != null) item.IsSelected = true;
            };

            RecentList.MouseUp += (s, e) =>
            {
                var item = ItemsControl.ContainerFromElement(RecentList, e.OriginalSource as DependencyObject) as ListBoxItem;
                if (item != null && e.ChangedButton == MouseButton.Left) OpenProjectRequested?.Invoke(item.Tag as string);
            };
            RecentList.KeyDown += (s, e) =>
            {
                var item = RecentList.SelectedItem as ListBoxItem;
                if (e.Key == Key.Enter && item != null) { e.Handled = true; OpenProjectRequested?.Invoke(item.Tag as string); }
            };

            DragOver += (s, e) =>
            {
                e.Effects = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
                e.Handled = true;
            };
            Drop += (s, e) =>
            {
                var files = e.Data.GetData(DataFormats.FileDrop) as string[];
                if (files == null) return;
                e.Handled = true;
                FilesDropped?.Invoke(files);
            };
        }

        public Project Project
        {
            get { return project; }
            set { project = value; Refresh(); }
        }

        /// <summary>Файл активной вкладки — в дереве выделяется жирным.</summary>
        public void SetActiveFile(string path)
        {
            activeFile = path;
            foreach (var item in FileItems())
                ((TextBlock)((StackPanel)item.Header).Children[1]).FontWeight = IsActive(item.Tag as string) ? FontWeights.SemiBold : FontWeights.Normal;
        }

        public void SetRecent(IEnumerable<string> paths)
        {
            RecentList.Items.Clear();
            foreach (string p in paths)
            {
                var row = new StackPanel { Orientation = Orientation.Horizontal };
                row.Children.Add(new Icon { Kind = IconKind.Layers, Width = 13, Height = 13, Margin = new Thickness(0, 0, 7, 0), VerticalAlignment = VerticalAlignment.Center });
                row.Children.Add(new TextBlock { Text = Path.GetFileNameWithoutExtension(p), VerticalAlignment = VerticalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis });
                RecentList.Items.Add(new ListBoxItem { Content = row, Tag = p, ToolTip = p, Padding = new Thickness(6, 4, 6, 4) });
            }
            RecentTitle.Visibility = RecentList.Items.Count > 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        /// <summary>Перерисовать дерево по проекту (после правки списка или свойств).</summary>
        public void Refresh()
        {
            ProjectView.Visibility = project != null ? Visibility.Visible : Visibility.Collapsed;
            EmptyView.Visibility = project != null ? Visibility.Collapsed : Visibility.Visible;
            Tree.Items.Clear();
            if (project == null) return;

            NameText.Text = project.Name;
            NameText.ToolTip = project.Path;
            OutputText.Text = ProgramKinds.Title(project.Kind) + " → " + project.OutputName;
            OutputText.ToolTip = project.OutputPath;

            var sources = project.Files.Where(f => Project.IsBuildFile(f) && !Project.IsResource(f) && !Project.IsLibrary(f)).ToList();
            var headers = project.Files.Where(Project.IsHeader).ToList();
            var resources = project.Files.Where(Project.IsResource).ToList();
            var libraries = project.Files.Where(Project.IsLibrary).ToList();
            var other = project.Files.Where(f => !Project.IsBuildFile(f) && !Project.IsHeader(f)).ToList();
            AddGroup(Loc.T("GroupSources"), IconKind.Code, sources);
            AddGroup(Loc.T("GroupHeaders"), IconKind.FileText, headers);
            AddGroup(Loc.T("GroupLibraries"), IconKind.Archive, libraries);
            AddGroup(Loc.T("GroupResources"), IconKind.Image, resources);
            AddGroup(Loc.T("GroupOther"), IconKind.File, other);
            NoFilesView.Visibility = project.Files.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            SetActiveFile(activeFile);
        }

        /// <summary>Папка в дереве: её полный путь (для меню «убрать папку», «показать в папке»).</summary>
        sealed class FolderNode
        {
            public string FullPath;
        }

        void AddGroup(string title, IconKind icon, List<string> files)
        {
            if (files.Count == 0) return;
            var group = new TreeViewItem { Header = Row(IconKind.Folder, title + " (" + files.Count + ")", true), IsExpanded = true, Focusable = false };
            // файлы во вложенных папках — под узлами этих папок: src\util\str.c → src › util › str.c
            var folders = new Dictionary<string, TreeViewItem>(StringComparer.OrdinalIgnoreCase);
            foreach (string f in files.OrderBy(x => FolderOf(x), StringComparer.OrdinalIgnoreCase).ThenBy(x => Path.GetFileName(x), StringComparer.OrdinalIgnoreCase))
            {
                string full = project.FullPath(f);
                bool exists = File.Exists(full);
                var item = new TreeViewItem { Header = Row(exists ? icon : IconKind.Warning, Path.GetFileName(f), exists), Tag = full };
                item.ToolTip = exists ? full : Loc.F("MissingOnDisk", full);
                item.ContextMenu = FileMenu(full);
                FolderItem(group, folders, FolderOf(f)).Items.Add(item);
            }
            Tree.Items.Add(group);
        }

        /// <summary>Папка файла, как она записана в проекте: «» — сама папка проекта; путь вне проекта — полный.</summary>
        static string FolderOf(string file)
        {
            return Path.GetDirectoryName(file) ?? "";
        }

        /// <summary>Узел папки (и все узлы над ней), созданный при первой надобности.</summary>
        ItemsControl FolderItem(TreeViewItem group, Dictionary<string, TreeViewItem> folders, string folder)
        {
            if (folder.Length == 0) return group;
            TreeViewItem node;
            if (folders.TryGetValue(folder, out node)) return node;
            // папка вне проекта (полный путь) — один узел с полным путём, без разбивки
            bool outside = Path.IsPathRooted(folder) || folder.StartsWith("..");
            string parent = outside ? "" : FolderOf(folder);
            ItemsControl owner = FolderItem(group, folders, parent);
            string full = project.FullPath(folder);
            node = new TreeViewItem
            {
                Header = Row(IconKind.FolderOpen, outside ? folder : Path.GetFileName(folder), true),
                IsExpanded = true,
                Tag = new FolderNode { FullPath = full },
                ToolTip = full
            };
            node.ContextMenu = FolderMenu(full);
            owner.Items.Add(node);
            folders[folder] = node;
            return node;
        }

        ContextMenu FolderMenu(string full)
        {
            var menu = new ContextMenu();
            menu.Items.Add(MakeItem(Loc.T("RemoveFolder"), IconKind.Minus, () => RemoveFolderRequested?.Invoke(full)));
            menu.Items.Add(new Separator());
            menu.Items.Add(MakeItem(Loc.T("ShowInFolder"), IconKind.Folder, () => ShowFolder(full)));
            return menu;
        }

        static void ShowFolder(string full)
        {
            try { if (Directory.Exists(full)) System.Diagnostics.Process.Start("explorer.exe", "\"" + full + "\""); }
            catch (Exception) { }
        }

        static StackPanel Row(IconKind icon, string text, bool normal)
        {
            var row = new StackPanel { Orientation = Orientation.Horizontal };
            var mark = new Icon { Kind = icon, Width = 14, Height = 14, Margin = new Thickness(0, 0, 7, 0), VerticalAlignment = VerticalAlignment.Center };
            var label = new TextBlock { Text = text, VerticalAlignment = VerticalAlignment.Center };
            if (!normal)
            {
                mark.SetResourceReference(Icon.ForegroundProperty, "Theme.Warning");
                label.SetResourceReference(TextBlock.ForegroundProperty, "Theme.TextDim");
            }
            row.Children.Add(mark);
            row.Children.Add(label);
            return row;
        }

        ContextMenu FileMenu(string full)
        {
            var menu = new ContextMenu();
            menu.Items.Add(MakeItem(Loc.T("Open"), IconKind.FolderOpen, () => OpenFileRequested?.Invoke(full)));
            menu.Items.Add(MakeItem(Loc.T("RemoveFromProject"), IconKind.Minus, () => RemoveFileRequested?.Invoke(full)));
            menu.Items.Add(new Separator());
            menu.Items.Add(MakeItem(Loc.T("ShowInFolder"), IconKind.Folder, () => ShowInFolder(full)));
            return menu;
        }

        static MenuItem MakeItem(string text, IconKind icon, Action click)
        {
            var item = new MenuItem { Header = text, Icon = new Icon { Kind = icon, Width = 14, Height = 14 } };
            item.Click += (s, e) => click();
            return item;
        }

        static void ShowInFolder(string full)
        {
            try
            {
                if (File.Exists(full)) System.Diagnostics.Process.Start("explorer.exe", "/select,\"" + full + "\"");
                else if (Directory.Exists(Path.GetDirectoryName(full))) System.Diagnostics.Process.Start("explorer.exe", "\"" + Path.GetDirectoryName(full) + "\"");
            }
            catch (Exception) { }
        }

        void Tree_KeyDown(object sender, KeyEventArgs e)
        {
            var item = Tree.SelectedItem as TreeViewItem;
            var folder = item != null ? item.Tag as FolderNode : null;
            if (folder != null && e.Key == Key.Delete) { e.Handled = true; RemoveFolderRequested?.Invoke(folder.FullPath); return; }
            string path = item != null ? item.Tag as string : null;
            if (path == null) return;
            if (e.Key == Key.Enter) { e.Handled = true; OpenFileRequested?.Invoke(path); }
            else if (e.Key == Key.Delete) { e.Handled = true; RemoveFileRequested?.Invoke(path); }
        }

        bool IsActive(string path)
        {
            return path != null && activeFile != null && string.Equals(path, activeFile, StringComparison.OrdinalIgnoreCase);
        }

        IEnumerable<TreeViewItem> FileItems()
        {
            return Tree.Items.OfType<TreeViewItem>().SelectMany(Descendants).Where(i => i.Tag is string);
        }

        static IEnumerable<TreeViewItem> Descendants(TreeViewItem item)
        {
            foreach (var child in item.Items.OfType<TreeViewItem>())
            {
                yield return child;
                foreach (var d in Descendants(child)) yield return d;
            }
        }

        static TreeViewItem ItemOf(DependencyObject d)
        {
            while (d != null && !(d is TreeViewItem))
                d = d is System.Windows.Media.Visual || d is System.Windows.Media.Media3D.Visual3D ? System.Windows.Media.VisualTreeHelper.GetParent(d) : LogicalTreeHelper.GetParent(d);
            return d as TreeViewItem;
        }

        static string FileOf(DependencyObject d)
        {
            var item = ItemOf(d);
            return item != null ? item.Tag as string : null;
        }
    }
}
