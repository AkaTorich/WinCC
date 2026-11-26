# Resources of wcc against rc.exe of Microsoft: tests\win_res.rc is compiled both ways,
# win_res.c is built with each (the one with rc.exe's .res checks the reading of .res files),
# both programs run their checks, and every resource of the two .exe is compared byte for byte.
# Only the manifest differs: rc.exe adds none, wcc adds its own.
#   powershell -File tests\res\compare.ps1
$ErrorActionPreference = "Continue"
$res = $PSScriptRoot
$tests = Split-Path -Parent $res
$root = Split-Path -Parent $tests
$wcc = Join-Path $root "wcc.exe"
$out = Join-Path $tests "out"
New-Item -ItemType Directory -Force $out | Out-Null
$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"

# rc.exe: the .res
$env:PATH += ";C:\Program Files (x86)\Microsoft Visual Studio\Installer"
# cmd gets the Cyrillic of the path wrong: it works in the tests folder with short names
Push-Location $tests
cmd /c "`"$vcvars`" >nul && rc /nologo /fo out\win_res_ms.res win_res.rc" 2>&1 | ForEach-Object { Write-Host "    $_" }
Pop-Location
if (!(Test-Path "$out\win_res_ms.res")) { Write-Host "rc.exe не сделал .res"; exit 1 }

$bad = 0
& $wcc "$tests\win_res.c" "$tests\win_res.rc" -o "$out\win_res_wcc.exe" | Out-Null
& $wcc "$tests\win_res.c" "$out\win_res_ms.res" -o "$out\win_res_ms.exe" | Out-Null
& $wcc "$res\resdump.c" -o "$out\resdump.exe" | Out-Null
foreach ($e in "win_res_wcc", "win_res_ms") {
    $r = & "$out\$e.exe"
    if ($LASTEXITCODE -ne 0) { Write-Host "$e.exe: проверки не прошли"; $r | ForEach-Object { Write-Host "    $_" }; $bad++ }
    else { Write-Host "$e.exe: проверки прошли" }
}

$a = & "$out\resdump.exe" "$out\win_res_wcc.exe" | Where-Object { $_ -notmatch '^#24 #1 ' }
$b = & "$out\resdump.exe" "$out\win_res_ms.exe" | Where-Object { $_ -notmatch '^#24 #1 ' }
$diff = Compare-Object $a $b
if ($diff) {
    Write-Host "Ресурсы различаются (<= wcc, => rc.exe):"
    $diff | ForEach-Object { Write-Host ("    {0} {1}" -f $_.SideIndicator, $_.InputObject) }
    $bad++
} else {
    Write-Host ("Ресурсов: {0}, все совпадают с rc.exe байт в байт" -f $a.Count)
}
exit $bad
