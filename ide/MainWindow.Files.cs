using System;
using System.IO;
using System.Linq;
using System.Windows;
using Microsoft.Win32;
using ThemeLibrary;
using WinCC.Documents;

namespace WinCC
{
    /// <summary>File tabs: new, open, save, close.</summary>
    public partial class MainWindow
    {
        /// <summary>Template for a new file.</summary>
        public const string NewFileText =
            "#include <stdio.h>\r\n" +
            "\r\n" +
            "int main(void)\r\n" +
            "{\r\n" +
            "    printf(\"Hello, world!\\n\");\r\n" +
            "    return 0;\r\n" +
            "}\r\n";

        public SourceDocument NewFile(string text = NewFileText, string name = null)
        {
            var doc = new SourceDocument(text, name);
            AddDocument(doc);
            return doc;
        }

        public void SelectDocument(SourceDocument doc) { Docs.SelectedItem = doc.Tab; }

        void OpenFiles()
        {
            var dlg = new OpenFileDialog { Filter = Loc.T("FilterOpen"), Multiselect = true, Title = Loc.T("OpenFileTitle") };
            string folder = DefaultFolder();
            if (folder != null) dlg.InitialDirectory = folder;
            if (dlg.ShowDialog(this) != true) return;
            foreach (string f in dlg.FileNames) OpenPath(f, true);
        }

        /// <summary>Open a file in a tab; an already open one is just shown. An untouched empty "untitled" tab is replaced.
        /// A .wccproj file opens as a project (no tab — null).</summary>
        public SourceDocument OpenPath(string path, bool activate)
        {
            if (Projects.Project.IsProjectFile(path))
            {
                OpenProject(path, true);
                return null;
            }
            var open = Documents.FirstOrDefault(d => d.IsSamePath(path));
            if (open != null)
            {
                if (activate) Docs.SelectedItem = open.Tab;
                return open;
            }
            SourceDocument doc;
            try { doc = SourceDocument.Load(path); }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, Loc.F("CannotOpenFile", path, ex.Message), Loc.T("Open"),
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }
            var pristine = Docs.Items.Count == 1 && Current != null && Current.IsPristine ? Current : null;
            AddDocument(doc, activate);
            if (pristine != null) RemoveDocument(pristine);
            return doc;
        }

        void AddDocument(SourceDocument doc, bool activate = true)
        {
            doc.CloseRequested += (s, e) => CloseDocument(doc);
            doc.StateChanged += (s, e) => { if (doc == Current) UpdateStatus(); };
            doc.Editor.CaretMoved += (s, e) => { if (doc == Current) UpdateStatus(); };
            Docs.Items.Add(doc.Tab);
            if (activate || Docs.Items.Count == 1) Docs.SelectedItem = doc.Tab;
            UpdateEmptyState();
        }

        void RemoveDocument(SourceDocument doc)
        {
            Docs.Items.Remove(doc.Tab);
            UpdateEmptyState();
            UpdateStatus();
        }

        void SaveCurrent(bool saveAs)
        {
            if (Current != null) SaveDocument(Current, saveAs);
        }

        /// <summary>Save; for a new file (or saveAs) ask where. false — the user cancelled or saving failed.</summary>
        bool SaveDocument(SourceDocument doc, bool saveAs)
        {
            string path = doc.Path;
            if (path == null || saveAs)
            {
                var dlg = new SaveFileDialog
                {
                    Filter = Loc.T("FilterSave"),
                    Title = Loc.T("SaveFileTitle"),
                    FileName = doc.Name,
                    DefaultExt = ".c",
                    AddExtension = true
                };
                string folder = doc.Path != null ? Path.GetDirectoryName(doc.Path) : DefaultFolder();
                if (folder != null) dlg.InitialDirectory = folder;
                if (dlg.ShowDialog(this) != true) return false;
                path = dlg.FileName;
            }
            try
            {
                doc.SaveAs(path);
                UpdateStatus();
                return true;
            }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, Loc.F("CannotSaveFile", path, ex.Message), Loc.T("Save"),
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
        }

        void CloseDocument(SourceDocument doc)
        {
            if (doc.IsModified)
            {
                Docs.SelectedItem = doc.Tab;
                if (!ConfirmDiscard(doc)) return;
            }
            RemoveDocument(doc);
        }

        /// <summary>The file is modified: save, don't save or cancel. true — it may be closed.</summary>
        bool ConfirmDiscard(SourceDocument doc)
        {
            var answer = ThemedMessageBox.Show(this, Loc.F("SaveChanges", doc.Name), Loc.T("FileModified"),
                MessageBoxButton.YesNoCancel, MessageBoxImage.Question);
            if (answer == MessageBoxResult.Yes) return SaveDocument(doc, false);
            return answer == MessageBoxResult.No;
        }

        string DefaultFolder()
        {
            var doc = Current;
            if (doc != null && doc.Path != null) return Path.GetDirectoryName(doc.Path);
            if (project != null) return project.Folder;
            var withPath = Documents.FirstOrDefault(d => d.Path != null);
            if (withPath != null) return Path.GetDirectoryName(withPath.Path);
            string examples = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "examples"));
            return Directory.Exists(examples) ? examples : Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        }
    }
}
