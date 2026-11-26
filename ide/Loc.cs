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
            Add("RunTip", "Build and run (F5)", "Собрать и запустить (F5)");
            Add("Run", "Run", "Запустить");
            Add("KindLabel", "Program kind", "Вид программы");
            Add("KindTip", "Console — runs in a console window, Window — a program with its own windows, DLL — a library",
                           "Консоль — программа в окне консоли, Окно — программа со своими окнами, DLL — библиотека");
            Add("KindConsole", "Console", "Консоль");
            Add("KindWindow", "Window", "Окно");
            Add("KindDll", "DLL", "DLL");
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
            Add("FilterProjectFiles", "C files and resources (*.c; *.h; *.asm; *.rc)|*.c;*.h;*.asm;*.rc|Resources (*.rc; *.res; *.ico; *.bmp; *.cur)|*.rc;*.res;*.ico;*.bmp;*.cur|All files (*.*)|*.*",
                                      "Файлы Си и ресурсы (*.c; *.h; *.asm; *.rc)|*.c;*.h;*.asm;*.rc|Ресурсы (*.rc; *.res; *.ico; *.bmp; *.cur)|*.rc;*.res;*.ico;*.bmp;*.cur|Все файлы (*.*)|*.*");
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
            Add("ShowInFolder", "Show in folder", "Показать в папке");

            // Свойства проекта
            Add("PropertiesTitle", "Project properties: {0}", "Свойства проекта «{0}»");
            Add("OutputFile", "Output file", "Готовый файл");
            Add("OutputEmpty", "Empty — {0}.exe / .dll", "Пусто — {0}.exe / .dll");
            Add("Macros", "Macros (-D)", "Макросы (-D)");
            Add("IncludeFolders", "#include folders (-I)", "Папки #include (-I)");
            Add("Dlls", "DLLs (-l)", "DLL (-l)");
            Add("StackMb", "Stack, MB", "Стек, МБ");
            Add("StackDefault", "0 — 8 MB", "0 — 8 МБ");
            Add("OtherArgs", "Other arguments", "Другие параметры");

            // Настройки
            Add("SettingsTitle", "Settings", "Настройки");
            Add("Language", "Language", "Язык");
        }
    }
}
