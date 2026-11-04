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
        private bool _busy, _finished;
        private string _installedExe;

        public SetupWindow(bool uninstallMode)
        {
            InitializeComponent();
            Logo.Source = LargestIcon();
            VersionText.Text = "версия " + Installer.Version;

            string installed = Installer.InstalledFolder();
            FolderBox.Text = string.IsNullOrEmpty(installed) ? Installer.DefaultFolder : installed;
            if (!string.IsNullOrEmpty(installed))
            {
                InstalledText.Text = "Сейчас установлена версия " + (Installer.InstalledVersion() ?? "?") + " — она будет обновлена.";
                InstallText.Text = "Обновить";
            }
            else
            {
                InstalledText.Text = "Ставится только для вас, права администратора не нужны.";
                UninstallButton.Visibility = Visibility.Collapsed;
            }

            if (!Installer.PayloadOk())
                ShowDone(false, "Установщик повреждён", "Внутри не нашлись файлы программы. Скачайте установщик заново.");

            if (uninstallMode)
                Dispatcher.BeginInvoke(new Action(delegate { Uninstall_Click(null, null); }), DispatcherPriority.Background);
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
            string path = FolderBrowser.Pick(this, "Куда установить WinCC", FolderBox.Text);
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
            ShowProgress("Установка WinCC…");
            Run(delegate { Installer.Install(opt, Step, Fraction); }, delegate
            {
                ShowDone(true, "WinCC установлен",
                         "Папка: " + opt.Folder + (opt.AddToPath ? "\nКоманда wcc работает в новых окнах консоли." : ""));
                if (launch) Launch();
            });
        }

        private void Uninstall_Click(object sender, RoutedEventArgs e)
        {
            if (_busy) return;
            if (ThemedMessageBox.Show(this, "Удалить WinCC? Уйдут только его собственные файлы; ваши файлы, даже в папке WinCC, останутся.",
                                      "Удаление", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes)
                return;
            ShowProgress("Удаление WinCC…");
            Run(delegate { Installer.Uninstall(Step); }, delegate
            {
                ShowDone(true, "WinCC удалён", "Ушли только файлы самого WinCC; ваши файлы не тронуты.");
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
            CloseButton.IsEnabled = true;
            if (ok)
            {
                CloseButton.Visibility = Visibility.Collapsed;
                InstallButton.IsEnabled = true;
                InstallIcon.Kind = IconKind.Check;
                InstallText.Text = "Готово";
            }
            else
            {
                InstallButton.Visibility = Visibility.Collapsed;
                CloseButton.Content = "Закрыть";
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
                    else ShowDone(false, "Не получилось", error);
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
            if (page == "ход") { ShowProgress("Установка WinCC…"); Progress.Value = 0.62; StepText.Text = "Распаковка файлов…"; }
            else if (page == "готово") ShowDone(true, "WinCC установлен", "Папка: " + Installer.DefaultFolder + "\nКоманда wcc работает в новых окнах консоли.");
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
