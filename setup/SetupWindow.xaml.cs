using System;
using System.IO;
using System.Threading;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using ThemeLibrary;

namespace WinCC.Setup
{
    public partial class SetupWindow : ThemedWindow
    {
        private bool _busy, _finished, _update;
        private string _installedExe;

        public SetupWindow(bool uninstallMode)
        {
            InitializeComponent();
            Logo.Source = LargestIcon();

            LanguageBox.Items.Add(new System.Windows.Controls.ComboBoxItem { Content = "English", Tag = false });
            LanguageBox.Items.Add(new System.Windows.Controls.ComboBoxItem { Content = "Русский", Tag = true });
            LanguageBox.SelectedIndex = Loc.Russian ? 1 : 0;
            LanguageBox.SelectionChanged += delegate
            {
                var item = LanguageBox.SelectedItem as System.Windows.Controls.ComboBoxItem;
                if (item == null) return;
                Loc.Apply((bool)item.Tag);
                RefreshTexts();
            };

            string installed = Installer.InstalledFolder();
            _update = !string.IsNullOrEmpty(installed);
            FolderBox.Text = _update ? installed : Installer.DefaultFolder;
            if (!_update) UninstallButton.Visibility = Visibility.Collapsed;
            RefreshTexts();

            if (!Installer.PayloadOk())
                ShowDone(false, Loc.T("Damaged"), Loc.T("DamagedText"));

            if (uninstallMode)
                Dispatcher.BeginInvoke(new Action(delegate { Uninstall_Click(null, null); }), DispatcherPriority.Background);
        }

        // надписи первой страницы, которые ставит код (на выбранном языке)
        private void RefreshTexts()
        {
            VersionText.Text = Loc.F("Version", Installer.Version);
            InstalledText.Text = _update ? Loc.F("InstalledUpdate", Installer.InstalledVersion() ?? "?") : Loc.T("ForYou");
            InstallText.Text = Loc.T(_update ? "Update" : "Install");
        }

        // самый крупный кадр значка программы
        private static ImageSource LargestIcon()
        {
            try
            {
                using (Stream s = Application.GetResourceStream(new Uri("pack://application:,,,/icon.ico")).Stream)
                {
                    var dec = new IconBitmapDecoder(s, BitmapCreateOptions.PreservePixelFormat, BitmapCacheOption.OnLoad);
                    BitmapFrame best = null;
                    foreach (BitmapFrame f in dec.Frames)
                        if (best == null || f.PixelWidth > best.PixelWidth) best = f;
                    return best;
                }
            }
            catch { return null; }
        }

        private void Browse_Click(object sender, RoutedEventArgs e)
        {
            string path = FolderBrowser.Pick(this, Loc.T("PickFolder"), FolderBox.Text);
            if (string.IsNullOrWhiteSpace(path)) return;
            // в выбранной папке — своя папка WinCC, если это не она сама
            if (!string.Equals(Path.GetFileName(path.TrimEnd('\\')), "WinCC", StringComparison.OrdinalIgnoreCase))
                path = Path.Combine(path, "WinCC");
            FolderBox.Text = path;
        }

        private void Install_Click(object sender, RoutedEventArgs e)
        {
            if (_finished)
            {
                Close();
                return;
            }
            if (_busy) return;
            var opt = new InstallOptions
            {
                Folder = FolderBox.Text.Trim(),
                DesktopShortcut = DesktopBox.IsChecked == true,
                StartMenuShortcut = StartBox.IsChecked == true,
                AddToPath = PathBox.IsChecked == true,
                OpenWith = OpenWithBox.IsChecked == true,
                LaunchAfter = false,
            };
            bool launch = LaunchBox.IsChecked == true;
            _installedExe = Path.Combine(opt.Folder, Installer.ExeName);
            ShowProgress(Loc.T("InstallingWinCC"));
            Run(delegate { Installer.Install(opt, Step, Fraction); Loc.SaveIdeLanguage(); }, delegate
            {
                ShowDone(true, Loc.T("Installed"),
                         Loc.F("FolderLine", opt.Folder) + (opt.AddToPath ? "\n" + Loc.T("WccNewConsoles") : ""));
                if (launch) Launch();
            });
        }

        private void Uninstall_Click(object sender, RoutedEventArgs e)
        {
            if (_busy) return;
            if (ThemedMessageBox.Show(this, Loc.T("UninstallAsk"),
                                      Loc.T("UninstallTitle"), MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes)
                return;
            ShowProgress(Loc.T("Uninstalling"));
            Run(delegate { Installer.Uninstall(Step); }, delegate
            {
                ShowDone(true, Loc.T("Uninstalled"), Loc.T("UninstalledText"));
            });
        }

        private void Close_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void Launch()
        {
            try
            {
                if (File.Exists(_installedExe))
                    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(_installedExe)
                    { WorkingDirectory = Path.GetDirectoryName(_installedExe) });
            }
            catch { }
        }

        // ---------- страницы ----------

        private void ShowProgress(string title)
        {
            _busy = true;
            ProgressTitle.Text = title;
            Progress.Value = 0;
            StepText.Text = "";
            PageOptions.Visibility = Visibility.Collapsed;
            PageDone.Visibility = Visibility.Collapsed;
            PageProgress.Visibility = Visibility.Visible;
            LanguagePanel.Visibility = Visibility.Collapsed;
            InstallButton.IsEnabled = false;
            CloseButton.IsEnabled = false;
            UninstallButton.IsEnabled = false;
        }

        private void ShowDone(bool ok, string title, string text)
        {
            _busy = false;
            _finished = ok;
            DoneIcon.Kind = ok ? IconKind.Success : IconKind.Error;
            DoneIcon.SetResourceReference(ForegroundProperty, ok ? "Theme.Success" : "Theme.Danger");
            DoneTitle.Text = title;
            DoneText.Text = text;
            PageOptions.Visibility = Visibility.Collapsed;
            PageProgress.Visibility = Visibility.Collapsed;
            PageDone.Visibility = Visibility.Visible;
            UninstallButton.Visibility = Visibility.Collapsed;
            LanguagePanel.Visibility = Visibility.Collapsed;
            CloseButton.IsEnabled = true;
            if (ok)
            {
                CloseButton.Visibility = Visibility.Collapsed;
                InstallButton.IsEnabled = true;
                InstallIcon.Kind = IconKind.Check;
                InstallText.Text = Loc.T("Done");
            }
            else
            {
                InstallButton.Visibility = Visibility.Collapsed;
                CloseButton.Content = Loc.T("Close");
            }
        }

        private void Run(Action work, Action done)
        {
            var t = new Thread(delegate ()
            {
                string error = null;
                try { work(); }
                catch (Exception ex) { error = ex.Message; }
                Dispatcher.BeginInvoke(new Action(delegate
                {
                    if (error == null) done();
                    else ShowDone(false, Loc.T("Failed"), error);
                }));
            });
            t.IsBackground = true;
            t.SetApartmentState(ApartmentState.STA);   // ярлыки создаются через COM
            t.Start();
        }

        private void Step(string text)
        {
            Dispatcher.BeginInvoke(new Action(delegate { StepText.Text = text; }));
        }

        private void Fraction(double f)
        {
            Dispatcher.BeginInvoke(new Action(delegate { Progress.Value = f; }));
        }

        // ---------- снимок окна для проверки (--снимок файл.png [страница]) ----------

        public void Snapshot(string png, string page)
        {
            if (page == "ход") { ShowProgress(Loc.T("InstallingWinCC")); Progress.Value = 0.62; StepText.Text = Loc.T("Unpacking"); }
            else if (page == "готово") ShowDone(true, Loc.T("Installed"), Loc.F("FolderLine", Installer.DefaultFolder) + "\n" + Loc.T("WccNewConsoles"));
            UpdateLayout();
            var dpi = VisualTreeHelper.GetDpi(this);
            var bmp = new RenderTargetBitmap((int)(ActualWidth * dpi.DpiScaleX), (int)(ActualHeight * dpi.DpiScaleY),
                                             dpi.PixelsPerInchX, dpi.PixelsPerInchY, PixelFormats.Pbgra32);
            bmp.Render(this);
            var enc = new PngBitmapEncoder();
            enc.Frames.Add(BitmapFrame.Create(bmp));
            using (var f = File.Create(png)) enc.Save(f);
        }
    }
}
