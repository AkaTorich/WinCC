namespace WinCC.Build
{
    /// <summary>Режим сборки: «Отладка» — сведения для отладчика, переменные в памяти; «Выпуск» — оптимизированный код.</summary>
    public enum BuildConfig { Debug, Release }

    public static class BuildConfigs
    {
        public static readonly BuildConfig[] All = { BuildConfig.Debug, BuildConfig.Release };

        public static string Title(BuildConfig c) { return Loc.T(c == BuildConfig.Debug ? "ConfigDebug" : "ConfigRelease"); }

        /// <summary>Ключи wcc режима; forDebugger — сборка под отладчик (F5): сведения для него нужны и в «Выпуске».</summary>
        public static string Switches(BuildConfig c, bool forDebugger)
        {
            if (c == BuildConfig.Debug) return "-g -O0";
            return forDebugger ? "-g" : "";
        }

        public static BuildConfig Parse(string s) { return s == "Release" ? BuildConfig.Release : BuildConfig.Debug; }
    }
}
