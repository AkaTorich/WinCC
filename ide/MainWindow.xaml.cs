using System;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using ThemeLibrary;
using WinCC.Build;
using WinCC.Documents;

namespace WinCC
{
    /// <summary>
    /// Главное окно: кнопки, вкладки файлов, нижняя панель с выводом компилятора и ошибками, строка состояния.
    /// Работа с файлами — MainWindow.Files.cs, сборка и запуск — MainWindow.Build.cs.
    /// </summary>
    public partial class MainWindow : ThemedWindow
    {
        readonly Settings settings;
        /// <summary>Окно для снимка: ничего не запоминает.</summary>
        readonly bool snapshotMode;

        public MainWindow() : this(false) { }

        public MainWindow(bool snapshot)
        {
            InitializeComponent();
            snapshotMode = snapshot;
            settings = snapshot ? new Settings() : Settings.Load();

            foreach (ProgramKind k in ProgramKinds.All)
                KindBox.Items.Add(new ComboBoxItem { Content = ProgramKinds.Title(k), Tag = k });
            KindBox.SelectedIndex = Array.IndexOf(ProgramKinds.All, settings.Kind);
            KindBox.SelectionChanged += (s, e) => { settings.Kind = CurrentKind; UpdateStatus(); };
            BottomRow.Height = new GridLength(settings.BottomShare, GridUnitType.Star);

            NewButton.Click += (s, e) => NewFile();
            HintNewButton.Click += (s, e) => NewFile();
            OpenButton.Click += (s, e) => OpenFiles();
            HintOpenButton.Click += (s, e) => OpenFiles();
            SaveButton.Click += (s, e) => SaveCurrent(false);
            BuildButton.Click += (s, e) => StartBuild(false);
            RunButton.Click += (s, e) => StartBuild(true);
            HelpButton.Click += (s, e) => OpenManual();

            Docs.SelectionChanged += (s, e) => { if (e.Source == Docs) OnActiveChanged(); };
            ErrorList.PreviewMouseLeftButtonUp += ErrorList_Click;
            ErrorList.KeyDown += (s, e) => { if (e.Key == Key.Enter) GoToMessage(ErrorList.SelectedItem as CompilerMessage); };
            OutputBox.MouseDoubleClick += OutputBox_DoubleClick;
            PreviewKeyDown += OnWindowKeyDown;
            DragOver += OnDragOver;
            Drop += OnDrop;
            Activated += (s, e) => UpdateCompilerState();

            UpdateCompilerState();
            UpdateErrors();
            UpdateEmptyState();
            UpdateStatus();
        }

        ProgramKind CurrentKind
        {
            get
            {
                var item = KindBox.SelectedItem as ComboBoxItem;
                return item != null ? (ProgramKind)item.Tag : ProgramKind.Console;
            }
        }

        SourceDocument Current
        {
            get
            {
                var tab = Docs.SelectedItem as TabItem;
                return tab != null ? tab.Tag as SourceDocument : null;
            }
        }

        System.Collections.Generic.IEnumerable<SourceDocument> Documents
        {
            get { return Docs.Items.OfType<TabItem>().Select(t => t.Tag as SourceDocument).Where(d => d != null); }
        }

        /// <summary>Открыть файлы прошлого раза; если их нет — один новый файл с заготовкой.</summary>
        public void RestoreSession()
        {
            foreach (string f in settings.Files)
                if (File.Exists(f)) OpenPath(f, false);
            if (settings.Active >= 0 && settings.Active < Docs.Items.Count) Docs.SelectedIndex = settings.Active;
            if (Docs.Items.Count == 0) NewFile();
        }

        void OnActiveChanged()
        {
            UpdateStatus();
            var doc = Current;
            if (doc != null) Dispatcher.BeginInvoke(new Action(() => doc.Editor.FocusEditor()), System.Windows.Threading.DispatcherPriority.Input);
        }

        void UpdateEmptyState()
        {
            bool empty = Docs.Items.Count == 0;
            EmptyHint.Visibility = empty ? Visibility.Visible : Visibility.Collapsed;
            Docs.Visibility = empty ? Visibility.Hidden : Visibility.Visible;
            SaveButton.IsEnabled = !empty && !building;
            BuildButton.IsEnabled = !empty && !building;
            RunButton.IsEnabled = !empty && !building;
        }

        void UpdateStatus()
        {
            var doc = Current;
            PathStatus.Text = doc == null ? "" : doc.Path ?? "Новый файл — сохраните его перед сборкой";
            CaretStatus.Text = doc == null ? "" : "Строка " + doc.Editor.CaretRow + ", столбец " + doc.Editor.CaretColumn;
            EncodingStatus.Text = doc == null ? "" : doc.EncodingName;
            KindStatus.Text = "Вид: " + ProgramKinds.Title(CurrentKind);
        }

        void UpdateCompilerState()
        {
            bool found = Compiler.Exists;
            CompilerIcon.Kind = found ? IconKind.Success : IconKind.Warning;
            CompilerIcon.SetResourceReference(ThemeLibrary.Icon.ForegroundProperty, found ? "Theme.Success" : "Theme.Warning");
            CompilerText.Text = found ? "wcc.exe на месте" : "wcc.exe не найден";
            CompilerText.ToolTip = Compiler.WccPath;
            CompilerIcon.ToolTip = Compiler.WccPath;
        }

        void OnWindowKeyDown(object sender, KeyEventArgs e)
        {
            var mods = Keyboard.Modifiers;
            Key key = e.Key == Key.System ? e.SystemKey : e.Key;
            bool handled = true;
            if (mods == ModifierKeys.Control && key == Key.N) NewFile();
            else if (mods == ModifierKeys.Control && key == Key.O) OpenFiles();
            else if (mods == ModifierKeys.Control && key == Key.S) SaveCurrent(false);
            else if (mods == (ModifierKeys.Control | ModifierKeys.Shift) && key == Key.S) SaveCurrent(true);
            else if (mods == ModifierKeys.Control && (key == Key.W || key == Key.F4)) { if (Current != null) CloseDocument(Current); }
            else if (mods == ModifierKeys.None && key == Key.F7) StartBuild(false);
            else if (mods == ModifierKeys.Control && key == Key.B) StartBuild(false);
            else if (mods == ModifierKeys.None && key == Key.F5) StartBuild(true);
            else if (mods == ModifierKeys.None && key == Key.F1) OpenManual();
            else handled = false;
            if (handled) e.Handled = true;
        }

        void OnDragOver(object sender, DragEventArgs e)
        {
            e.Effects = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
            e.Handled = true;
        }

        void OnDrop(object sender, DragEventArgs e)
        {
            var files = e.Data.GetData(DataFormats.FileDrop) as string[];
            if (files == null) return;
            foreach (string f in files)
                if (File.Exists(f)) OpenPath(f, true);
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            base.OnClosing(e);
            if (e.Cancel) return;
            foreach (var doc in Documents.ToList())
            {
                if (!doc.IsModified) continue;
                Docs.SelectedItem = doc.Tab;
                if (!ConfirmDiscard(doc)) { e.Cancel = true; return; }
            }
            compiler.Cancel();
            if (snapshotMode) return;
            settings.Files.Clear();
            foreach (var doc in Documents)
                if (!doc.IsUntitled) settings.Files.Add(doc.Path);
            settings.Active = Docs.SelectedIndex;
            settings.Kind = CurrentKind;
            if (BottomRow.ActualHeight > 0 && EditorRow.ActualHeight > 0)
                settings.BottomShare = BottomRow.ActualHeight / EditorRow.ActualHeight;
            settings.Save();
        }
    }
}
