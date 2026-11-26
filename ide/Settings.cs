using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using WinCC.Build;

namespace WinCC
{
    /// <summary>
    /// Что окно помнит между запусками: язык окна, вид программы, открытые файлы, какой из них был впереди, проект, недавние проекты,
    /// панель проекта (показана ли, ширина), высоту нижней панели
    /// (долей от высоты редактора — так она уменьшается вместе с окном).
    /// Файл %APPDATA%\WinCC\settings.txt, строки «ключ=значение». Тема у окна одна — «Киберпанк».
    /// </summary>
    public class Settings
    {
        public UiLanguage Language = Loc.SystemDefault;
        public ProgramKind Kind = ProgramKind.Console;
        public BuildConfig Config = BuildConfig.Debug;
        /// <summary>Точки останова: «путь|строка».</summary>
        public readonly List<string> Breakpoints = new List<string>();
        public readonly List<string> Files = new List<string>();
        public int Active = -1;
        public double BottomShare = 0.36;
        /// <summary>Открытый проект (.wccproj) или null.</summary>
        public string Project;
        /// <summary>Недавние проекты, свежий первым.</summary>
        public readonly List<string> Recent = new List<string>();
        public double PanelWidth = 250;
        public bool PanelShown = true;
        /// <summary>Подсказки ввода открываются сами после стольких букв имени; 0 — только по Ctrl+Пробел.</summary>
        public int HintLetters = 2;

        static string FilePath
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "WinCC", "settings.txt"); }
        }

        public static Settings Load()
        {
            var s = new Settings();
            try
            {
                if (!File.Exists(FilePath)) return s;
                foreach (string raw in File.ReadAllLines(FilePath, Encoding.UTF8))
                {
                    int eq = raw.IndexOf('=');
                    if (eq <= 0) continue;
                    string key = raw.Substring(0, eq).Trim(), value = raw.Substring(eq + 1).Trim();
                    double d;
                    int n;
                    switch (key)
                    {
                        case "language": s.Language = Loc.Parse(value); break;
                        case "kind": s.Kind = ProgramKinds.Parse(value); break;
                        case "config": s.Config = BuildConfigs.Parse(value); break;
                        case "breakpoint": if (value.Length > 0) s.Breakpoints.Add(value); break;
                        case "file": if (value.Length > 0) s.Files.Add(value); break;
                        case "active": if (int.TryParse(value, out n)) s.Active = n; break;
                        case "project": s.Project = value.Length > 0 ? value : null; break;
                        case "recent": if (value.Length > 0) s.Recent.Add(value); break;
                        case "panel": s.PanelShown = value != "0"; break;
                        case "hints": if (int.TryParse(value, out n)) s.HintLetters = Math.Max(0, Math.Min(4, n)); break;
                        case "panelwidth":
                            if (double.TryParse(value, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out d))
                                s.PanelWidth = Math.Max(170, Math.Min(800, d));
                            break;
                        case "bottom":
                            if (double.TryParse(value, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out d))
                                s.BottomShare = Math.Max(0.1, Math.Min(3, d));
                            break;
                    }
                }
            }
            catch (Exception) { }
            return s;
        }

        public void Save()
        {
            try
            {
                var sb = new StringBuilder();
                sb.AppendLine("language=" + Loc.Code(Language));
                sb.AppendLine("kind=" + Kind);
                sb.AppendLine("config=" + Config);
                foreach (string b in Breakpoints) sb.AppendLine("breakpoint=" + b);
                foreach (string f in Files) sb.AppendLine("file=" + f);
                sb.AppendLine("active=" + Active);
                sb.AppendLine("bottom=" + BottomShare.ToString("0.###", System.Globalization.CultureInfo.InvariantCulture));
                if (Project != null) sb.AppendLine("project=" + Project);
                foreach (string r in Recent) sb.AppendLine("recent=" + r);
                sb.AppendLine("panel=" + (PanelShown ? "1" : "0"));
                sb.AppendLine("hints=" + HintLetters);
                sb.AppendLine("panelwidth=" + PanelWidth.ToString("0", System.Globalization.CultureInfo.InvariantCulture));
                Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
                File.WriteAllText(FilePath, sb.ToString(), new UTF8Encoding(false));
            }
            catch (Exception) { }
        }
    }
}
