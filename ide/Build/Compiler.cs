using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace WinCC.Build
{
    public class BuildResult
    {
        public bool Success;
        public int ExitCode;
        public string OutputFile;
        public TimeSpan Elapsed;
        /// <summary>Why the build did not start (no compiler, file locked); null — it started.</summary>
        public string NotStarted;
    }

    /// <summary>Runs the wcc.exe next to WinCC.exe: "wcc file.c -o file.exe [--gui | --dll]" or with the project's arguments.</summary>
    public class Compiler
    {
        Process running;

        public static string WccPath { get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "wcc.exe"); } }
        public static bool Exists { get { return File.Exists(WccPath); } }

        public bool IsBusy { get { return running != null; } }

        /// <summary>Where the output goes: next to the source, with a .exe or .dll extension.</summary>
        public static string OutputFor(string source, ProgramKind kind)
        {
            return Path.ChangeExtension(source, ProgramKinds.Extension(kind));
        }

        public static string Arguments(string source, string output, ProgramKind kind)
        {
            var sb = new StringBuilder();
            sb.Append(Quote(source)).Append(" -o ").Append(Quote(output));
            string sw = ProgramKinds.Switch(kind);
            if (sw != null) sb.Append(' ').Append(sw);
            return sb.ToString();
        }

        /// <summary>Build: folder — base for relative paths, arguments — wcc arguments, output — the expected result.
        /// Every compiler output line goes to onLine (from another thread).</summary>
        public async Task<BuildResult> BuildAsync(string folder, string arguments, string output, Action<string> onLine)
        {
            var result = new BuildResult { OutputFile = output };

            if (!Exists)
            {
                result.NotStarted = Loc.F("WccNotFound", WccPath);
                return result;
            }
            if (IsLocked(output))
            {
                result.NotStarted = Loc.F("FileLocked", Path.GetFileName(output));
                return result;
            }

            var psi = new ProcessStartInfo(WccPath, arguments)
            {
                WorkingDirectory = folder,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8
            };
            var p = new Process { StartInfo = psi };
            p.OutputDataReceived += (s, e) => { if (e.Data != null) onLine(e.Data); };
            p.ErrorDataReceived += (s, e) => { if (e.Data != null) onLine(e.Data); };

            var watch = Stopwatch.StartNew();
            running = p;
            try
            {
                await Task.Run(() =>
                {
                    p.Start();
                    p.BeginOutputReadLine();
                    p.BeginErrorReadLine();
                    p.WaitForExit();
                });
                result.ExitCode = p.ExitCode;
                result.Success = p.ExitCode == 0 && File.Exists(output);
            }
            catch (Exception ex)
            {
                result.NotStarted = Loc.F("WccNoStart", ex.Message);
            }
            finally
            {
                running = null;
                p.Dispose();
            }
            result.Elapsed = watch.Elapsed;
            return result;
        }

        /// <summary>Abort a running build (when the window closes).</summary>
        public void Cancel()
        {
            var p = running;
            if (p == null) return;
            try { if (!p.HasExited) p.Kill(); } catch (Exception) { }
        }

        static bool IsLocked(string file)
        {
            if (!File.Exists(file)) return false;
            try
            {
                using (File.Open(file, FileMode.Open, FileAccess.ReadWrite, FileShare.None)) { }
                return false;
            }
            catch (IOException) { return true; }
            catch (UnauthorizedAccessException) { return false; }
        }

        static string Quote(string s) { return "\"" + s + "\""; }
    }
}
