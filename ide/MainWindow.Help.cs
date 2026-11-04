using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using ThemeLibrary;

namespace WinCC
{
    /// <summary>Кнопка «Справка» и F1: открывает «Руководство WinCC.pdf» программой, которая в Windows открывает PDF.</summary>
    public partial class MainWindow
    {
        const string ManualName = "Руководство WinCC.pdf";

        /// <summary>Руководство: рядом с WinCC.exe (установленная программа) или в docs\руководство (папка проекта).</summary>
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
                ThemedMessageBox.Show(this, "Файл «" + ManualName + "» не найден рядом с WinCC.exe.\nОн ставится вместе с WinCC — переустановите программу.",
                    "Справка", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }
            try { Process.Start(new ProcessStartInfo(path) { UseShellExecute = true }); }
            catch (Exception ex)
            {
                ThemedMessageBox.Show(this, "Не удалось открыть руководство:\n" + path + "\n\n" + ex.Message, "Справка",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}
