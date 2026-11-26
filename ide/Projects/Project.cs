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
    /// A WinCC project — a "name.wccproj" file: which files make up the program, its kind, output name and wcc arguments.
    /// UTF-8 text, "key=value" lines, paths relative to the project folder:
    ///   kind=Window            kind: Console, Window, Dll
    ///   output=Calc.exe        output file name (empty — the project name)
    ///   file=main.c            a project file (one per line, in build order); .rc and .res are resources
    ///   define=DEBUG=1         macro (-D)
    ///   include=inc            #include folder (-I)
    ///   lib=dwmapi             DLL with functions (-l)
    ///   stack=16               stack in MB (--stack), 0 — default
    ///   options=-S             other wcc arguments as is
    /// Lines starting with # are comments and are skipped.
    /// </summary>
    public class Project
    {
        public const string Extension = ".wccproj";

        /// <summary>Files wcc builds into the program (code and resources); others (headers, images) are only listed.</summary>
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

        /// <summary>Output file name: the explicit one, or the project name with the kind's extension.</summary>
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

        /// <summary>Resources: a .rc script or a compiled .res.</summary>
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

        /// <summary>The path as written in the project: relative to the project folder if the file is inside it, otherwise full.</summary>
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

        /// <summary>Add a file (full path); one already in the project is not repeated. true — added.</summary>
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

        /// <summary>wcc arguments: files in order, -o, kind, macros, folders, DLLs, stack, the rest.</summary>
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
