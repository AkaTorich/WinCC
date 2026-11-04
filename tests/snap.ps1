# Starts a windowed program, waits for its window and saves a picture of it (PNG).
#   powershell -File tests\snap.ps1 программа.exe снимок.png [секунды] [ширина высота] [параметры программы]
param([string]$Exe, [string]$Png, [int]$Wait = 2, [int]$Width = 0, [int]$Height = 0, [string]$ArgList = "")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Snap {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
}
"@
$p = if ($ArgList) { Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru } else { Start-Process -FilePath $Exe -PassThru }
for ($i = 0; $i -lt 50 -and $p.MainWindowHandle -eq 0; $i++) { Start-Sleep -Milliseconds 100; $p.Refresh() }
$h = $p.MainWindowHandle
if ($h -eq 0) { Write-Host "окно не появилось"; exit 1 }
if ($Width -gt 0) { [Snap]::MoveWindow($h, 100, 100, $Width, $Height, $true) | Out-Null }
[Snap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Seconds $Wait
$r = New-Object Snap+RECT
[Snap]::GetWindowRect($h, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[Snap]::PrintWindow($h, $dc, 2) | Out-Null       # PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc)
$bmp.Save($Png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "снимок: $Png"
$p.CloseMainWindow() | Out-Null
Start-Sleep -Milliseconds 500
if (!$p.HasExited) { $p.Kill() }
