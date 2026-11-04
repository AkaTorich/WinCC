# -*- coding: utf-8 -*-
"""Собирает каждый пример из глав руководства компилятором wcc.exe (корень WinCC).

Запуск:  python "проверка примеров.py" [номер главы ...] [--размеры]
         (--размеры — показать и удачные сборки: строки «Готово: … байт», чтобы сверить их с книгой)

Блок кода — пример на Си, если в нём есть main, wmain, WinMain или wWinMain: он
собирается сам по себе. Программа из нескольких файлов пишется блоками, у которых
первая строка «// файл: имя.c» (или .h, .asm): такие блоки ложатся в одну папку
главы, а строки «// сборка: ключи wcc» в любом из них — команды сборки, они идут
по порядку после того, как все файлы главы записаны. Остальные блоки (куски кода,
вывод программ, командные строки) не собираются.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ПАПКА = os.path.dirname(os.path.abspath(__file__))
ГЛАВЫ = os.path.join(ПАПКА, "главы")
# компилятор — из корня WinCC; другой (например, собранный в отдельную папку) — переменной WINCC_WCC
WCC = os.environ.get("WINCC_WCC") or os.path.normpath(os.path.join(ПАПКА, "..", "..", "wcc.exe"))


def блоки(путь):
    with open(путь, encoding="utf-8-sig") as ф:
        строки = ф.read().splitlines()
    найдено, текущий, начало = [], None, 0
    for н, с in enumerate(строки, 1):
        if с.strip().startswith("```"):
            if текущий is None:
                текущий, начало = [], н
            else:
                найдено.append((начало, "\n".join(текущий) + "\n"))
                текущий = None
        elif текущий is not None:
            текущий.append(с)
    return найдено


def собрать(папка, ключи):
    р = subprocess.run([WCC] + ключи, cwd=папка, capture_output=True, timeout=120)
    вывод = (р.stdout + р.stderr).decode("utf-8", "replace").strip()
    return р.returncode == 0, вывод


def проверить(файл_главы, рабочая):
    имя = os.path.basename(файл_главы)
    папка = os.path.join(рабочая, имя[:2])
    os.makedirs(папка, exist_ok=True)
    итог, команды, n = [], [], 0
    for строка, код in блоки(файл_главы):
        первая = код.split("\n", 1)[0].strip()
        м = re.match(r"//\s*файл:\s*(\S+)", первая)
        for к in re.findall(r"^\s*//\s*сборка:\s*(.+)$", код, re.M):
            команды.append((строка, к.split()))
        if м:
            with open(os.path.join(папка, м.group(1)), "w", encoding="utf-8") as ф:
                ф.write(код)
            continue
        if not re.search(r"\b(w?main|w?WinMain)\s*\(", код) or "{" not in код:     # вывод «нет функции main (…)» — не программа
            continue
        n += 1
        исходник = "пример%d.c" % n
        with open(os.path.join(папка, исходник), "w", encoding="utf-8") as ф:
            ф.write(код)
        ок, вывод = собрать(папка, [исходник, "-o", "пример%d.exe" % n])
        итог.append((строка, ок, вывод))
    for строка, ключи in команды:
        if ключи and ключи[0].lower() in ("wcc", "wcc.exe"):
            ключи = ключи[1:]
        ок, вывод = собрать(папка, ключи)
        итог.append((строка, ок, "wcc " + " ".join(ключи) + ": " + вывод))
    return итог


def главное():
    if not os.path.exists(WCC):
        raise SystemExit("Нет " + WCC)
    подробно = "--размеры" in sys.argv          # и удачные сборки: их строки «Готово: … байт» — сверить с книгой
    выбор = set(a.zfill(2) for a in sys.argv[1:] if not a.startswith("--"))
    рабочая = tempfile.mkdtemp(prefix="wincc_examples_")
    всего = плохих = 0
    try:
        for ф in sorted(os.listdir(ГЛАВЫ)):
            if not ф.endswith(".txt") or (выбор and ф[:2] not in выбор):
                continue
            итог = проверить(os.path.join(ГЛАВЫ, ф), рабочая)
            if not итог:
                continue
            ошибок = sum(1 for _, ок, _ in итог if not ок)
            всего += len(итог)
            плохих += ошибок
            print("%-60s примеров %2d, с ошибками %d" % (ф[:60], len(итог), ошибок))
            for строка, ок, вывод in итог:
                if not ок or подробно:
                    print("    строка %d: %s" % (строка, вывод.replace("\n", "\n        ")))
    finally:
        shutil.rmtree(рабочая, ignore_errors=True)
    print("\nПримеров %d, не собралось %d" % (всего, плохих))
    sys.exit(1 if плохих else 0)


if __name__ == "__main__":
    главное()
