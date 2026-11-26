namespace WinCC
{
    /// <summary>Образцы кода и вывода для проверочных снимков окна (--window).</summary>
    static class Samples
    {
        public const string Hello =
            "/* Example: a table of squares and a greeting.\r\n" +
            "   wcc builds it into an ordinary Windows .exe. */\r\n" +
            "#include <stdio.h>\r\n" +
            "#include <string.h>\r\n" +
            "#define COUNT 10\r\n" +
            "\r\n" +
            "static int square(int x)\r\n" +
            "{\r\n" +
            "    return x * x;   // the square of a number\r\n" +
            "}\r\n" +
            "\r\n" +
            "int main(int argc, char **argv)\r\n" +
            "{\r\n" +
            "    const char *name = argc > 1 ? argv[1] : \"world\";\r\n" +
            "    double total = 0.0\r\n" +
            "    for (int i = 0; i < COUNT; i++) {\r\n" +
            "        total += square(i) * 1.5e-1;\r\n" +
            "        printf(\"%2d -> %d\\n\", i, square(i));\r\n" +
            "    }\r\n" +
            "    printf(\"Hello, %s! Total: %.2f\\n\", name, total);\r\n" +
            "    return strlen(name) == 0 ? 1 : 0;\r\n" +
            "}\r\n";

        public const string Window =
            "#include <windows.h>\r\n" +
            "\r\n" +
            "int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)\r\n" +
            "{\r\n" +
            "    MessageBoxA(NULL, \"Window built by WinCC\", \"WinCC\", MB_OK | MB_ICONINFORMATION);\r\n" +
            "    return 0;\r\n" +
            "}\r\n";

        public static string[] BuildOutput(string file)
        {
            return new[]
            {
                Loc.F("Building", file, System.IO.Path.ChangeExtension(file, ".exe"), Loc.T("KindConsole")),
                file + ":15: expected ';'",
                file + ":21: warning: strlen returns size_t, but main returns int"
            };
        }
    }
}
