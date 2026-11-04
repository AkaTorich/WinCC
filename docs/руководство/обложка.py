# -*- coding: utf-8 -*-
"""Обложка «Руководства WinCC» — в манере серии: ночной градиент, мягкие пятна света,
тонкая рамка, светлое заглавие с ореолом. Рисунок — окно редактора со строками кода
и светящимися фигурными скобками. Вызывается из сборка.py.
"""
import math
import os
import pymupdf

СВЕТЛЫЙ = os.path.join(os.environ["WINDIR"], "Fonts", "segoeuil.ttf")
ОБЫЧНЫЙ = os.path.join(os.environ["WINDIR"], "Fonts", "segoeui.ttf")
КУРСИВ = os.path.join(os.environ["WINDIR"], "Fonts", "georgiai.ttf")

БИРЮЗА = (0.16, 0.66, 0.79)
БЕЛЫЙ = (0.93, 0.96, 0.98)
ДЫМ = (0.58, 0.68, 0.74)


def смешать(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def фон(стр, наложить=True):
    верх, серёдка, низ = (0.030, 0.060, 0.090), (0.050, 0.120, 0.170), (0.012, 0.030, 0.050)
    w, h = стр.rect.width, стр.rect.height
    шагов = 240
    фигура = стр.new_shape()
    for i in range(шагов):
        t = i / (шагов - 1)
        цвет = смешать(верх, серёдка, t / 0.45) if t < 0.45 else смешать(серёдка, низ, (t - 0.45) / 0.55)
        фигура.draw_rect(pymupdf.Rect(0, h * i / шагов - 0.6, w, h * (i + 1) / шагов + 0.6))
        фигура.finish(color=None, fill=цвет)
    фигура.commit(overlay=наложить)


def свечение(стр, центр, радиус, цвет, сила=0.16, колец=36, наложить=True):
    x, y = центр
    фигура = стр.new_shape()
    for i in range(колец, 0, -1):
        r = радиус * i / колец
        фигура.draw_oval(pymupdf.Rect(x - r, y - r * 0.72, x + r, y + r * 0.72))
        фигура.finish(color=None, fill=цвет, fill_opacity=сила / колец * 1.6)
    фигура.commit(overlay=наложить)


def строка(стр, текст, шрифт, имя, кегль, y, цвет, разрядка=0.0, непрозр=1.0, x=None):
    ширина = шрифт.text_length(текст, кегль) + разрядка * кегль * (len(текст) - 1)
    if x is None:
        x = (стр.rect.width - ширина) / 2
    for знак in текст:
        стр.insert_text((x, y), знак, fontname=имя, fontsize=кегль, color=цвет, fill_opacity=непрозр)
        x += шрифт.text_length(знак, кегль) + разрядка * кегль
    return ширина


def заглавие(стр, текст, шрифт, имя, кегль, y):
    ширина = шрифт.text_length(текст, кегль)
    x0 = (стр.rect.width - ширина) / 2
    for сдвиг, непрозр in ((1.8, 0.10), (1.0, 0.14)):
        for dx, dy in ((-сдвиг, 0), (сдвиг, 0), (0, -сдвиг), (0, сдвиг)):
            стр.insert_text((x0 + dx, y + dy), текст, fontname=имя, fontsize=кегль, color=БИРЮЗА, fill_opacity=непрозр)
    стр.insert_text((x0, y), текст, fontname=имя, fontsize=кегль, color=БЕЛЫЙ)


def окно_кода(стр, cx, cy, w, h):
    """Окно редактора: ореол, рамка, заголовок с тремя точками, строки кода, скобки { }."""
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    for расширение, непрозр in ((12, 0.04), (7, 0.07), (3, 0.11)):
        ф = стр.new_shape()
        ф.draw_rect(pymupdf.Rect(x0 - расширение, y0 - расширение, x1 + расширение, y1 + расширение), radius=0.06)
        ф.finish(color=None, fill=БИРЮЗА, fill_opacity=непрозр)
        ф.commit()
    ф = стр.new_shape()
    ф.draw_rect(pymupdf.Rect(x0, y0, x1, y1), radius=0.05)
    ф.finish(color=БИРЮЗА, fill=(0.035, 0.085, 0.125), width=1.2)
    ф.commit()
    ф = стр.new_shape()
    ф.draw_line(pymupdf.Point(x0, y0 + 22), pymupdf.Point(x1, y0 + 22))
    ф.finish(color=БИРЮЗА, width=0.7, stroke_opacity=0.6)
    ф.commit()
    for i, цвет in enumerate(((0.95, 0.45, 0.40), (0.95, 0.78, 0.35), (0.40, 0.85, 0.55))):
        ф = стр.new_shape()
        ф.draw_circle(pymupdf.Point(x0 + 14 + i * 12, y0 + 11), 3.2)
        ф.finish(color=None, fill=цвет, fill_opacity=0.85)
        ф.commit()
    # строки кода: отступ и длина каждой
    строки = [(0, 0.46, 1), (0, 0.30, 2), (0, 0.0, 0), (0, 0.55, 1), (0, 0.06, 0), (1, 0.62, 2), (1, 0.48, 1),
              (1, 0.70, 2), (2, 0.40, 1), (2, 0.52, 2), (1, 0.12, 0), (1, 0.58, 1), (0, 0.06, 0)]
    цвета = {0: БЕЛЫЙ, 1: БИРЮЗА, 2: (0.62, 0.55, 0.95)}
    y = y0 + 40
    for отступ, длина, род in строки:
        if длина:
            xs = x0 + 18 + отступ * 18
            ф = стр.new_shape()
            ф.draw_rect(pymupdf.Rect(xs, y - 2.6, xs + (w - 60) * длина, y + 2.6), radius=0.5)
            ф.finish(color=None, fill=цвета[род], fill_opacity=0.55 if род else 0.38)
            ф.commit()
        y += 13.5
    # курсор
    ф = стр.new_shape()
    ф.draw_rect(pymupdf.Rect(x0 + 18 + 18 * 1 + (w - 60) * 0.58 + 4, y - 27 - 6, x0 + 18 + 18 + (w - 60) * 0.58 + 6, y - 27 + 6))
    ф.finish(color=None, fill=БЕЛЫЙ, fill_opacity=0.9)
    ф.commit()


def скобка(стр, текст, шрифт, имя, кегль, x, y):
    for сдвиг, непрозр in ((3.0, 0.07), (1.8, 0.12), (0.9, 0.18)):
        for dx, dy in ((-сдвиг, 0), (сдвиг, 0), (0, -сдвиг), (0, сдвиг)):
            стр.insert_text((x + dx, y + dy), текст, fontname=имя, fontsize=кегль, color=БИРЮЗА, fill_opacity=непрозр)
    стр.insert_text((x, y), текст, fontname=имя, fontsize=кегль, color=БЕЛЫЙ, fill_opacity=0.92)


def обложка(док, глав=26):
    стр = док.new_page(0, width=док[0].rect.width, height=док[0].rect.height)
    W, H = стр.rect.width, стр.rect.height
    фон(стр)
    свечение(стр, (297, 250), 300, (0.12, 0.55, 0.75), сила=0.20)
    свечение(стр, (450, 700), 220, (0.20, 0.35, 0.80), сила=0.10)
    свечение(стр, (130, 620), 160, (0.10, 0.70, 0.55), сила=0.07)

    рамка = стр.new_shape()
    рамка.draw_rect(pymupdf.Rect(30, 30, W - 30, H - 30))
    рамка.finish(color=БИРЮЗА, width=0.7, stroke_opacity=0.45)
    рамка.commit()

    стр.insert_font(fontname="light", fontfile=СВЕТЛЫЙ)
    стр.insert_font(fontname="reg", fontfile=ОБЫЧНЫЙ)
    стр.insert_font(fontname="ital", fontfile=КУРСИВ)
    ш_light = pymupdf.Font(fontfile=СВЕТЛЫЙ)
    ш_reg = pymupdf.Font(fontfile=ОБЫЧНЫЙ)
    ш_ital = pymupdf.Font(fontfile=КУРСИВ)

    заглавие(стр, "WinCC", ш_light, "light", 84, 230)
    строка(стр, "РУКОВОДСТВО", ш_reg, "reg", 15, 272, БИРЮЗА, разрядка=0.42)

    черта = стр.new_shape()
    черта.draw_line(pymupdf.Point(W / 2 - 60, 300), pymupdf.Point(W / 2 + 60, 300))
    черта.finish(color=БИРЮЗА, width=1.4)
    черта.commit()
    строка(стр, "Компилятор Си для Windows", ш_ital, "ital", 18, 340, БЕЛЫЙ)
    строка(стр, "и его библиотеки", ш_ital, "ital", 18, 366, БЕЛЫЙ)

    # рисунок: окно редактора с кодом между светящимися скобками
    окно_кода(стр, W / 2, 585, 250, 220)
    скобка(стр, "{", ш_light, "light", 120, W / 2 - 205, 630)
    скобка(стр, "}", ш_light, "light", 120, W / 2 + 160, 630)
    return стр
