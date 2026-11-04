namespace WinCC
{
    /// <summary>Образцы кода и вывода для проверочных снимков окна (--window).</summary>
    static class Samples
    {
        public const string Hello =
            "/* Пример: таблица квадратов и приветствие.\r\n" +
            "   wcc собирает его в обычный .exe для Windows. */\r\n" +
            "#include <stdio.h>\r\n" +
            "#include <string.h>\r\n" +
            "#define COUNT 10\r\n" +
            "\r\n" +
            "static int square(int x)\r\n" +
            "{\r\n" +
            "    return x * x;   // квадрат числа\r\n" +
            "}\r\n" +
            "\r\n" +
            "int main(int argc, char **argv)\r\n" +
            "{\r\n" +
            "    const char *name = argc > 1 ? argv[1] : \"мир\";\r\n" +
            "    double total = 0.0\r\n" +
            "    for (int i = 0; i < COUNT; i++) {\r\n" +
            "        total += square(i) * 1.5e-1;\r\n" +
            "        printf(\"%2d -> %d\\n\", i, square(i));\r\n" +
            "    }\r\n" +
            "    printf(\"Привет, %s! Сумма: %.2f\\n\", name, total);\r\n" +
            "    return strlen(name) == 0 ? 1 : 0;\r\n" +
            "}\r\n";

        public const string Window =
            "#include <windows.h>\r\n" +
            "\r\n" +
            "int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)\r\n" +
            "{\r\n" +
            "    MessageBoxA(NULL, \"Окно собрано WinCC\", \"WinCC\", MB_OK | MB_ICONINFORMATION);\r\n" +
            "    return 0;\r\n" +
            "}\r\n";

        public static string[] BuildOutput(string file)
        {
            return new[]
            {
                "Сборка: " + file + " → " + System.IO.Path.ChangeExtension(file, ".exe") + " (Консоль)",
                file + ":15: ожидалось ';'",
                file + ":21: предупреждение: strlen возвращает size_t, а main — int"
            };
        }
    }
}
