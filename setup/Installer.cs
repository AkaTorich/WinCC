using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

namespace WinCC.Setup
{
    /// <summary>Что делать при установке.</summary>
    public sealed class InstallOptions
    {
        public string Folder = Installer.DefaultFolder;
        public bool DesktopShortcut = true;
        public bool StartMenuShortcut = true;
        public bool AddToPath = true;
        public bool OpenWith = true;
        public bool LaunchAfter;
    }

    /// <summary>
    /// Установка без прав администратора: программа кладётся в папку пользователя, записи — в его ветку реестра.
    /// </summary>
    public static class Installer
    {
        public const string AppName = "WinCC";
        public static string LongName { get { return Loc.T("LongName"); } }
        public const string ExeName = "WinCC.exe";
        public const string CompilerName = "wcc.exe";
        public const string SetupName = "WinCCSetup.exe";
        public const string ProgId = "WinCC.Source";
        public const string Publisher = "О. Кобенко";
        public const string UninstallKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\WinCC";
        private static readonly string[] Extensions = { ".c", ".h", ".wccproj" };

        public static string Version
        {
            get
            {
                Version v = Assembly.GetExecutingAssembly().GetName().Version;
                return v.Major + "." + v.Minor + "." + v.Build;
            }
        }

        public static string DefaultFolder
        {
            get
            {
                string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                return Path.Combine(Path.Combine(local, "Programs"), AppName);
            }
        }

        // ---------- уже установленная копия ----------

        public static string InstalledFolder()
        {
            using (RegistryKey k = Registry.CurrentUser.OpenSubKey(UninstallKey))
                return k == null ? null : k.GetValue("InstallLocation") as string;
        }

        public static string InstalledVersion()
        {
            using (RegistryKey k = Registry.CurrentUser.OpenSubKey(UninstallKey))
                return k == null ? null : k.GetValue("DisplayVersion") as string;
        }

        // ---------- установка ----------

        /// <summary>Ставит программу. say — что делается, progress — доля сделанного (0…1), оба из этого же потока.</summary>
        public static void Install(InstallOptions opt, Action<string> say, Action<double> progress = null)
        {
            if (opt == null) throw new ArgumentNullException("opt");
            if (say == null) say = delegate { };
            if (progress == null) progress = delegate { };
            progress(0);

            string folder = opt.Folder.Trim();
            if (folder.Length == 0) throw new InvalidOperationException(Loc.T("NoFolder"));

            say(Loc.F("FolderLine", folder));
            Directory.CreateDirectory(folder);
            CheckWritable(folder);

            string exe = Path.Combine(folder, ExeName);
            if (IsRunning(Path.Combine(folder, ExeName)) || IsRunning(Path.Combine(folder, CompilerName)))
                throw new InvalidOperationException(Loc.T("RunningInstall"));

            // библиотека и заголовки заменяются целиком: от прежней версии не должно остаться лишних файлов
            foreach (string sub in new[] { "runtime", "include" })
            {
                string p = Path.Combine(folder, sub);
                if (Directory.Exists(p)) { try { Directory.Delete(p, true); } catch { } }
            }

            say(Loc.T("Unpacking"));
            int count = ExtractPayload(folder, say, f => progress(0.05 + 0.8 * f));
            say(Loc.F("Copied", count.ToString(CultureInfo.CurrentCulture)));
            progress(0.85);

            string selfTarget = Path.Combine(folder, SetupName);
            string self = Assembly.GetExecutingAssembly().Location;
            if (!string.Equals(self, selfTarget, StringComparison.OrdinalIgnoreCase))
                File.Copy(self, selfTarget, true);

            if (opt.DesktopShortcut)
            {
                say(Loc.T("SayDesktop"));
                CreateShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                                            AppName + ".lnk"), exe, folder);
            }
            if (opt.StartMenuShortcut)
            {
                say(Loc.T("SayStart"));
                CreateShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs),
                                            AppName + ".lnk"), exe, folder);
            }
            if (opt.AddToPath)
            {
                say(Loc.T("SayPath"));
                AddToUserPath(folder);
            }
            if (opt.OpenWith)
            {
                say(Loc.T("SayOpenWith"));
                RegisterOpenWith(exe);
            }

            say(Loc.T("SayRegister"));
            RegisterUninstall(folder, exe);
            NotifyShell();
            progress(1);
            say(Loc.T("SayInstallDone"));

            if (opt.LaunchAfter)
            {
                say(Loc.T("SayLaunch"));
                try { Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = folder }); }
                catch (Exception ex) { say(Loc.F("LaunchFailed", ex.Message)); }
            }
        }

        private static void CheckWritable(string folder)
        {
            string probe = Path.Combine(folder, ".проба");
            try
            {
                File.WriteAllText(probe, "1");
                File.Delete(probe);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException(Loc.F("NotWritable", ex.Message));
            }
        }

        private static bool IsRunning(string exePath)
        {
            try
            {
                foreach (Process p in Process.GetProcessesByName(Path.GetFileNameWithoutExtension(exePath)))
                {
                    try
                    {
                        if (string.Equals(p.MainModule.FileName, exePath, StringComparison.OrdinalIgnoreCase))
                            return true;
                    }
                    catch { }
                }
            }
            catch { }
            return false;
        }

        // ---------- содержимое ----------

        public static Stream OpenPayload()
        {
            Assembly asm = Assembly.GetExecutingAssembly();
            foreach (string name in asm.GetManifestResourceNames())
                if (name.EndsWith("payload.zip", StringComparison.OrdinalIgnoreCase))
                    return asm.GetManifestResourceStream(name);
            throw new InvalidOperationException(Loc.T("NoPayload"));
        }

        public static bool PayloadOk()
        {
            try
            {
                using (Stream s = OpenPayload()) return s != null && s.Length > 0;
            }
            catch { return false; }
        }

        public static int ExtractPayload(string folder, Action<string> say, Action<double> fraction = null)
        {
            int n = 0;
            string root = Path.GetFullPath(folder);
            using (Stream s = OpenPayload())
            using (ZipArchive zip = new ZipArchive(s, ZipArchiveMode.Read))
            {
                int total = Math.Max(1, zip.Entries.Count);
                foreach (ZipArchiveEntry e in zip.Entries)
                {
                    if (e.FullName.EndsWith("/", StringComparison.Ordinal)) continue;
                    string target = Path.GetFullPath(Path.Combine(folder, e.FullName.Replace('/', '\\')));
                    if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                        continue;                       // защита от путей наружу папки
                    Directory.CreateDirectory(Path.GetDirectoryName(target));
                    e.ExtractToFile(target, true);
                    n++;
                    if (fraction != null) fraction((double)n / total);
                }
            }
            return n;
        }

        // ---------- ярлыки ----------

        public static void CreateShortcut(string linkPath, string exe, string workDir)
        {
            Type shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null) return;
            object shell = Activator.CreateInstance(shellType);
            try
            {
                object link = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod,
                                                     null, shell, new object[] { linkPath });
                Type lt = link.GetType();
                lt.InvokeMember("TargetPath", BindingFlags.SetProperty, null, link, new object[] { exe });
                lt.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, link, new object[] { workDir });
                lt.InvokeMember("IconLocation", BindingFlags.SetProperty, null, link, new object[] { exe + ",0" });
                lt.InvokeMember("Description", BindingFlags.SetProperty, null, link, new object[] { LongName });
                lt.InvokeMember("Save", BindingFlags.InvokeMethod, null, link, null);
            }
            finally
            {
                Marshal.FinalReleaseComObject(shell);
            }
        }

        // ---------- PATH пользователя: wcc из любой консоли ----------

        private static List<string> UserPathParts(out RegistryValueKind kind)
        {
            kind = RegistryValueKind.ExpandString;
            List<string> parts = new List<string>();
            using (RegistryKey k = Registry.CurrentUser.OpenSubKey("Environment"))
            {
                if (k == null) return parts;
                object v = k.GetValue("Path", null, RegistryValueOptions.DoNotExpandEnvironmentNames);
                if (v == null) return parts;
                kind = k.GetValueKind("Path");
                foreach (string p in ((string)v).Split(';'))
                    if (p.Trim().Length > 0) parts.Add(p);
            }
            return parts;
        }

        private static bool SamePath(string a, string b)
        {
            return string.Equals(Environment.ExpandEnvironmentVariables(a).TrimEnd('\\'),
                                 b.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);
        }

        public static void AddToUserPath(string folder)
        {
            RegistryValueKind kind;
            List<string> parts = UserPathParts(out kind);
            foreach (string p in parts)
                if (SamePath(p, folder)) return;
            parts.Add(folder);
            using (RegistryKey k = Registry.CurrentUser.CreateSubKey("Environment"))
                k.SetValue("Path", string.Join(";", parts.ToArray()), kind);
            BroadcastEnvironment();
        }

        public static void RemoveFromUserPath(string folder)
        {
            RegistryValueKind kind;
            List<string> parts = UserPathParts(out kind);
            int before = parts.Count;
            parts.RemoveAll(p => SamePath(p, folder));
            if (parts.Count == before) return;
            using (RegistryKey k = Registry.CurrentUser.CreateSubKey("Environment"))
            {
                if (parts.Count == 0) k.DeleteValue("Path", false);
                else k.SetValue("Path", string.Join(";", parts.ToArray()), kind);
            }
            BroadcastEnvironment();
        }

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint msg, UIntPtr wp, string lp,
                                                        uint flags, uint timeout, out UIntPtr result);

        // новые консоли и проводник перечитывают переменные среды
        private static void BroadcastEnvironment()
        {
            UIntPtr r;
            try { SendMessageTimeout((IntPtr)0xffff, 0x001A, UIntPtr.Zero, "Environment", 2, 3000, out r); } catch { }
        }

        // ---------- «Открыть с помощью» для .c, .h и проектов .wccproj (программу по умолчанию не меняет) ----------

        public static void RegisterOpenWith(string exe)
        {
            string command = "\"" + exe + "\" \"%1\"";
            using (RegistryKey k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\" + ProgId))
            {
                k.SetValue("", Loc.T("FileTypeC"));
                using (RegistryKey icon = k.CreateSubKey("DefaultIcon"))
                    icon.SetValue("", exe + ",0");
                using (RegistryKey open = k.CreateSubKey(@"shell\open\command"))
                    open.SetValue("", command);
            }
            foreach (string ext in Extensions)
                using (RegistryKey progids = Registry.CurrentUser.CreateSubKey(@"Software\Classes\" + ext + @"\OpenWithProgids"))
                    progids.SetValue(ProgId, new byte[0], RegistryValueKind.None);
            using (RegistryKey k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Applications\" + ExeName))
            {
                k.SetValue("FriendlyAppName", AppName);
                using (RegistryKey open = k.CreateSubKey(@"shell\open\command"))
                    open.SetValue("", command);
                using (RegistryKey types = k.CreateSubKey("SupportedTypes"))
                    foreach (string ext in Extensions) types.SetValue(ext, "");
            }
        }

        public static void UnregisterOpenWith()
        {
            DeleteKeyTree(Registry.CurrentUser, @"Software\Classes\" + ProgId);
            DeleteKeyTree(Registry.CurrentUser, @"Software\Classes\Applications\" + ExeName);
            foreach (string ext in Extensions)
                using (RegistryKey progids = Registry.CurrentUser.OpenSubKey(@"Software\Classes\" + ext + @"\OpenWithProgids", true))
                    if (progids != null) { try { progids.DeleteValue(ProgId, false); } catch { } }
        }

        // ---------- запись для «Приложения и возможности» ----------

        private static void RegisterUninstall(string folder, string exe)
        {
            long size = 0;
            try
            {
                foreach (string f in Directory.GetFiles(folder, "*", SearchOption.AllDirectories))
                    size += new FileInfo(f).Length;
            }
            catch { }

            using (RegistryKey k = Registry.CurrentUser.CreateSubKey(UninstallKey))
            {
                k.SetValue("DisplayName", LongName);
                k.SetValue("DisplayVersion", Version);
                k.SetValue("Publisher", Publisher);
                k.SetValue("DisplayIcon", exe + ",0");
                k.SetValue("InstallLocation", folder);
                k.SetValue("UninstallString", "\"" + Path.Combine(folder, SetupName) + "\" /uninstall");
                k.SetValue("QuietUninstallString", "\"" + Path.Combine(folder, SetupName) + "\" /uninstall /silent");
                k.SetValue("EstimatedSize", (int)(size / 1024), RegistryValueKind.DWord);
                k.SetValue("NoModify", 1, RegistryValueKind.DWord);
                k.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                k.SetValue("InstallDate", DateTime.Now.ToString("yyyyMMdd", CultureInfo.InvariantCulture));
            }
        }

        // ---------- удаление ----------

        public static void Uninstall(Action<string> say)
        {
            if (say == null) say = delegate { };
            string folder = InstalledFolder();
            if (string.IsNullOrEmpty(folder)) folder = DefaultFolder;

            if (IsRunning(Path.Combine(folder, ExeName)) || IsRunning(Path.Combine(folder, CompilerName)))
                throw new InvalidOperationException(Loc.T("RunningUninstall"));

            say(Loc.T("SayRemoveShortcuts"));
            SafeDelete(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), AppName + ".lnk"));
            SafeDelete(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), AppName + ".lnk"));

            say(Loc.T("SayRemovePath"));
            RemoveFromUserPath(folder);

            say(Loc.T("SayRemoveOpenWith"));
            UnregisterOpenWith();

            say(Loc.T("SayCleanRegistry"));
            DeleteKeyTree(Registry.CurrentUser, UninstallKey);

            say(Loc.T("SayRemoveFiles"));
            int left = RemoveFolderLater(folder);

            NotifyShell();
            say(left == 0
                ? Loc.T("UninstallDone")
                : Loc.F("UninstallDoneLeft", folder, left.ToString(CultureInfo.CurrentCulture)));
        }

        private static void SafeDelete(string path)
        {
            try { if (File.Exists(path)) File.Delete(path); } catch { }
        }

        private static void DeleteKeyTree(RegistryKey root, string path)
        {
            try { root.DeleteSubKeyTree(path, false); } catch { }
        }

        /// <summary>
        /// Уходят только файлы самого WinCC — те, что лежат в поставке этого установщика (и собранные из
        /// примеров exe рядом с ними); чужие файлы и папки в папке WinCC остаются. Опустевшие папки
        /// убираются. Установщик лежит внутри и сам себя удалить не может — его и пустую папку сносит
        /// отдельная команда. Возвращает, сколько чужих файлов осталось.
        /// </summary>
        private static int RemoveFolderLater(string folder)
        {
            int left = 0;
            try
            {
                var ours = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                using (Stream s = OpenPayload())
                using (ZipArchive zip = new ZipArchive(s, ZipArchiveMode.Read))
                    foreach (ZipArchiveEntry e in zip.Entries)
                        if (!e.FullName.EndsWith("/", StringComparison.Ordinal))
                            ours.Add(Path.GetFullPath(Path.Combine(folder, e.FullName.Replace('/', '\\'))));
                foreach (string f in ours)
                {
                    SafeDelete(f);
                    if (f.EndsWith(".c", StringComparison.OrdinalIgnoreCase))     // пример, собранный на месте
                        SafeDelete(Path.ChangeExtension(f, ".exe"));
                }
                var dirs = new List<string>(Directory.GetDirectories(folder, "*", SearchOption.AllDirectories));
                dirs.Sort((a, b) => b.Length.CompareTo(a.Length));          // сначала самые глубокие
                foreach (string d in dirs)
                    try { if (Directory.GetFileSystemEntries(d).Length == 0) Directory.Delete(d); } catch { }
                foreach (string f in Directory.GetFiles(folder, "*", SearchOption.AllDirectories))
                    if (!string.Equals(Path.GetFileName(f), SetupName, StringComparison.OrdinalIgnoreCase)) left++;
            }
            catch { }

            // сам установщик, затем папка — если она опустела (rd без /s чужое не сносит)
            string cmd = string.Format(CultureInfo.InvariantCulture,
                "/c ping 127.0.0.1 -n 3 >nul & del /f /q \"{0}\" & rd \"{1}\"", Path.Combine(folder, SetupName), folder);
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo("cmd.exe", cmd);
                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                Process.Start(psi);
            }
            catch { }
            return left;
        }

        [DllImport("shell32.dll")]
        private static extern void SHChangeNotify(int eventId, uint flags, IntPtr item1, IntPtr item2);

        public static void NotifyShell()
        {
            try { SHChangeNotify(0x08000000, 0x0000, IntPtr.Zero, IntPtr.Zero); } catch { }
        }

        // ---------- проверка без изменений в системе ----------

        public static string DryRun()
        {
            StringBuilder sb = new StringBuilder();
            string tmp = Path.Combine(Path.GetTempPath(), "wincc-setup-проба");
            if (Directory.Exists(tmp)) Directory.Delete(tmp, true);
            Directory.CreateDirectory(tmp);

            Stopwatch sw = Stopwatch.StartNew();
            int n = ExtractPayload(tmp, null);
            sw.Stop();
            long size = 0;
            foreach (string f in Directory.GetFiles(tmp, "*", SearchOption.AllDirectories))
                size += new FileInfo(f).Length;

            sb.AppendLine("Установщик " + LongName + " " + Version);
            sb.AppendLine("Файлов внутри: " + n + ", распаковано за " + sw.ElapsedMilliseconds + " мс");
            sb.AppendLine("Объём после установки: " + (size / 1024) + " КБ");
            foreach (string f in new[] { ExeName, CompilerName, @"runtime\runtime.c", @"include\windows.h", "README.md" })
                sb.AppendLine(f + ": " + (File.Exists(Path.Combine(tmp, f)) ? "на месте" : "НЕТ"));
            sb.AppendLine("Примеров: " + Directory.GetFiles(Path.Combine(tmp, "examples"), "*.c", SearchOption.AllDirectories).Length);
            sb.AppendLine("Папка по умолчанию: " + DefaultFolder);
            try { Directory.Delete(tmp, true); } catch { }
            return sb.ToString();
        }
    }
}
