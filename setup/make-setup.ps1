# Сборка установщика WinCC: wcc.exe и WinCC.exe собираются заново, файлы поставки укладываются
# в setup\payload.zip, из него собирается WinCCSetup.exe (корень WinCC\) и копия кладётся на рабочий стол.
#   powershell -ExecutionPolicy Bypass -File setup\make-setup.ps1
$ErrorActionPreference = "Continue"     # vcvars и dotnet пишут в stderr не только ошибки
$setup = $PSScriptRoot
$root = Split-Path -Parent $setup
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

Write-Host "1/4 компилятор wcc.exe…"
Push-Location $root
& ".\build.cmd" 2>&1 | Where-Object { "$_" -match " error |\[wcc\]" } | ForEach-Object { Write-Host "    $_" }
Pop-Location
if (!(Test-Path "$root\wcc.exe")) { throw "wcc.exe не собрался" }

Write-Host "2/4 окно WinCC.exe…"
$r = & dotnet build "$root\ide\WinCC.csproj" -c Release -nologo -v q 2>&1
if ($LASTEXITCODE -ne 0) { $r | Select-Object -Last 15 | ForEach-Object { Write-Host "    $_" }; throw "WinCC.exe не собрался" }

Write-Host "3/4 поставка payload.zip…"
# что едет в установщик: две программы, библиотеки (runtime, lib: wui, net, elements), заголовки, примеры (исходники),
# описание и руководство (его собирает docs\руководство\сборка.py; кнопка «Справка» в окне открывает его)
$manual = "$root\docs\руководство\Руководство WinCC.pdf"
if (!(Test-Path $manual)) { throw "нет руководства: соберите его — python docs\руководство\сборка.py" }
$items = New-Object System.Collections.Generic.List[object]
foreach ($f in "WinCC.exe", "wcc.exe", "README.md") { $items.Add(@("$root\$f", $f)) }
$items.Add(@($manual, "Руководство WinCC.pdf"))
# то же руководство справкой Windows (docs\руководство\сборка chm.py) — если собрано
$chm = "$root\docs\руководство\Руководство WinCC.chm"
if (Test-Path $chm) { $items.Add(@($chm, "Руководство WinCC.chm")) }
foreach ($dir in "runtime", "include", "lib") {
    Get-ChildItem "$root\$dir" -Recurse -File | ForEach-Object {
        $items.Add(@($_.FullName, $_.FullName.Substring($root.Length + 1).Replace('\', '/')))
    }
}
# примеры: исходники, проекты, шрифты и описания Elements (examples\elements)
Get-ChildItem "$root\examples" -Recurse -File -Include *.c, *.h, *.wccproj, *.ttf, *.md | ForEach-Object {
    $items.Add(@($_.FullName, $_.FullName.Substring($root.Length + 1).Replace('\', '/')))
}
$zipPath = "$setup\payload.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath }
$zip = [System.IO.Compression.ZipFile]::Open($zipPath, "Create")
try {
    foreach ($it in $items) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $it[0], $it[1], "Optimal") | Out-Null
    }
} finally { $zip.Dispose() }
Write-Host ("    файлов: {0}, {1:N1} МБ" -f $items.Count, ((Get-Item $zipPath).Length / 1MB))

Write-Host "4/4 установщик WinCCSetup.exe…"
$r = & dotnet build "$setup\WinCC.Setup.csproj" -c Release -nologo -v q 2>&1
if ($LASTEXITCODE -ne 0) { $r | Select-Object -Last 15 | ForEach-Object { Write-Host "    $_" }; throw "установщик не собрался" }
$exe = "$root\WinCCSetup.exe"
$desk = [Environment]::GetFolderPath("Desktop")
Copy-Item $exe "$desk\WinCCSetup.exe" -Force
Write-Host ("Готово: {0} ({1:N1} МБ), копия на рабочем столе" -f $exe, ((Get-Item $exe).Length / 1MB))
