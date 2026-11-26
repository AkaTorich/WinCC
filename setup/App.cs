using System;
using System.Windows;
using ThemeLibrary;

namespace WinCC.Setup
{
    /// <summary>
    /// Установщик WinCC. Без параметров — окно. /silent [/dir="папка"] — тихая установка с галочками по умолчанию,
    /// /uninstall [/silent] — удаление, /drytest — распаковать во временную папку и рассказать, что внутри.
    /// </summary>
    public sealed class App : Application
    {
        [STAThread]
        public static void Main(string[] args)
        {
            bool silent = Has(args, "/silent") || Has(args, "-silent");
            bool uninstall = Has(args, "/uninstall") || Has(args, "-uninstall");
            // /lang=en|ru — язык установщика (и окна WinCC); без него — как уже выбран в WinCC, иначе язык Windows
            string lang = Value(args, "/lang=");
            bool russian = lang == "ru" || (lang != "en" && Loc.DefaultRussian());

            if (Has(args, "/drytest"))
            {
                Console(Installer.DryRun());
                return;
            }

            if (silent)
            {
                Loc.Apply(russian);
                try
                {
                    if (uninstall) Installer.Uninstall(null);
                    else
                    {
                        InstallOptions opt = new InstallOptions();
                        string dir = Value(args, "/dir=");
                        if (!string.IsNullOrEmpty(dir)) opt.Folder = dir;
                        Installer.Install(opt, null);
                        if (lang != null) Loc.SaveIdeLanguage();
                    }
                }
                catch (Exception ex)
                {
                    Console(Loc.F("Error", ex.Message));
                    Environment.ExitCode = 1;
                }
                return;
            }

            DpiSupport.EnableHiDpi();
            App app = new App();
            // одно оформление — в цветах WinCC, без выбора тем
            app.Resources.MergedDictionaries.Add(new ThemeDictionary { Theme = "Steel" });
            Loc.Apply(russian);
            var window = new SetupWindow(uninstall);

            // --снимок файл.png [ход|готово]: окно рисуется в картинку и программа выходит, ничего не ставя
            string png = Value(args, "--снимок=");
            if (png != null)
            {
                string page = Value(args, "--страница=") ?? "";
                window.Left = -10000;
                window.ShowActivated = false;
                window.ContentRendered += async delegate
                {
                    await System.Threading.Tasks.Task.Delay(500);
                    window.Snapshot(png, page);
                    app.Shutdown();
                };
                app.Run(window);
                return;
            }
            app.Run(window);
        }

        private static bool Has(string[] args, string key)
        {
            foreach (string a in args)
                if (string.Equals(a, key, StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }

        private static string Value(string[] args, string prefix)
        {
            foreach (string a in args)
                if (a.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    return a.Substring(prefix.Length).Trim('"');
            return null;
        }

        private static void Console(string text)
        {
            try
            {
                if (!AttachConsole(-1)) AllocConsole();
                System.Console.OutputEncoding = System.Text.Encoding.UTF8;
                System.Console.WriteLine();
                System.Console.WriteLine(text);
            }
            catch
            {
                MessageBox.Show(text, Installer.AppName, MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        [System.Runtime.InteropServices.DllImport("kernel32.dll")]
        private static extern bool AttachConsole(int processId);

        [System.Runtime.InteropServices.DllImport("kernel32.dll")]
        private static extern bool AllocConsole();
    }
}
