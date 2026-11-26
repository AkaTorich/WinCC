using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using ThemeLibrary;

namespace WinCC
{
    /// <summary>Help button and F1: opens the manual PDF with the program Windows uses for PDF files.</summary>
    public partial class MainWindow
    {
        const string ManualName = "Руководство WinCC.pdf";

        /// <summary>The manual: next to WinCC.exe (installed program) or in docs\руководство (source tree).</summary>
        static string ManualPath
        {
            get
            {
                string home = AppDomain.CurrentDomain.BaseDirectory;
                foreach (string p in new[] { Path.Combine(home, ManualName), Path.Combine(home, "docs", "руководство", ManualName) })
                    if (File.Exists(p)) return p;
                return null;
            }
        }

        void OpenManual()
        {
            string path = ManualPath;
            if (path == null)
            {
                ThemedMessageBox.Show(this, Loc.F("ManualMissing", ManualName), Loc.T("Help"), MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }
            try { Process.Start(new ProcessStartInfo(path) { UseShellExecute = true }); }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, Loc.F("CannotOpenManual", path, ex.Message), Loc.T("Help"),
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}
