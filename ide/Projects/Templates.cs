using System.Text;
using WinCC.Build;

namespace WinCC.Projects
{
    /// <summary>
    /// Заготовки нового проекта и нового файла по виду: консоль и окно — программа (main / WinMain),
    /// DLL и библиотека — заголовок с объявлениями и файл Си с функциями, без main.
    /// </summary>
    public static class Templates
    {
        /// <summary>Имя Си из имени проекта: буквы, цифры, «_»; не с цифры.</summary>
        public static string Ident(string name)
        {
            var sb = new StringBuilder();
            foreach (char c in name ?? "")
                sb.Append(c < 128 && (char.IsLetterOrDigit(c) || c == '_') ? char.ToLowerInvariant(c) : '_');
            string s = sb.ToString().Trim('_');
            if (s.Length == 0) s = "lib";
            if (char.IsDigit(s[0])) s = "lib_" + s;
            return s;
        }

        public static bool IsLibraryKind(ProgramKind kind) { return kind == ProgramKind.Dll || kind == ProgramKind.Library; }

        /// <summary>Заголовок библиотеки: то, что видят программы, которые её берут.</summary>
        public static string Header(ProgramKind kind, string name)
        {
            string id = Ident(name);
            return "#pragma once\r\n\r\n" +
                   (kind == ProgramKind.Dll
                       ? "// " + name + ".dll: functions other programs call (wcc main.c -l " + name + ")\r\n"
                       : "// " + name + ".lib: functions programs build in (wcc main.c " + name + ".lib)\r\n") +
                   "\r\n" +
                   "int " + id + "_add(int a, int b);\r\n";
        }

        /// <summary>Файл Си библиотеки (DLL — её функции наружу помечены __declspec(dllexport)).</summary>
        public static string LibrarySource(ProgramKind kind, string name)
        {
            string id = Ident(name);
            if (kind == ProgramKind.Dll)
                return "#include <windows.h>\r\n" +
                       "#include \"" + name + ".h\"\r\n" +
                       "\r\n" +
                       "__declspec(dllexport) int " + id + "_add(int a, int b)\r\n" +
                       "{\r\n" +
                       "    return a + b;\r\n" +
                       "}\r\n" +
                       "\r\n" +
                       "BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved)\r\n" +
                       "{\r\n" +
                       "    return TRUE;\r\n" +
                       "}\r\n";
            return "#include \"" + name + ".h\"\r\n" +
                   "\r\n" +
                   "int " + id + "_add(int a, int b)\r\n" +
                   "{\r\n" +
                   "    return a + b;\r\n" +
                   "}\r\n";
        }

        /// <summary>Новый файл без проекта по виду: программа консоли, окно (WinMain), функции DLL или библиотеки.</summary>
        public static string NewFile(ProgramKind kind, string consoleProgram)
        {
            switch (kind)
            {
                case ProgramKind.Window: return WindowProgram;
                case ProgramKind.Dll:
                    return "#include <windows.h>\r\n" +
                           "\r\n" +
                           "__declspec(dllexport) int add(int a, int b)\r\n" +
                           "{\r\n" +
                           "    return a + b;\r\n" +
                           "}\r\n";
                case ProgramKind.Library:
                    return "int add(int a, int b)\r\n" +
                           "{\r\n" +
                           "    return a + b;\r\n" +
                           "}\r\n";
                default: return consoleProgram;
            }
        }

        /// <summary>Новый пустой файл Си в библиотеке: только подключение её заголовка.</summary>
        public static string LibraryFile(string name)
        {
            return "#include \"" + name + ".h\"\r\n\r\n";
        }

        public const string WindowProgram =
            "#include <windows.h>\r\n" +
            "\r\n" +
            "int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)\r\n" +
            "{\r\n" +
            "    MessageBoxA(NULL, \"Hello, world!\", \"WinCC\", MB_OK);\r\n" +
            "    return 0;\r\n" +
            "}\r\n";
    }
}
