using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace WinCC.Debugger
{
    public enum DebugCommand { Continue, StepInto, StepOver, StepOut, Stop }

    /// <summary>Почему программа остановилась.</summary>
    public class StopInfo
    {
        public int ThreadId;
        public ulong Rip;
        public string Reason;               // текст для строки вывода; null — обычная остановка (шаг)
        public bool IsBreakpoint, IsException;
    }

    /// <summary>Кадр стека вызовов: функция, строка, её rbp и значения сохраняемых регистров в ней.</summary>
    public class StackFrame
    {
        public ulong Rip, Rbp, Rsp;
        public DbgFunction Function;
        public DbgLine Line;
        public readonly Dictionary<string, ulong> Regs = new Dictionary<string, ulong>();
        public bool HasVariables { get { return Function != null && Function.HasFrame; } }
    }

    /// <summary>
    /// Отладчик одной программы. Свой поток ждёт событий отладки Windows (WaitForDebugEvent); остановившись,
    /// говорит об этом (Stopped) и ждёт команды окна: дальше, шаг с заходом, шаг с обходом, шаг наружу, стоп.
    /// Точки останова — int3 (0xCC) в коде; шаги — флаг TF и временные точки на адресе возврата.
    /// </summary>
    public sealed class DebugEngine
    {
        public event Action<StopInfo> Stopped;
        public event Action<string> Output;
        public event Action<int> Exited;

        public DebugInfo Info { get; private set; }
        public ulong ImageBase { get; private set; }
        public bool IsStopped { get; private set; }

        readonly object sync = new object();
        readonly BlockingCollection<DebugCommand> commands = new BlockingCollection<DebugCommand>();
        readonly Dictionary<ulong, byte> patched = new Dictionary<ulong, byte>();   // адрес — исходный байт
        readonly HashSet<ulong> userBps = new HashSet<ulong>();
        readonly Dictionary<ulong, ulong> tempBps = new Dictionary<ulong, ulong>(); // адрес — каким должен быть rsp
        readonly Dictionary<int, IntPtr> threads = new Dictionary<int, IntPtr>();
        IntPtr process;
        int processId;
        Thread loop;
        volatile bool running;

        // шаг: вид, поток, откуда начат (строка и рамка)
        DebugCommand stepKind = DebugCommand.Continue;
        int stepTid;
        ulong stepStartRip, stepFrameBase, prevRsp, prevRip;
        DbgLine stepLine;
        ulong reinsertAt;                   // точка, снятая на один шаг, чтобы выполнить её команду
        int reinsertTid;
        bool loaderBreakSeen, fatal;
        List<Tuple<string, int>> pendingBps = new List<Tuple<string, int>>();

        public DebugEngine(DebugInfo info) { Info = info; }

        // ---- запуск и команды ----

        public void Start(string exe, string folder, bool console)
        {
            loop = new Thread(() => Run(exe, folder, console)) { IsBackground = true, Name = "WinCC debugger" };
            running = true;
            loop.Start();
        }

        public void Command(DebugCommand c)
        {
            if (c == DebugCommand.Stop && !IsStopped)
            {
                lock (sync) if (process != IntPtr.Zero) Native.TerminateProcess(process, 1);
                return;
            }
            if (IsStopped) commands.Add(c);
        }

        /// <summary>Точки останова окна: файл и строка (с единицы). Можно и до запуска, и на ходу.</summary>
        public void SetBreakpoints(IEnumerable<Tuple<string, int>> list)
        {
            lock (sync)
            {
                pendingBps = new List<Tuple<string, int>>(list);
                if (process == IntPtr.Zero || ImageBase == 0) return;
                var want = new HashSet<ulong>();
                foreach (var bp in pendingBps)
                {
                    int actual;
                    foreach (uint rva in Info.AddressesForLine(bp.Item1, bp.Item2, out actual)) want.Add(ImageBase + rva);
                }
                foreach (ulong a in new List<ulong>(userBps))
                    if (!want.Contains(a)) { userBps.Remove(a); if (!tempBps.ContainsKey(a)) Unpatch(a); }
                foreach (ulong a in want)
                    if (userBps.Add(a)) Patch(a);
            }
        }

        // ---- память ----

        public byte[] Read(ulong address, int size)
        {
            if (size <= 0 || process == IntPtr.Zero) return null;
            var buf = new byte[size];
            IntPtr n;
            if (!Native.ReadProcessMemory(process, (IntPtr)(long)address, buf, (IntPtr)size, out n) || (long)n != size) return null;
            lock (sync)     // в прочитанном — исходные байты, а не int3 точек
                foreach (var p in patched)
                    if (p.Key >= address && p.Key < address + (ulong)size) buf[p.Key - address] = p.Value;
            return buf;
        }

        public bool ReadU64(ulong address, out ulong value)
        {
            var b = Read(address, 8);
            value = b != null ? BitConverter.ToUInt64(b, 0) : 0;
            return b != null;
        }

        void Write(ulong address, byte value)
        {
            IntPtr n;
            Native.WriteProcessMemory(process, (IntPtr)(long)address, new[] { value }, (IntPtr)1, out n);
            Native.FlushInstructionCache(process, (IntPtr)(long)address, (IntPtr)1);
        }

        void Patch(ulong a)
        {
            if (patched.ContainsKey(a)) return;
            var b = new byte[1];
            IntPtr n;
            if (!Native.ReadProcessMemory(process, (IntPtr)(long)a, b, (IntPtr)1, out n)) return;
            patched[a] = b[0];
            Write(a, 0xCC);
        }

        void Unpatch(ulong a)
        {
            byte orig;
            if (!patched.TryGetValue(a, out orig)) return;
            patched.Remove(a);
            Write(a, orig);
        }

        // ---- контекст потока (выровненный CONTEXT) ----

        sealed class Context
        {
            readonly IntPtr raw, ptr;
            public Context()
            {
                raw = Marshal.AllocHGlobal(Native.ContextSize + 16);
                ptr = (IntPtr)(((long)raw + 15) & ~15L);
            }
            public bool Load(IntPtr thread)
            {
                Marshal.WriteInt32(ptr, Native.CtxFlags, (int)Native.CONTEXT_FULL);
                return Native.GetThreadContext(thread, ptr);
            }
            public bool Save(IntPtr thread) { return Native.SetThreadContext(thread, ptr); }
            public ulong this[int offset]
            {
                get { return (ulong)Marshal.ReadInt64(ptr, offset); }
                set { Marshal.WriteInt64(ptr, offset, (long)value); }
            }
            public uint EFlags
            {
                get { return (uint)Marshal.ReadInt32(ptr, Native.CtxEFlags); }
                set { Marshal.WriteInt32(ptr, Native.CtxEFlags, (int)value); }
            }
            public double Xmm0 { get { return BitConverter.Int64BitsToDouble(Marshal.ReadInt64(ptr, Native.CtxXmm0)); } }
        }

        readonly Context ctx = new Context();       // поток отладки
        readonly Context uiCtx = new Context();     // окно (кадры стека), пока программа стоит

        // ---- петля событий ----

        void Run(string exe, string folder, bool console)
        {
            var si = new Native.STARTUPINFO { cb = Marshal.SizeOf(typeof(Native.STARTUPINFO)) };
            Native.PROCESS_INFORMATION pi;
            uint flags = Native.DEBUG_ONLY_THIS_PROCESS | Native.CREATE_UNICODE_ENVIRONMENT | (console ? Native.CREATE_NEW_CONSOLE : 0);
            if (!Native.CreateProcess(exe, "\"" + exe + "\"", IntPtr.Zero, IntPtr.Zero, false, flags, IntPtr.Zero, folder, ref si, out pi))
            {
                Output?.Invoke(Loc.F("DbgCannotStart", new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()).Message));
                Exited?.Invoke(-1);
                return;
            }
            Native.CloseHandle(pi.hThread);
            processId = pi.dwProcessId;
            IntPtr ev = Marshal.AllocHGlobal(256);
            int exitCode = 0;
            try
            {
                while (running)
                {
                    if (!Native.WaitForDebugEvent(ev, 0xFFFFFFFF)) break;
                    uint code = (uint)Marshal.ReadInt32(ev, 0);
                    int tid = Marshal.ReadInt32(ev, 8);
                    uint status = Native.DBG_CONTINUE;
                    IntPtr u = ev + 16;
                    switch (code)
                    {
                        case Native.CREATE_PROCESS_DEBUG_EVENT:
                            {
                                IntPtr file = Marshal.ReadIntPtr(u, 0);
                                if (file != IntPtr.Zero) Native.CloseHandle(file);
                                lock (sync)
                                {
                                    process = Marshal.ReadIntPtr(u, 8);
                                    threads[tid] = Marshal.ReadIntPtr(u, 16);
                                    ImageBase = (ulong)(long)Marshal.ReadIntPtr(u, 24);
                                }
                                SetBreakpoints(pendingBps);
                                break;
                            }
                        case Native.CREATE_THREAD_DEBUG_EVENT:
                            lock (sync) threads[tid] = Marshal.ReadIntPtr(u, 0);
                            break;
                        case Native.EXIT_THREAD_DEBUG_EVENT:
                            lock (sync) threads.Remove(tid);
                            break;
                        case Native.LOAD_DLL_DEBUG_EVENT:
                            {
                                IntPtr file = Marshal.ReadIntPtr(u, 0);
                                if (file != IntPtr.Zero) Native.CloseHandle(file);
                                break;
                            }
                        case Native.OUTPUT_DEBUG_STRING_EVENT:
                            DebugString(u);
                            break;
                        case Native.EXCEPTION_DEBUG_EVENT:
                            status = OnException(u, tid);
                            break;
                        case Native.EXIT_PROCESS_DEBUG_EVENT:
                            exitCode = Marshal.ReadInt32(u, 0);
                            running = false;
                            break;
                    }
                    Native.ContinueDebugEvent(processId, tid, status);
                }
            }
            finally
            {
                Marshal.FreeHGlobal(ev);
                lock (sync)
                {
                    process = IntPtr.Zero;
                    patched.Clear();
                }
                Native.CloseHandle(pi.hProcess);
                Exited?.Invoke(exitCode);
            }
        }

        void DebugString(IntPtr u)
        {
            ulong at = (ulong)(long)Marshal.ReadIntPtr(u, 0);
            bool unicode = Marshal.ReadInt16(u, 8) != 0;
            int len = (ushort)Marshal.ReadInt16(u, 10);
            var b = Read(at, unicode ? len * 2 : len);
            if (b == null) return;
            string s = unicode ? Encoding.Unicode.GetString(b) : Encoding.UTF8.GetString(b);
            Output?.Invoke(s.TrimEnd('\0', '\r', '\n'));
        }

        IntPtr Thread(int tid)
        {
            lock (sync) { IntPtr h; return threads.TryGetValue(tid, out h) ? h : IntPtr.Zero; }
        }

        uint OnException(IntPtr u, int tid)
        {
            uint code = (uint)Marshal.ReadInt32(u, 0);
            ulong addr = (ulong)(long)Marshal.ReadIntPtr(u, 16);
            bool firstChance = Marshal.ReadInt32(u, 152) != 0;
            IntPtr th = Thread(tid);

            if (code == Native.EXCEPTION_BREAKPOINT)
            {
                bool ours;
                lock (sync) ours = patched.ContainsKey(addr);
                if (!ours)
                {
                    if (!loaderBreakSeen) { loaderBreakSeen = true; return Native.DBG_CONTINUE; }     // загрузчик Windows
                    // __debugbreak() самой программы: остановиться после него
                    Stop(tid, new StopInfo { ThreadId = tid, Rip = addr + 1, Reason = Loc.T("DbgProgramBreak"), IsBreakpoint = true });
                    return Native.DBG_CONTINUE;
                }
                ctx.Load(th);
                ctx[Native.CtxRip] = addr;                  // int3 уже выполнена: назад на неё
                ctx.Save(th);
                ulong rsp = ctx[Native.CtxRsp];
                bool user, temp;
                ulong expect;
                lock (sync)
                {
                    user = userBps.Contains(addr);
                    temp = tempBps.TryGetValue(addr, out expect);
                }
                if (temp && tid == stepTid && rsp == expect)
                {
                    lock (sync) { tempBps.Remove(addr); if (!userBps.Contains(addr)) Unpatch(addr); }
                    if (stepKind == DebugCommand.StepOut)
                    {
                        if (IsUserCode(addr)) { Stop(tid, new StopInfo { ThreadId = tid, Rip = addr }); return Native.DBG_CONTINUE; }
                        EndStep();
                        ResumeOver(tid, addr, false);
                        return Native.DBG_CONTINUE;
                    }
                    // шаг продолжается из точки возврата
                    if (CheckStepStop(tid, addr, rsp)) return Native.DBG_CONTINUE;
                    prevRsp = rsp;
                    prevRip = addr;
                    ResumeOver(tid, addr, true);
                    return Native.DBG_CONTINUE;
                }
                if (user)
                {
                    EndStep();
                    Stop(tid, new StopInfo { ThreadId = tid, Rip = addr, IsBreakpoint = true });
                    return Native.DBG_CONTINUE;
                }
                // чужая временная точка (рекурсия глубже, другой поток): выполнить команду под ней и дальше
                ResumeOver(tid, addr, false);
                return Native.DBG_CONTINUE;
            }

            if (code == Native.EXCEPTION_SINGLE_STEP)
            {
                ctx.Load(th);
                ulong rip = ctx[Native.CtxRip], rsp = ctx[Native.CtxRsp];
                if (reinsertAt != 0 && reinsertTid == tid)
                {
                    lock (sync)
                        if (patched.ContainsKey(reinsertAt))
                        {
                            if (userBps.Contains(reinsertAt) || tempBps.ContainsKey(reinsertAt)) Write(reinsertAt, 0xCC);
                            else patched.Remove(reinsertAt);    // её убрали, пока она была снята
                        }
                    reinsertAt = 0;
                }
                if (tid != stepTid || stepKind == DebugCommand.Continue || stepKind == DebugCommand.StepOut)
                    return Native.DBG_CONTINUE;     // шаг был только ради снятой точки
                StepEvent(tid, rip, rsp);
                return Native.DBG_CONTINUE;
            }

            if (firstChance)
                return Native.DBG_EXCEPTION_NOT_HANDLED;    // программа обработает сама (__try) — или нет: второй раз
            // необработанное исключение: остановиться на месте; дальше программа кончится
            ctx.Load(th);
            EndStep();
            fatal = true;
            Stop(tid, new StopInfo
            {
                ThreadId = tid,
                Rip = ctx[Native.CtxRip],
                Reason = ExceptionText(code, u),
                IsException = true
            });
            return Native.DBG_EXCEPTION_NOT_HANDLED;
        }

        string ExceptionText(uint code, IntPtr u)
        {
            string name;
            switch (code)
            {
                case 0xC0000005:
                    {
                        long kind = Marshal.ReadInt64(u, 32), at = Marshal.ReadInt64(u, 40);
                        return Loc.F(kind == 1 ? "DbgAccessWrite" : kind == 8 ? "DbgAccessExecute" : "DbgAccessRead", at.ToString("X"));
                    }
                case 0xC0000094: name = Loc.T("DbgDivZero"); break;
                case 0xC00000FD: name = Loc.T("DbgStackOverflow"); break;
                case 0xC000001D: name = Loc.T("DbgIllegal"); break;
                case 0xC000008C: name = Loc.T("DbgArrayBounds"); break;
                default: name = ""; break;
            }
            return Loc.F("DbgException", code.ToString("X8"), name);
        }

        // ---- шаги ----

        bool IsUserCode(ulong rip)
        {
            if (rip < ImageBase) return false;
            var f = Info.FunctionAt((uint)(rip - ImageBase));
            return f != null && f.IsUser;
        }

        ulong FrameBase(DbgFunction f, ulong rbp)
        {
            return rbp + 8UL * (ulong)(f.SavePad + f.SaveCount) + 16;
        }

        void EndStep()
        {
            stepKind = DebugCommand.Continue;
            lock (sync)
            {
                foreach (ulong a in new List<ulong>(tempBps.Keys))
                    if (!userBps.Contains(a)) Unpatch(a);
                tempBps.Clear();
            }
        }

        /// <summary>Пора ли остановить шаг на адресе rip: начало строки программы, другая строка или другой кадр.</summary>
        bool CheckStepStop(int tid, ulong rip, ulong rsp)
        {
            if (rip < ImageBase) return false;
            uint rva = (uint)(rip - ImageBase);
            DbgLine line;
            if (!Info.IsLineStart(rva, out line)) return false;
            var f = Info.FunctionAt(rva);
            ctx.Load(Thread(tid));
            ulong fb = FrameBase(f, ctx[Native.CtxRbp]);
            if (stepKind == DebugCommand.StepOver && fb < stepFrameBase) return false;       // глубже — не наше
            bool sameLine = stepLine != null && line.Line == stepLine.Line && line.File == stepLine.File && fb == stepFrameBase;
            if (sameLine && rip != stepStartRip) return false;
            Stop(tid, new StopInfo { ThreadId = tid, Rip = rip });
            return true;
        }

        void StepEvent(int tid, ulong rip, ulong rsp)
        {
            if (CheckStepStop(tid, rip, rsp)) return;
            bool user = IsUserCode(rip);
            // вызов: rsp ниже на 8 и на стеке — адрес сразу за прошлой командой (а не push: там значение)
            ulong top;
            bool called = rsp == prevRsp - 8 && ReadU64(rsp, out top) && top > prevRip && top <= prevRip + 15;
            if (!user)
            {
                if (called)
                {
                    // вызов кода без строк (библиотека, Windows): до возврата — без шагов
                    ulong ret;
                    if (ReadU64(rsp, out ret)) { SetTemp(ret, rsp + 8); return; }
                }
                else if (rsp > prevRsp)
                {
                    EndStep();                      // вернулись туда, где строк нет (main кончилась): просто дальше
                    return;
                }
            }
            else if (called && stepKind == DebugCommand.StepOver)
            {
                ulong ret;
                if (ReadU64(rsp, out ret)) { SetTemp(ret, rsp + 8); return; }
            }
            prevRsp = rsp;
            prevRip = rip;
            SetTrap(tid, true);
        }

        void SetTemp(ulong address, ulong expectRsp)
        {
            lock (sync)
            {
                tempBps[address] = expectRsp;
                Patch(address);
            }
            prevRsp = expectRsp;
        }

        void SetTrap(int tid, bool on)
        {
            IntPtr th = Thread(tid);
            ctx.Load(th);
            ctx.EFlags = on ? ctx.EFlags | 0x100 : ctx.EFlags & ~0x100u;
            ctx.Save(th);
        }

        /// <summary>Продолжить поток, стоящий на снятой (или временной) точке: один шаг без неё, потом она снова на месте.</summary>
        void ResumeOver(int tid, ulong rip, bool stepping)
        {
            bool atPatch;
            lock (sync) atPatch = patched.ContainsKey(rip);
            if (atPatch)
            {
                lock (sync)
                {
                    byte orig = patched[rip];
                    Write(rip, orig);
                    patched[rip] = orig;            // запись остаётся: Read отдаёт исходный байт, вернётся после шага
                }
                reinsertAt = rip;
                reinsertTid = tid;
                SetTrap(tid, true);
            }
            else if (stepping) SetTrap(tid, true);
        }

        /// <summary>Остановка: окно узнаёт о ней и даёт команду; поток отладки ждёт её здесь.</summary>
        void Stop(int tid, StopInfo info)
        {
            IsStopped = true;
            Stopped?.Invoke(info);
            DebugCommand c = commands.Take();
            IsStopped = false;
            if (c == DebugCommand.Stop || fatal)
            {
                if (c == DebugCommand.Stop) lock (sync) Native.TerminateProcess(process, 1);
                return;
            }
            IntPtr th = Thread(tid);
            ctx.Load(th);
            ulong rip = ctx[Native.CtxRip], rsp = ctx[Native.CtxRsp], rbp = ctx[Native.CtxRbp];
            stepTid = tid;
            stepKind = c;
            stepStartRip = rip;
            prevRsp = rsp;
            prevRip = rip;
            var f = rip >= ImageBase ? Info.FunctionAt((uint)(rip - ImageBase)) : null;
            stepLine = rip >= ImageBase ? Info.LineAt((uint)(rip - ImageBase)) : null;
            stepFrameBase = f != null && f.HasFrame ? FrameBase(f, rbp) : rsp;
            if (c == DebugCommand.Continue)
            {
                ResumeOver(tid, rip, false);
                return;
            }
            if (c == DebugCommand.StepOut)
            {
                var frames = Frames(tid, ctx, 2);
                if (frames.Count > 1 && frames[1].Function != null)
                    SetTemp(frames[1].Rip, frames[1].Rsp);
                else stepKind = DebugCommand.Continue;      // наружу некуда: просто дальше
                ResumeOver(tid, rip, false);
                return;
            }
            // шаг с заходом / с обходом: по одной команде, пока не начнётся другая строка
            bool atPatch;
            lock (sync) atPatch = patched.ContainsKey(rip);
            if (atPatch) ResumeOver(tid, rip, true);
            else SetTrap(tid, true);
        }

        // ---- стек вызовов ----

        /// <summary>Кадры стека потока, пока программа стоит (вызывает окно).</summary>
        public List<StackFrame> Frames(int tid)
        {
            lock (uiCtx)
            {
                IntPtr th = Thread(tid);
                if (th == IntPtr.Zero || !uiCtx.Load(th)) return new List<StackFrame>();
                return Frames(tid, uiCtx, 128);
            }
        }

        public double ReturnXmm0(int tid)
        {
            lock (uiCtx) { uiCtx.Load(Thread(tid)); return uiCtx.Xmm0; }
        }

        List<StackFrame> Frames(int tid, Context c, int max)
        {
            var list = new List<StackFrame>();
            var regs = new Dictionary<string, ulong>();
            foreach (string r in new[] { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" })
                regs[r] = c[Native.RegOffset(r)];
            ulong rip = c[Native.CtxRip];
            for (int depth = 0; depth < max; depth++)
            {
                ulong rbp = regs["rbp"], rsp = regs["rsp"];
                DbgFunction f = rip >= ImageBase && rip - ImageBase < 0xFFFFFFFF ? Info.FunctionAt((uint)(rip - ImageBase)) : null;
                var fr = new StackFrame { Rip = rip, Rbp = rbp, Rsp = rsp, Function = f };
                if (f != null) fr.Line = Info.LineAt((uint)(rip - ImageBase - (depth > 0 ? 1UL : 0UL)));
                foreach (var kv in regs) fr.Regs[kv.Key] = kv.Value;
                list.Add(fr);
                if (f != null && f.HasFrame)
                {
                    ulong top = rbp + 8UL * (ulong)(f.SavePad + f.SaveCount);
                    for (int i = 0; i < f.SaveCount; i++)
                    {
                        ulong v;
                        if (ReadU64(rbp + 8UL * (ulong)(f.SavePad + f.SaveCount - 1 - i), out v)) regs[f.SavedRegs[i]] = v;
                    }
                    ulong nextRbp, ret;
                    if (!ReadU64(top, out nextRbp) || !ReadU64(top + 8, out ret)) break;
                    regs["rbp"] = nextRbp;
                    regs["rsp"] = top + 16;
                    rip = ret;
                }
                else if (depth == 0)
                {
                    // остановились в коде без рамки (библиотека, Windows): ближайший адрес возврата в программу на стеке
                    bool found = false;
                    for (int k = 0; k < 4096 && !found; k++)
                    {
                        ulong v;
                        if (!ReadU64(rsp + 8UL * (ulong)k, out v)) break;
                        if (!IsUserCode(v) || !AfterCall(v)) continue;
                        ulong slot = rsp + 8UL * (ulong)k;
                        ulong saved;
                        if (rbp > rsp && rbp < slot && ReadU64(rbp + 8, out saved) && saved == v && ReadU64(rbp, out saved))
                            regs["rbp"] = saved;    // функция библиотеки с обычной рамкой
                        regs["rsp"] = slot + 8;
                        rip = v;
                        found = true;
                    }
                    if (!found) break;
                }
                else break;
                if (f != null && f.IsUser && (f.Name == "main" || f.Name == "wmain" || f.Name == "WinMain" || f.Name == "wWinMain")) break;
            }
            return list;
        }

        /// <summary>Перед адресом — команда вызова (call rel32, call [rip+x], call reg): это адрес возврата.</summary>
        bool AfterCall(ulong ret)
        {
            var b = Read(ret - 6, 6);
            if (b == null) return false;
            return b[1] == 0xE8 || (b[0] == 0xFF && (b[1] & 0x38) == 0x10) || (b[4] == 0xFF && (b[5] & 0xF8) == 0xD0)
                   || (b[3] == 0x41 && b[4] == 0xFF && (b[5] & 0xF8) == 0xD0);
        }
    }
}
