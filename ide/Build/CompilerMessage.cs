using System;
using System.IO;
using System.Text.RegularExpressions;

namespace WinCC.Build
{
    /// <summary>
    /// A compiler message "file:line: text" (also "file:line:column: text").
    /// A path may start with a drive letter ("C:\…"), so the colon is searched for before a number.
    /// wcc prints its messages in Russian, so the Russian prefixes are recognised alongside the English ones.
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

        /// <summary>A message without file and line ("function declared but not defined", "no main function"):
        /// the build failed and the error has no location.</summary>
        public static CompilerMessage Plain(string outputLine)
        {
            string text = outputLine.Trim();
            bool warning = false;
            foreach (string prefix in new[] { "предупреждение:", "warning:" })
                if (text.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) { warning = true; text = text.Substring(prefix.Length).TrimStart(); break; }
            return new CompilerMessage { File = "", FullPath = null, Text = text, IsWarning = warning };
        }
        public string Kind { get { return IsWarning ? Loc.T("Warning") : Loc.T("Error"); } }

        /// <summary>Parses an output line; null — not an error message. folder — base for relative paths.</summary>
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
            // "ERROR: …" and similar lines without a path are skipped: a path has an extension dot
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
