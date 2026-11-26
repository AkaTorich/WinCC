using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using Microsoft.Win32;
using ThemeLibrary;
using WinCC.Build;
using WinCC.Projects;

namespace WinCC
{
    /// <summary>Проект: панель сбоку, новый / открыть / закрыть, файлы проекта, свойства, недавние проекты.
    /// Пока проект открыт, «Собрать» и «Запустить» собирают его целиком (MainWindow.Build.cs).</summary>
    public partial class MainWindow
    {
        const string ProjectFilesFilter = "Файлы Си и ресурсы (*.c; *.h; *.asm; *.rc)|*.c;*.h;*.asm;*.rc|Ресурсы (*.rc; *.res; *.ico; *.bmp; *.cur)|*.rc;*.res;*.ico;*.bmp;*.cur|Все файлы (*.*)|*.*";
        /// <summary>Заготовка нового .rc: значок программы — первым, Windows берёт его для ярлыков.</summary>
        const string NewRcText = "#include <windows.h>\r\n\r\n1 ICON \"app.ico\"\r\n";
        const int RecentLimit = 8;

        Project project;
        /// <summary>Выбор вида меняется из кода — не записывать его ни в проект, ни в настройки.</summary>
        bool syncingKind;

        void InitProjectPanel()
        {
            ProjectSide.OpenFileRequested += OpenProjectFile;
            ProjectSide.RemoveFileRequested += RemoveFromProject;
            ProjectSide.FilesDropped += OnProjectDrop;
            ProjectSide.AddFilesRequested += AddFilesToProject;
            ProjectSide.NewFileRequested += NewProjectFile;
            ProjectSide.PropertiesRequested += EditProjectProperties;
            ProjectSide.CloseProjectRequested += CloseProject;
            ProjectSide.NewProjectRequested += NewProject;
            ProjectSide.OpenProjectRequested += p => { if (p == null) OpenProjectDialog(); else OpenProject(p, true); };
            ProjectButton.Click += (s, e) => ShowProjectPanel(ProjectButton.IsChecked == true);
            ProjectSide.SetRecent(settings.Recent);
            ShowProjectPanel(settings.PanelShown);
        }

        void ShowProjectPanel(bool show)
        {
            if (show && ProjectColumn.Width.Value < 1) ProjectColumn.Width = new GridLength(settings.PanelWidth);
            ProjectColumn.MinWidth = show ? 170 : 0;
            if (!show)
            {
                if (ProjectColumn.ActualWidth > 0) settings.PanelWidth = ProjectColumn.ActualWidth;
                ProjectColumn.Width = new GridLength(0);
            }
            ProjectBorder.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
            ProjectSplitter.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
            ProjectButton.IsChecked = show;
            settings.PanelShown = show;
        }

        bool ProjectPanelShown { get { return ProjectBorder.Visibility == Visibility.Visible; } }

        void SyncKindBox()
        {
            syncingKind = true;
            KindBox.SelectedIndex = Array.IndexOf(ProgramKinds.All, project != null ? project.Kind : settings.Kind);
            syncingKind = false;
            UpdateStatus();
        }

        /// <summary>Вид программы выбран в окне: у проекта — в проект, без проекта — в настройки.</summary>
        void OnKindChosen()
        {
            if (syncingKind) return;
            if (project != null)
            {
                project.Kind = CurrentKind;
                SaveProject();
                ProjectSide.Refresh();
            }
            else settings.Kind = CurrentKind;
            UpdateStatus();
        }

        void OpenProjectDialog()
        {
            var dlg = new OpenFileDialog { Filter = Project.FileFilter, Title = "Открыть проект" };
            string folder = project != null ? project.Folder : DefaultFolder();
            if (folder != null) dlg.InitialDirectory = folder;
            if (dlg.ShowDialog(this) == true) OpenProject(dlg.FileName, true);
        }

        /// <summary>Открыть проект (прежний закрывается). openFirst — открыть первый исходник, если ни один файл проекта не открыт.</summary>
        public bool OpenProject(string path, bool openFirst)
        {
            Project loaded;
            try { loaded = Project.Load(path); }
            catch (Exception ex)
            {
                settings.Recent.RemoveAll(r => string.Equals(r, path, StringComparison.OrdinalIgnoreCase));
                ProjectSide.SetRecent(settings.Recent);
                ThemedMessageBox.Show(this, "Не удалось открыть проект:\n" + path + "\n\n" + ex.Message, "Проект",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
            project = loaded;
            ProjectSide.Project = project;
            RememberRecent(project.Path);
            SyncKindBox();
            if (!ProjectPanelShown) ShowProjectPanel(true);

            if (openFirst && !Documents.Any(d => d.Path != null && project.Contains(d.Path)))
            {
                string first = project.Files.Select(project.FullPath).FirstOrDefault(f => Project.IsBuildFile(f) && File.Exists(f))
                            ?? project.Files.Select(project.FullPath).FirstOrDefault(File.Exists);
                if (first != null) OpenPath(first, true);
            }
            UpdateEmptyState();
            UpdateStatus();
            return true;
        }

        void CloseProject()
        {
            project = null;
            ProjectSide.Project = null;
            ProjectSide.SetRecent(settings.Recent);
            SyncKindBox();
            UpdateEmptyState();
        }

        void RememberRecent(string path)
        {
            settings.Recent.RemoveAll(r => string.Equals(r, path, StringComparison.OrdinalIgnoreCase));
            settings.Recent.Insert(0, path);
            if (settings.Recent.Count > RecentLimit) settings.Recent.RemoveRange(RecentLimit, settings.Recent.Count - RecentLimit);
            ProjectSide.SetRecent(settings.Recent);
        }

        bool SaveProject()
        {
            if (project == null) return false;
            try
            {
                project.Save();
                return true;
            }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, "Не удалось сохранить проект:\n" + project.Path + "\n\n" + ex.Message, "Проект",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
        }

        /// <summary>Новый проект: файл .wccproj в выбранной папке. Файлы Си, что там уже лежат, — по согласию в проект;
        /// пустая папка — main.c с заготовкой.</summary>
        void NewProject()
        {
            var dlg = new SaveFileDialog
            {
                Filter = Project.FileFilter,
                Title = "Новый проект",
                FileName = "Программа",
                DefaultExt = Project.Extension,
                AddExtension = true
            };
            string folder = DefaultFolder();
            if (folder != null) dlg.InitialDirectory = folder;
            if (dlg.ShowDialog(this) != true) return;

            var p = new Project(dlg.FileName) { Kind = CurrentKind };
            var found = Directory.GetFiles(p.Folder)
                .Where(f => (Project.IsBuildFile(f) || Project.IsHeader(f)) && !f.EndsWith(".res", StringComparison.OrdinalIgnoreCase))
                .OrderBy(f => Project.IsHeader(f) ? 1 : 0)
                .ThenBy(f => Path.GetFileName(f).Equals("main.c", StringComparison.OrdinalIgnoreCase) ? 0 : 1)
                .ThenBy(f => Path.GetFileName(f), StringComparer.OrdinalIgnoreCase)
                .ToList();
            try
            {
                if (found.Count > 0)
                {
                    var answer = ThemedMessageBox.Show(this, "В папке уже есть файлы Си: " + found.Count + ".\nДобавить их в проект?",
                        "Новый проект", MessageBoxButton.YesNo, MessageBoxImage.Question);
                    if (answer == MessageBoxResult.Yes) foreach (string f in found) p.Add(f);
                }
                else
                {
                    string main = Path.Combine(p.Folder, "main.c");
                    File.WriteAllText(main, NewFileText, new UTF8Encoding(false));
                    p.Add(main);
                }
                p.Save();
            }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, "Не удалось создать проект:\n" + p.Path + "\n\n" + ex.Message, "Новый проект",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            OpenProject(p.Path, true);
        }

        /// <summary>Не текст: такой файл проекта открывает программа Windows, а не вкладка редактора.</summary>
        static readonly string[] binaryExtensions = { ".res", ".ico", ".cur", ".bmp", ".png", ".jpg", ".gif", ".ani", ".bin" };

        void OpenProjectFile(string path)
        {
            if (File.Exists(path) && binaryExtensions.Contains(Path.GetExtension(path).ToLowerInvariant()))
            {
                try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(path) { UseShellExecute = true }); }
                catch (Exception ex)
                {
                    ThemedMessageBox.Show(this, "Не удалось открыть файл:\n" + path + "\n\n" + ex.Message, "Проект",
                        MessageBoxButton.OK, MessageBoxImage.Error);
                }
                return;
            }
            if (File.Exists(path)) { OpenPath(path, true); return; }
            var answer = ThemedMessageBox.Show(this, "Файла нет на диске:\n" + path + "\n\nУбрать его из проекта?", "Проект",
                MessageBoxButton.YesNo, MessageBoxImage.Warning);
            if (answer == MessageBoxResult.Yes) RemoveFromProject(path);
        }

        void AddFilesToProject()
        {
            if (project == null) return;
            var dlg = new OpenFileDialog { Filter = ProjectFilesFilter, Multiselect = true, Title = "Добавить файлы в проект", InitialDirectory = project.Folder };
            if (dlg.ShowDialog(this) != true) return;
            AddToProject(dlg.FileNames);
        }

        void AddToProject(string[] files)
        {
            bool changed = false;
            foreach (string f in files)
                if (File.Exists(f) && !Project.IsProjectFile(f)) changed |= project.Add(f);
            if (changed && SaveProject()) ProjectSide.Refresh();
        }

        /// <summary>Файлы брошены на панель: проект — открыть, остальное — в проект (без проекта — просто открыть).</summary>
        void OnProjectDrop(string[] files)
        {
            string proj = files.FirstOrDefault(Project.IsProjectFile);
            if (proj != null) { OpenProject(proj, true); return; }
            if (project == null)
            {
                foreach (string f in files) if (File.Exists(f)) OpenPath(f, true);
                return;
            }
            AddToProject(files);
        }

        void NewProjectFile()
        {
            if (project == null) return;
            var dlg = new SaveFileDialog
            {
                Filter = ProjectFilesFilter,
                Title = "Новый файл в проекте",
                FileName = project.Files.Count == 0 ? "main.c" : "",
                DefaultExt = ".c",
                AddExtension = true,
                InitialDirectory = project.Folder
            };
            if (dlg.ShowDialog(this) != true) return;
            string path = dlg.FileName;
            try
            {
                if (!File.Exists(path))
                {
                    string text = Project.IsHeader(path) ? "#pragma once\r\n\r\n"
                                : string.Equals(Path.GetExtension(path), ".rc", StringComparison.OrdinalIgnoreCase) ? NewRcText
                                : project.BuildFiles.Any() ? "" : NewFileText;
                    File.WriteAllText(path, text, new UTF8Encoding(false));
                }
            }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, "Не удалось создать файл:\n" + path + "\n\n" + ex.Message, "Новый файл",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            if (project.Add(path) && SaveProject()) ProjectSide.Refresh();
            OpenPath(path, true);
        }

        void RemoveFromProject(string path)
        {
            if (project == null) return;
            if (project.Remove(path) && SaveProject()) ProjectSide.Refresh();
        }

        void EditProjectProperties()
        {
            if (project == null) return;
            if (new ProjectDialog(this, project).ShowDialog() != true) return;
            SaveProject();
            ProjectSide.Refresh();
            SyncKindBox();
        }
    }
}
