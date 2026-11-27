# Tests of WinCC: every tests\*.c becomes a real .exe (wcc.exe), runs, and its main
# returns the number of failed checks: 0 = ok. A first line "// with: b.part c.part"
# adds more sources of the same program. The .exe files go to tests\out.
#   powershell -File tests\run.ps1 [имя-теста] [-Flags "--tiny"]
# -Flags: extra wcc switches for every test (the IDE builds with --tiny)
param([string]$Only = "", [string]$Flags = "")
$ErrorActionPreference = "Continue"
$here = $PSScriptRoot
$wcc = Join-Path (Split-Path -Parent $here) "wcc.exe"
$out = Join-Path $here "out"
New-Item -ItemType Directory -Force $out | Out-Null
$bad = 0; $count = 0
foreach ($t in Get-ChildItem $here -Filter *.c | Sort-Object Name) {
    if ($Only -and $t.BaseName -ne $Only) { continue }
    $count++
    $srcs = @($t.FullName)
    $first = Get-Content $t.FullName -TotalCount 1 -Encoding UTF8
    if ($first -match '^// with: (.+)$') {
        foreach ($p in ($Matches[1].Trim() -split '\s+')) { $srcs += (Join-Path $here $p) }
    }
    $exe = Join-Path $out ($t.BaseName + ".exe")
    $extra = @($Flags -split '\s+' | Where-Object { $_ })
    $build = & $wcc @srcs -o $exe @extra 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("{0,-24} НЕ СОБРАЛСЯ: {1}" -f $t.Name, ($build -join " "))
        $bad++; continue
    }
    Push-Location $out
    $rc = -1                        # an .exe Windows does not start counts as failed
    try { $global:LASTEXITCODE = -1; $res = & $exe 2>&1; $rc = $LASTEXITCODE }
    catch { $res = @($_.ToString()) }
    Pop-Location
    if ($rc -eq 0) { Write-Host ("{0,-24} ok" -f $t.Name) }
    else {
        Write-Host ("{0,-24} ПРОВАЛ ({1})" -f $t.Name, $rc)
        $res | Select-Object -Last 8 | ForEach-Object { Write-Host "    $_" }
        $bad++
    }
}
Write-Host ""
Write-Host "Тестов: $count, с ошибками: $bad"
exit $bad
