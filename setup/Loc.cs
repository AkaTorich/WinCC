using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Windows;

namespace WinCC.Setup
{
    /// <summary>
    /// Надписи установщика на английском и русском, рядом. Apply кладёт выбранный язык в ресурсы приложения как «S.ключ»
    /// (XAML: {DynamicResource S.ключ}, меняется на ходу); код берёт T("ключ") / F("ключ", …).
    /// Выбранный язык записывается и окну WinCC — в его settings.txt (строка language=).
    /// </summary>
    public static class Loc
    {
        static readonly Dictionary<string, string[]> table = new Dictionary<string, string[]>();

        /// <summary>true — русский, false — английский.</summary>
        public static bool Russian { get; private set; }

        public static string T(string key)
        {
            string[] s;
            return table.TryGetValue(key, out s) ? s[Russian ? 1 : 0] : key;
        }

        public static string F(string key, params object[] args) { return string.Format(T(key), args); }

        public static void Apply(bool russian)
        {
            Russian = russian;
            ThemeLibrary.ThemeManager.Language = russian ? "ru-RU" : "en-US";
            var app = Application.Current;
            if (app == null) return;
            foreach (var pair in table) app.Resources["S." + pair.Key] = pair.Value[russian ? 1 : 0];
        }

        /// <summary>Язык по умолчанию: тот, что уже выбран в окне WinCC, иначе язык Windows.</summary>
        public static bool DefaultRussian()
        {
            string code = ReadIdeLanguage();
            if (code == "ru") return true;
            if (code == "en") return false;
            return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "ru";
        }

        static string IdeSettingsPath
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "WinCC", "settings.txt"); }
        }

        static string ReadIdeLanguage()
        {
            try
            {
                if (!File.Exists(IdeSettingsPath)) return null;
                foreach (string line in File.ReadAllLines(IdeSettingsPath, Encoding.UTF8))
                    if (line.StartsWith("language=")) return line.Substring(9).Trim();
            }
            catch { }
            return null;
        }

        /// <summary>Записать язык окну WinCC: остальные строки его настроек остаются как были.</summary>
        public static void SaveIdeLanguage()
        {
            try
            {
                var lines = new List<string>();
                if (File.Exists(IdeSettingsPath))
                    foreach (string line in File.ReadAllLines(IdeSettingsPath, Encoding.UTF8))
                        if (!line.StartsWith("language=")) lines.Add(line);
                lines.Insert(0, "language=" + (Russian ? "ru" : "en"));
                Directory.CreateDirectory(Path.GetDirectoryName(IdeSettingsPath));
                File.WriteAllLines(IdeSettingsPath, lines, new UTF8Encoding(false));
            }
            catch { }
        }

        static void Add(string key, string en, string ru) { table[key] = new[] { en, ru }; }

        static Loc()
        {
            // Окно
            Add("Title", "WinCC Setup", "Установка WinCC");
            Add("Subtitle", "C compiler for Windows", "компилятор Си для Windows");
            Add("LongName", "WinCC — C compiler for Windows", "WinCC — компилятор Си для Windows");
            Add("Tagline", "A C compiler of its own for Windows: console and window programs, DLL libraries",
                           "Свой компилятор Си для Windows: консольные и оконные программы, библиотеки DLL");
            Add("FeatEditor", "Editor with highlighting", "Редактор с подсветкой");
            Add("FeatWcc", "wcc command", "Команда wcc");
            Add("FeatDll", "Your own DLLs", "Свои DLL");
            Add("FeatNoAdmin", "No administrator rights", "Без прав администратора");
            Add("WhereTitle", "Install to", "Куда установить");
            Add("Browse", "Browse…", "Обзор…");
            Add("OptDesktop", "Desktop shortcut", "Ярлык на рабочем столе");
            Add("OptStart", "Start menu shortcut", "Ярлык в меню «Пуск»");
            Add("OptPath", "wcc command in any console", "Команда wcc в любой консоли");
            Add("OptPathTip", "The WinCC folder is added to the user PATH variable", "Папка WinCC добавляется в переменную PATH пользователя");
            Add("OptOpenWith", "WinCC in \"Open with\" for .c, .h and .wccproj", "В «Открыть с помощью» для .c, .h, .wccproj");
            Add("OptOpenWithTip", "The program that opens .c files now stays the same", "Программа, которая открывает .c сейчас, остаётся прежней");
            Add("OptLaunch", "Start WinCC after installation", "Запустить WinCC после установки");
            Add("LanguageTip", "Language of the installer and of WinCC", "Язык установщика и WinCC");
            Add("Installing", "Installing…", "Установка…");
            Add("UninstallButton", "Uninstall WinCC", "Удалить WinCC");
            Add("Cancel", "Cancel", "Отмена");
            Add("Install", "Install", "Установить");
            Add("Update", "Update", "Обновить");
            Add("Done", "Done", "Готово");
            Add("Close", "Close", "Закрыть");
            Add("Version", "version {0}", "версия {0}");
            Add("InstalledUpdate", "Version {0} is installed now — it will be updated.", "Сейчас установлена версия {0} — она будет обновлена.");
            Add("ForYou", "Installs for you only, no administrator rights needed.", "Ставится только для вас, права администратора не нужны.");
            Add("Damaged", "The installer is damaged", "Установщик повреждён");
            Add("DamagedText", "The program files were not found inside. Download the installer again.", "Внутри не нашлись файлы программы. Скачайте установщик заново.");
            Add("PickFolder", "Where to install WinCC", "Куда установить WinCC");
            Add("InstallingWinCC", "Installing WinCC…", "Установка WinCC…");
            Add("Installed", "WinCC is installed", "WinCC установлен");
            Add("FolderLine", "Folder: {0}", "Папка: {0}");
            Add("WccNewConsoles", "The wcc command works in new console windows.", "Команда wcc работает в новых окнах консоли.");
            Add("UninstallAsk", "Uninstall WinCC? Only its own files are removed; your files, even in the WinCC folder, stay.",
                                "Удалить WinCC? Уйдут только его собственные файлы; ваши файлы, даже в папке WinCC, останутся.");
            Add("UninstallTitle", "Uninstall", "Удаление");
            Add("Uninstalling", "Uninstalling WinCC…", "Удаление WinCC…");
            Add("Uninstalled", "WinCC is uninstalled", "WinCC удалён");
            Add("UninstalledText", "Only the files of WinCC itself were removed; your files were not touched.", "Ушли только файлы самого WinCC; ваши файлы не тронуты.");
            Add("Failed", "Something went wrong", "Не получилось");
            Add("Error", "Error: {0}", "Ошибка: {0}");

            // Ход установки и удаления
            Add("NoFolder", "The installation folder is not set.", "Не задана папка установки.");
            Add("RunningInstall", "WinCC is running now. Close it and repeat the installation.", "WinCC сейчас запущен. Закройте его и повторите установку.");
            Add("Unpacking", "Unpacking files…", "Распаковка файлов…");
            Add("Copied", "Files copied: {0}", "Скопировано файлов: {0}");
            Add("SayDesktop", "Desktop shortcut…", "Ярлык на рабочем столе…");
            Add("SayStart", "Start menu shortcut…", "Ярлык в меню «Пуск»…");
            Add("SayPath", "wcc command in any console (PATH)…", "Команда wcc в любой консоли (PATH)…");
            Add("SayOpenWith", "WinCC in \"Open with\" for .c, .h and .wccproj…", "WinCC в «Открыть с помощью» для .c, .h и .wccproj…");
            Add("SayRegister", "Registering the program…", "Регистрация программы…");
            Add("SayInstallDone", "Installation complete.", "Установка завершена.");
            Add("SayLaunch", "Starting WinCC…", "Запуск WinCC…");
            Add("LaunchFailed", "Could not start: {0}", "Запустить не удалось: {0}");
            Add("NotWritable", "Cannot write to the folder: {0}", "В папку нельзя писать: {0}");
            Add("NoPayload", "The installer has no program files inside (payload.zip).", "В установщике нет вложенных файлов программы (payload.zip).");
            Add("FileTypeC", "C source (WinCC)", "Исходник Си (WinCC)");
            Add("RunningUninstall", "WinCC is running. Close it and repeat the uninstallation.", "WinCC запущен. Закройте его и повторите удаление.");
            Add("SayRemoveShortcuts", "Removing shortcuts…", "Удаление ярлыков…");
            Add("SayRemovePath", "Removing the wcc command from PATH…", "Команда wcc убирается из PATH…");
            Add("SayRemoveOpenWith", "Removing WinCC from \"Open with\"…", "WinCC убирается из «Открыть с помощью»…");
            Add("SayCleanRegistry", "Cleaning the program records…", "Очистка записей программы…");
            Add("SayRemoveFiles", "Removing files…", "Удаление файлов…");
            Add("UninstallDone", "Uninstallation complete.", "Удаление завершено.");
            Add("UninstallDoneLeft", "Uninstallation complete. Your files are left in {0} ({1}) — they were not touched.",
                                     "Удаление завершено. В папке {0} остались ваши файлы ({1}) — они не тронуты.");
        }
    }
}
