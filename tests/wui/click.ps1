# Проверка WUI мышью и клавишами без рук: показ (examples\wui) получает сообщения окну —
# щелчки, набор, клавиши — и после каждого шага сохраняет снимок (WUI_SNAPSHOT_WAIT).
#   powershell -File tests\wui\click.ps1 показ.exe папка_для_снимков
param([string]$Exe, [string]$Out)
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class W {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    public static IntPtr[] Windows(uint pid, string cls) {
        var list = new System.Collections.Generic.List<IntPtr>();
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            var s = new StringBuilder(64); GetClassNameW(h, s, 64);
            if (p == pid && s.ToString() == cls && IsWindowVisible(h)) list.Add(h);
            return true;
        }, IntPtr.Zero);
        return list.ToArray();
    }
}
"@
New-Item -ItemType Directory -Force $Out | Out-Null
$env:WUI_SNAPSHOT = Join-Path $Out "шаг.png"
$env:WUI_SNAPSHOT_WAIT = "1"
$env:WUI_THEME = "Emerald"
$p = Start-Process -FilePath $Exe -PassThru
$main = [IntPtr]::Zero
for ($i = 0; $i -lt 50 -and $main -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 100
    $all = [W]::Windows($p.Id, "WuiWindow"); if ($all.Count) { $main = $all[0] }
}
if ($main -eq [IntPtr]::Zero) { Write-Host "окно не появилось"; exit 1 }
Start-Sleep -Milliseconds 400

function Post($h, $msg, $wp, $lp) { [W]::PostMessageW($h, $msg, [IntPtr]$wp, [IntPtr]$lp) | Out-Null }
function Pt($x, $y) { return ($y -shl 16) -bor $x }
function Click($h, $x, $y) {
    Post $h 0x200 0 (Pt $x $y); Post $h 0x201 1 (Pt $x $y); Post $h 0x202 0 (Pt $x $y); Start-Sleep -Milliseconds 120
}
function Drag($h, $x1, $x2, $y) {
    Post $h 0x201 1 (Pt $x1 $y); Post $h 0x200 1 (Pt $x2 $y); Post $h 0x202 0 (Pt $x2 $y); Start-Sleep -Milliseconds 120
}
function Typ($h, $text) { foreach ($c in $text.ToCharArray()) { Post $h 0x102 ([int]$c) 0 }; Start-Sleep -Milliseconds 120 }
function Key($h, $vk) { Post $h 0x100 $vk 0; Start-Sleep -Milliseconds 150 }
function Snap($h, $n) { Post $h 0x8003 $n 0; Start-Sleep -Milliseconds 400 }

# 1 — имя в поле; 2 — Enter: всплывающая строка
Click $main 650 332; Typ $main "Олег"; Snap $main 1
Key $main 0x0D; Snap $main 2
# 3 — флажок, кнопка выбора, ползунок мышью
Click $main 300 508; Click $main 505 538; Drag $main 656 900 609; Snap $main 3
# 4 — раскрывающийся список: вниз, Enter
Click $main 370 709; Key $main 0x28; Key $main 0x0D; Snap $main 4
# 5 — выбор темы: открыть, три раза вниз, Enter
Click $main 933 42; Key $main 0x28; Key $main 0x28; Key $main 0x28; Key $main 0x0D; Start-Sleep -Milliseconds 800; Snap $main 5
# 6 — вторая страница, строка списка
Click $main 144 174; Click $main 330 368; Snap $main 6
# 7 — вкладка «Сообщения», «Спросить»: окно-вопрос; 8 — Enter: ответ «да»
Click $main 565 190; Click $main 330 240; Start-Sleep -Milliseconds 600
$all = [W]::Windows($p.Id, "WuiWindow")
foreach ($h in $all) { if ($h -ne $main) { Snap $h 7; Key $h 0x0D } }
Start-Sleep -Milliseconds 400; Snap $main 8
# 9 — развернуть
Post $main 0x112 0xF030 0; Start-Sleep -Milliseconds 800; Snap $main 9
Post $main 0x10 0 0
$p.WaitForExit(5000) | Out-Null
if (!$p.HasExited) { $p.Kill(); Write-Host "не закрылась" }
Get-ChildItem $Out -Filter "шаг-*.png" | ForEach-Object { $_.Name }
