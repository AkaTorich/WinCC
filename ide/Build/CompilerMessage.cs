using System;
using System.IO;
using System.Text.RegularExpressions;

namespace WinCC.Build
{
    /// <summary>
    /// Сообщение компилятора вида «файл:строка: текст» (бывает и «файл:строка:столбец: текст»).
    /// Путь может начинаться с буквы диска — «C:\…», поэтому двоеточие ищется перед числом.
    /// </summary>
    public class CompilerMessage
    {
        static readonly Regex pattern = new Regex(@"^(?<file>.+?):(?<line>\d+):(?:(?<col>\d+):)?\s*(?<text>.*)$",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);

        public string File { get; private set; }
        public string FullPath { get; private set; }
        public int Line { get; private set; }
        public int Column { get; private set; }
        public string Text { get; private set; }
        public bool IsWarning { get; private set; }

        public string FileName { get { return string.IsNullOrEmpty(File) ? "" : Path.GetFileName(File); } }
        public string LineText { get { return Line > 0 ? Line.ToString() : ""; } }

        /// <summary>Сообщение без файла и строки («функция объявлена, но не написана», «нет функции main»):
        /// сборка не удалась, а места у ошибки нет.</summary>
        public static CompilerMessage Plain(string outputLine)
        {
            string text = outputLine.Trim();
            bool warning = text.StartsWith("предупреждение:", StringComparison.OrdinalIgnoreCase);
            if (warning) text = text.Substring("предупреждение:".Length).TrimStart();
            return new CompilerMessage { File = "", FullPath = null, Text = text, IsWarning = warning };
        }
        public string Kind { get { return IsWarning ? "Предупреждение" : "Ошибка"; } }

        /// <summary>Разбирает строку вывода; null — это не сообщение об ошибке. folder — откуда считать относительные пути.</summary>
        public static CompilerMessage TryParse(string outputLine, string folder)
        {
            if (string.IsNullOrWhiteSpace(outputLine)) return null;
            var m = pattern.Match(outputLine.Trim());
            if (!m.Success) return null;
            int line;
            if (!int.TryParse(m.Groups["line"].Value, out line) || line <= 0) return null;
            int col = 0;
            if (m.Groups["col"].Success) int.TryParse(m.Groups["col"].Value, out col);

            string file = m.Groups["file"].Value.Trim();
            // «ОШИБКА: …» и подобные строки без пути отсеиваются: у пути есть точка расширения
            if (file.IndexOfAny(Path.GetInvalidPathChars()) >= 0 || Path.GetExtension(file).Length == 0) return null;

            string full;
            try { full = Path.GetFullPath(Path.IsPathRooted(file) ? file : Path.Combine(folder ?? "", file)); }
            catch (Exception) { return null; }

            string text = m.Groups["text"].Value.Trim();
            string lower = text.ToLowerInvariant();
            bool warning = lower.StartsWith("предупреждение") || lower.StartsWith("warning");
            foreach (string prefix in new[] { "ошибка:", "error:", "предупреждение:", "warning:" })
                if (lower.StartsWith(prefix)) { text = text.Substring(prefix.Length).TrimStart(); break; }

            return new CompilerMessage
            {
                File = file, FullPath = full, Line = line, Column = col, Text = text, IsWarning = warning
            };
        }
    }
}
