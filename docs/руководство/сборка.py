# -*- coding: utf-8 -*-
"""Собирает «Руководство WinCC» из глав в папке «главы».

Запуск:  python сборка.py
Главы → HTML → печать в PDF браузером → доводка PyMuPDF: тёмные страницы частей,
колонтитулы, номера страниц в оглавлении, ссылки, закладки, обложка.
Рисунки — из папки «рисунки» (строка главы «@ имя.png | подпись»).
"""
import html
import os
import re
import shutil
import subprocess
import tempfile

import pymupdf

import обложка

ПАПКА = os.path.dirname(os.path.abspath(__file__))
ГЛАВЫ = os.path.join(ПАПКА, "главы")
РИСУНКИ = os.path.join(ПАПКА, "рисунки")
ЗАГЛАВИЕ = "Руководство WinCC"
ПОДЗАГОЛОВОК = "Компилятор Си для Windows и его библиотеки"
ПОДПОДЗАГОЛОВОК = "Язык, окна, оформление, сеть"
КНИГА = os.path.join(ПАПКА, ЗАГЛАВИЕ + ".pdf")

ЧАСТИ = [
    (1, "I", "Начало", "Что такое WinCC, как его поставить, первая программа, окно редактора и командная строка"),
    (5, "II", "Язык и библиотека Си", "Какой Си понимает компилятор, что умеет его библиотека, кириллица и вставки на ассемблере"),
    (9, "III", "Программы для Windows", "Окна и сообщения, свои DLL, функции любых DLL системы, рисование GDI и GDI+"),
    (13, "IV", "Оформление окон WUI", "Шестьдесят одна тема ThemeLibrary для программ на Си: окно, раскладка, элементы, рисование"),
    (18, "V", "Сеть", "HTTP и HTTPS, соединения TCP, пакеты UDP, свой веб-сервер и то, что лежит под ними"),
    (22, "VI", "Приложения", "Справочники: ключи, сообщения компилятора, заголовки, все функции WUI и сети, ограничения"),
]

К_ЧАСТЬ, К_ГЛАВА, К_РАЗДЕЛ = 34.0, 24.0, 13.5

# рисунки для печати: не шире 1300 точек; тяжёлые (фактуры тем) — на белом листе в JPEG
ПЕЧАТЬ_РИСУНКОВ = tempfile.mkdtemp(prefix="wincc_book_fig_")
ШИРЕ_НЕ_НАДО, ТЯЖЁЛЫЙ = 1300, 300 * 1024


def рисунок_для_печати(путь):
    pix = pymupdf.Pixmap(путь)
    if pix.width <= ШИРЕ_НЕ_НАДО and os.path.getsize(путь) <= ТЯЖЁЛЫЙ:
        return путь
    доля = min(1.0, ШИРЕ_НЕ_НАДО / pix.width)
    w, h = pix.width * доля, pix.height * доля
    лист = pymupdf.open()
    стр = лист.new_page(width=w, height=h)
    стр.draw_rect(стр.rect, color=None, fill=(1, 1, 1))
    стр.insert_image(стр.rect, filename=путь)
    готовый = стр.get_pixmap(alpha=False)
    куда = os.path.join(ПЕЧАТЬ_РИСУНКОВ, os.path.splitext(os.path.basename(путь))[0] + ".jpg")
    готовый.save(куда, jpg_quality=88)
    лист.close()
    return куда


# ─────────────────────────── разметка глав ───────────────────────────

def строчный(текст):
    """Абзацный текст: экранирование, `код` в обратных кавычках."""
    куски = re.split(r"`([^`]+)`", текст)
    вывод = []
    for i, к in enumerate(куски):
        вывод.append("<code>%s</code>" % html.escape(к) if i % 2 else html.escape(к))
    return "".join(вывод).replace(" — ", "&nbsp;— ")


def прочесть_главу(путь):
    """Возвращает (заголовок, html тела, [разделы], слов)."""
    with open(путь, encoding="utf-8-sig") as ф:
        строки = ф.read().splitlines()
    заголовок, тело, разделы = None, [], []
    абзац, таблица, список, код = [], [], [], None
    тип_списка = None
    слов = 0
    первый = [True]

    def сбросить_абзац():
        if абзац:
            класс = ' class="lead"' if первый[0] and not разделы else ""
            тело.append("<p%s>%s</p>" % (класс, строчный(" ".join(абзац))))
            первый[0] = False
            абзац.clear()

    def сбросить_таблицу():
        if таблица:
            тело.append('<table class="t">%s</table>' % "".join(таблица))
            таблица.clear()

    def сбросить_список():
        nonlocal тип_списка
        if список:
            тело.append("<%s>%s</%s>" % (тип_списка, "".join(список), тип_списка))
            список.clear()
        тип_списка = None

    def сбросить_всё():
        сбросить_абзац(); сбросить_таблицу(); сбросить_список()

    for сырая in строки:
        if код is not None:
            if сырая.strip() == "```":
                тело.append('<pre>%s</pre>' % html.escape("\n".join(код)))
                код = None
            else:
                код.append(сырая.rstrip())
            continue
        с = сырая.strip()
        if not с:
            сбросить_абзац(); сбросить_список()
            continue
        if с.startswith("```"):
            сбросить_всё()
            код = []
            continue
        if с.startswith("# ") and заголовок is None:
            заголовок = с[2:].strip()
            continue
        if с.startswith(":"):
            сбросить_абзац(); сбросить_список()
            шапка = с.startswith(":=")
            поля = [п.strip() for п in с.lstrip(":=").split("|")]
            тег = "th" if шапка else "td"
            таблица.append("<tr>%s</tr>" % "".join("<%s>%s</%s>" % (тег, строчный(п), тег) for п in поля))
            слов += sum(len(п.split()) for п in поля)
            continue
        сбросить_таблицу()
        м = re.match(r"(\d+)\.\s+(.*)", с)
        if м or с.startswith("- "):
            сбросить_абзац()
            новый = "ol" if м else "ul"
            if тип_списка != новый:
                сбросить_список()
                тип_списка = новый
            текст = м.group(2) if м else с[2:]
            список.append("<li>%s</li>" % строчный(текст))
            слов += len(текст.split())
            continue
        сбросить_список()
        if с.startswith("### "):
            сбросить_абзац()
            тело.append("<h3>%s</h3>" % строчный(с[4:]))
        elif с.startswith("## "):
            сбросить_абзац()
            разделы.append(с[3:].strip())
            тело.append("<h2>%s</h2>" % строчный(с[3:]))
        elif с.startswith("! "):
            сбросить_абзац()
            тело.append('<div class="tip">%s</div>' % строчный(с[2:]))
            слов += len(с.split())
        elif с.startswith("@ "):
            сбросить_абзац()
            файл, _, подпись = с[2:].partition("|")
            путь = os.path.join(РИСУНКИ, файл.strip())
            if not os.path.exists(путь):
                raise SystemExit("Нет рисунка %s (глава %s)" % (файл.strip(), os.path.basename(путь)))
            тело.append('<figure><img src="%s"><figcaption>%s</figcaption></figure>'
                        % ("file:///" + рисунок_для_печати(путь).replace("\\", "/"), строчный(подпись.strip())))
        else:
            абзац.append(с)
            слов += len(с.split())
    сбросить_всё()
    # рисунки подряд — по два в ряд
    собранное, ряд = [], []
    for кусок in тело + [""]:
        if кусок.startswith("<figure"):
            ряд.append(кусок)
            continue
        if len(ряд) > 1:
            собранное.append('<div class="figs">%s</div>' % "".join(ряд))
        else:
            собранное.extend(ряд)
        ряд = []
        if кусок:
            собранное.append(кусок)
    return заголовок or os.path.basename(путь), "\n".join(собранное), разделы, слов


def главы():
    файлы = sorted(ф for ф in os.listdir(ГЛАВЫ) if ф.endswith(".txt"))
    найдено = []
    for ф in файлы:
        м = re.match(r"(\d+)", ф)
        if not м or int(м.group(1)) == 0:
            continue
        заголовок, тело, разделы, слов = прочесть_главу(os.path.join(ГЛАВЫ, ф))
        найдено.append({"н": int(м.group(1)), "заголовок": заголовок, "тело": тело,
                        "разделы": разделы, "слов": слов})
    return найдено


# ─────────────────────────── HTML ───────────────────────────

CSS = r"""
@page { size: A4; margin: 21mm 18mm 20mm 18mm; }
* { -webkit-print-color-adjust: exact; print-color-adjust: exact; box-sizing: border-box; }
html, body { margin: 0; padding: 0; }
body { font-family: Georgia, serif; font-size: 10.5pt; line-height: 1.5; color: #121a1f;
       text-align: justify; hyphens: auto; }
p { margin: 0 0 6pt 0; orphans: 2; widows: 2; overflow-wrap: anywhere; }
p.lead { font-size: 11.5pt; font-style: italic; color: #1f3340; }

.title { height: 250mm; display: flex; flex-direction: column; align-items: center;
         justify-content: center; text-align: center; break-after: page; }
.title .t { font-family: "Segoe UI Light", "Segoe UI"; font-weight: 300; font-size: 30pt;
            color: #122f45; letter-spacing: 0.5pt; }
.title .bar { width: 48mm; height: 1.6mm; background: #2aa7c9; margin: 8mm 0 7mm; border-radius: 1mm; }
.title .s { font-style: italic; color: #1d6f8a; font-size: 12.5pt; line-height: 1.6; }

.part { height: 250mm; display: flex; flex-direction: column; justify-content: center;
        align-items: center; text-align: center; break-before: page; break-after: page; }
.part .l { font-family: "Segoe UI"; font-size: 10pt; letter-spacing: 5pt; color: #8fd8ea; }
.part .t { font-family: "Segoe UI Light", "Segoe UI"; font-weight: 300; font-size: 34pt;
           color: #eef7fb; margin: 6mm 0 7mm; }
.part .bar { width: 36mm; height: 0.5mm; background: #2aa7c9; margin-bottom: 7mm; }
.part .s { font-style: italic; color: #b3cfd9; font-size: 12pt; max-width: 120mm; line-height: 1.6; }

.chapter { break-before: page; }
.chapter .l { font-family: "Segoe UI"; font-size: 9pt; letter-spacing: 3.5pt; color: #1f8fb0; margin-top: 4mm; }
.chapter h1 { font-family: "Segoe UI Light", "Segoe UI"; font-weight: 300; font-size: 24pt;
              line-height: 1.2; color: #122f45; margin: 2mm 0 4mm; text-align: left; }
.chapter .bar { width: 22mm; height: 1.1mm; background: #2aa7c9; border-radius: 1mm; margin-bottom: 7mm; }

h2 { font-family: "Segoe UI Semibold", "Segoe UI"; font-weight: 600; font-size: 13.5pt; color: #175f78;
     margin: 14pt 0 6pt; padding-top: 8pt; border-top: 0.6pt solid #d8e9ef; text-align: left; break-after: avoid; }
h3 { font-family: "Segoe UI Semibold", "Segoe UI"; font-weight: 600; font-size: 11.5pt; color: #1f7088;
     margin: 10pt 0 4pt; break-after: avoid; text-align: left; }

table.t { width: 100%; border-collapse: collapse; margin: 4pt 0 10pt; text-align: left; font-size: 9.8pt; }
table.t tr { break-inside: avoid; }
table.t th { font-family: "Segoe UI Semibold", "Segoe UI"; font-weight: 600; font-size: 8.8pt; color: #175f78;
             padding: 3pt 6pt 3pt 0; border-bottom: 1pt solid #2aa7c9; vertical-align: bottom; }
table.t td { overflow-wrap: anywhere; vertical-align: top; padding: 3pt 6pt 3pt 0; border-bottom: 0.5pt solid #e3eef2;
             line-height: 1.4; }
table.t td:first-child { font-family: "Segoe UI Semibold", "Segoe UI"; font-size: 9.2pt; color: #163d4f; overflow-wrap: anywhere; }

pre { font-family: Consolas, monospace; font-size: 8.6pt; line-height: 1.38; color: #e6f1f5; background: #10222c;
      border-left: 2.2pt solid #2aa7c9; border-radius: 2pt; padding: 7pt 9pt; margin: 5pt 0 10pt; white-space: pre-wrap;
      overflow-wrap: anywhere; text-align: left; }
code { font-family: Consolas, monospace; font-size: 9pt; color: #0f4a5e; background: #eaf4f8;
       border-radius: 2.5pt; padding: 0.4pt 3pt; }
ol, ul { margin: 2pt 0 8pt 0; padding-left: 18pt; }
li { margin: 0 0 3pt 0; text-align: left; }
.tip { margin: 6pt 0 10pt; padding: 7pt 10pt 7pt 12pt; background: #eef6f9; border-left: 2.2pt solid #2aa7c9;
       font-size: 10pt; line-height: 1.5; break-inside: avoid; color: #1a2a33; }

figure { margin: 6pt 0 12pt; text-align: center; break-inside: avoid; }
figure img { max-width: 100%; max-height: 120mm; }
.figs { display: flex; flex-wrap: wrap; justify-content: space-between; margin: 6pt 0 8pt; }
.figs figure { width: 48.5%; margin: 0 0 8pt; }
.figs img { max-height: 80mm; }
figcaption { font-style: italic; font-size: 9.5pt; color: #1d6f8a; margin-top: 3pt; text-align: center; }

.toc { break-before: page; }
.toc h1, .about h1 { font-family: "Segoe UI Light", "Segoe UI"; font-weight: 300; font-size: 22pt; color: #122f45;
                     margin: 4mm 0 8mm; }
.toc .pt { font-family: "Segoe UI Semibold", "Segoe UI"; font-size: 10.5pt; color: #175f78; margin: 9pt 0 3pt; display: flex; }
.toc .ch { display: flex; font-size: 10.5pt; margin: 2.2pt 0; }
.toc .n { width: 9mm; color: #6f8d99; font-family: "Segoe UI"; font-size: 9.5pt; }
.toc .t { flex: none; }
.toc .dots { flex: 1; border-bottom: 0.7pt dotted #9cc3d0; margin: 0 4pt 3.5pt; }
.toc .p { width: 9mm; text-align: right; font-family: "Segoe UI"; font-size: 9.5pt; color: #175f78; }
.about { break-before: page; }
"""


def страница_о_книге():
    путь = os.path.join(ГЛАВЫ, "00 Об этой книге.txt")
    if not os.path.exists(путь):
        return ""
    заголовок, тело, _, _ = прочесть_главу(путь)
    return '<section class="about"><h1>%s</h1>%s</section>' % (html.escape(заголовок), тело)


def собрать_html(все, номера):
    части = ['<div class="title"><div class="t">%s</div><div class="bar"></div>'
             '<div class="s">%s<br>%s</div></div>'
             % (ЗАГЛАВИЕ, ПОДЗАГОЛОВОК, ПОДПОДЗАГОЛОВОК), страница_о_книге()]
    toc = ['<section class="toc"><h1>Содержание</h1>']
    по_частям = {п[0]: п for п in ЧАСТИ}
    for гл in все:
        if гл["н"] in по_частям:
            _, рим, имя, _ = по_частям[гл["н"]]
            toc.append('<div class="pt"><span class="t">Часть %s. %s</span><span class="dots"></span>'
                       '<span class="p">%s</span></div>' % (рим, имя, номера.get(("часть", рим), "")))
        toc.append('<div class="ch"><span class="n">%d</span><span class="t">%s</span><span class="dots"></span>'
                   '<span class="p">%s</span></div>'
                   % (гл["н"], html.escape(гл["заголовок"]), номера.get(("глава", гл["н"]), "")))
    toc.append("</section>")
    части.append("".join(toc))
    for гл in все:
        if гл["н"] in по_частям:
            _, рим, имя, суть = по_частям[гл["н"]]
            части.append('<section class="part"><div class="l">ЧАСТЬ %s</div><div class="t">%s</div>'
                         '<div class="bar"></div><div class="s">%s</div></section>' % (рим, имя, суть))
        части.append('<section class="chapter"><div class="l">ГЛАВА %d</div><h1>%s</h1><div class="bar"></div>%s</section>'
                     % (гл["н"], html.escape(гл["заголовок"]), гл["тело"]))
    return ('<!DOCTYPE html><html lang="ru"><head><meta charset="utf-8"><title>%s</title><style>%s</style>'
            '</head><body>%s</body></html>' % (ЗАГЛАВИЕ, CSS, "\n".join(части)))


# ─────────────────────────── печать ───────────────────────────

def браузер():
    for путь in (r"C:\Program Files\Google\Chrome\Application\chrome.exe",
                 r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
                 os.path.expandvars(r"%LOCALAPPDATA%\Google\Chrome\Application\chrome.exe"),
                 r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
                 r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"):
        if os.path.exists(путь):
            return путь
    raise SystemExit("Не найден ни Chrome, ни Edge")


def напечатать(разметка, рабочая):
    вход = os.path.join(рабочая, "book.html")
    выход = os.path.join(рабочая, "book.pdf")
    with open(вход, "w", encoding="utf-8") as ф:
        ф.write(разметка)
    subprocess.run([браузер(), "--headless=new", "--disable-gpu", "--no-pdf-header-footer",
                    "--print-to-pdf=" + выход, "file:///" + вход.replace("\\", "/")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=900)
    return выход


# ─────────────────────────── разметка PDF ───────────────────────────

def кегли(стр):
    найдено = []
    for блок in стр.get_text("dict")["blocks"]:
        for строка in блок.get("lines", []):
            for кусок in строка["spans"]:
                if кусок["text"].strip():
                    найдено.append((round(кусок["size"] * 2) / 2, кусок["text"].strip(), кусок["bbox"][1]))
    return найдено


def карта(док):
    части, главы_, разделы = [], [], []
    for н in range(док.page_count):
        for к, т, y in кегли(док[н]):
            if к == К_ЧАСТЬ:
                части.append(н)
            elif к == К_ГЛАВА:
                if not главы_ or главы_[-1][0] != н:
                    главы_.append((н, т))
            elif к == К_РАЗДЕЛ:
                разделы.append((н, т, y))
    return sorted(set(части)), [н for н, _ in главы_], разделы


# ─────────────────────────── доводка ───────────────────────────

GEORGIA = os.path.join(os.environ["WINDIR"], "Fonts", "georgia.ttf")
SEGOE = os.path.join(os.environ["WINDIR"], "Fonts", "segoeui.ttf")


def тёмная_страница(стр):
    обложка.фон(стр, наложить=False)
    обложка.свечение(стр, (297, 380), 280, обложка.БИРЮЗА, сила=0.14, наложить=False)
    рамка = стр.new_shape()
    рамка.draw_rect(pymupdf.Rect(30, 30, стр.rect.width - 30, стр.rect.height - 30))
    рамка.finish(color=обложка.БИРЮЗА, width=0.7, stroke_opacity=0.4)
    рамка.commit()


def колонтитулы(док, все, части, главы_):
    ш_seg = pymupdf.Font(fontfile=SEGOE)
    ш_geo = pymupdf.Font(fontfile=GEORGIA)
    первая_глава = главы_[0]
    for н in range(док.page_count):
        стр = док[н]
        if н in части or н == 0:
            continue
        w, h = стр.rect.width, стр.rect.height
        номер = str(н + 2)
        стр.insert_font(fontname="geo", fontfile=GEORGIA)
        стр.insert_font(fontname="seg", fontfile=SEGOE)
        x = (w - ш_geo.text_length(номер, 9)) / 2
        стр.insert_text((x, h - 28), номер, fontname="geo", fontsize=9, color=(0.35, 0.45, 0.52))
        if н < первая_глава or н in главы_:
            continue
        текущая = max(i for i, g in enumerate(главы_) if g <= н)
        шапка = все[текущая]["заголовок"].upper()
        разрядка = 1.1
        ширина = ш_seg.text_length(шапка, 6.8) + разрядка * (len(шапка) - 1)
        x = (w - ширина) / 2
        for б in шапка:
            стр.insert_text((x, 36), б, fontname="seg", fontsize=6.8, color=(0.40, 0.52, 0.60))
            x += ш_seg.text_length(б, 6.8) + разрядка
        черта = стр.new_shape()
        черта.draw_line(pymupdf.Point(w / 2 - 14, 43), pymupdf.Point(w / 2 + 14, 43))
        черта.finish(color=обложка.БИРЮЗА, width=0.5)
        черта.commit()


def ссылки_оглавления(док, все, части, главы_):
    цели, к_части = [], 0
    по_частям = {п[0]: п for п in ЧАСТИ}
    for i, гл in enumerate(все):
        if гл["н"] in по_частям:
            _, рим, имя, _ = по_частям[гл["н"]]
            цели.append(("Часть %s. %s" % (рим, имя), части[к_части] + 1))
            к_части += 1
        цели.append((гл["заголовок"], главы_[i] + 1))
    страницы = []
    for н in range(1, min(8, док.page_count)):
        if "Содержание" in док[н].get_text():
            страницы = [н, н + 1]
            break
    for текст, цель in цели:
        for н in страницы:
            места = док[н].search_for(текст)
            if места:
                r = места[0]
                док[н].insert_link({"kind": pymupdf.LINK_GOTO, "from": pymupdf.Rect(60, r.y0 - 1, док[н].rect.width - 50, r.y1 + 1),
                                    "page": цель, "to": pymupdf.Point(0, 0)})
                break


def закладки(док, все, части, главы_, разделы):
    по_частям = {п[0]: п for п in ЧАСТИ}
    toc = [[1, "Обложка", 1], [1, "Титул", 2]]
    about = next((н for н in range(1, 6) if "Об этой книге" in док[н].get_text()), None)
    if about is not None:
        toc.append([1, "Об этой книге", about + 2])
    oглав = next((н for н in range(1, 8) if "Содержание" in док[н].get_text()), None)
    if oглав is not None:
        toc.append([1, "Содержание", oглав + 2])
    к_части = 0
    for i, гл in enumerate(все):
        if гл["н"] in по_частям:
            _, рим, имя, _ = по_частям[гл["н"]]
            toc.append([1, "Часть %s. %s" % (рим, имя), части[к_части] + 2])
            к_части += 1
        начало = главы_[i]
        конец = главы_[i + 1] if i + 1 < len(главы_) else 10 ** 6
        toc.append([2, "%d. %s" % (гл["н"], гл["заголовок"]), начало + 2])
        for н, т, _ in разделы:
            if начало <= н < конец:
                toc.append([3, т, н + 2])
    return toc


def главное():
    все = главы()
    if not все:
        raise SystemExit("В папке «главы» нет глав")
    рабочая = tempfile.mkdtemp(prefix="wincc_book_")
    номера = {}
    for проход in range(3):
        pdf = напечатать(собрать_html(все, номера), рабочая)
        док = pymupdf.open(pdf)
        части, главы_, разделы = карта(док)
        if len(главы_) != len(все) or len(части) != len(ЧАСТИ):
            print("Внимание: найдено глав %d из %d, частей %d из %d" % (len(главы_), len(все), len(части), len(ЧАСТИ)))
        новые = {}
        for i, гл in enumerate(все):
            if i < len(главы_):
                новые[("глава", гл["н"])] = главы_[i] + 2
        for к, п in enumerate(ЧАСТИ):
            if к < len(части):
                новые[("часть", п[1])] = части[к] + 2
        if новые == номера or проход == 2:
            break
        номера = новые
        док.close()

    for н in части:
        тёмная_страница(док[н])
    колонтитулы(док, все, части, главы_)
    toc = закладки(док, все, части, главы_, разделы)
    обложка.обложка(док, глав=len(все))
    ссылки_оглавления(док, все, части, главы_)
    док.set_toc(toc)
    док.set_metadata({"title": ЗАГЛАВИЕ, "author": "", "subject": ПОДЗАГОЛОВОК + ". " + ПОДПОДЗАГОЛОВОК,
                      "keywords": "WinCC, компилятор Си, Windows, WUI, сеть, WinHTTP, сокеты, GDI+"})
    врем = os.path.join(рабочая, "final.pdf")
    док.save(врем, garbage=3, deflate=True)
    док.close()
    shutil.copyfile(врем, КНИГА)
    shutil.copyfile(os.path.join(рабочая, "book.html"), os.path.join(ПАПКА, "книга.html"))
    shutil.rmtree(рабочая, ignore_errors=True)
    shutil.rmtree(ПЕЧАТЬ_РИСУНКОВ, ignore_errors=True)
    итог = pymupdf.open(КНИГА)
    print("Готово: %s — страниц %d, глав %d, слов %d" % (КНИГА, итог.page_count, len(все), sum(г["слов"] for г in все)))
    for г in все:
        print("  %2d  %-60s %6d слов" % (г["н"], г["заголовок"][:60], г["слов"]))


if __name__ == "__main__":
    главное()
