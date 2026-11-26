using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using WinCC.Build;
using WinCC.Debugger;
using WinCC.Documents;

namespace WinCC
{
    /// <summary>
    /// Отладка: точки останова в редакторах, запуск под отладчиком (F5), шаги (F10, F11, Shift+F11), стоп (Shift+F5),
    /// вкладки «Переменные» и «Стек вызовов». Движок — Debugger\DebugEngine.cs, сведения о программе — её .wdbg (wcc -g).
    /// Режим сборки «Отладка» / «Выпуск» — список на панели кнопок (у проекта — в проекте).
    /// </summary>
    public partial class MainWindow
    {
        DebugEngine engine;
        ValueReader reader;
        StopInfo stopInfo;
        List<StackFrame> frames = new List<StackFrame>();
        Dictionary<string, string> lastValues = new Dictionary<string, string>();
        Dictionary<string, string> nowValues = new Dictionary<string, string>();
        readonly Dictionary<string, SortedSet<int>> breakpoints = new Dictionary<string, SortedSet<int>>(StringComparer.OrdinalIgnoreCase);
        bool syncingConfig;

        bool Debugging { get { return engine != null; } }

        BuildConfig CurrentConfig
        {
            get
            {
                var item = ConfigBox.SelectedItem as ComboBoxItem;
                return item != null ? (BuildConfig)item.Tag : BuildConfig.Debug;
            }
        }

        void InitDebug()
        {
            foreach (BuildConfig c in BuildConfigs.All)
                ConfigBox.Items.Add(new ComboBoxItem { Content = BuildConfigs.Title(c), Tag = c });
            ConfigBox.SelectedIndex = Array.IndexOf(BuildConfigs.All, settings.Config);
            ConfigBox.SelectionChanged += (s, e) => OnConfigChosen();

            foreach (string b in settings.Breakpoints)
            {
                int bar = b.LastIndexOf('|');
                int line;
                if (bar > 0 && int.TryParse(b.Substring(bar + 1), out line)) BreakpointSet(b.Substring(0, bar)).Add(line);
            }

            DebugButton.Click += (s, e) => DebugOrContinue();
            StopButton.Click += (s, e) => DebugCommandSend(DebugCommand.Stop);
            StepOverButton.Click += (s, e) => DebugCommandSend(DebugCommand.StepOver);
            StepIntoButton.Click += (s, e) => DebugCommandSend(DebugCommand.StepInto);
            StepOutButton.Click += (s, e) => DebugCommandSend(DebugCommand.StepOut);
            StackList.SelectionChanged += (s, e) => { if (StackList.SelectedIndex >= 0) ShowFrame(StackList.SelectedIndex, false); };
            UpdateDebugUi();
        }

        SortedSet<int> BreakpointSet(string path)
        {
            SortedSet<int> set;
            if (!breakpoints.TryGetValue(path, out set)) breakpoints[path] = set = new SortedSet<int>();
            return set;
        }

        /// <summary>Новый редактор: его точки останова — из запомненных, его правки точек — обратно.</summary>
        void HookDebug(SourceDocument doc)
        {
            if (doc.Path != null && breakpoints.ContainsKey(doc.Path)) doc.Editor.SetBreakpoints(breakpoints[doc.Path]);
            doc.Editor.BreakpointsChanged += (s, e) =>
            {
                if (doc.Path == null) return;
                breakpoints[doc.Path] = new SortedSet<int>(doc.Editor.Breakpoints);
                if (engine != null) engine.SetBreakpoints(AllBreakpoints());
            };
            doc.Editor.ReadOnly = Debugging;
        }

        IEnumerable<Tuple<string, int>> AllBreakpoints()
        {
            foreach (var kv in breakpoints)
                foreach (int l in kv.Value) yield return Tuple.Create(kv.Key, l);
        }

        void SaveBreakpoints()
        {
            settings.Breakpoints.Clear();
            foreach (var bp in AllBreakpoints()) settings.Breakpoints.Add(bp.Item1 + "|" + bp.Item2);
        }

        void ToggleBreakpointAtCaret()
        {
            var doc = Current;
            if (doc != null) doc.Editor.ToggleBreakpoint(doc.Editor.CaretLine);
        }

        void SyncConfigBox()
        {
            syncingConfig = true;
            ConfigBox.SelectedIndex = Array.IndexOf(BuildConfigs.All, project != null ? project.Config : settings.Config);
            syncingConfig = false;
            UpdateStatus();
        }

        /// <summary>Режим выбран в окне: у проекта — в проект, без проекта — в настройки.</summary>
        void OnConfigChosen()
        {
            if (syncingConfig) return;
            if (project != null) { project.Config = CurrentConfig; SaveProject(); }
            else settings.Config = CurrentConfig;
            UpdateStatus();
        }

        // ---- запуск и команды ----

        void DebugOrContinue()
        {
            if (engine == null) StartBuild(true, true);
            else DebugCommandSend(DebugCommand.Continue);
        }

        void DebugCommandSend(DebugCommand c)
        {
            if (engine == null) return;
            if (c != DebugCommand.Stop && !engine.IsStopped) return;
            ClearExecMarks();
            if (engine.IsStopped) { stopInfo = null; UpdateDebugUi(); }
            engine.Command(c);
        }

        void StartDebugger(string exe, string folder, ProgramKind kind)
        {
            string wdbg = Path.ChangeExtension(exe, ".wdbg");
            if (!File.Exists(wdbg)) { AppendOutput(Loc.F("DbgNoInfo", wdbg)); return; }
            DebugInfo info;
            try { info = DebugInfo.Load(wdbg, folder, Path.GetDirectoryName(Compiler.WccPath)); }
            catch (Exception ex) { AppendOutput(Loc.F("DbgNoInfo", wdbg) + " — " + ex.Message); return; }

            engine = new DebugEngine(info);
            reader = new ValueReader(engine);
            lastValues = new Dictionary<string, string>();
            engine.Stopped += s => Dispatcher.BeginInvoke(new Action(() => OnStopped(s)));
            engine.Output += s => Dispatcher.BeginInvoke(new Action(() => AppendOutput(s)));
            engine.Exited += code => Dispatcher.BeginInvoke(new Action(() => OnExited(code)));
            engine.SetBreakpoints(AllBreakpoints());
            AppendOutput(Loc.F("DbgStarted", exe));
            foreach (var d in Documents) d.Editor.ReadOnly = true;
            UpdateDebugUi();
            UpdateEmptyState();
            engine.Start(exe, Path.GetDirectoryName(exe), kind == ProgramKind.Console);
        }

        void OnExited(int code)
        {
            AppendOutput(Loc.F("DbgExited", code));
            engine = null;
            reader = null;
            stopInfo = null;
            frames = new List<StackFrame>();
            ClearExecMarks();
            VarTree.Items.Clear();
            StackList.ItemsSource = null;
            foreach (var d in Documents) d.Editor.ReadOnly = false;
            UpdateDebugUi();
            UpdateEmptyState();
        }

        void OnStopped(StopInfo s)
        {
            if (engine == null) return;
            stopInfo = s;
            frames = engine.Frames(s.ThreadId);
            // первый кадр с исходником программы (в коде библиотеки или Windows — выше по стеку)
            int pick = 0;
            for (int i = 0; i < frames.Count; i++)
                if (FrameSource(frames[i]) != null) { pick = i; break; }
            StackList.ItemsSource = frames.Select(f => new StackRow { Function = FrameName(f), Place = FramePlace(f) }).ToList();
            ShowFrame(pick, true);
            if (StackList.SelectedIndex != pick) StackList.SelectedIndex = pick;

            if (s.Reason != null) AppendOutput(s.Reason);
            if (s.IsException)
            {
                AppendOutput(Loc.T("DbgExceptionEnd"));
                BottomTabs.SelectedItem = OutputTab;
            }
            else if (BottomTabs.SelectedItem != StackTab) BottomTabs.SelectedItem = VarsTab;
            UpdateDebugUi();
            if (WindowState == WindowState.Minimized) WindowState = WindowState.Normal;
            Activate();
        }

        sealed class StackRow
        {
            public string Function { get; set; }
            public string Place { get; set; }
        }

        string FrameName(StackFrame f)
        {
            return f.Function != null ? f.Function.Name : "0x" + f.Rip.ToString("X");
        }

        string FramePlace(StackFrame f)
        {
            string src = FrameSource(f);
            if (f.Line == null || f.Line.File >= engine.Info.Files.Count) return Loc.T("DbgNoSource");
            return Loc.F("DbgLine", engine.Info.Files[f.Line.File], f.Line.Line) + (src == null ? "" : "");
        }

        /// <summary>Исходник кадра, если это файл программы и он есть на диске.</summary>
        string FrameSource(StackFrame f)
        {
            if (engine == null || f.Line == null || f.Function == null || !f.Function.IsUser) return null;
            if (f.Line.File >= engine.Info.Files.Count || !engine.Info.UserFiles[f.Line.File]) return null;
            string path = engine.Info.Files[f.Line.File];
            return File.Exists(path) ? path : null;
        }

        void ShowFrame(int index, bool top)
        {
            if (engine == null || index < 0 || index >= frames.Count) return;
            var f = frames[index];
            ClearExecMarks();
            string path = FrameSource(f);
            if (path != null)
            {
                var doc = OpenPath(path, true);
                if (doc != null)
                {
                    doc.Editor.ReadOnly = true;
                    doc.Editor.ExecutionLine = f.Line.Line;
                }
            }
            FillVariables(f);
        }

        void ClearExecMarks()
        {
            foreach (var d in Documents) if (d.Editor.ExecutionLine != 0) d.Editor.ExecutionLine = 0;
        }

        // ---- переменные ----

        void FillVariables(StackFrame f)
        {
            VarTree.Items.Clear();
            nowValues = new Dictionary<string, string>();
            if (f.HasVariables)
            {
                int line = f.Line != null ? f.Line.Line : int.MaxValue;
                // параметры, потом локальные по месту объявления; ещё не объявленные (ниже строки) — не показываются
                var vars = f.Function.Variables.Where(v => v.IsParameter).ToList();
                vars.AddRange(f.Function.Variables.Where(v => !v.IsParameter && v.Line <= line).OrderBy(v => v.Line));
                foreach (var v in vars)
                    VarTree.Items.Add(VarItem(reader.Variable(v, f, f.Function.Name + ":"), 0));
            }
            if (engine.Info.Globals.Count > 0)
            {
                var g = new TreeViewItem { Header = Row(Loc.T("Globals"), "", "", false, true, 0), IsExpanded = false };
                foreach (var v in engine.Info.Globals)
                    g.Items.Add(VarItem(reader.Variable(v, f, "::"), 1));
                VarTree.Items.Add(g);
            }
            foreach (var kv in nowValues) lastValues[kv.Key] = kv.Value;
            NoVarsText.Visibility = VarTree.Items.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        TreeViewItem VarItem(VarNode n, int depth)
        {
            string before;
            bool changed = n.Path != null && lastValues.TryGetValue(n.Path, out before) && before != n.Value;
            if (n.Path != null) nowValues[n.Path] = n.Value;
            var item = new TreeViewItem { Header = Row(n.Name, n.Value, n.TypeName, changed, false, depth) };
            if (n.Children != null)
            {
                item.Items.Add(new TreeViewItem());         // пустышка: стрелка раскрытия; поля читаются по раскрытию
                item.Expanded += (s, e) =>
                {
                    if (e.OriginalSource != item || n.Children == null) return;
                    var kids = n.Children();
                    n.Children = null;
                    item.Items.Clear();
                    foreach (var k in kids) item.Items.Add(VarItem(k, depth + 1));
                    foreach (var kv in nowValues) lastValues[kv.Key] = kv.Value;
                };
            }
            return item;
        }

        /// <summary>Строка дерева: имя, значение, тип; имя уже на отступ уровня — значения и типы стоят ровными столбцами.</summary>
        static FrameworkElement Row(string name, string value, string type, bool changed, bool heading, int depth)
        {
            var g = new Grid();
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(Math.Max(60, 200 - 19 * depth)) });
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(460) });
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            var a = new TextBlock { Text = name, TextTrimming = TextTrimming.CharacterEllipsis, Margin = new Thickness(0, 1, 12, 1) };
            if (heading) a.FontWeight = FontWeights.SemiBold;
            var b = new TextBlock { Text = value, TextTrimming = TextTrimming.CharacterEllipsis, Margin = new Thickness(0, 1, 12, 1), ToolTip = value };
            if (changed) b.SetResourceReference(TextBlock.ForegroundProperty, "Theme.Danger");
            var c = new TextBlock { Text = type, Margin = new Thickness(0, 1, 0, 1) };
            c.SetResourceReference(TextBlock.ForegroundProperty, "Theme.TextDim");
            Grid.SetColumn(b, 1);
            Grid.SetColumn(c, 2);
            g.Children.Add(a);
            g.Children.Add(b);
            g.Children.Add(c);
            return g;
        }

        void UpdateDebugUi()
        {
            bool on = engine != null, stopped = on && engine.IsStopped && stopInfo != null;
            DebugBar.Visibility = on ? Visibility.Visible : Visibility.Collapsed;
            DebugText.SetResourceReference(TextBlock.TextProperty, on ? "S.Continue" : "S.Debug");
            DebugButton.SetResourceReference(ToolTipProperty, on ? "S.ContinueTip" : "S.DebugTip");
            DebugButton.IsEnabled = !on || stopped;
            StepOverButton.IsEnabled = StepIntoButton.IsEnabled = StepOutButton.IsEnabled = stopped;
            StopButton.IsEnabled = on;
            ConfigBox.IsEnabled = !on;
            KindBox.IsEnabled = !on;
        }

        /// <summary>Снимок окна под отладкой (--window … debug:файл:строка): точка, сборка, ожидание остановки.</summary>
        public async System.Threading.Tasks.Task DebugSnapshotRun(string file, int line)
        {
            var doc = OpenPath(Path.GetFullPath(file), true);
            if (doc == null) return;
            doc.Editor.ToggleBreakpoint(line);
            KindBox.SelectedIndex = Array.IndexOf(ProgramKinds.All, ProgramKind.Window);    // без окна консоли на экране
            StartBuild(true, true);
            for (int i = 0; i < 300 && (engine == null || stopInfo == null); i++)
                await System.Threading.Tasks.Task.Delay(100);
            foreach (TreeViewItem item in VarTree.Items) item.IsExpanded = true;
            await System.Threading.Tasks.Task.Delay(400);
        }

        public void StopDebuggingForSnapshot() { DebugCommandSend(DebugCommand.Stop); }

        /// <summary>Клавиши отладки; true — клавиша была её.</summary>
        bool DebugKey(ModifierKeys mods, Key key)
        {
            if (mods == ModifierKeys.None && key == Key.F5) { DebugOrContinue(); return true; }
            if (mods == ModifierKeys.Control && key == Key.F5) { if (!Debugging) StartBuild(true); return true; }
            if (mods == ModifierKeys.Shift && key == Key.F5) { DebugCommandSend(DebugCommand.Stop); return true; }
            if (mods == ModifierKeys.None && key == Key.F9) { ToggleBreakpointAtCaret(); return true; }
            if (mods == ModifierKeys.None && key == Key.F10) { DebugCommandSend(DebugCommand.StepOver); return true; }
            if (mods == ModifierKeys.None && key == Key.F11) { DebugCommandSend(DebugCommand.StepInto); return true; }
            if (mods == ModifierKeys.Shift && key == Key.F11) { DebugCommandSend(DebugCommand.StepOut); return true; }
            return false;
        }
    }
}
