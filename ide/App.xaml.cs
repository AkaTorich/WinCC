using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using ThemeLibrary;

namespace WinCC
{
    public partial class App : Application
    {
        public App()
        {
            DpiSupport.EnableHiDpi();
        }

        protected override async void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            ThemeManager.Apply("Cyberpunk");       // у окна одна тема — «Киберпанк»
            var args = e.Args;

            // --debugtest отчёт.txt программа.exe файл.c:строка… команды: проверка отладчика без окна (Debugger\DebugTest.cs)
            if (args.Length >= 3 && args[0] == "--debugtest")
            {
                ShutdownMode = ShutdownMode.OnExplicitShutdown;
                Loc.Apply(UiLanguage.English);
                int rc = 1;
                await Task.Run(() => { rc = Debugger.DebugTest.Run(args); });
                Shutdown(rc);
                return;
            }

            // --window <файл.png> [ШxВ] [empty] [project:файл.wccproj] [lang:en|ru]: снимок окна без показа —
            // с образцом кода, выводом и ошибками или с открытым проектом; lang — язык окна
            if (args.Length >= 2 && args[0] == "--window")
            {
                ShutdownMode = ShutdownMode.OnExplicitShutdown;
                double w = 1280, h = 820;
                bool empty = false;
                string projectFile = null;
                UiLanguage lang = Loc.SystemDefault;
                string debugAt = null;
                foreach (string a in args.Skip(2))
                {
                    if (a.StartsWith("debug:")) debugAt = a.Substring(6);
                    else if (a.StartsWith("project:")) projectFile = a.Substring(8);
                    else if (a.StartsWith("lang:")) lang = Loc.Parse(a.Substring(5));
                    else if (a == "empty") empty = true;
                    else if (a.Contains("x"))
                    {
                        var parts = a.Split('x');
                        w = double.Parse(parts[0]);
                        h = double.Parse(parts[1]);
                    }
                }
                Loc.Apply(lang);
                var win = new MainWindow(true) { Width = w, Height = h };
                if (debugAt != null)
                {
                    // debug:файл.c:строка — собрать под отладчик, остановиться на строке и снять окно
                    int colon = debugAt.LastIndexOf(':');
                    win.WindowStartupLocation = WindowStartupLocation.Manual;
                    win.Left = -30000;
                    win.ShowActivated = false;
                    win.Show();
                    await win.DebugSnapshotRun(debugAt.Substring(0, colon), int.Parse(debugAt.Substring(colon + 1)));
                    win.SaveSnapshot(args[1], w, h, false);
                    win.StopDebuggingForSnapshot();
                    await Task.Delay(500);
                    Shutdown();
                    return;
                }
                if (projectFile != null) win.OpenProject(projectFile, true);
                else if (!empty)
                {
                    var hello = win.NewFile(Samples.Hello, "hello.c");
                    win.NewFile(Samples.Window, "window.c");
                    win.SelectDocument(hello);
                    win.ShowSampleBuild(hello, Samples.BuildOutput(hello.Name));
                }
                await Task.Delay(600);
                win.SaveSnapshot(args[1], w, h, false);
                Shutdown();
                return;
            }

            Loc.Apply(Settings.Load().Language);
            var main = new MainWindow();
            main.Show();
            main.RestoreSession();
            foreach (string f in args)
                if (File.Exists(f)) main.OpenPath(f, true);
        }
    }
}
