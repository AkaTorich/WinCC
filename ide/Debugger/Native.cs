using System;
using System.Runtime.InteropServices;

namespace WinCC.Debugger
{
    /// <summary>Отладочный интерфейс Windows (kernel32): процесс под отладкой, его память, контекст потоков.</summary>
    static class Native
    {
        public const uint DEBUG_ONLY_THIS_PROCESS = 0x2, CREATE_NEW_CONSOLE = 0x10, CREATE_UNICODE_ENVIRONMENT = 0x400;
        public const uint DBG_CONTINUE = 0x00010002, DBG_EXCEPTION_NOT_HANDLED = 0x80010001;
        public const uint EXCEPTION_DEBUG_EVENT = 1, CREATE_THREAD_DEBUG_EVENT = 2, CREATE_PROCESS_DEBUG_EVENT = 3,
                          EXIT_THREAD_DEBUG_EVENT = 4, EXIT_PROCESS_DEBUG_EVENT = 5, LOAD_DLL_DEBUG_EVENT = 6,
                          OUTPUT_DEBUG_STRING_EVENT = 8;
        public const uint EXCEPTION_BREAKPOINT = 0x80000003, EXCEPTION_SINGLE_STEP = 0x80000004;
        public const uint CONTEXT_FULL = 0x10000B;          // AMD64: управление, целые, плавающие
        public const int ContextSize = 1232;

        // смещения в CONTEXT (x64)
        public const int CtxFlags = 0x30, CtxEFlags = 0x44, CtxRax = 0x78, CtxRsp = 0x98, CtxRbp = 0xA0, CtxRip = 0xF8, CtxXmm0 = 0x1A0;

        /// <summary>Смещение регистра общего назначения в CONTEXT: rax rcx rdx rbx rsp rbp rsi rdi r8..r15.</summary>
        public static int RegOffset(string name)
        {
            string[] order = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
            int i = Array.IndexOf(order, name);
            return i < 0 ? -1 : CtxRax + 8 * i;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct STARTUPINFO
        {
            public int cb;
            public string lpReserved, lpDesktop, lpTitle;
            public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
            public short wShowWindow, cbReserved2;
            public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PROCESS_INFORMATION
        {
            public IntPtr hProcess, hThread;
            public int dwProcessId, dwThreadId;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool CreateProcess(string app, string cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags,
                                                IntPtr env, string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);

        /// <summary>DEBUG_EVENT читается как байты (176 на x64): код @0, процесс @4, поток @8, объединение @16.</summary>
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool WaitForDebugEvent(IntPtr debugEvent, uint milliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool ContinueDebugEvent(int processId, int threadId, uint status);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, IntPtr size, out IntPtr read);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] buffer, IntPtr size, out IntPtr written);

        [DllImport("kernel32.dll")]
        public static extern bool FlushInstructionCache(IntPtr process, IntPtr address, IntPtr size);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetThreadContext(IntPtr thread, IntPtr context);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool SetThreadContext(IntPtr thread, IntPtr context);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool TerminateProcess(IntPtr process, uint code);

        [DllImport("kernel32.dll")]
        public static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll")]
        public static extern bool DebugSetProcessKillOnExit(bool kill);
    }
}
