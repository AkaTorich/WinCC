using System.Diagnostics;
using System.IO;

namespace WinCC.Build
{
    /// <summary>Runs the built program: a console one in its own console window that waits for a key; a GUI one as is.</summary>
    public static class Runner
    {
        public static void Start(string exe, ProgramKind kind)
        {
            string folder = Path.GetDirectoryName(exe);
            if (kind == ProgramKind.Console)
            {
                // The console window does not close at once: what the program printed and its exit code stay visible
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
