using System;
using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using ThemeLibrary;
using WinCC.Editor;

namespace WinCC.Documents
{
    /// <summary>
    /// Открытый файл: путь, кодировка, признак правки, свой редактор и вкладка.
    /// Файлы читаются как UTF-8 (если в нём не читаются — как 1251) и пишутся обратно в той же кодировке, строки — CRLF.
    /// </summary>
    public class SourceDocument
    {
        static readonly Encoding utf8 = new UTF8Encoding(false, true);
        static int untitledCounter;

        readonly TextBlock nameText = new TextBlock { VerticalAlignment = VerticalAlignment.Center };
        bool modified;

        public string Path { get; private set; }
        public Encoding Encoding { get; private set; }
        public CodeEditor Editor { get; private set; }
        public TabItem Tab { get; private set; }
        public string UntitledName { get; private set; }

        /// <summary>Щелчок по крестику на вкладке.</summary>
        public event EventHandler CloseRequested;
        /// <summary>Сменилось имя или признак правки.</summary>
        public event EventHandler StateChanged;

        public SourceDocument(string untitledText, string untitledName = null)
        {
            UntitledName = untitledName ?? "без имени " + (++untitledCounter) + ".c";
            Encoding = new UTF8Encoding(false);
            Build();
            Editor.Text = untitledText ?? "";
            UpdateHeader();
        }

        SourceDocument() { }

        public static SourceDocument Load(string path)
        {
            byte[] bytes = File.ReadAllBytes(path);
            Encoding enc;
            string text = Decode(bytes, out enc);
            var doc = new SourceDocument { Path = System.IO.Path.GetFullPath(path), Encoding = enc };
            doc.Build();
            doc.Editor.Text = NormalizeNewlines(text);
            doc.UpdateHeader();
            return doc;
        }

        public string Name { get { return Path != null ? System.IO.Path.GetFileName(Path) : UntitledName; } }
        public bool IsUntitled { get { return Path == null; } }

        public bool IsModified
        {
            get { return modified; }
            private set
            {
                if (modified == value) return;
                modified = value;
                UpdateHeader();
            }
        }

        /// <summary>Пустой новый файл, которого не трогали, — его можно молча заменить открытым.</summary>
        public bool IsPristine { get { return IsUntitled && !IsModified; } }

        public void Save() { SaveAs(Path); }

        public void SaveAs(string path)
        {
            if (path == null) throw new InvalidOperationException("У файла нет пути");
            string text = NormalizeNewlines(Editor.Text);
            byte[] bytes = Encoding.GetBytes(text);
            byte[] bom = Encoding.GetPreamble();
            using (var f = new FileStream(path, FileMode.Create, FileAccess.Write))
            {
                f.Write(bom, 0, bom.Length);
                f.Write(bytes, 0, bytes.Length);
            }
            Path = System.IO.Path.GetFullPath(path);
            IsModified = false;
            UpdateHeader();
        }

        public bool IsSamePath(string path)
        {
            return Path != null && string.Equals(Path, System.IO.Path.GetFullPath(path), StringComparison.OrdinalIgnoreCase);
        }

        void Build()
        {
            Editor = new CodeEditor();
            Editor.TextEdited += (s, e) => IsModified = true;

            var close = new Button
            {
                Content = new Icon { Kind = IconKind.Close, Width = 12, Height = 12 },
                Padding = new Thickness(3),
                Margin = new Thickness(8, 0, -4, 0),
                VerticalAlignment = VerticalAlignment.Center,
                ToolTip = "Закрыть вкладку",
                Focusable = false
            };
            close.SetResourceReference(FrameworkElement.StyleProperty, "Theme.IconButton");
            close.Click += (s, e) => CloseRequested?.Invoke(this, EventArgs.Empty);

            var header = new StackPanel { Orientation = Orientation.Horizontal };
            header.Children.Add(new Icon { Kind = IconKind.Code, Width = 14, Height = 14, Margin = new Thickness(0, 0, 6, 0), VerticalAlignment = VerticalAlignment.Center });
            header.Children.Add(nameText);
            header.Children.Add(close);

            Tab = new TabItem { Header = header, Content = Editor, Tag = this };
            // Средняя кнопка мыши по вкладке — закрыть
            Tab.MouseUp += (s, e) =>
            {
                if (e.ChangedButton == System.Windows.Input.MouseButton.Middle)
                {
                    e.Handled = true;
                    CloseRequested?.Invoke(this, EventArgs.Empty);
                }
            };
        }

        void UpdateHeader()
        {
            nameText.Text = Name + (IsModified ? " •" : "");
            Tab.ToolTip = Path ?? "Файл ещё не сохранён";
            StateChanged?.Invoke(this, EventArgs.Empty);
        }

        static string Decode(byte[] bytes, out Encoding enc)
        {
            if (bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            {
                enc = new UTF8Encoding(true);
                return Encoding.UTF8.GetString(bytes, 3, bytes.Length - 3);
            }
            if (bytes.Length >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
            {
                enc = new UnicodeEncoding(false, true);
                return Encoding.Unicode.GetString(bytes, 2, bytes.Length - 2);
            }
            try
            {
                string s = utf8.GetString(bytes);
                enc = new UTF8Encoding(false);
                return s;
            }
            catch (DecoderFallbackException)
            {
                enc = Encoding.GetEncoding(1251);
                return enc.GetString(bytes);
            }
        }

        static string NormalizeNewlines(string s)
        {
            return s.Replace("\r\n", "\n").Replace('\r', '\n').Replace("\n", "\r\n");
        }

        /// <summary>Название кодировки для строки состояния.</summary>
        public string EncodingName
        {
            get
            {
                if (Encoding is UTF8Encoding) return Encoding.GetPreamble().Length > 0 ? "UTF-8 с BOM" : "UTF-8";
                if (Encoding is UnicodeEncoding) return "UTF-16";
                return "Windows-1251";
            }
        }
    }
}
