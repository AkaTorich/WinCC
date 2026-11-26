using System.Diagnostics;
using System.IO;

namespace WinCC.Build
{
    /// <summary>Запуск собранной программы: консольная — в своём окне консоли, которое ждёт клавишу; оконная — как есть.</summary>
    public static class Runner
    {
        public static void Start(string exe, ProgramKind kind)
        {
            string folder = Path.GetDirectoryName(exe);
            if (kind == ProgramKind.Console)
            {
                // Окно консоли не закрывается сразу: видно, что программа напечатала и с каким кодом вышла
                string command = "/v:on /c \"chcp 65001>nul & title " + Path.GetFileName(exe) + " & \"" + exe + "\" & echo. & echo " +
                                 Loc.F("ProgramExited", "!errorlevel!") + " & echo " + Loc.T("PressAnyKey") + " & pause>nul\"";
                var psi = new ProcessStartInfo("cmd.exe", command)
                {
                    WorkingDirectory = folder,
                    UseShellExecute = true
                };
                Process.Start(psi);
            }
            else
            {
                Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = folder, UseShellExecute = true });
            }
        }
    }
}
