using System.Collections.Generic;

namespace WinCC.Editor
{
    /// <summary>Что за кусок текста — от этого зависит его цвет.</summary>
    /// Function — вызов функции, FunctionDecl — её объявление или определение (и макрос с параметрами).
    public enum TokenKind : byte { Plain, Keyword, Type, Constant, String, Comment, Number, Preprocessor, Function, FunctionDecl }

    public struct Token
    {
        public int Start;
        public int Length;
        public TokenKind Kind;

        public Token(int start, int length, TokenKind kind) { Start = start; Length = length; Kind = kind; }
    }

    /// <summary>
    /// Разбор Си по строкам. Строка разбирается сама по себе, с переходящим состоянием: открытый
    /// многострочный комментарий и продолжение директивы препроцессора через «\» в конце строки.
    /// </summary>
    public static class CLexer
    {
        public const int InBlockComment = 1;
        public const int InDirective = 2;

        static readonly HashSet<string> keywords = new HashSet<string>
        {
            "auto", "break", "case", "const", "continue", "default", "do", "else", "enum", "extern", "for", "goto",
            "if", "inline", "register", "restrict", "return", "sizeof", "static", "struct", "switch", "typedef",
            "union", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Generic", "_Noreturn",
            "_Static_assert", "_Thread_local", "alignas", "alignof", "static_assert", "thread_local", "typeof",
            "constexpr", "__declspec", "__stdcall", "__cdecl", "__fastcall", "__asm", "asm", "__inline",
            "__forceinline", "__attribute__", "__int64", "__restrict"
        };

        static readonly HashSet<string> types = new HashSet<string>
        {
            "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "_Bool", "bool",
            "_Complex", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t", "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t", "wchar_t", "char16_t", "char32_t", "va_list", "FILE",
            "BOOL", "BYTE", "WORD", "DWORD", "UINT", "LONG", "ULONG", "HANDLE", "HWND", "HINSTANCE", "HMODULE",
            "HDC", "HBRUSH", "HICON", "HCURSOR", "HMENU", "LPSTR", "LPCSTR", "LPWSTR", "LPCWSTR", "LPVOID",
            "WPARAM", "LPARAM", "LRESULT", "MSG", "WNDCLASSA", "WNDCLASSW", "RECT", "POINT", "PAINTSTRUCT"
        };

        /// <summary>Слова, после которых имя с «(» — объявление функции, а не вызов: «static int f(», «extern f(».</summary>
        static readonly HashSet<string> declWords = new HashSet<string>
        {
            "static", "extern", "inline", "__inline", "__forceinline", "const", "volatile", "_Noreturn", "struct", "union",
            "enum", "__stdcall", "__cdecl", "__fastcall", "__int64", "constexpr"
        };

        static readonly HashSet<string> constants = new HashSet<string>
        {
            "NULL", "true", "false", "nullptr", "EOF", "TRUE", "FALSE", "stdin", "stdout", "stderr",
            "__FILE__", "__LINE__", "__func__", "__DATE__", "__TIME__"
        };

        /// <summary>
        /// Разбирает строку text[start, end) (без перевода строки). Куски кладёт в output, если он не null.
        /// Возвращает состояние для следующей строки.
        /// </summary>
        public static int LexLine(string text, int start, int end, int state, List<Token> output)
        {
            int i = start;

            if ((state & InBlockComment) != 0)
            {
                int close = IndexOf(text, "*/", i, end);
                if (close < 0)
                {
                    Add(output, i, end - i, TokenKind.Comment);
                    return InBlockComment;
                }
                Add(output, i, close + 2 - i, TokenKind.Comment);
                i = close + 2;
                state &= ~InBlockComment;
            }

            bool directive = (state & InDirective) != 0;
            bool defineLine = false;            // «#define имя(» — макрос с параметрами: объявление
            // что было перед именем в этой строке: последнее и предпоследнее слово или знак
            string prev = null, prev2 = null;
            TokenKind prevKind = TokenKind.Plain, prev2Kind = TokenKind.Plain;
            bool prevWord = false, prev2Word = false;
            if (!directive)
            {
                int j = i;
                while (j < end && (text[j] == ' ' || text[j] == '\t')) j++;
                if (j < end && text[j] == '#')
                {
                    // Знак # и слово директивы: «#include», «# define»
                    int k = j + 1;
                    while (k < end && (text[k] == ' ' || text[k] == '\t')) k++;
                    while (k < end && IsIdentChar(text[k])) k++;
                    Add(output, j, k - j, TokenKind.Preprocessor);
                    string word = text.Substring(j + 1, k - j - 1).Trim();
                    i = k;
                    directive = true;
                    defineLine = word == "define";
                    if (word == "include")
                    {
                        // <stdio.h> — как строка
                        int m = i;
                        while (m < end && (text[m] == ' ' || text[m] == '\t')) m++;
                        if (m < end && text[m] == '<')
                        {
                            int close = text.IndexOf('>', m, end - m);
                            int stop = close < 0 ? end : close + 1;
                            Add(output, m, stop - m, TokenKind.String);
                            i = stop;
                        }
                    }
                }
            }

            int newState = 0;
            while (i < end)
            {
                char c = text[i];
                if (c == ' ' || c == '\t') { i++; continue; }

                if (c == '/' && i + 1 < end && text[i + 1] == '/')
                {
                    Add(output, i, end - i, TokenKind.Comment);
                    i = end;
                    break;
                }
                if (c == '/' && i + 1 < end && text[i + 1] == '*')
                {
                    int close = IndexOf(text, "*/", i + 2, end);
                    if (close < 0)
                    {
                        Add(output, i, end - i, TokenKind.Comment);
                        newState |= InBlockComment;
                        i = end;
                        break;
                    }
                    Add(output, i, close + 2 - i, TokenKind.Comment);
                    i = close + 2;
                    continue;
                }
                if (c == '"' || c == '\'')
                {
                    int stop = SkipQuoted(text, i, end, c);
                    Add(output, i, stop - i, TokenKind.String);
                    Remember(ref prev, ref prev2, ref prevKind, ref prev2Kind, ref prevWord, ref prev2Word, "\"", TokenKind.String, false);
                    i = stop;
                    continue;
                }
                if (char.IsDigit(c) || (c == '.' && i + 1 < end && char.IsDigit(text[i + 1])))
                {
                    int stop = SkipNumber(text, i, end);
                    Add(output, i, stop - i, TokenKind.Number);
                    Remember(ref prev, ref prev2, ref prevKind, ref prev2Kind, ref prevWord, ref prev2Word, "0", TokenKind.Number, false);
                    i = stop;
                    continue;
                }
                if (IsIdentStart(c))
                {
                    int k = i + 1;
                    while (k < end && IsIdentChar(text[k])) k++;
                    string word = text.Substring(i, k - i);
                    // L"…", u8"…", u'…' — приставка входит в строку
                    if (k < end && (text[k] == '"' || text[k] == '\'') &&
                        (word == "L" || word == "u" || word == "U" || word == "u8"))
                    {
                        int stop = SkipQuoted(text, k, end, text[k]);
                        Add(output, i, stop - i, TokenKind.String);
                        i = stop;
                        continue;
                    }
                    TokenKind kind = TokenKind.Plain;
                    if (keywords.Contains(word)) kind = TokenKind.Keyword;
                    else if (types.Contains(word)) kind = TokenKind.Type;
                    else if (constants.Contains(word)) kind = TokenKind.Constant;
                    else if (directive && (word == "defined" || word == "__has_include")) kind = TokenKind.Preprocessor;
                    else
                    {
                        // имя перед «(» — функция: объявление (перед ним тип, или оно с начала строки, или #define)
                        // или вызов (всё остальное)
                        int n = k;
                        while (n < end && (text[n] == ' ' || text[n] == '\t')) n++;
                        if (n < end && text[n] == '(')
                        {
                            bool typeBefore = prevWord && (prevKind == TokenKind.Type || prevKind == TokenKind.Plain || declWords.Contains(prev));
                            bool pointerBefore = !prevWord && prev != null && prev.Trim('*').Length == 0 && prev2Word &&
                                                 (prev2Kind == TokenKind.Type || prev2Kind == TokenKind.Plain || declWords.Contains(prev2));
                            bool lineStart = prev == null && (directive ? defineLine : i == start);
                            kind = typeBefore || pointerBefore || lineStart ? TokenKind.FunctionDecl : TokenKind.Function;
                        }
                    }
                    Add(output, i, k - i, kind);
                    Remember(ref prev, ref prev2, ref prevKind, ref prev2Kind, ref prevWord, ref prev2Word, word, kind, true);
                    i = k;
                    continue;
                }

                // Знаки — подряд одним куском
                int s = i;
                while (i < end && !IsIdentChar(text[i]) && text[i] != ' ' && text[i] != '\t' &&
                       text[i] != '"' && text[i] != '\'' && !(text[i] == '/' && i + 1 < end && (text[i + 1] == '/' || text[i + 1] == '*')))
                {
                    if (text[i] == '.' && i + 1 < end && char.IsDigit(text[i + 1])) break;
                    i++;
                }
                if (i == s) i++;
                Add(output, s, i - s, TokenKind.Plain);
                Remember(ref prev, ref prev2, ref prevKind, ref prev2Kind, ref prevWord, ref prev2Word, text.Substring(s, i - s), TokenKind.Plain, false);
            }

            if (directive && (newState & InBlockComment) == 0 && EndsWithBackslash(text, start, end))
                newState |= InDirective;
            return newState;
        }

        static void Remember(ref string prev, ref string prev2, ref TokenKind prevKind, ref TokenKind prev2Kind,
                             ref bool prevWord, ref bool prev2Word, string text, TokenKind kind, bool word)
        {
            prev2 = prev; prev2Kind = prevKind; prev2Word = prevWord;
            prev = text; prevKind = kind; prevWord = word;
        }

        static void Add(List<Token> output, int start, int length, TokenKind kind)
        {
            if (output != null && length > 0) output.Add(new Token(start, length, kind));
        }

        static int SkipQuoted(string text, int i, int end, char quote)
        {
            int k = i + 1;
            while (k < end)
            {
                if (text[k] == '\\') { k += 2; continue; }
                if (text[k] == quote) return k + 1;
                k++;
            }
            return end;
        }

        static int SkipNumber(string text, int i, int end)
        {
            int k = i;
            if (text[k] == '0' && k + 1 < end && (text[k + 1] == 'x' || text[k + 1] == 'X' || text[k + 1] == 'b' || text[k + 1] == 'B'))
                k += 2;
            while (k < end)
            {
                char c = text[k];
                if (char.IsLetterOrDigit(c) || c == '.' || c == '_' || c == '\'') { k++; continue; }
                // 1e-5, 0x1p+3
                if ((c == '+' || c == '-') && (text[k - 1] == 'e' || text[k - 1] == 'E' || text[k - 1] == 'p' || text[k - 1] == 'P'))
                {
                    k++;
                    continue;
                }
                break;
            }
            return k;
        }

        static bool EndsWithBackslash(string text, int start, int end)
        {
            int k = end - 1;
            while (k >= start && (text[k] == ' ' || text[k] == '\t')) k--;
            return k >= start && text[k] == '\\';
        }

        static int IndexOf(string text, string what, int from, int end)
        {
            if (from >= end) return -1;
            int k = text.IndexOf(what, from, end - from, System.StringComparison.Ordinal);
            return k;
        }

        static bool IsIdentStart(char c) { return char.IsLetter(c) || c == '_' || c == '$'; }
        static bool IsIdentChar(char c) { return char.IsLetterOrDigit(c) || c == '_' || c == '$'; }
    }
}
