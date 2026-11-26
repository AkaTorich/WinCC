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
    /// An open file: path, encoding, modified flag, its own editor and tab.
    /// Files are read as UTF-8 (or 1251 if they are not valid UTF-8) and written back in the same encoding, CRLF lines.
    /// </summary>
    public class SourceDocument
    {
        static readonly Encoding utf8 = new UTF8Encoding(false, true);
        static int untitledCounter;

        readonly TextBlock nameText = new TextBlock { VerticalAlignment = VerticalAlignment.Center };
        Button closeButton;
        bool modified;
        /// <summary>A fixed name given for an untitled file, or null — "untitled N.c" in the current language.</summary>
        string fixedUntitledName;
        int untitledNumber;

        public string Path { get; private set; }
        public Encoding Encoding { get; private set; }
        public CodeEditor Editor { get; private set; }
        public TabItem Tab { get; private set; }
        public string UntitledName { get { return fixedUntitledName ?? Loc.F("Untitled", untitledNumber); } }

        /// <summary>The cross on the tab was clicked.</summary>
        public event EventHandler CloseRequested;
        /// <summary>The name or the modified flag changed.</summary>
        public event EventHandler StateChanged;

        public SourceDocument(string untitledText, string untitledName = null)
        {
            fixedUntitledName = untitledName;
            if (untitledName == null) untitledNumber = ++untitledCounter;
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

        /// <summary>An untouched new empty file — it may be silently replaced by an opened one.</summary>
        public bool IsPristine { get { return IsUntitled && !IsModified; } }

        public void Save() { SaveAs(Path); }

        public void SaveAs(string path)
        {
            if (path == null) throw new InvalidOperationException("The file has no path");
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

            var close = closeButton = new Button
            {
                Content = new Icon { Kind = IconKind.Close, Width = 9, Height = 9 },
                Padding = new Thickness(2),
                Margin = new Thickness(5, 0, -4, 0),
                Visibility = Visibility.Hidden,     // the cross shows on the front tab and under the mouse
                VerticalAlignment = VerticalAlignment.Center,
                ToolTip = Loc.T("CloseTab"),
                Focusable = false
            };
            close.SetResourceReference(FrameworkElement.StyleProperty, "Theme.IconButton");
            close.Click += (s, e) => CloseRequested?.Invoke(this, EventArgs.Empty);

            var header = new StackPanel { Orientation = Orientation.Horizontal };
            header.Children.Add(new Icon { Kind = IconKind.Code, Width = 12, Height = 12, Margin = new Thickness(0, 0, 5, 0), VerticalAlignment = VerticalAlignment.Center });
            header.Children.Add(nameText);
            header.Children.Add(close);

            Tab = new TabItem { Header = header, Content = Editor, Tag = this };
            Action cross = () => close.Visibility = Tab.IsSelected || Tab.IsMouseOver ? Visibility.Visible : Visibility.Hidden;
            Tab.MouseEnter += (s, e) => cross();
            Tab.MouseLeave += (s, e) => cross();
            System.ComponentModel.DependencyPropertyDescriptor.FromProperty(TabItem.IsSelectedProperty, typeof(TabItem))
                .AddValueChanged(Tab, (s, e) => cross());
            // Middle click on the tab closes it
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
            Tab.ToolTip = Path ?? Loc.T("NotSavedYet");
            StateChanged?.Invoke(this, EventArgs.Empty);
        }

        /// <summary>The interface language changed: rename an untitled tab, refresh tooltips.</summary>
        public void RefreshTexts()
        {
            closeButton.ToolTip = Loc.T("CloseTab");
            UpdateHeader();
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

        /// <summary>Encoding name for the status bar.</summary>
        public string EncodingName
        {
            get
            {
                if (Encoding is UTF8Encoding) return Encoding.GetPreamble().Length > 0 ? Loc.T("Utf8Bom") : "UTF-8";
                if (Encoding is UnicodeEncoding) return "UTF-16";
                return "Windows-1251";
            }
        }
    }
}
