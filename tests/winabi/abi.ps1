# The headers of WinCC against the real ones: every program here is built by MSVC (the
# Windows SDK) and by wcc.exe, both print the same list, and the lists are compared line
# by line. sizes.c: windows.h; net.c: winsock2.h, ws2tcpip.h, winhttp.h; consts.c is made
# here from every number #define of the sockets and WinHTTP headers of WinCC; bits.c: how
# bit fields are laid out (the bytes of each field); nt.c and ntconsts.c: ntdll.h against
# winternl.h and ntstatus.h.
#   powershell -File tests\winabi\abi.ps1
$ErrorActionPreference = "Continue"
$here = $PSScriptRoot
$root = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $here "out"
New-Item -ItemType Directory -Force $out | Out-Null
$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"

# consts.c: one line per "#define NAME value" (not macros with arguments, not text, not names of functions)
$lines = @("#include <winsock2.h>", "#include <ws2tcpip.h>", "#include <winhttp.h>", "#include <stdio.h>",
           "#define P(n) printf(""%-56s %lld\n"", #n, (long long)(n))", "int main(void)", "{")
foreach ($h in "winsock2.h", "ws2tcpip.h", "winhttp.h") {
    foreach ($l in Get-Content (Join-Path $root "include\$h") -Encoding UTF8) {
        if ($l -cnotmatch '^#define ([A-Z_][A-Z0-9_]*)[ \t]+(.+)$') { continue }
        if ($Matches[2] -cmatch '"|__AW|^[a-z]|^(Get|Free|Inet)\w*$|^ADDRINFO|^Byte$|^{') { continue }
        $lines += "    P($($Matches[1]));"
    }
}
$lines += "    return 0;", "}"
[IO.File]::WriteAllLines((Join-Path $out "consts.c"), $lines)

# ntconsts.c: the same for ntdll.h and what it needs from win\kernel32.h; the SE_ privileges and
# THREAD_CREATE_FLAGS_ are not in the SDK (the SE_ ones are in the WDK's wdm.h: checked by hand)
$lines = @("#ifdef __WINCC__", "#include <ntdll.h>", "#else", "#define WIN32_NO_STATUS", "#include <windows.h>",
           "#undef WIN32_NO_STATUS", "#include <ntstatus.h>", "#include <winternl.h>", "#endif", "#include <stdio.h>",
           "#define P(n) printf(""%-56s %lld\n"", #n, (long long)(n))", "int main(void)", "{")
foreach ($h in "ntdll.h", "win\kernel32.h") {
    foreach ($l in Get-Content (Join-Path $root "include\$h") -Encoding UTF8) {
        if ($l -cnotmatch '^#define ([A-Z_][A-Z0-9_]*)[ \t]+(.+)$') { continue }
        if ($Matches[1] -cmatch '^(SE_|THREAD_CREATE_FLAGS_|NTAPI)' -or $Matches[2] -cmatch '"|memset|memcpy|memmove|memcmp|__readgsqword|HANDLE|^Nt|^\{|__AW') { continue }
        $lines += "    P($($Matches[1]));"
    }
}
$lines += "    return 0;", "}"
[IO.File]::WriteAllLines((Join-Path $out "ntconsts.c"), $lines)

$progs = @(
    @{ src = (Join-Path $here "sizes.c"); libs = "user32.lib" },
    @{ src = (Join-Path $here "net.c"); libs = "ws2_32.lib winhttp.lib" },
    @{ src = (Join-Path $out "consts.c"); libs = "ws2_32.lib winhttp.lib" },
    @{ src = (Join-Path $here "bits.c"); libs = "" },
    @{ src = (Join-Path $here "nt.c"); libs = "" },
    @{ src = (Join-Path $out "ntconsts.c"); libs = "" }
)
$bad = 0; $count = 0
foreach ($p in $progs) {
    $name = [IO.Path]::GetFileNameWithoutExtension($p.src)
    $msvc = Join-Path $out "$name-msvc.exe"; $wincc = Join-Path $out "$name-wincc.exe"
    Remove-Item $msvc, $wincc -ErrorAction SilentlyContinue
    $err = cmd /c "call `"$vcvars`" >nul 2>nul && cl /nologo /utf-8 /W0 /Fe`"$msvc`" /Fo`"$out\\`" `"$($p.src)`" $($p.libs)"
    if (-not (Test-Path $msvc)) {
        Write-Host "MSVC не собрал $name"
        $err | Select-Object -Last 15 | ForEach-Object { Write-Host "    $_" }
        $bad++; continue
    }
    $err = & (Join-Path $root "wcc.exe") $p.src -o $wincc 2>&1
    if (-not (Test-Path $wincc)) { Write-Host "WinCC не собрал ${name}: $err"; $bad++; continue }
    $a = & $msvc
    $b = & $wincc
    for ($i = 0; $i -lt [Math]::Max($a.Count, $b.Count); $i++) {
        if ($a[$i] -ne $b[$i]) { Write-Host ("{0}`n  MSVC:  {1}`n  WinCC: {2}" -f $name, $a[$i], $b[$i]); $bad++ }
    }
    $count += $a.Count
}
Write-Host ("Строк: {0}, расхождений: {1}" -f $count, $bad)
exit $bad
