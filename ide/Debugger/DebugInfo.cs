using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace WinCC.Debugger
{
    /// <summary>
    /// Сведения отладчика о программе — файл «программа.wdbg», его пишет wcc -g (compiler\asm.c, dbg_text):
    /// функции и их код (адреса — RVA), таблица строк, рамки функций, переменные, глобальные, структуры.
    /// </summary>
    public class DebugInfo
    {
        public readonly List<string> Files = new List<string>();
        /// <summary>Файл программы (а не библиотеки WinCC): в его строках отладчик останавливается.</summary>
        public readonly List<bool> UserFiles = new List<bool>();
        public readonly List<DbgFunction> Functions = new List<DbgFunction>();     // по адресу
        public readonly List<DbgLine> Lines = new List<DbgLine>();                 // по адресу
        public readonly List<DbgVariable> Globals = new List<DbgVariable>();
        public readonly Dictionary<int, DbgStruct> Structs = new Dictionary<int, DbgStruct>();

        /// <summary>Прочитать файл; folder — откуда считаются относительные пути исходников, home — папка WinCC (её runtime\ и lib\ — библиотека).</summary>
        public static DebugInfo Load(string path, string folder, string home)
        {
            var info = new DebugInfo();
            DbgFunction fn = null;
            DbgStruct st = null;
            string lib1 = Path.Combine(home, "runtime") + "\\", lib2 = Path.Combine(home, "lib") + "\\";
            foreach (string raw in File.ReadAllLines(path, Encoding.UTF8))
            {
                string[] w = raw.Split(' ');
                switch (w[0])
                {
                    case "file":
                        {
                            int n = int.Parse(w[1], CultureInfo.InvariantCulture);
                            string name = raw.Substring(raw.IndexOf(' ', 5) + 1);
                            string full = name.Length == 0 ? "" : Full(name, folder);
                            while (info.Files.Count <= n) { info.Files.Add(""); info.UserFiles.Add(false); }
                            info.Files[n] = full;
                            info.UserFiles[n] = full.Length > 0 && !full.StartsWith(lib1, StringComparison.OrdinalIgnoreCase)
                                                && !full.StartsWith(lib2, StringComparison.OrdinalIgnoreCase);
                            break;
                        }
                    case "func":
                        fn = new DbgFunction
                        {
                            Start = Hex(w[1]),
                            End = Hex(w[2]),
                            Name = Rest(raw, 3)
                        };
                        info.Functions.Add(fn);
                        break;
                    case "frame":
                        if (fn == null) break;
                        fn.HasFrame = true;
                        fn.SaveCount = int.Parse(w[1], CultureInfo.InvariantCulture);
                        fn.SavePad = int.Parse(w[2], CultureInfo.InvariantCulture);
                        fn.SavedRegs = w.Skip(3).Where(x => x.Length > 0).ToArray();
                        break;
                    case "var":
                        if (fn == null || w.Length < 5) break;
                        fn.Variables.Add(new DbgVariable
                        {
                            Location = w[1],
                            Line = int.Parse(w[2], CultureInfo.InvariantCulture),
                            Type = DbgType.Parse(w[3]),
                            Name = w[4]
                        });
                        break;
                    case "line":
                        info.Lines.Add(new DbgLine
                        {
                            Rva = Hex(w[1]),
                            File = int.Parse(w[2], CultureInfo.InvariantCulture),
                            Line = int.Parse(w[3], CultureInfo.InvariantCulture)
                        });
                        break;
                    case "global":      // RVA FILE LINE TYPE NAME: только переменные файлов программы (и её библиотек .lib)
                        {
                            if (w.Length < 6) break;
                            int file = int.Parse(w[2], CultureInfo.InvariantCulture);
                            info.Globals.Add(new DbgVariable
                            {
                                Location = "g" + w[1],
                                Rva = Hex(w[1]),
                                File = file,
                                Line = int.Parse(w[3], CultureInfo.InvariantCulture),
                                Type = DbgType.Parse(w[4]),
                                Name = w[5]
                            });
                            break;
                        }
                    case "struct":
                        st = new DbgStruct
                        {
                            Id = int.Parse(w[1], CultureInfo.InvariantCulture),
                            Size = int.Parse(w[2], CultureInfo.InvariantCulture),
                            IsUnion = w[3] == "u"
                        };
                        info.Structs[st.Id] = st;
                        break;
                    case "field":
                        if (st == null || w.Length < 6) break;
                        st.Fields.Add(new DbgField
                        {
                            Offset = int.Parse(w[1], CultureInfo.InvariantCulture),
                            BitOffset = int.Parse(w[2], CultureInfo.InvariantCulture),
                            BitWidth = int.Parse(w[3], CultureInfo.InvariantCulture),
                            Type = DbgType.Parse(w[4]),
                            Name = w[5] == "-" ? "" : w[5]
                        });
                        break;
                }
            }
            info.Globals.RemoveAll(g => g.File >= info.UserFiles.Count || !info.UserFiles[g.File]);
            info.Functions.Sort((a, b) => a.Start.CompareTo(b.Start));
            info.Lines.Sort((a, b) => a.Rva.CompareTo(b.Rva));
            // строки — по функциям: так строка не «перетекает» в пролог следующей функции
            foreach (var f in info.Functions)
            {
                int i = info.FirstLineAtOrAfter(f.Start);
                for (; i < info.Lines.Count && info.Lines[i].Rva < f.End; i++)
                {
                    f.Lines.Add(info.Lines[i]);
                    if (info.Lines[i].File < info.UserFiles.Count && info.UserFiles[info.Lines[i].File]) f.IsUser = true;
                }
            }
            return info;
        }

        static string Full(string name, string folder)
        {
            try { return Path.GetFullPath(Path.IsPathRooted(name) ? name : Path.Combine(folder, name)); }
            catch (Exception) { return name; }
        }

        static uint Hex(string s) { return uint.Parse(s, NumberStyles.HexNumber, CultureInfo.InvariantCulture); }

        static string Rest(string line, int words)
        {
            int p = 0;
            for (int i = 0; i < words && p >= 0; i++) p = line.IndexOf(' ', p) + 1;
            return p > 0 ? line.Substring(p) : "";
        }

        int FirstLineAtOrAfter(uint rva)
        {
            int lo = 0, hi = Lines.Count;
            while (lo < hi)
            {
                int mid = (lo + hi) / 2;
                if (Lines[mid].Rva < rva) lo = mid + 1; else hi = mid;
            }
            return lo;
        }

        public DbgFunction FunctionAt(uint rva)
        {
            int lo = 0, hi = Functions.Count - 1;
            while (lo <= hi)
            {
                int mid = (lo + hi) / 2;
                var f = Functions[mid];
                if (rva < f.Start) hi = mid - 1;
                else if (rva >= f.End) lo = mid + 1;
                else return f;
            }
            return null;
        }

        /// <summary>Строка, к которой относится адрес (в пределах его функции); null — пролог или код без строк.</summary>
        public DbgLine LineAt(uint rva)
        {
            var f = FunctionAt(rva);
            if (f == null) return null;
            DbgLine best = null;
            foreach (var l in f.Lines)
            {
                if (l.Rva > rva) break;
                best = l;
            }
            return best;
        }

        /// <summary>Адрес — начало строки (её первый байт) в функции программы.</summary>
        public bool IsLineStart(uint rva, out DbgLine line)
        {
            line = null;
            var f = FunctionAt(rva);
            if (f == null || !f.IsUser) return false;
            foreach (var l in f.Lines)
                if (l.Rva == rva) { line = l; return UserFiles[l.File]; }
            return false;
        }

        public int FileIndex(string path)
        {
            for (int i = 0; i < Files.Count; i++)
                if (string.Equals(Files[i], path, StringComparison.OrdinalIgnoreCase)) return i;
            return -1;
        }

        /// <summary>Адреса для точки останова на строке: её начала; на строке без кода — ближайшая следующая строка с кодом
        /// (не дальше 20 строк). Возвращает и строку, на которую она встала.</summary>
        public List<uint> AddressesForLine(string path, int line, out int actualLine)
        {
            actualLine = line;
            var result = new List<uint>();
            int file = FileIndex(path);
            if (file < 0) return result;
            for (int l = line; l < line + 20 && result.Count == 0; l++)
            {
                foreach (var f in Functions)
                    foreach (var e in f.Lines)
                        if (e.File == file && e.Line == l) result.Add(e.Rva);
                if (result.Count > 0) actualLine = l;
            }
            return result;
        }
    }

    public class DbgFunction
    {
        public uint Start, End;
        public string Name;
        public bool HasFrame;               // известна рамка (функция программы)
        public int SaveCount, SavePad;
        public string[] SavedRegs = new string[0];
        public bool IsUser;                 // есть строки файлов программы
        public readonly List<DbgVariable> Variables = new List<DbgVariable>();
        public readonly List<DbgLine> Lines = new List<DbgLine>();
    }

    public class DbgLine
    {
        public uint Rva;
        public int File, Line;
    }

    public class DbgVariable
    {
        /// <summary>s-16 — по адресу rbp-16; S-16 — по rbp-16 лежит адрес; rrbx — в регистре; gRVA — глобальная.</summary>
        public string Location;
        public uint Rva;
        public int File;                    // глобальная: её файл
        public int Line;                    // 0 — параметр
        public DbgType Type;
        public string Name;
        public bool IsParameter { get { return Line == 0; } }
    }

    public class DbgStruct
    {
        public int Id, Size;
        public bool IsUnion;
        public readonly List<DbgField> Fields = new List<DbgField>();
    }

    public class DbgField
    {
        public int Offset, BitOffset, BitWidth;
        public DbgType Type;
        public string Name;
    }

    public enum DbgTypeKind { Void, Bool, Char, UChar, Int, UInt, Enum, Float, Pointer, Array, Struct, Function, Unknown }

    /// <summary>Тип из строки wcc: c C b i4 u8 f8 e4 v F *T [N]T sID.</summary>
    public class DbgType
    {
        public DbgTypeKind Kind;
        public int Size;
        public int Count;                   // массив
        public int StructId;
        public DbgType Element;             // указатель, массив

        public static DbgType Parse(string s)
        {
            int p = 0;
            return Parse(s, ref p);
        }

        static DbgType Parse(string s, ref int p)
        {
            if (p >= s.Length) return new DbgType { Kind = DbgTypeKind.Unknown };
            char c = s[p++];
            switch (c)
            {
                case 'v': return new DbgType { Kind = DbgTypeKind.Void };
                case 'b': return new DbgType { Kind = DbgTypeKind.Bool, Size = 1 };
                case 'c': return new DbgType { Kind = DbgTypeKind.Char, Size = 1 };
                case 'C': return new DbgType { Kind = DbgTypeKind.UChar, Size = 1 };
                case 'F': return new DbgType { Kind = DbgTypeKind.Function };
                case 'i': case 'u': case 'f': case 'e':
                    {
                        int n = Number(s, ref p);
                        var k = c == 'i' ? DbgTypeKind.Int : c == 'u' ? DbgTypeKind.UInt : c == 'f' ? DbgTypeKind.Float : DbgTypeKind.Enum;
                        return new DbgType { Kind = k, Size = n };
                    }
                case '*': return new DbgType { Kind = DbgTypeKind.Pointer, Size = 8, Element = Parse(s, ref p) };
                case '[':
                    {
                        int n = Number(s, ref p);
                        if (p < s.Length && s[p] == ']') p++;
                        var el = Parse(s, ref p);
                        return new DbgType { Kind = DbgTypeKind.Array, Count = n, Element = el };
                    }
                case 's': return new DbgType { Kind = DbgTypeKind.Struct, StructId = Number(s, ref p) };
                default: return new DbgType { Kind = DbgTypeKind.Unknown };
            }
        }

        static int Number(string s, ref int p)
        {
            int n = 0;
            while (p < s.Length && char.IsDigit(s[p])) n = n * 10 + (s[p++] - '0');
            return n;
        }

        public int SizeIn(DebugInfo info)
        {
            switch (Kind)
            {
                case DbgTypeKind.Array: return Count * (Element != null ? Element.SizeIn(info) : 0);
                case DbgTypeKind.Struct:
                    DbgStruct st;
                    return info.Structs.TryGetValue(StructId, out st) ? st.Size : 0;
                default: return Size;
            }
        }

        /// <summary>Имя типа по-сишному: int, unsigned char *, int[4], struct.</summary>
        public string Name(DebugInfo info)
        {
            switch (Kind)
            {
                case DbgTypeKind.Void: return "void";
                case DbgTypeKind.Bool: return "bool";
                case DbgTypeKind.Char: return "char";
                case DbgTypeKind.UChar: return "unsigned char";
                case DbgTypeKind.Int: return Size == 2 ? "short" : Size == 8 ? "long long" : "int";
                case DbgTypeKind.UInt: return Size == 2 ? "unsigned short" : Size == 8 ? "unsigned long long" : "unsigned";
                case DbgTypeKind.Enum: return "enum";
                case DbgTypeKind.Float: return Size == 4 ? "float" : "double";
                case DbgTypeKind.Function: return "function";
                case DbgTypeKind.Pointer: return (Element != null ? Element.Name(info) : "void") + " *";
                case DbgTypeKind.Array: return (Element != null ? Element.Name(info) : "?") + "[" + Count + "]";
                case DbgTypeKind.Struct:
                    DbgStruct st;
                    return info.Structs.TryGetValue(StructId, out st) && st.IsUnion ? "union" : "struct";
                default: return "?";
            }
        }
    }
}
