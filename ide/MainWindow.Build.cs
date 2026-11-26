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
    /// <summary>Сборка через wcc.exe, вывод в нижнюю панель, список ошибок, запуск готовой программы.</summary>
    public partial class MainWindow
    {
        readonly Compiler compiler = new Compiler();
        readonly ObservableCollection<CompilerMessage> messages = new ObservableCollection<CompilerMessage>();
        /// <summary>Строки вывода не вида «файл:строка: текст» — если сборка не удалась, а ошибок с местом нет, они и есть ошибки.</summary>
        readonly List<string> looseLines = new List<string>();
        bool building;
        string buildFolder;

        async void StartBuild(bool run)
        {
            if (building) return;
            if (project != null) await BuildProject(run);
            else if (Current != null) await BuildCurrent(run);
        }

        /// <summary>Сохранить изменённые файлы перед сборкой: заголовки тоже могли поменяться.</summary>
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
                "Сборка: " + Path.GetFileName(source) + " → " + Path.GetFileName(output) + " (" + ProgramKinds.Title(kind) + ")",
                "wcc " + Compiler.Arguments(Path.GetFileName(source), Path.GetFileName(output), kind));
        }

        /// <summary>Сборка проекта целиком: все его .c и .asm одной программой, параметры — из проекта.</summary>
        async Task BuildProject(bool run)
        {
            var p = project;
            SaveModifiedFiles(null);
            if (!p.BuildFiles.Any())
            {
                BeginOutput();
                AppendOutput("В проекте «" + p.Name + "» нет файлов .c — собирать нечего.");
                return;
            }
            var missing = p.BuildFiles.Where(f => !File.Exists(p.FullPath(f))).ToList();
            if (missing.Count > 0)
            {
                BeginOutput();
                AppendOutput("Файлов проекта нет на диске:");
                foreach (string f in missing) AppendOutput("    " + p.FullPath(f));
                return;
            }
            string args = p.Arguments();
            await RunBuild(p.Folder, args, p.OutputPath, p.Kind, run,
                "Сборка проекта «" + p.Name + "»: файлов " + p.BuildFiles.Count() + " → " + p.OutputName + " (" + ProgramKinds.Title(p.Kind) + ")",
                "wcc " + args);
        }

        /// <summary>Очистить вывод, ошибки и пометки в редакторах.</summary>
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

            string time = result.Elapsed.TotalSeconds.ToString("0.00") + " с";
            AppendOutput("");
            if (!result.Success)
            {
                AppendOutput(result.ExitCode != 0
                    ? "Сборка не удалась (код " + result.ExitCode + ", " + time + ")."
                    : "Компилятор не сделал файл " + Path.GetFileName(output) + " (" + time + ").");
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
            AppendOutput("Собрано за " + time + ": " + output);
            if (!run) return;
            if (kind == ProgramKind.Dll)
            {
                AppendOutput("DLL сама не запускается — её загружает другая программа.");
                return;
            }
            try
            {
                Runner.Start(output, kind);
                AppendOutput(kind == ProgramKind.Console ? "Запущена в своём окне консоли." : "Запущена.");
            }
            catch (Exception ex)
            {
                AppendOutput("Не удалось запустить: " + ex.Message);
            }
        }

        void OnCompilerLine(string line)
        {
            AppendOutput(line);
            var m = CompilerMessage.TryParse(line, buildFolder);
            if (m == null)
            {
                if (!string.IsNullOrWhiteSpace(line) && !line.StartsWith("Готово:")) looseLines.Add(line);
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
            ErrorsHeader.Text = messages.Count == 0 ? "Ошибки"
                : warnings == 0 ? "Ошибки (" + errors + ")"
                : "Ошибки (" + errors + ", предупреждений " + warnings + ")";
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

        /// <summary>Открыть файл сообщения (если ещё не открыт) и встать на его строку.</summary>
        void GoToMessage(CompilerMessage m)
        {
            if (m == null || m.FullPath == null) return;     // у ошибки нет места в исходнике
            var doc = Documents.FirstOrDefault(d => d.IsSamePath(m.FullPath));
            if (doc == null)
            {
                if (!File.Exists(m.FullPath)) return;
                doc = OpenPath(m.FullPath, true);
                if (doc == null) return;
                MarkErrorLines(doc, x => doc.IsSamePath(x.FullPath));
            }
            Docs.SelectedItem = doc.Tab;
            // После смены вкладки редактор ещё не разложен — переход чуть позже
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

        /// <summary>Вывод и ошибки для снимка окна: как после неудачной сборки.</summary>
        public void ShowSampleBuild(SourceDocument doc, string[] lines)
        {
            buildFolder = doc.Path != null ? Path.GetDirectoryName(doc.Path) : Environment.CurrentDirectory;
            foreach (string l in lines) OnCompilerLine(l);
            MarkErrorLines(doc, m => m.FileName == doc.Name);
            BottomTabs.SelectedItem = ErrorsTab;
        }
    }
}
