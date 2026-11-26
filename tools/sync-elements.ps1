# Elements (Code\Libraries\Elements) внутри WinCC: библиотека окон, которую wcc собирает с программой сам,
# как WUI и net. Скрипт берёт её свежую из Libraries\Elements (её саму не меняет) и кладёт:
#   include\elements.h            — её заголовок + #pragma comment(lib, "elements") и окно без консоли
#   lib\elements\                 — её src\ (el_internal.h подключает <elements.h>, а не ../include/elements.h)
#   examples\elements\demos\      — её примеры и проект .wccproj на каждый (вид «Окно»)
#   examples\elements\fonts\      — 30 её шрифтов (примеры ищут их в ..\fonts рядом с exe)
#   examples\elements\*.md        — её README и DOCUMENTATION
# Запуск из корня WinCC:  powershell -ExecutionPolicy Bypass -File tools\sync-elements.ps1 [папка Elements]
param([string]$From = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $From) { $From = Join-Path $root "..\Libraries\Elements" }
$From = (Resolve-Path $From).Path
if (-not (Test-Path "$From\include\elements.h")) { throw "нет $From\include\elements.h" }
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Write-Text([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text, $utf8) }
function Read-Text([string]$path) { [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8) }

# заголовок: после первой #pragma once — то, что делает Elements библиотекой WinCC
$h = Read-Text "$From\include\elements.h"
$mark = "#pragma once"
$at = $h.IndexOf($mark)
if ($at -lt 0) { throw "в elements.h нет #pragma once" }
$add = "`r`n#ifdef __WINCC__`r`n#pragma comment(lib, `"elements`")                  // wcc собирает lib\elements с программой`r`n" +
       "#pragma comment(linker, `"/SUBSYSTEM:WINDOWS`")       // окно без консоли, даже с main()`r`n#endif"
$h = $h.Insert($at + $mark.Length, $add)
Write-Text "$root\include\elements.h" $h

# исходники библиотеки
$lib = "$root\lib\elements"
if (Test-Path $lib) { Get-ChildItem $lib -File | Remove-Item }
New-Item -ItemType Directory -Force $lib | Out-Null
foreach ($f in Get-ChildItem "$From\src" -File -Include *.c, *.h -Recurse) {
    $t = Read-Text $f.FullName
    $t = $t.Replace('#include "../include/elements.h"', '#include <elements.h>')
    Write-Text "$lib\$($f.Name)" $t
}

# примеры, по проекту на каждый
$demos = "$root\examples\elements\demos"
if (Test-Path $demos) { Get-ChildItem $demos -File | Remove-Item }
New-Item -ItemType Directory -Force $demos | Out-Null
Get-ChildItem "$From\examples" -File -Include *.h -Recurse | Copy-Item -Destination $demos
foreach ($c in Get-ChildItem "$From\examples" -File -Filter *.c) {
    Copy-Item $c.FullName $demos
    $p = "# Elements: пример $($c.Name)`r`nkind=Window`r`nconfig=Debug`r`nfile=$($c.Name)`r`n"
    if ((Read-Text $c.FullName) -match 'elm_fonts\.h') { $p += "file=elm_fonts.h`r`n" }
    Write-Text "$demos\$($c.BaseName).wccproj" $p
}

# шрифты и описания
$fonts = "$root\examples\elements\fonts"
New-Item -ItemType Directory -Force $fonts | Out-Null
Get-ChildItem "$From\fonts" -File | Copy-Item -Destination $fonts -Force
foreach ($md in "README.md", "DOCUMENTATION.md") {
    if (Test-Path "$From\$md") { Copy-Item "$From\$md" "$root\examples\elements\$md" -Force }
}

$n = (Get-ChildItem $lib -File).Count
$d = (Get-ChildItem $demos -Filter *.c).Count
Write-Host "Elements -> WinCC: файлов библиотеки $n, примеров $d, шрифтов $((Get-ChildItem $fonts -File).Count)"
