using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using WinCC.Build;

namespace WinCC.Projects
{
    /// <summary>
    /// Проект WinCC — файл «имя.wccproj»: какие файлы входят в программу, её вид, имя готового файла и параметры wcc.
    /// Текст UTF-8, строки «ключ=значение», пути — от папки проекта:
    ///   kind=Window            вид: Console, Window, Dll
    ///   output=Калькулятор.exe имя готового файла (пусто — имя проекта)
    ///   file=main.c            файл проекта (каждый своей строкой, по порядку сборки); .rc и .res — ресурсы
    ///   define=DEBUG=1         макрос (-D)
    ///   include=inc            папка #include (-I)
    ///   lib=dwmapi             DLL с функциями (-l)
    ///   stack=16               стек в МБ (--stack), 0 — по умолчанию
    ///   options=-S             остальные параметры wcc как есть
    /// Строки с # — пояснения, читаются мимо.
    /// </summary>
    public class Project
    {
        public const string Extension = ".wccproj";

        /// <summary>Файлы, которые wcc собирает в программу (код и ресурсы); остальные (заголовки, картинки) — только в списке.</summary>
        static readonly string[] buildExtensions = { ".c", ".asm", ".rc", ".res" };

        public string Path { get; private set; }
        public ProgramKind Kind = ProgramKind.Console;
        public string Output = "";
        public readonly List<string> Files = new List<string>();
        public readonly List<string> Defines = new List<string>();
        public readonly List<string> Includes = new List<string>();
        public readonly List<string> Libs = new List<string>();
        public int StackMb;
        public string Options = "";

        public Project(string path) { Path = System.IO.Path.GetFullPath(path); }

        public string Name { get { return System.IO.Path.GetFileNameWithoutExtension(Path); } }
        public string Folder { get { return System.IO.Path.GetDirectoryName(Path); } }

        /// <summary>Имя готового файла: своё или имя проекта с расширением вида.</summary>
        public string OutputName
        {
            get
            {
                string name = string.IsNullOrWhiteSpace(Output) ? Name : Output.Trim();
                return System.IO.Path.HasExtension(name) ? name : name + ProgramKinds.Extension(Kind);
            }
        }

        public string OutputPath { get { return System.IO.Path.GetFullPath(System.IO.Path.Combine(Folder, OutputName)); } }

        public string FullPath(string file) { return System.IO.Path.GetFullPath(System.IO.Path.Combine(Folder, file)); }

        public static bool IsBuildFile(string file)
        {
            string ext = System.IO.Path.GetExtension(file);
            return buildExtensions.Any(e => string.Equals(e, ext, StringComparison.OrdinalIgnoreCase));
        }

        /// <summary>Ресурсы: сценарий .rc или готовый .res.</summary>
        public static bool IsResource(string file)
        {
            string ext = System.IO.Path.GetExtension(file);
            return string.Equals(ext, ".rc", StringComparison.OrdinalIgnoreCase) || string.Equals(ext, ".res", StringComparison.OrdinalIgnoreCase);
        }

        public static bool IsHeader(string file)
        {
            return string.Equals(System.IO.Path.GetExtension(file), ".h", StringComparison.OrdinalIgnoreCase);
        }

        public static bool IsProjectFile(string path)
        {
            return string.Equals(System.IO.Path.GetExtension(path), Extension, StringComparison.OrdinalIgnoreCase);
        }

        public IEnumerable<string> BuildFiles { get { return Files.Where(IsBuildFile); } }

        /// <summary>Путь как он пишется в проекте: от папки проекта, если файл внутри неё, иначе полный.</summary>
        public string Relative(string fullPath)
        {
            string full = System.IO.Path.GetFullPath(fullPath);
            string folder = Folder.TrimEnd('\\') + "\\";
            return full.StartsWith(folder, StringComparison.OrdinalIgnoreCase) ? full.Substring(folder.Length) : full;
        }

        public bool Contains(string fullPath)
        {
            return IndexOf(fullPath) >= 0;
        }

        public int IndexOf(string fullPath)
        {
            string full = System.IO.Path.GetFullPath(fullPath);
            return Files.FindIndex(f => string.Equals(FullPath(f), full, StringComparison.OrdinalIgnoreCase));
        }

        /// <summary>Добавить файл (полный путь); уже входящий не повторяется. true — добавлен.</summary>
        public bool Add(string fullPath)
        {
            if (Contains(fullPath)) return false;
            Files.Add(Relative(fullPath));
            return true;
        }

        public bool Remove(string fullPath)
        {
            int i = IndexOf(fullPath);
            if (i < 0) return false;
            Files.RemoveAt(i);
            return true;
        }

        /// <summary>Параметры wcc: файлы по порядку, -o, вид, макросы, папки, DLL, стек, остальное.</summary>
        public string Arguments()
        {
            var sb = new StringBuilder();
            foreach (string f in BuildFiles) sb.Append(Quote(f)).Append(' ');
            sb.Append("-o ").Append(Quote(OutputName));
            string sw = ProgramKinds.Switch(Kind);
            if (sw != null) sb.Append(' ').Append(sw);
            foreach (string d in Defines) sb.Append(" -D").Append(Quote(d));
            foreach (string i in Includes) sb.Append(" -I").Append(Quote(i));
            foreach (string l in Libs) sb.Append(" -l").Append(Quote(l));
            if (StackMb > 0) sb.Append(" --stack ").Append(StackMb.ToString(CultureInfo.InvariantCulture));
            if (!string.IsNullOrWhiteSpace(Options)) sb.Append(' ').Append(Options.Trim());
            return sb.ToString();
        }

        static string Quote(string s) { return s.IndexOfAny(new[] { ' ', '\t' }) >= 0 ? "\"" + s + "\"" : s; }

        public static Project Load(string path)
        {
            var p = new Project(path);
            foreach (string raw in File.ReadAllLines(path, Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line[0] == '#') continue;
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                string key = line.Substring(0, eq).Trim().ToLowerInvariant(), value = line.Substring(eq + 1).Trim();
                switch (key)
                {
                    case "kind": p.Kind = ProgramKinds.Parse(value); break;
                    case "output": p.Output = value; break;
                    case "file": if (value.Length > 0) p.Files.Add(value); break;
                    case "define": if (value.Length > 0) p.Defines.Add(value); break;
                    case "include": if (value.Length > 0) p.Includes.Add(value); break;
                    case "lib": if (value.Length > 0) p.Libs.Add(value); break;
                    case "stack":
                        int mb;
                        if (int.TryParse(value, out mb) && mb >= 0) p.StackMb = mb;
                        break;
                    case "options": p.Options = value; break;
                }
            }
            return p;
        }

        public void Save()
        {
            var sb = new StringBuilder();
            sb.AppendLine("# WinCC project");
            sb.AppendLine("kind=" + Kind);
            if (!string.IsNullOrWhiteSpace(Output)) sb.AppendLine("output=" + Output.Trim());
            foreach (string f in Files) sb.AppendLine("file=" + f);
            foreach (string d in Defines) sb.AppendLine("define=" + d);
            foreach (string i in Includes) sb.AppendLine("include=" + i);
            foreach (string l in Libs) sb.AppendLine("lib=" + l);
            if (StackMb > 0) sb.AppendLine("stack=" + StackMb.ToString(CultureInfo.InvariantCulture));
            if (!string.IsNullOrWhiteSpace(Options)) sb.AppendLine("options=" + Options.Trim());
            File.WriteAllText(Path, sb.ToString(), new UTF8Encoding(false));
        }
    }
}
