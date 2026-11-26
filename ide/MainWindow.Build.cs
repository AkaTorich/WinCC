using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using WinCC.Build;
using WinCC.Documents;

namespace WinCC
{
    /// <summary>Building with wcc.exe, output in the bottom panel, error list, running the result.</summary>
    public partial class MainWindow
    {
        readonly Compiler compiler = new Compiler();
        readonly ObservableCollection<CompilerMessage> messages = new ObservableCollection<CompilerMessage>();
        /// <summary>Output lines not shaped "file:line: text" — when a build fails without located errors, these are the errors.</summary>
        readonly List<string> looseLines = new List<string>();
        bool building;
        string buildFolder;

        async void StartBuild(bool run)
        {
            if (building) return;
            if (project != null) await BuildProject(run);
            else if (Current != null) await BuildCurrent(run);
        }

        /// <summary>Save modified files before building: headers may have changed too.</summary>
        void SaveModifiedFiles(SourceDocument except)
        {
            foreach (var other in Documents.Where(d => d != except && d.IsModified && !d.IsUntitled).ToList())
                SaveDocument(other, false);
        }

        async Task BuildCurrent(bool run)
        {
            var doc = Current;
            if (doc == null) return;
            if (doc.IsUntitled && !SaveDocument(doc, false)) return;
            if (doc.IsModified && !SaveDocument(doc, false)) return;
            SaveModifiedFiles(doc);

            ProgramKind kind = CurrentKind;
            string source = doc.Path;
            string output = Compiler.OutputFor(source, kind);
            await RunBuild(Path.GetDirectoryName(source), Compiler.Arguments(source, output, kind), output, kind, run,
                Loc.F("Building", Path.GetFileName(source), Path.GetFileName(output), ProgramKinds.Title(kind)),
                "wcc " + Compiler.Arguments(Path.GetFileName(source), Path.GetFileName(output), kind));
        }

        /// <summary>Build the whole project: all its .c and .asm files into one program, options from the project.</summary>
        async Task BuildProject(bool run)
        {
            var p = project;
            SaveModifiedFiles(null);
            if (!p.BuildFiles.Any())
            {
                BeginOutput();
                AppendOutput(Loc.F("NothingToBuild", p.Name));
                return;
            }
            var missing = p.BuildFiles.Where(f => !File.Exists(p.FullPath(f))).ToList();
            if (missing.Count > 0)
            {
                BeginOutput();
                AppendOutput(Loc.T("MissingFiles"));
                foreach (string f in missing) AppendOutput("    " + p.FullPath(f));
                return;
            }
            string args = p.Arguments();
            await RunBuild(p.Folder, args, p.OutputPath, p.Kind, run,
                Loc.F("BuildingProject", p.Name, p.BuildFiles.Count(), p.OutputName, ProgramKinds.Title(p.Kind)),
                "wcc " + args);
        }

        /// <summary>Clear the output, errors and editor marks.</summary>
        void BeginOutput()
        {
            messages.Clear();
            looseLines.Clear();
            foreach (var d in Documents) d.Editor.SetErrorLines(null);
            UpdateErrors();
            OutputBox.Clear();
            BottomTabs.SelectedItem = OutputTab;
        }

        async Task RunBuild(string folder, string arguments, string output, ProgramKind kind, bool run, string title, string commandLine)
        {
            buildFolder = folder;
            BeginOutput();
            AppendOutput(title);
            AppendOutput("> " + commandLine);

            building = true;
            UpdateEmptyState();
            BuildResult result;
            try
            {
                result = await compiler.BuildAsync(folder, arguments, output, line =>
                    Dispatcher.BeginInvoke(new Action(() => OnCompilerLine(line))));
            }
            finally
            {
                building = false;
                UpdateEmptyState();
            }

            if (result.NotStarted != null)
            {
                AppendOutput("");
                AppendOutput(result.NotStarted);
                UpdateCompilerState();
                return;
            }

            string time = Loc.F("Seconds", result.Elapsed.TotalSeconds.ToString("0.00"));
            AppendOutput("");
            if (!result.Success)
            {
                AppendOutput(result.ExitCode != 0
                    ? Loc.F("BuildFailed", result.ExitCode, time)
                    : Loc.F("NoOutputFile", Path.GetFileName(output), time));
                if (!messages.Any(x => !x.IsWarning))
                {
                    foreach (string l in looseLines) messages.Add(CompilerMessage.Plain(l));
                    UpdateErrors();
                }
                MarkErrorLines();
                var first = messages.FirstOrDefault(m => !m.IsWarning) ?? messages.FirstOrDefault();
                if (first != null)
                {
                    BottomTabs.SelectedItem = ErrorsTab;
                    ErrorList.SelectedItem = first;
                    GoToMessage(first);
                }
                return;
            }

            MarkErrorLines();
            AppendOutput(Loc.F("Built", time, output));
            if (!run) return;
            if (kind == ProgramKind.Dll)
            {
                AppendOutput(Loc.T("DllNoRun"));
                return;
            }
            try
            {
                Runner.Start(output, kind);
                AppendOutput(kind == ProgramKind.Console ? Loc.T("StartedConsole") : Loc.T("Started"));
            }
            catch (Exception ex)
            {
                AppendOutput(Loc.F("CannotStart", ex.Message));
            }
        }

        void OnCompilerLine(string line)
        {
            AppendOutput(line);
            var m = CompilerMessage.TryParse(line, buildFolder);
            if (m == null)
            {
                if (!string.IsNullOrWhiteSpace(line) && !line.StartsWith("Готово:") && !line.StartsWith("Done:")) looseLines.Add(line);
                return;
            }
            messages.Add(m);
            UpdateErrors();
        }

        void AppendOutput(string line)
        {
            OutputBox.AppendText(line + "\r\n");
            OutputBox.ScrollToEnd();
        }

        void UpdateErrors()
        {
            if (ErrorList.ItemsSource == null) ErrorList.ItemsSource = messages;
            int errors = messages.Count(m => !m.IsWarning), warnings = messages.Count - errors;
            ErrorsHeader.Text = messages.Count == 0 ? Loc.T("Errors")
                : warnings == 0 ? Loc.F("ErrorsCount", errors)
                : Loc.F("ErrorsWarnings", errors, warnings);
            NoErrorsText.Visibility = messages.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        void MarkErrorLines()
        {
            foreach (var doc in Documents)
            {
                if (doc.Path == null) continue;
                MarkErrorLines(doc, m => doc.IsSamePath(m.FullPath));
            }
        }

        void MarkErrorLines(SourceDocument doc, Func<CompilerMessage, bool> belongs)
        {
            var own = messages.Where(m => m.FullPath != null && belongs(m)).ToList();
            doc.Editor.SetErrorLines(own.Where(m => !m.IsWarning).Select(m => m.Line).ToList(),
                                     own.Where(m => m.IsWarning).Select(m => m.Line).ToList());
        }

        /// <summary>Open the message's file (if not open yet) and go to its line.</summary>
        void GoToMessage(CompilerMessage m)
        {
            if (m == null || m.FullPath == null) return;     // the error has no source location
            var doc = Documents.FirstOrDefault(d => d.IsSamePath(m.FullPath));
            if (doc == null)
            {
                if (!File.Exists(m.FullPath)) return;
                doc = OpenPath(m.FullPath, true);
                if (doc == null) return;
                MarkErrorLines(doc, x => doc.IsSamePath(x.FullPath));
            }
            Docs.SelectedItem = doc.Tab;
            // Right after a tab switch the editor is not laid out yet — jump a bit later
            Dispatcher.BeginInvoke(new Action(() => doc.Editor.GoToLine(m.Line, m.Column)), DispatcherPriority.Loaded);
        }

        void ErrorList_Click(object sender, MouseButtonEventArgs e)
        {
            var item = ItemsControl.ContainerFromElement(ErrorList, e.OriginalSource as DependencyObject) as ListViewItem;
            if (item != null) GoToMessage(item.Content as CompilerMessage);
        }

        void OutputBox_DoubleClick(object sender, MouseButtonEventArgs e)
        {
            int line = OutputBox.GetLineIndexFromCharacterIndex(OutputBox.CaretIndex);
            if (line < 0) return;
            var m = CompilerMessage.TryParse(OutputBox.GetLineText(line), buildFolder);
            if (m != null) GoToMessage(m);
        }

        /// <summary>Output and errors for a window snapshot: as after a failed build.</summary>
        public void ShowSampleBuild(SourceDocument doc, string[] lines)
        {
            buildFolder = doc.Path != null ? Path.GetDirectoryName(doc.Path) : Environment.CurrentDirectory;
            foreach (string l in lines) OnCompilerLine(l);
            MarkErrorLines(doc, m => m.FileName == doc.Name);
            BottomTabs.SelectedItem = ErrorsTab;
        }
    }
}
