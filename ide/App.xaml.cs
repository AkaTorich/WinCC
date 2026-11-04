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
            ThemeSelector.ApplySaved("WinCC");
            var args = e.Args;

            // --window <файл.png> [ШxВ] [тема:Id] [пусто]: снимок окна без показа — с образцом кода, выводом и ошибками
            if (args.Length >= 2 && args[0] == "--window")
            {
                ShutdownMode = ShutdownMode.OnExplicitShutdown;
                double w = 1280, h = 820;
                bool empty = false;
                foreach (string a in args.Skip(2))
                {
                    if (a.StartsWith("тема:")) ThemeManager.Apply(a.Substring(5));
                    else if (a == "пусто") empty = true;
                    else if (a.Contains("x"))
                    {
                        var parts = a.Split('x');
                        w = double.Parse(parts[0]);
                        h = double.Parse(parts[1]);
                    }
                }
                var win = new MainWindow(true) { Width = w, Height = h };
                if (!empty)
                {
                    var hello = win.NewFile(Samples.Hello, "привет.c");
                    win.NewFile(Samples.Window, "окно.c");
                    win.SelectDocument(hello);
                    win.ShowSampleBuild(hello, Samples.BuildOutput(hello.Name));
                }
                await Task.Delay(600);
                win.SaveSnapshot(args[1], w, h, false);
                Shutdown();
                return;
            }

            var main = new MainWindow();
            main.Show();
            main.RestoreSession();
            foreach (string f in args)
                if (File.Exists(f)) main.OpenPath(f, true);
        }
    }
}
