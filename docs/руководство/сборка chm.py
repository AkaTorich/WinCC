# -*- coding: utf-8 -*-
"""Собирает справку «Руководство WinCC.chm» из тех же глав, что и PDF.

Запуск:  python "сборка chm.py"
Главы → страницы HTML (глава — страница, разделы — якоря) → оглавление .hhc и указатель .hhk →
HTML Help Workshop (hhc.exe). Разбор глав — общий со сборка.py. Справка Windows читает
русский текст в кодировке 1251: страницы пишутся в ней, знаки вне её — числовыми ссылками.
"""
import html
import os
import re
import shutil
import subprocess
import tempfile

import сборка

ПАПКА = сборка.ПАПКА
КНИГА = os.path.join(ПАПКА, сборка.ЗАГЛАВИЕ + ".chm")
HHC = r"C:\Program Files (x86)\HTML Help Workshop\hhc.exe"

# рисунки — как есть, файлами справки, по имени
сборка.рисунок_для_печати = lambda путь: путь

CSS = """
body { font-family: Segoe UI, Tahoma, sans-serif; font-size: 10pt; color: #1d1d24; background: #ffffff;
       margin: 18px 26px; line-height: 1.45; }
h1 { font-size: 20pt; color: #14120a; margin: 0 0 4px 0; }
h2 { font-size: 13.5pt; color: #14120a; border-bottom: 2px solid #e0c800; padding-bottom: 2px; margin-top: 22px; }
h3 { font-size: 11pt; margin-top: 16px; }
.l { font-size: 9pt; letter-spacing: 2px; color: #8a7d00; font-weight: bold; }
.bar { height: 3px; width: 90px; background: #e0c800; margin: 6px 0 14px 0; }
p.lead { font-size: 11pt; }
code { font-family: Consolas, monospace; font-size: 9.5pt; background: #f2f0e4; padding: 0 2px; }
pre { font-family: Consolas, monospace; font-size: 9.5pt; background: #12101e; color: #f0f4ff;
      padding: 8px 10px; border-left: 3px solid #e0c800; }
table.t { border-collapse: collapse; margin: 8px 0; }
table.t th { background: #14120a; color: #fcee0a; text-align: left; padding: 4px 8px; font-weight: normal; }
table.t td { border-bottom: 1px solid #d8d6cc; padding: 4px 8px; vertical-align: top; }
.tip { background: #fdf9d6; border-left: 3px solid #e0c800; padding: 6px 10px; margin: 10px 0; }
figure { margin: 12px 0; }
figure img { max-width: 100%; border: 1px solid #c8c6bc; }
figcaption { font-size: 9pt; color: #55535e; margin-top: 3px; }
.part { background: #12101e; color: #f0f4ff; padding: 24px 28px; }
.part .t { font-size: 22pt; color: #fcee0a; }
ul.toc li { margin: 3px 0; }
"""

КОДИРОВКА = "cp1251"


def в_1251(текст):
    return текст.encode(КОДИРОВКА, "xmlcharrefreplace")


def страница(заголовок, тело):
    return ('<html><head><meta http-equiv="Content-Type" content="text/html; charset=windows-1251">'
            '<title>%s</title><link rel="stylesheet" href="book.css"></head><body>%s</body></html>'
            % (html.escape(заголовок), тело))


def якоря(тело):
    """Каждый раздел ## получает якорь r1, r2…; возвращает тело и [(имя раздела, якорь)]."""
    найдено = []

    def заменить(м):
        найдено.append((re.sub("<[^>]+>", "", html.unescape(м.group(1))), "r%d" % (len(найдено) + 1)))
        return '<h2><a name="%s"></a>%s</h2>' % (найдено[-1][1], м.group(1))

    return re.sub(r"<h2>(.*?)</h2>", заменить, тело), найдено


ИМЕНА_РИСУНКОВ = {}


def рисунки(тело, папка):
    """file:///…рисунки/имя.png → img/figN.png (справка не открывает рисунки с кириллицей в имени)."""
    def заменить(м):
        путь = м.group(1).replace("/", os.sep)
        if путь not in ИМЕНА_РИСУНКОВ:
            ИМЕНА_РИСУНКОВ[путь] = "fig%02d%s" % (len(ИМЕНА_РИСУНКОВ) + 1, os.path.splitext(путь)[1].lower())
            shutil.copyfile(путь, os.path.join(папка, "img", ИМЕНА_РИСУНКОВ[путь]))
        return 'src="img/%s"' % ИМЕНА_РИСУНКОВ[путь]

    return re.sub(r'src="file:///([^"]+)"', заменить, тело)


def пункт(имя, файл):
    return ('<LI><OBJECT type="text/sitemap"><param name="Name" value="%s"><param name="Local" value="%s"></OBJECT>\n'
            % (html.escape(имя, quote=True), файл))


def главное():
    if not os.path.exists(HHC):
        raise SystemExit("Нет HTML Help Workshop: " + HHC)
    все = сборка.главы()
    рабочая = tempfile.mkdtemp(prefix="wincc_chm_")
    os.makedirs(os.path.join(рабочая, "img"))
    with open(os.path.join(рабочая, "book.css"), "w", encoding="ascii") as ф:
        ф.write(CSS)

    файлы = ["index.htm", "book.css"]
    по_частям = {п[0]: п for п in сборка.ЧАСТИ}

    # первая страница: заглавие, «Об этой книге», содержание
    о_книге = сборка.страница_о_книге()
    содержание = ['<h2>Содержание</h2><ul class="toc">']
    for гл in все:
        if гл["н"] in по_частям:
            _, рим, имя, _ = по_частям[гл["н"]]
            содержание.append('</ul><h3>Часть %s. %s</h3><ul class="toc">' % (рим, html.escape(имя)))
        содержание.append('<li><a href="%02d.htm">%d. %s</a></li>' % (гл["н"], гл["н"], html.escape(гл["заголовок"])))
    содержание.append("</ul>")
    первая = ('<div class="l">%s</div><h1>%s</h1><div class="bar"></div>%s%s'
              % (html.escape(сборка.ПОДЗАГОЛОВОК.upper()), сборка.ЗАГЛАВИЕ,
                 рисунки(о_книге, рабочая), "".join(содержание)))
    with open(os.path.join(рабочая, "index.htm"), "wb") as ф:
        ф.write(в_1251(страница(сборка.ЗАГЛАВИЕ, первая)))

    оглавление = ['<UL>\n', пункт(сборка.ЗАГЛАВИЕ, "index.htm")]
    указатель = []
    открыта_часть = False
    for i, гл in enumerate(все):
        if гл["н"] in по_частям:
            _, рим, имя, суть = по_частям[гл["н"]]
            файл_части = "part%s.htm" % рим
            # страница части: имя, о чём она, её главы
            главы_части = []
            for г in все[i:]:
                if г is not гл and г["н"] in по_частям:
                    break
                главы_части.append('<li><a href="%02d.htm">%d. %s</a></li>' % (г["н"], г["н"], html.escape(г["заголовок"])))
            тело = ('<div class="part"><div class="l">ЧАСТЬ %s</div><div class="t">%s</div><p>%s</p></div>'
                    '<ul class="toc">%s</ul>' % (рим, html.escape(имя), html.escape(суть), "".join(главы_части)))
            with open(os.path.join(рабочая, файл_части), "wb") as ф:
                ф.write(в_1251(страница("Часть %s. %s" % (рим, имя), тело)))
            файлы.append(файл_части)
            if открыта_часть:
                оглавление.append("</UL>\n")
            оглавление.append(пункт("Часть %s. %s" % (рим, имя), файл_части))
            оглавление.append("<UL>\n")
            открыта_часть = True

        файл = "%02d.htm" % гл["н"]
        тело, разделы = якоря(рисунки(гл["тело"], рабочая))
        назад = '<a href="%02d.htm">&larr; %d. %s</a>' % (все[i - 1]["н"], все[i - 1]["н"], html.escape(все[i - 1]["заголовок"])) if i > 0 else ""
        вперёд = '<a href="%02d.htm">%d. %s &rarr;</a>' % (все[i + 1]["н"], все[i + 1]["н"], html.escape(все[i + 1]["заголовок"])) if i + 1 < len(все) else ""
        ход = '<p style="margin-top:28px;border-top:1px solid #d8d6cc;padding-top:6px">%s%s%s</p>' % (
            назад, " &nbsp;|&nbsp; " if назад and вперёд else "", вперёд)
        with open(os.path.join(рабочая, файл), "wb") as ф:
            ф.write(в_1251(страница("%d. %s" % (гл["н"], гл["заголовок"]),
                                     '<div class="l">ГЛАВА %d</div><h1>%s</h1><div class="bar"></div>%s%s'
                                     % (гл["н"], html.escape(гл["заголовок"]), тело, ход))))
        файлы.append(файл)
        оглавление.append(пункт("%d. %s" % (гл["н"], гл["заголовок"]), файл))
        указатель.append((гл["заголовок"], файл))
        if разделы:
            оглавление.append("<UL>\n")
            for имя, як in разделы:
                оглавление.append(пункт(имя, "%s#%s" % (файл, як)))
                указатель.append(("%s — %s" % (имя, гл["заголовок"]), "%s#%s" % (файл, як)))
            оглавление.append("</UL>\n")
    if открыта_часть:
        оглавление.append("</UL>\n")
    оглавление.append("</UL>\n")

    шапка = '<HTML><HEAD><meta http-equiv="Content-Type" content="text/html; charset=windows-1251"></HEAD><BODY>\n'
    with open(os.path.join(рабочая, "book.hhc"), "wb") as ф:
        # без «text/site properties»: с ним hhc.exe падает (HHC6000, HHC5007)
        ф.write(в_1251(шапка + "".join(оглавление) + "</BODY></HTML>\n"))
    указатель.sort(key=lambda п: п[0].lower())
    with open(os.path.join(рабочая, "book.hhk"), "wb") as ф:
        ф.write(в_1251(шапка + "<UL>\n" + "".join(пункт(и, ф_) for и, ф_ in указатель) + "</UL></BODY></HTML>\n"))

    картинки = ["img\\" + и for и in sorted(os.listdir(os.path.join(рабочая, "img")))]
    проект = "\r\n".join([
        "[OPTIONS]",
        "Compatibility=1.1 or later",
        "Compiled file=book.chm",
        "Contents file=book.hhc",
        "Index file=book.hhk",
        "Default Window=main",
        "Default topic=index.htm",
        "Display compile progress=No",
        "Full-text search=Yes",
        "Binary TOC=Yes",
        "Language=0x419 Russian",
        "Title=" + сборка.ЗАГЛАВИЕ,
        "",
        "[WINDOWS]",
        'main="%s","book.hhc","book.hhk","index.htm","index.htm",,,,,0x63520,320,0x10184e,[60,40,1180,820],,,,,,,0'
        % сборка.ЗАГЛАВИЕ,
        "",
        "[FILES]",
    ] + файлы + картинки) + "\r\n"
    with open(os.path.join(рабочая, "book.hhp"), "wb") as ф:
        ф.write(в_1251(проект))

    # hhc.exe возвращает 1 при удаче и 0 при ошибке
    итог = subprocess.run([HHC, os.path.join(рабочая, "book.hhp")], capture_output=True)
    вывод = итог.stdout.decode(КОДИРОВКА, "replace")
    готовый = os.path.join(рабочая, "book.chm")
    if итог.returncode != 1 or not os.path.exists(готовый):
        print(вывод)
        raise SystemExit("hhc.exe не собрал справку (код %d), файлы — в %s" % (итог.returncode, рабочая))
    for строка in вывод.splitlines():
        if "HHC" in строка and ("Warning" in строка or "Error" in строка):
            print(строка)
    shutil.copyfile(готовый, КНИГА)
    shutil.rmtree(рабочая, ignore_errors=True)
    shutil.rmtree(сборка.ПЕЧАТЬ_РИСУНКОВ, ignore_errors=True)
    print("Готово: %s — страниц %d, рисунков %d, %d КБ"
          % (КНИГА, len(файлы) - 1, len(картинки), os.path.getsize(КНИГА) // 1024))


if __name__ == "__main__":
    главное()
