using System.Collections.Generic;
using System.Globalization;
using System.Windows;

namespace WinCC
{
    /// <summary>Язык окна.</summary>
    public enum UiLanguage { English, Russian }

    /// <summary>
    /// Надписи окна на всех языках, рядом. Loc.Apply кладёт выбранный язык в ресурсы приложения как «S.&lt;ключ&gt;» —
    /// XAML берёт их через {DynamicResource S.ключ} и меняется на ходу; код — Loc.T("ключ") или Loc.F("ключ", …)
    /// (места для значений — как в string.Format).
    /// </summary>
    public static class Loc
    {
        static readonly Dictionary<string, string[]> table = new Dictionary<string, string[]>();

        public static UiLanguage Current { get; private set; }

        public static readonly UiLanguage[] All = { UiLanguage.English, UiLanguage.Russian };

        /// <summary>Имя языка на нём самом (для списка в настройках).</summary>
        public static string NativeName(UiLanguage lang) { return lang == UiLanguage.Russian ? "Русский" : "English"; }

        public static string Code(UiLanguage lang) { return lang == UiLanguage.Russian ? "ru" : "en"; }

        public static UiLanguage Parse(string code)
        {
            return code == "ru" ? UiLanguage.Russian : code == "en" ? UiLanguage.English : SystemDefault;
        }

        /// <summary>Русский для русской Windows, иначе английский.</summary>
        public static UiLanguage SystemDefault
        {
            get { return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "ru" ? UiLanguage.Russian : UiLanguage.English; }
        }

        public static string T(string key)
        {
            string[] s;
            return table.TryGetValue(key, out s) ? s[(int)Current] : key;
        }

        public static string F(string key, params object[] args) { return string.Format(T(key), args); }

        public static void Apply(UiLanguage lang)
        {
            Current = lang;
            // надписи самой ThemeLibrary (кнопки сообщений, подсказки кнопок окна) — на том же языке
            ThemeLibrary.ThemeManager.Language = lang == UiLanguage.Russian ? "ru-RU" : "en-US";
            var res = Application.Current.Resources;
            foreach (var pair in table) res["S." + pair.Key] = pair.Value[(int)lang];
        }

        static void Add(string key, string en, string ru) { table[key] = new[] { en, ru }; }

        static Loc()
        {
            // Окно и кнопки
            Add("Subtitle", "C compiler for Windows", "компилятор Си для Windows");
            Add("HelpTip", "Help — WinCC manual (F1)", "Справка — руководство WinCC (F1)");
            Add("SettingsTip", "Settings", "Настройки");
            Add("ProjectTip", "Project panel (Ctrl+Shift+E)", "Панель проекта (Ctrl+Shift+E)");
            Add("Project", "Project", "Проект");
            Add("NewTip", "New file (Ctrl+N)", "Новый файл (Ctrl+N)");
            Add("New", "New", "Новый");
            Add("OpenTip", "Open file (Ctrl+O)", "Открыть файл (Ctrl+O)");
            Add("Open", "Open", "Открыть");
            Add("SaveTip", "Save (Ctrl+S), Save as — Ctrl+Shift+S", "Сохранить (Ctrl+S), сохранить как — Ctrl+Shift+S");
            Add("Save", "Save", "Сохранить");
            Add("BuildTip", "Build (F7)", "Собрать (F7)");
            Add("Build", "Build", "Собрать");
            Add("RunTip", "Build and run without the debugger (Ctrl+F5)", "Собрать и запустить без отладчика (Ctrl+F5)");
            Add("Run", "Run", "Запустить");
            Add("DebugTip", "Build and debug: breakpoints, steps, variables (F5)", "Собрать и отлаживать: точки останова, шаги, переменные (F5)");
            Add("Debug", "Debug", "Отладка");
            Add("Continue", "Continue", "Продолжить");
            Add("ContinueTip", "Continue to the next breakpoint (F5)", "Продолжить до следующей точки останова (F5)");
            Add("StopTip", "Stop debugging (Shift+F5)", "Остановить отладку (Shift+F5)");
            Add("StepOverTip", "Step over: the next line, calls without going into them (F10)", "Шаг с обходом: следующая строка, вызовы — не заходя в них (F10)");
            Add("StepIntoTip", "Step into: the next line, into called functions (F11)", "Шаг с заходом: следующая строка, с заходом в вызванные функции (F11)");
            Add("StepOutTip", "Step out: run to the end of this function (Shift+F11)", "Шаг наружу: до конца этой функции (Shift+F11)");
            Add("ConfigTip", "Debug — for the debugger: debug info, every variable in memory; Release — optimized, small and fast",
                             "Отладка — для отладчика: сведения о программе, все переменные в памяти; Выпуск — оптимизированный код, маленький и быстрый");
            Add("ConfigDebug", "Debug", "Отладка");
            Add("ConfigRelease", "Release", "Выпуск");
            Add("StatusConfig", "{0} · ", "{0} · ");
            Add("Variables", "Variables", "Переменные");
            Add("CallStack", "Call stack", "Стек вызовов");
            Add("ColName", "Name", "Имя");
            Add("ColValue", "Value", "Значение");
            Add("ColType", "Type", "Тип");
            Add("ColFunction", "Function", "Функция");
            Add("ColPlace", "Place", "Место");
            Add("NoVariables", "Variables appear here when the program stops under the debugger", "Переменные появятся здесь, когда программа остановится под отладчиком");
            Add("Globals", "Globals", "Глобальные");
            Add("DbgStarted", "Debugging {0}", "Отладка {0}");
            Add("DbgExited", "The program exited with code {0}.", "Программа завершилась, код {0}.");
            Add("DbgNoInfo", "No debug info next to the program: {0}", "Рядом с программой нет сведений для отладчика: {0}");
            Add("DbgNoDll", "A DLL is not debugged on its own — debug the program that loads it.", "DLL сама не отлаживается — отлаживайте программу, которая её загружает.");
            Add("DbgCannotStart", "Could not start the program under the debugger: {0}", "Не удалось запустить программу под отладчиком: {0}");
            Add("DbgProgramBreak", "The program stopped itself (__debugbreak).", "Программа остановилась сама (__debugbreak).");
            Add("DbgBreakpointAt", "Breakpoint: {0}, line {1}", "Точка останова: {0}, строка {1}");
            Add("DbgAccessRead", "Access violation: reading address 0x{0}", "Обращение к памяти: чтение по адресу 0x{0}");
            Add("DbgAccessWrite", "Access violation: writing to address 0x{0}", "Обращение к памяти: запись по адресу 0x{0}");
            Add("DbgAccessExecute", "Access violation: running code at address 0x{0}", "Обращение к памяти: выполнение кода по адресу 0x{0}");
            Add("DbgDivZero", "integer division by zero", "целочисленное деление на ноль");
            Add("DbgStackOverflow", "stack overflow", "переполнение стека");
            Add("DbgIllegal", "illegal instruction", "недопустимая команда");
            Add("DbgArrayBounds", "array bounds exceeded", "выход за границы массива");
            Add("DbgException", "Unhandled exception 0x{0} {1}", "Необработанное исключение 0x{0} {1}");
            Add("DbgExceptionEnd", "Continue or Stop ends the program.", "«Продолжить» или «Стоп» завершат программу.");
            Add("DbgUnreadable", "<cannot read>", "<не прочитать>");
            Add("DbgAnonymous", "anonymous", "безымянное");
            Add("DbgNoSource", "no source", "без исходника");
            Add("DbgLine", "{0}, line {1}", "{0}, строка {1}");
            Add("KindLabel", "Program kind", "Вид программы");
            Add("KindTip", "Console — runs in a console window, Window — a program with its own windows, DLL — a library loaded while running, Library — your own .lib that programs build in",
                           "Консоль — программа в окне консоли, Окно — программа со своими окнами, DLL — библиотека, загружаемая при работе, Библиотека — своя .lib, которую программы вшивают в себя");
            Add("KindConsole", "Console", "Консоль");
            Add("KindWindow", "Window", "Окно");
            Add("KindDll", "DLL", "DLL");
            Add("KindLibrary", "Library", "Библиотека");
            Add("LibNoRun", "A library does not run by itself — add its .lib to a program's project (or: wcc main.c mylib.lib).",
                            "Библиотека сама не запускается — добавьте её .lib в проект программы (или: wcc main.c моя.lib).");
            Add("GroupLibraries", "Libraries", "Библиотеки");
            Add("NoOpenFiles", "No open files", "Нет открытых файлов");
            Add("NoOpenFilesHint", "Create a new file or open an existing one — you can also drop it onto the window.",
                                   "Создайте новый файл или откройте готовый — его можно и перетащить в окно.");
            Add("NewFile", "New file", "Новый файл");
            Add("OpenEllipsis", "Open…", "Открыть…");
            Add("Output", "Output", "Вывод");
            Add("OutputTip", "Double-click an error line to go to it", "Двойной щелчок по строке с ошибкой — перейти к ней");
            Add("Errors", "Errors", "Ошибки");
            Add("ErrorsCount", "Errors ({0})", "Ошибки ({0})");
            Add("ErrorsWarnings", "Errors ({0}, warnings {1})", "Ошибки ({0}, предупреждений {1})");
            Add("ColFile", "File", "Файл");
            Add("ColLine", "Line", "Строка");
            Add("ColMessage", "Message", "Сообщение");
            Add("NoErrors", "No errors", "Ошибок нет");
            Add("Warning", "Warning", "Предупреждение");
            Add("Error", "Error", "Ошибка");

            // Строка состояния
            Add("UnsavedPath", "New file — save it before building", "Новый файл — сохраните его перед сборкой");
            Add("Caret", "Ln {0}, Col {1}", "Строка {0}, столбец {1}");
            Add("StatusProject", "Project: {0} · ", "Проект: {0} · ");
            Add("StatusKind", "Kind: {0}", "Вид: {0}");
            Add("WccFound", "wcc.exe found", "wcc.exe на месте");
            Add("WccMissing", "wcc.exe not found", "wcc.exe не найден");

            // Сборка
            Add("Building", "Building: {0} → {1} ({2})", "Сборка: {0} → {1} ({2})");
            Add("BuildingProject", "Building project \"{0}\": {1} file(s) → {2} ({3})", "Сборка проекта «{0}»: файлов {1} → {2} ({3})");
            Add("NothingToBuild", "Project \"{0}\" has no .c files — nothing to build.", "В проекте «{0}» нет файлов .c — собирать нечего.");
            Add("MissingFiles", "Project files missing on disk:", "Файлов проекта нет на диске:");
            Add("Seconds", "{0} s", "{0} с");
            Add("BuildFailed", "Build failed (code {0}, {1}).", "Сборка не удалась (код {0}, {1}).");
            Add("NoOutputFile", "The compiler did not produce {0} ({1}).", "Компилятор не сделал файл {0} ({1}).");
            Add("Built", "Built in {0}: {1}", "Собрано за {0}: {1}");
            Add("DllNoRun", "A DLL does not run by itself — another program loads it.", "DLL сама не запускается — её загружает другая программа.");
            Add("StartedConsole", "Started in its own console window.", "Запущена в своём окне консоли.");
            Add("Started", "Started.", "Запущена.");
            Add("CannotStart", "Could not start: {0}", "Не удалось запустить: {0}");
            Add("WccNotFound", "Compiler not found: {0}\nPut wcc.exe into the folder with WinCC.exe.",
                               "Компилятор не найден: {0}\nПоложите wcc.exe в папку, где лежит WinCC.exe.");
            Add("FileLocked", "{0} is in use — the program is probably still running. Close it and build again.",
                              "Файл {0} занят — видимо, программа ещё запущена. Закройте её и соберите снова.");
            Add("WccNoStart", "Could not start the compiler: {0}", "Не удалось запустить компилятор: {0}");
            Add("ProgramExited", "[Program finished, exit code {0}]", "[Программа завершилась, код {0}]");
            Add("PressAnyKey", "Press any key to close the window.", "Нажмите любую клавишу, чтобы закрыть окно.");

            // Файлы
            Add("FilterSave", "C sources (*.c; *.h)|*.c;*.h|Resources (*.rc)|*.rc|All files (*.*)|*.*",
                              "Исходники Си (*.c; *.h)|*.c;*.h|Ресурсы (*.rc)|*.rc|Все файлы (*.*)|*.*");
            Add("FilterOpen", "C sources, resources and projects (*.c; *.h; *.rc; *.wccproj)|*.c;*.h;*.rc;*.wccproj|WinCC projects (*.wccproj)|*.wccproj|All files (*.*)|*.*",
                              "Исходники Си, ресурсы и проекты (*.c; *.h; *.rc; *.wccproj)|*.c;*.h;*.rc;*.wccproj|Проекты WinCC (*.wccproj)|*.wccproj|Все файлы (*.*)|*.*");
            Add("FilterProject", "WinCC projects (*.wccproj)|*.wccproj", "Проекты WinCC (*.wccproj)|*.wccproj");
            Add("FilterProjectFiles", "C files, libraries and resources (*.c; *.h; *.asm; *.lib; *.rc)|*.c;*.h;*.asm;*.lib;*.rc|Libraries WinCC (*.lib)|*.lib|Resources (*.rc; *.res; *.ico; *.bmp; *.cur)|*.rc;*.res;*.ico;*.bmp;*.cur|All files (*.*)|*.*",
                                      "Файлы Си, библиотеки и ресурсы (*.c; *.h; *.asm; *.lib; *.rc)|*.c;*.h;*.asm;*.lib;*.rc|Библиотеки WinCC (*.lib)|*.lib|Ресурсы (*.rc; *.res; *.ico; *.bmp; *.cur)|*.rc;*.res;*.ico;*.bmp;*.cur|Все файлы (*.*)|*.*");
            Add("OpenFileTitle", "Open file", "Открыть файл");
            Add("SaveFileTitle", "Save file", "Сохранить файл");
            Add("CannotOpenFile", "Could not open the file:\n{0}\n\n{1}", "Не удалось открыть файл:\n{0}\n\n{1}");
            Add("CannotSaveFile", "Could not save the file:\n{0}\n\n{1}", "Не удалось сохранить файл:\n{0}\n\n{1}");
            Add("CannotCreateFile", "Could not create the file:\n{0}\n\n{1}", "Не удалось создать файл:\n{0}\n\n{1}");
            Add("SaveChanges", "Save changes to \"{0}\"?", "Сохранить изменения в файле «{0}»?");
            Add("FileModified", "File modified", "Файл изменён");
            Add("Untitled", "untitled {0}.c", "без имени {0}.c");
            Add("NotSavedYet", "The file is not saved yet", "Файл ещё не сохранён");
            Add("CloseTab", "Close tab", "Закрыть вкладку");
            Add("Utf8Bom", "UTF-8 with BOM", "UTF-8 с BOM");

            // Справка
            Add("Help", "Help", "Справка");
            Add("ManualMissing", "\"{0}\" was not found next to WinCC.exe.\nIt is installed with WinCC — reinstall the program.",
                                 "Файл «{0}» не найден рядом с WinCC.exe.\nОн ставится вместе с WinCC — переустановите программу.");
            Add("CannotOpenManual", "Could not open the manual:\n{0}\n\n{1}", "Не удалось открыть руководство:\n{0}\n\n{1}");

            // Проект
            Add("OpenProjectTitle", "Open project", "Открыть проект");
            Add("CannotOpenProject", "Could not open the project:\n{0}\n\n{1}", "Не удалось открыть проект:\n{0}\n\n{1}");
            Add("CannotSaveProject", "Could not save the project:\n{0}\n\n{1}", "Не удалось сохранить проект:\n{0}\n\n{1}");
            Add("CannotCreateProject", "Could not create the project:\n{0}\n\n{1}", "Не удалось создать проект:\n{0}\n\n{1}");
            Add("NewProjectTitle", "New project", "Новый проект");
            Add("NewProjectName", "Program", "Программа");
            Add("NewProjectNext", "Next…", "Дальше…");
            Add("NewKindConsole", "A program in a console window: main.c with main()", "Программа в окне консоли: main.c с main()");
            Add("NewKindWindow", "A program with its own windows: main.c with WinMain()", "Программа со своими окнами: main.c с WinMain()");
            Add("NewKindDll", "A DLL: name.h with the declarations, name.c with __declspec(dllexport) functions and DllMain",
                              "DLL: имя.h с объявлениями, имя.c с функциями __declspec(dllexport) и DllMain");
            Add("NewKindLibrary", "Your own library .lib, built into the programs that take it: name.h and name.c, no main",
                                  "Своя библиотека .lib, которую программы вшивают в себя: имя.h и имя.c, без main");
            Add("NewLibraryName", "MyLibrary", "МояБиблиотека");
            Add("AddFoundFiles", "The folder already has C files: {0}.\nAdd them to the project?", "В папке уже есть файлы Си: {0}.\nДобавить их в проект?");
            Add("FileMissingRemove", "The file is missing on disk:\n{0}\n\nRemove it from the project?", "Файла нет на диске:\n{0}\n\nУбрать его из проекта?");
            Add("AddFilesTitle", "Add files to the project", "Добавить файлы в проект");
            Add("NewFileInProject", "New file in the project", "Новый файл в проекте");
            Add("CloseProject", "Close project", "Закрыть проект");
            Add("ProjectProperties", "Project properties", "Свойства проекта");
            Add("ProjectNoFiles", "The project has no files", "В проекте нет файлов");
            Add("AddFilesEllipsis", "Add files…", "Добавить файлы…");
            Add("NoProject", "No project open", "Проект не открыт");
            Add("NewProjectEllipsis", "New project…", "Новый проект…");
            Add("OpenProjectEllipsis", "Open project…", "Открыть проект…");
            Add("Recent", "Recent", "Недавние");
            Add("GroupSources", "Sources", "Исходники");
            Add("GroupHeaders", "Headers", "Заголовки");
            Add("GroupResources", "Resources", "Ресурсы");
            Add("GroupOther", "Other", "Прочие");
            Add("MissingOnDisk", "Missing on disk: {0}", "Файла нет на диске: {0}");
            Add("RemoveFromProject", "Remove from project", "Убрать из проекта");
            Add("AddFolderTip", "Add a folder with its subfolders", "Добавить папку с вложенными папками");
            Add("AddFolderTitle", "Add a folder to the project", "Добавить папку в проект");
            Add("FolderNoFiles", "There are no C files, headers, assembler files or resources in this folder:\n{0}",
                                 "В папке нет файлов Си, заголовков, ассемблера и ресурсов:\n{0}");
            Add("RemoveFolder", "Remove folder from project", "Убрать папку из проекта");
            Add("ShowInFolder", "Show in folder", "Показать в папке");

            // Свойства проекта
            Add("PropertiesTitle", "Project properties: {0}", "Свойства проекта «{0}»");
            Add("OutputFile", "Output file", "Готовый файл");
            Add("OutputEmpty", "Empty — {0}.exe / .dll", "Пусто — {0}.exe / .dll");
            Add("Macros", "Macros (-D)", "Макросы (-D)");
            Add("IncludeFolders", "#include folders (-I)", "Папки #include (-I)");
            Add("Dlls", "DLLs (-l)", "DLL (-l)");
            Add("LibrariesHint", "Your .lib files go into the project as files: Add files… → *.lib", "Свои .lib входят в проект файлами: «Добавить файлы…» → *.lib");
            Add("StackMb", "Stack, MB", "Стек, МБ");
            Add("StackDefault", "0 — 8 MB", "0 — 8 МБ");
            Add("OtherArgs", "Other arguments", "Другие параметры");

            // Настройки
            Add("SettingsTitle", "Settings", "Настройки");
            Add("Language", "Language", "Язык");
        }
    }
}
