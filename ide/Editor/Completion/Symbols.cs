using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;

namespace WinCC.Editor.Completion
{
    /// <summary>Что за имя в подсказке — от этого его значок и цвет.</summary>
    public enum SymbolKind { Keyword, Type, Function, Macro, Constant, Variable, Word }

    /// <summary>Имя для подсказки: само имя, вид, описание (объявление функции, значение макроса) и откуда оно.</summary>
    public class Symbol
    {
        public string Name;
        public SymbolKind Kind;
        public string Detail;
        public string Source;
    }

    /// <summary>
    /// Имена из текста на Си: функции (с объявлением), макросы, typedef, постоянные enum, глобальные переменные.
    /// Разбор грубый, без препроцессора: ему нужно найти имена, а не понять программу.
    /// </summary>
    public static class Declarations
    {
        static readonly Regex defineRe = new Regex(@"^\s*#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?\s*(.*)$", RegexOptions.Compiled);
        static readonly Regex identRe = new Regex(@"[A-Za-z_]\w*", RegexOptions.Compiled);
        static readonly HashSet<string> notNames = new HashSet<string>
        {
            "const", "volatile", "static", "extern", "inline", "struct", "union", "enum", "unsigned", "signed",
            "int", "char", "short", "long", "float", "double", "void", "typedef", "restrict", "__declspec",
            "WINAPI", "CALLBACK", "APIENTRY", "__stdcall", "__cdecl", "_Noreturn", "register", "sizeof", "return"
        };

        /// <summary>Строки с «\» в конце склеиваются, комментарии убираются (строки в кавычках сохраняются).</summary>
        public static string Clean(string text)
        {
            var sb = new StringBuilder(text.Length);
            for (int i = 0; i < text.Length; i++)
            {
                char c = text[i];
                if (c == '"' || c == '\'')
                {
                    char q = c;
                    sb.Append(c);
                    for (i++; i < text.Length && text[i] != q && text[i] != '\n'; i++)
                    {
                        if (text[i] == '\\' && i + 1 < text.Length) sb.Append(text[i++]);
                        sb.Append(text[i]);
                    }
                    if (i < text.Length) sb.Append(text[i]);
                }
                else if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    while (i < text.Length && text[i] != '\n') i++;
                    if (i < text.Length) sb.Append('\n');
                }
                else if (c == '/' && i + 1 < text.Length && text[i + 1] == '*')
                {
                    int end = text.IndexOf("*/", i + 2, StringComparison.Ordinal);
                    if (end < 0) end = text.Length - 2;
                    for (int k = i; k < end + 2 && k < text.Length; k++) if (text[k] == '\n') sb.Append('\n');
                    sb.Append(' ');
                    i = end + 1;
                }
                else if (c == '\\' && i + 1 < text.Length && (text[i + 1] == '\n' || text[i + 1] == '\r'))
                {
                    i++;
                    if (text[i] == '\r' && i + 1 < text.Length && text[i + 1] == '\n') i++;
                    sb.Append(' ');
                }
                else sb.Append(c);
            }
            return sb.ToString();
        }

        /// <summary>Строки «#include …» текста: (имя, в угловых скобках ли).</summary>
        public static IEnumerable<KeyValuePair<string, bool>> Includes(string clean)
        {
            foreach (Match m in Regex.Matches(clean, @"^\s*#\s*include\s*([<""])([^>""]+)[>""]", RegexOptions.Multiline))
                yield return new KeyValuePair<string, bool>(m.Groups[2].Value.Trim(), m.Groups[1].Value == "<");
        }

        /// <summary>Все имена, объявленные в тексте на верхнем уровне.</summary>
        public static List<Symbol> Parse(string text, string source)
        {
            var list = new List<Symbol>();
            string clean = Clean(text);
            var code = new StringBuilder(clean.Length);
            foreach (string raw in clean.Split('\n'))
            {
                string line = raw.TrimEnd('\r');
                if (line.TrimStart().StartsWith("#"))
                {
                    var m = defineRe.Match(line);
                    if (m.Success && !m.Groups[1].Value.StartsWith("__") && !m.Groups[1].Value.EndsWith("_H"))
                    {
                        string args = m.Groups[2].Value;
                        string value = Squeeze(m.Groups[3].Value);
                        bool isType = !string.IsNullOrEmpty(value) && args.Length == 0 && Regex.IsMatch(value, @"^(struct\s+)?[A-Za-z_]\w*\s*\**$")
                                      && !Regex.IsMatch(value, @"^[A-Z0-9_]+$");
                        list.Add(new Symbol
                        {
                            Name = m.Groups[1].Value,
                            Kind = args.Length > 0 ? SymbolKind.Macro : isType ? SymbolKind.Type : SymbolKind.Constant,
                            Detail = "#define " + m.Groups[1].Value + args + (value.Length > 0 ? " " + Clip(value, 70) : ""),
                            Source = source
                        });
                    }
                    code.Append('\n');
                    continue;
                }
                code.Append(line).Append('\n');
            }
            Statements(code.ToString(), source, list);
            return list;
        }

        // Верхний уровень: объявления до «;», определения функций — заголовок до «{».
        static void Statements(string code, string source, List<Symbol> list)
        {
            var stmt = new StringBuilder();
            int i = 0, n = code.Length;
            while (i < n)
            {
                char c = code[i];
                if (c == '"' || c == '\'')
                {
                    char q = c;
                    stmt.Append(c);
                    for (i++; i < n && code[i] != q; i++) { if (code[i] == '\\') stmt.Append(code[i++]); if (i < n) stmt.Append(code[i]); }
                    if (i < n) stmt.Append(code[i]);
                    i++;
                    continue;
                }
                if (c == ';')
                {
                    Statement(stmt.ToString(), source, list);
                    stmt.Clear();
                    i++;
                    continue;
                }
                if (c == '}')
                {
                    stmt.Clear();           // конец extern "C" { … }
                    i++;
                    continue;
                }
                if (c == '{' && Regex.IsMatch(stmt.ToString(), @"^\s*extern\s*""C""\s*$"))
                {
                    stmt.Clear();           // extern "C" { — объявления внутри как на верхнем уровне
                    i++;
                    continue;
                }
                if (c == '{')
                {
                    int depth = 1, start = i + 1;
                    for (i++; i < n && depth > 0; i++)
                    {
                        if (code[i] == '{') depth++;
                        else if (code[i] == '}') depth--;
                    }
                    string head = stmt.ToString().TrimEnd();
                    string body = code.Substring(start, Math.Max(0, i - 1 - start));
                    if (head.EndsWith(")"))
                    {
                        Function(head, source, list);       // определение функции
                        stmt.Clear();
                    }
                    else
                    {
                        if (Regex.IsMatch(head, @"\benum\b")) EnumBody(body, source, list);
                        stmt.Append(" {…} ");
                    }
                    continue;
                }
                stmt.Append(c);
                i++;
            }
        }

        static void Statement(string s, string source, List<Symbol> list)
        {
            s = Squeeze(s);
            if (s.Length == 0) return;
            if (s.StartsWith("typedef "))
            {
                // последнее имя каждого из перечисленных через запятую: typedef struct {…} RECT, *PRECT;
                string rest = s.Substring(8);
                int paren = rest.IndexOf("(*", StringComparison.Ordinal);
                if (paren >= 0)
                {
                    var m = Regex.Match(rest, @"\(\s*\*\s*([A-Za-z_]\w*)\s*\)");
                    if (m.Success) list.Add(new Symbol { Name = m.Groups[1].Value, Kind = SymbolKind.Type, Detail = Clip(s, 90), Source = source });
                    return;
                }
                int brace = rest.LastIndexOf('}');
                string tail = brace >= 0 ? rest.Substring(brace + 1) : rest;
                string[] parts = tail.Split(',');
                for (int k = 0; k < parts.Length; k++)
                {
                    var ids = identRe.Matches(parts[k]);
                    if (ids.Count == 0) continue;
                    if (k == 0 && brace < 0 && ids.Count < 2) continue;
                    string name = ids[ids.Count - 1].Value;
                    list.Add(new Symbol { Name = name, Kind = SymbolKind.Type, Detail = Clip("typedef " + (brace >= 0 ? rest.Substring(0, rest.IndexOf('{')).Trim() + " {…} " : "") + parts[k].Trim(), 90), Source = source });
                }
                return;
            }
            if (s.EndsWith(")") || Regex.IsMatch(s, @"\)\s*(__attribute__.*)?$"))
            {
                Function(s, source, list);
                return;
            }
            // переменные: extern int errno; static HWND g_hwnd = 0;
            if (s.Contains("(")) return;
            string decl = s.Split('=')[0];
            if (decl.Contains("{…}") && !decl.TrimEnd().EndsWith("}")) decl = decl.Substring(decl.LastIndexOf('}') + 1);
            foreach (string part in decl.Split(','))
            {
                var ids = identRe.Matches(part.Split('[')[0]);
                if (ids.Count < 1) continue;
                string name = ids[ids.Count - 1].Value;
                if (notNames.Contains(name) || ids.Count < 2 && !part.Contains("*")) continue;
                list.Add(new Symbol { Name = name, Kind = SymbolKind.Variable, Detail = Clip(s, 90), Source = source });
            }
        }

        static void Function(string head, string source, List<Symbol> list)
        {
            head = Squeeze(head);
            int open = head.IndexOf('(');
            if (open <= 0) return;
            // указатель на функцию в объявлении: int (*name)(…)
            var fp = Regex.Match(head, @"^\s*[^()]*\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(");
            if (fp.Success)
            {
                list.Add(new Symbol { Name = fp.Groups[1].Value, Kind = SymbolKind.Variable, Detail = Clip(head, 110), Source = source });
                return;
            }
            var ids = identRe.Matches(head.Substring(0, open));
            if (ids.Count < 1) return;
            string name = ids[ids.Count - 1].Value;
            if (notNames.Contains(name) || name == "if" || name == "while" || name == "for" || name == "switch") return;
            string sig = Regex.Replace(head, @"\b(extern|__declspec\([^)]*\)|WINAPI|__stdcall|__cdecl)\s+", "").Trim();
            list.Add(new Symbol { Name = name, Kind = SymbolKind.Function, Detail = Clip(sig, 400), Source = source });
        }

        static void EnumBody(string body, string source, List<Symbol> list)
        {
            foreach (string part in body.Split(','))
            {
                var m = Regex.Match(part, @"^\s*([A-Za-z_]\w*)\s*(=\s*(.+))?$", RegexOptions.Singleline);
                if (!m.Success) continue;
                string v = Squeeze(m.Groups[3].Value);
                list.Add(new Symbol { Name = m.Groups[1].Value, Kind = SymbolKind.Constant, Detail = "enum " + m.Groups[1].Value + (v.Length > 0 ? " = " + Clip(v, 50) : ""), Source = source });
            }
        }

        static string Squeeze(string s) { return Regex.Replace(s ?? "", @"\s+", " ").Trim(); }
        static string Clip(string s, int max) { return s.Length > max ? s.Substring(0, max - 1) + "…" : s; }
    }

    /// <summary>
    /// Имена заголовков, которые подключает файл: встроенные заголовки wcc (их текст взят из compiler\headers.c,
    /// вшитого в окно), заголовки папки include рядом с wcc.exe и свои «"имя.h"» рядом с файлом. Вложенные #include
    /// проходятся тоже. Разобранный заголовок помнится, пока его файл не изменился.
    /// </summary>
    public static class HeaderIndex
    {
        class Entry { public DateTime Stamp; public List<Symbol> Symbols; public List<KeyValuePair<string, bool>> Includes; public string Dir; }

        static readonly Dictionary<string, Entry> cache = new Dictionary<string, Entry>(StringComparer.OrdinalIgnoreCase);
        static Dictionary<string, string> builtin;

        static string IncludeDir { get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "include"); } }

        /// <summary>Имена всех заголовков, которые подключает текст (с вложенными), — по одному разу.</summary>
        public static List<Symbol> For(string text, string fileDir)
        {
            var result = new List<Symbol>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var queue = new Queue<KeyValuePair<string, string>>();          // (ключ заголовка, его папка)
            foreach (var inc in Declarations.Includes(Declarations.Clean(text)))
                Enqueue(queue, inc, fileDir);
            while (queue.Count > 0 && seen.Count < 200)
            {
                var item = queue.Dequeue();
                if (!seen.Add(item.Key)) continue;
                var e = Load(item.Key);
                if (e == null) continue;
                result.AddRange(e.Symbols);
                foreach (var inc in e.Includes) Enqueue(queue, inc, e.Dir);
            }
            return result;
        }

        // ключ: полный путь файла или «builtin:имя.h»
        static void Enqueue(Queue<KeyValuePair<string, string>> queue, KeyValuePair<string, bool> inc, string dir)
        {
            string name = inc.Key.Replace('/', '\\');
            if (!inc.Value && dir != null)
            {
                string near = Path.Combine(dir, name);
                if (File.Exists(near)) { queue.Enqueue(new KeyValuePair<string, string>(Path.GetFullPath(near), null)); return; }
            }
            string lib = Path.Combine(IncludeDir, name);
            if (File.Exists(lib)) { queue.Enqueue(new KeyValuePair<string, string>(Path.GetFullPath(lib), null)); return; }
            if (Builtin().ContainsKey(inc.Key)) queue.Enqueue(new KeyValuePair<string, string>("builtin:" + inc.Key, null));
        }

        static Entry Load(string key)
        {
            try
            {
                if (key.StartsWith("builtin:"))
                {
                    Entry b;
                    if (cache.TryGetValue(key, out b)) return b;
                    string name = key.Substring(8), text = Builtin()[name];
                    b = Make(text, name, null);
                    cache[key] = b;
                    return b;
                }
                DateTime stamp = File.GetLastWriteTimeUtc(key);
                Entry e;
                if (cache.TryGetValue(key, out e) && e.Stamp == stamp) return e;
                e = Make(File.ReadAllText(key), Path.GetFileName(key), Path.GetDirectoryName(key));
                e.Stamp = stamp;
                cache[key] = e;
                return e;
            }
            catch (IOException) { return null; }
            catch (UnauthorizedAccessException) { return null; }
        }

        static Entry Make(string text, string source, string dir)
        {
            return new Entry
            {
                Symbols = Declarations.Parse(text, source),
                Includes = new List<KeyValuePair<string, bool>>(Declarations.Includes(Declarations.Clean(text))),
                Dir = dir
            };
        }

        /// <summary>Встроенные заголовки wcc: из compiler\headers.c — пары { "имя.h", "текст" "текст"… }.</summary>
        static Dictionary<string, string> Builtin()
        {
            if (builtin != null) return builtin;
            builtin = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            using (var s = Assembly.GetExecutingAssembly().GetManifestResourceStream("headers.c"))
            {
                if (s == null) return builtin;
                string src = new StreamReader(s, Encoding.UTF8).ReadToEnd();
                int at = src.IndexOf("g_headers[]", StringComparison.Ordinal);
                if (at < 0) return builtin;
                int i = src.IndexOf('{', at) + 1;
                while (i > 0 && i < src.Length)
                {
                    // между элементами — пробелы, запятые и «//»; не «{» — массив кончился
                    while (i < src.Length)
                    {
                        if (src[i] == '/' && i + 1 < src.Length && src[i + 1] == '/') { while (i < src.Length && src[i] != '\n') i++; continue; }
                        if (char.IsWhiteSpace(src[i]) || src[i] == ',') { i++; continue; }
                        break;
                    }
                    if (i >= src.Length || src[i] != '{') break;
                    i++;
                    var parts = new List<string>();
                    // строки в кавычках до закрывающей «}» элемента; «//» до конца строки пропускается
                    while (i < src.Length && src[i] != '}')
                    {
                        if (src[i] == '/' && i + 1 < src.Length && src[i + 1] == '/') { while (i < src.Length && src[i] != '\n') i++; continue; }
                        if (src[i] == '"')
                        {
                            var sb = new StringBuilder();
                            for (i++; i < src.Length && src[i] != '"'; i++)
                            {
                                if (src[i] != '\\') { sb.Append(src[i]); continue; }
                                char e = src[++i];
                                sb.Append(e == 'n' ? '\n' : e == 't' ? '\t' : e == '0' ? '\0' : e);
                            }
                            parts.Add(sb.ToString());
                            i++;
                            if (parts.Count == 1)
                            {
                                // после имени — запятая, дальше куски текста; их склеиваем подряд
                                parts.Add("");
                            }
                            else
                            {
                                parts[1] += parts[parts.Count - 1];
                                parts.RemoveAt(parts.Count - 1);
                            }
                            continue;
                        }
                        i++;
                    }
                    if (parts.Count >= 2 && parts[0].EndsWith(".h")) builtin[parts[0]] = parts[1];
                    i++;
                }
            }
            return builtin;
        }
    }
}
