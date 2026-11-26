using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace WinCC.Debugger
{
    /// <summary>Переменная в окне «Переменные»: имя, значение словами, тип; поля и элементы — по раскрытию.</summary>
    public class VarNode
    {
        public string Name, Value, TypeName, Path;
        public Func<List<VarNode>> Children;            // null — раскрывать нечего
        public bool Changed;
    }

    /// <summary>Читает значения переменных из памяти остановленной программы и пишет их по-сишному.</summary>
    public class ValueReader
    {
        readonly DebugEngine engine;
        readonly DebugInfo info;
        const int MaxElements = 1000;

        public ValueReader(DebugEngine engine)
        {
            this.engine = engine;
            info = engine.Info;
        }

        /// <summary>Переменная кадра: в памяти (адрес) или в регистре (его значение).</summary>
        public VarNode Variable(DbgVariable v, StackFrame frame, string pathPrefix)
        {
            string loc = v.Location;
            if (loc.StartsWith("r"))
            {
                ulong reg;
                frame.Regs.TryGetValue(loc.Substring(1), out reg);
                return FromBytes(v.Name, v.Type, BitConverter.GetBytes(reg), pathPrefix + v.Name);
            }
            if (loc.StartsWith("g"))
                return At(v.Name, v.Type, engine.ImageBase + v.Rva, pathPrefix + v.Name);
            long off = long.Parse(loc.Substring(1), CultureInfo.InvariantCulture);
            ulong addr = (ulong)((long)frame.Rbp + off);
            if (loc[0] == 'S')
            {
                ulong p;
                if (!engine.ReadU64(addr, out p)) return Unreadable(v.Name, v.Type, pathPrefix + v.Name);
                addr = p;
            }
            return At(v.Name, v.Type, addr, pathPrefix + v.Name);
        }

        VarNode Unreadable(string name, DbgType t, string path)
        {
            return new VarNode { Name = name, Value = Loc.T("DbgUnreadable"), TypeName = t.Name(info), Path = path };
        }

        /// <summary>Значение типа t по адресу addr.</summary>
        public VarNode At(string name, DbgType t, ulong addr, string path)
        {
            var node = new VarNode { Name = name, TypeName = t.Name(info), Path = path };
            switch (t.Kind)
            {
                case DbgTypeKind.Array:
                    {
                        int esz = t.Element.SizeIn(info);
                        if (t.Element.Kind == DbgTypeKind.Char || t.Element.Kind == DbgTypeKind.UChar)
                        {
                            var b = engine.Read(addr, Math.Min(t.Count, 512));
                            node.Value = b == null ? Loc.T("DbgUnreadable") : Quote(b);
                        }
                        else node.Value = Preview(t, addr);
                        int n = Math.Min(t.Count, MaxElements);
                        node.Children = () =>
                        {
                            var list = new List<VarNode>();
                            for (int i = 0; i < n; i++)
                                list.Add(At("[" + i + "]", t.Element, addr + (ulong)(i * esz), path + "[" + i + "]"));
                            return list;
                        };
                        return node;
                    }
                case DbgTypeKind.Struct:
                    {
                        DbgStruct st;
                        if (!info.Structs.TryGetValue(t.StructId, out st)) { node.Value = "{…}"; return node; }
                        node.Value = Preview(t, addr);
                        node.Children = () =>
                        {
                            var list = new List<VarNode>();
                            foreach (var f in st.Fields)
                            {
                                string fname = f.Name.Length > 0 ? f.Name : "(" + Loc.T("DbgAnonymous") + ")";
                                if (f.BitWidth > 0) list.Add(Bitfield(fname, f, addr, path + "." + fname));
                                else list.Add(At(fname, f.Type, addr + (ulong)f.Offset, path + "." + fname));
                            }
                            return list;
                        };
                        return node;
                    }
                default:
                    {
                        int size = Math.Max(1, t.Size);
                        var b = engine.Read(addr, size);
                        if (b == null) { node.Value = Loc.T("DbgUnreadable"); return node; }
                        return FromBytes(name, t, b, path);
                    }
            }
        }

        VarNode Bitfield(string name, DbgField f, ulong addr, string path)
        {
            int size = Math.Max(1, f.Type.Size);
            var b = engine.Read(addr + (ulong)f.Offset, size);
            var node = new VarNode { Name = name, TypeName = f.Type.Name(info) + " : " + f.BitWidth, Path = path };
            if (b == null) { node.Value = Loc.T("DbgUnreadable"); return node; }
            ulong raw = 0;
            for (int i = 0; i < size; i++) raw |= (ulong)b[i] << (8 * i);
            ulong v = (raw >> f.BitOffset) & (f.BitWidth >= 64 ? ulong.MaxValue : (1UL << f.BitWidth) - 1);
            bool signed = f.Type.Kind == DbgTypeKind.Int || f.Type.Kind == DbgTypeKind.Char || f.Type.Kind == DbgTypeKind.Enum;
            if (signed && f.BitWidth < 64 && (v >> (f.BitWidth - 1)) != 0) node.Value = ((long)(v | ~((1UL << f.BitWidth) - 1))).ToString(CultureInfo.InvariantCulture);
            else node.Value = v.ToString(CultureInfo.InvariantCulture);
            return node;
        }

        /// <summary>Значение из готовых байтов (регистр или прочитанная память): числа, символы, указатели.</summary>
        VarNode FromBytes(string name, DbgType t, byte[] b, string path)
        {
            var node = new VarNode { Name = name, TypeName = t.Name(info), Path = path };
            switch (t.Kind)
            {
                case DbgTypeKind.Bool: node.Value = b[0] != 0 ? "true" : "false"; break;
                case DbgTypeKind.Char:
                case DbgTypeKind.UChar:
                    {
                        int v = t.Kind == DbgTypeKind.Char ? (int)(sbyte)b[0] : (int)b[0];
                        node.Value = v.ToString(CultureInfo.InvariantCulture) + " " + CharText(b[0]);
                        break;
                    }
                case DbgTypeKind.Int:
                case DbgTypeKind.Enum:
                    node.Value = (t.Size == 1 ? (long)(sbyte)b[0] : t.Size == 2 ? (long)BitConverter.ToInt16(b, 0)
                                  : t.Size == 8 ? BitConverter.ToInt64(b, 0) : (long)BitConverter.ToInt32(b, 0)).ToString(CultureInfo.InvariantCulture);
                    break;
                case DbgTypeKind.UInt:
                    node.Value = (t.Size == 1 ? (ulong)b[0] : t.Size == 2 ? (ulong)BitConverter.ToUInt16(b, 0)
                                  : t.Size == 8 ? BitConverter.ToUInt64(b, 0) : (ulong)BitConverter.ToUInt32(b, 0)).ToString(CultureInfo.InvariantCulture);
                    break;
                case DbgTypeKind.Float:
                    node.Value = t.Size == 4 ? BitConverter.ToSingle(b, 0).ToString("G7", CultureInfo.InvariantCulture)
                                             : BitConverter.ToDouble(b, 0).ToString("G15", CultureInfo.InvariantCulture);
                    break;
                case DbgTypeKind.Pointer:
                    {
                        ulong p = BitConverter.ToUInt64(b, 0);
                        node.Value = Pointer(t, p);
                        var el = t.Element;
                        if (p != 0 && el != null && el.Kind != DbgTypeKind.Void && el.Kind != DbgTypeKind.Function && el.Kind != DbgTypeKind.Unknown)
                        {
                            if (el.Kind == DbgTypeKind.Struct || el.Kind == DbgTypeKind.Array)
                            {
                                // указатель на структуру раскрывается сразу в её поля; читается только по раскрытию —
                                // иначе список из узлов (next → next → …) читался бы без конца
                                node.Children = () =>
                                {
                                    var target = At("*" + name, el, p, "*" + path);
                                    return target.Children != null ? target.Children() : new List<VarNode> { target };
                                };
                            }
                            else
                            {
                                // указатель на числа — как массив: [0], [1]… по PointerPage штук, дальше — узел следующей страницы
                                if (el.Kind != DbgTypeKind.Char && el.Kind != DbgTypeKind.UChar && el.Kind != DbgTypeKind.Pointer)
                                    node.Value += " → " + At("", el, p, "").Value;
                                node.Children = () => PointerElements(el, p, path, 0);
                            }
                        }
                        break;
                    }
                case DbgTypeKind.Function:
                    node.Value = Pointer(t, BitConverter.ToUInt64(b, 0));
                    break;
                default:
                    node.Value = "?";
                    break;
            }
            return node;
        }

        const int PointerPage = 10;

        /// <summary>Элементы, на которые смотрит указатель: [from]…[from+9]; длины указатель не знает, поэтому дальше — по раскрытию.</summary>
        List<VarNode> PointerElements(DbgType el, ulong p, string path, int from)
        {
            var list = new List<VarNode>();
            int esz = Math.Max(1, el.SizeIn(info));
            for (int i = from; i < from + PointerPage; i++)
            {
                ulong a = p + (ulong)((long)i * esz);
                if (engine.Read(a, esz) == null) return list;       // память кончилась — дальше не читать
                list.Add(At("[" + i + "]", el, a, path + "[" + i + "]"));
            }
            int next = from + PointerPage;
            if (next < MaxElements)
                list.Add(new VarNode
                {
                    Name = "[" + next + "…" + (next + PointerPage - 1) + "]",
                    Value = "",
                    TypeName = "",
                    Path = path + "[" + next + "…]",
                    Children = () => PointerElements(el, p, path, next)
                });
            return list;
        }

        string Pointer(DbgType t, ulong p)
        {
            if (p == 0) return "NULL";
            string s = "0x" + p.ToString("X", CultureInfo.InvariantCulture);
            var el = t.Element;
            if (el != null && (el.Kind == DbgTypeKind.Char || el.Kind == DbgTypeKind.UChar))
            {
                var b = ReadString(p, 256);
                if (b != null) s += " " + Quote(b);
            }
            else if ((el != null && el.Kind == DbgTypeKind.Function) || t.Kind == DbgTypeKind.Function)
            {
                var f = p >= engine.ImageBase ? info.FunctionAt((uint)(p - engine.ImageBase)) : null;
                if (f != null) s += " " + f.Name;
            }
            return s;
        }

        byte[] ReadString(ulong p, int max)
        {
            var list = new List<byte>();
            for (int chunk = 0; chunk < max; chunk += 64)
            {
                var b = engine.Read(p + (ulong)chunk, 64);
                if (b == null) return list.Count > 0 ? list.ToArray() : null;
                foreach (byte x in b)
                {
                    if (x == 0) return list.ToArray();
                    list.Add(x);
                }
            }
            return list.ToArray();
        }

        static string Quote(byte[] b)
        {
            int n = Array.IndexOf(b, (byte)0);
            if (n < 0) n = b.Length;
            string s = Encoding.UTF8.GetString(b, 0, n);
            var sb = new StringBuilder("\"");
            foreach (char c in s)
            {
                if (c == '\n') sb.Append("\\n");
                else if (c == '\r') sb.Append("\\r");
                else if (c == '\t') sb.Append("\\t");
                else if (c == '"') sb.Append("\\\"");
                else if (c < ' ') sb.Append("\\x").Append(((int)c).ToString("X2"));
                else sb.Append(c);
            }
            return sb.Append('"').ToString();
        }

        static string CharText(byte c)
        {
            switch (c)
            {
                case 0: return "'\\0'";
                case (byte)'\n': return "'\\n'";
                case (byte)'\r': return "'\\r'";
                case (byte)'\t': return "'\\t'";
            }
            return c >= 32 && c < 127 ? "'" + (char)c + "'" : "";
        }

        /// <summary>Строка-образец составного значения: {1, 2, 3} или {x=1, y=2}.</summary>
        string Preview(DbgType t, ulong addr)
        {
            var parts = new List<string>();
            if (t.Kind == DbgTypeKind.Array)
            {
                int esz = t.Element.SizeIn(info);
                for (int i = 0; i < t.Count && i < 8; i++)
                    parts.Add(Short(t.Element, addr + (ulong)(i * esz)));
                if (t.Count > 8) parts.Add("…");
                return "{" + string.Join(", ", parts) + "}";
            }
            DbgStruct st;
            if (!info.Structs.TryGetValue(t.StructId, out st)) return "{…}";
            foreach (var f in st.Fields)
            {
                if (parts.Count >= 5) { parts.Add("…"); break; }
                if (f.BitWidth > 0) { parts.Add(f.Name + "=" + Bitfield(f.Name, f, addr, "").Value); continue; }
                parts.Add((f.Name.Length > 0 ? f.Name + "=" : "") + Short(f.Type, addr + (ulong)f.Offset));
            }
            return "{" + string.Join(", ", parts) + "}";
        }

        string Short(DbgType t, ulong addr)
        {
            if (t.Kind == DbgTypeKind.Struct || t.Kind == DbgTypeKind.Array) return "{…}";
            var n = At("", t, addr, "");
            string v = n.Value ?? "";
            return v.Length > 40 ? v.Substring(0, 40) + "…" : v;
        }
    }
}
