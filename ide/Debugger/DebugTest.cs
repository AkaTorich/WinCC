using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace WinCC.Debugger
{
    /// <summary>
    /// Проверка отладчика без окна: WinCC.exe --debugtest отчёт.txt программа.exe файл.c:строка[,строка] команды —
    /// команды: c дальше, n шаг с обходом, s шаг с заходом, o шаг наружу, x стоп. На каждой остановке в отчёт идут
    /// функция, строка, стек и переменные (раскрытые на уровень).
    /// </summary>
    static class DebugTest
    {
        public static int Run(string[] args)
        {
            string report = args[1], exe = Path.GetFullPath(args[2]);
            var log = new StringBuilder();
            var bps = new List<Tuple<string, int>>();
            string script = "";
            foreach (string a in args.Skip(3))
            {
                int colon = a.LastIndexOf(':');
                if (colon > 1 && a.EndsWith(".c", StringComparison.OrdinalIgnoreCase) == false && a.Contains(".c:"))
                {
                    string file = Path.GetFullPath(a.Substring(0, colon));
                    foreach (string l in a.Substring(colon + 1).Split(',')) bps.Add(Tuple.Create(file, int.Parse(l)));
                }
                else script += a;
            }
            string home = Path.GetDirectoryName(Build.Compiler.WccPath);
            var info = DebugInfo.Load(Path.ChangeExtension(exe, ".wdbg"), Path.GetDirectoryName(exe), home);
            var engine = new DebugEngine(info);
            var reader = new ValueReader(engine);
            var stopped = new AutoResetEvent(false);
            var exited = new ManualResetEvent(false);
            StopInfo last = null;
            engine.Stopped += s => { last = s; stopped.Set(); };
            engine.Output += s => { lock (log) log.AppendLine("output: " + s); };
            engine.Exited += code => { lock (log) log.AppendLine("exit " + code); exited.Set(); };
            engine.SetBreakpoints(bps);
            engine.Start(exe, Path.GetDirectoryName(exe), true);
            int step = 0;
            while (true)
            {
                int which = WaitHandle.WaitAny(new WaitHandle[] { stopped, exited }, 20000);
                if (which != 0) { if (which == WaitHandle.WaitTimeout) log.AppendLine("timeout"); break; }
                Describe(engine, reader, last, log);
                char c = step < script.Length ? script[step++] : 'c';
                var cmd = c == 'n' ? DebugCommand.StepOver : c == 's' ? DebugCommand.StepInto : c == 'o' ? DebugCommand.StepOut
                        : c == 'x' ? DebugCommand.Stop : DebugCommand.Continue;
                lock (log) log.AppendLine("> " + cmd);
                engine.Command(cmd);
            }
            exited.WaitOne(5000);
            File.WriteAllText(report, log.ToString(), new UTF8Encoding(false));
            return 0;
        }

        static void Describe(DebugEngine engine, ValueReader reader, StopInfo s, StringBuilder log)
        {
            var frames = engine.Frames(s.ThreadId);
            lock (log)
            {
                log.AppendLine("stop" + (s.IsBreakpoint ? " [bp]" : "") + (s.Reason != null ? " " + s.Reason : ""));
                foreach (var f in frames)
                    log.AppendLine("  frame " + (f.Function != null ? f.Function.Name : "?") +
                                   (f.Line != null ? " " + Path.GetFileName(engine.Info.Files[f.Line.File]) + ":" + f.Line.Line : ""));
                var top = frames.FirstOrDefault(f => f.Function != null && f.Function.IsUser && f.HasVariables);
                if (top == null) return;
                int line = top.Line != null ? top.Line.Line : int.MaxValue;
                foreach (var v in top.Function.Variables.Where(v => v.IsParameter || v.Line <= line))
                {
                    var n = reader.Variable(v, top, "");
                    log.AppendLine("    " + n.Name + " = " + n.Value + "  (" + n.TypeName + ")");
                    if (n.Children != null)
                        foreach (var k in n.Children().Take(6))
                            log.AppendLine("      " + k.Name + " = " + k.Value);
                }
                foreach (var g in engine.Info.Globals)
                {
                    var n = reader.Variable(g, top, "");
                    log.AppendLine("    global " + n.Name + " = " + n.Value);
                }
            }
        }
    }
}
