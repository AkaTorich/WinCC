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
            ThemeManager.Apply("Cyberpunk");       // the window has one theme, Cyberpunk
            var args = e.Args;

            // --window <file.png> [WxH] [empty] [project:file.wccproj] [lang:en|ru]: a window snapshot without showing it —
            // with sample code, output and errors, or with a project open
            if (args.Length >= 2 && args[0] == "--window")
            {
                ShutdownMode = ShutdownMode.OnExplicitShutdown;
                double w = 1280, h = 820;
                bool empty = false;
                string projectFile = null;
                UiLanguage lang = Loc.SystemDefault;
                foreach (string a in args.Skip(2))
                {
                    if (a.StartsWith("project:")) projectFile = a.Substring(8);
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
