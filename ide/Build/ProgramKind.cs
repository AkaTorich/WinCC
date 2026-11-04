namespace WinCC.Build
{
    /// <summary>Вид собираемой программы — выбирается в окне.</summary>
    public enum ProgramKind { Console, Window, Dll }

    public static class ProgramKinds
    {
        public static readonly ProgramKind[] All = { ProgramKind.Console, ProgramKind.Window, ProgramKind.Dll };

        public static string Title(ProgramKind kind)
        {
            switch (kind)
            {
                case ProgramKind.Window: return "Окно";
                case ProgramKind.Dll: return "DLL";
                default: return "Консоль";
            }
        }

        /// <summary>Расширение готового файла.</summary>
        public static string Extension(ProgramKind kind) { return kind == ProgramKind.Dll ? ".dll" : ".exe"; }

        /// <summary>Ключ wcc для вида: --gui для окна, --dll для DLL, у консоли нет.</summary>
        public static string Switch(ProgramKind kind)
        {
            switch (kind)
            {
                case ProgramKind.Window: return "--gui";
                case ProgramKind.Dll: return "--dll";
                default: return null;
            }
        }

        public static ProgramKind Parse(string s)
        {
            switch (s)
            {
                case "Window": return ProgramKind.Window;
                case "Dll": return ProgramKind.Dll;
                default: return ProgramKind.Console;
            }
        }
    }
}
