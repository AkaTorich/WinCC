# Рисует icon.ico окна WinCC: тёмная плитка, окно Windows с полосой заголовка, в нём толстая буква C и знак подсказки.
# Запуск: powershell.exe -NoProfile -File tools\make-icon.ps1  (Windows PowerShell 5.1, файл сохранён с BOM)
param([string]$Out = (Join-Path $PSScriptRoot '..\icon.ico'), [string]$PreviewPng = '')

Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'

function RoundRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function C([int]$a, [int]$r, [int]$g, [int]$b) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function P([float]$x, [float]$y) { New-Object System.Drawing.PointF($x, $y) }

function Draw([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.PixelOffsetMode = 'HighQuality'
    $s = $size / 256.0
    $g.ScaleTransform($s, $s)

    # Плитка: глубокий синий с бликом сверху
    $tile = RoundRect 8 8 240 240 44
    $g.FillPath((New-Object System.Drawing.Drawing2D.LinearGradientBrush((P 0 8), (P 0 248), (C 255 34 58 104), (C 255 12 20 40))), $tile)
    $g.FillPath((New-Object System.Drawing.Drawing2D.LinearGradientBrush((P 0 8), (P 0 120), (C 60 255 255 255), (C 0 255 255 255))), (RoundRect 8 8 240 112 44))
    $g.DrawPath((New-Object System.Drawing.Pen((C 255 92 128 190), 4)), $tile)

    # Окно: светлая рамка, полоса заголовка с тремя кнопками
    $win = RoundRect 34 46 188 164 14
    $g.FillPath((New-Object System.Drawing.SolidBrush((C 255 18 28 52))), $win)
    $bar = New-Object System.Drawing.Drawing2D.GraphicsPath
    $bar.AddArc(34, 46, 28, 28, 180, 90)
    $bar.AddArc(194, 46, 28, 28, 270, 90)
    $bar.AddLine(222, 78, 34, 78)
    $bar.CloseFigure()
    $g.FillPath((New-Object System.Drawing.Drawing2D.LinearGradientBrush((P 0 46), (P 0 78), (C 255 120 190 255), (C 255 60 130 230))), $bar)
    foreach ($bx in 176, 194) { $g.FillEllipse((New-Object System.Drawing.SolidBrush((C 230 255 255 255))), $bx, 56, 12, 12) }
    $g.FillEllipse((New-Object System.Drawing.SolidBrush((C 255 255 120 90))), 158, 56, 12, 12)
    $g.DrawPath((New-Object System.Drawing.Pen((C 255 160 205 255), 6)), $win)

    # Буква C: толстая дуга, янтарный градиент
    $arc = New-Object System.Drawing.Pen((New-Object System.Drawing.Drawing2D.LinearGradientBrush((P 70 96), (P 150 190), (C 255 255 214 90), (C 255 245 140 30))), 26)
    $arc.StartCap = 'Round'; $arc.EndCap = 'Round'
    $g.DrawArc($arc, 66, 100, 86, 86, 45, 270)

    # Подсказка «>_» справа
    $prompt = New-Object System.Drawing.Pen((C 255 130 230 170), 12)
    $prompt.StartCap = 'Round'; $prompt.EndCap = 'Round'; $prompt.LineJoin = 'Round'
    $g.DrawLines($prompt, [System.Drawing.PointF[]]@((P 166 122), (P 186 142), (P 166 162)))
    $g.DrawLine($prompt, 172, 180, 198, 180)

    $g.Dispose()
    return $bmp
}

$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach ($sz in $sizes) {
    $b = Draw $sz
    $ms = New-Object System.IO.MemoryStream
    $b.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    if ($PreviewPng -and $sz -eq 256) { $b.Save($PreviewPng, [System.Drawing.Imaging.ImageFormat]::Png) }
    $b.Dispose()
    $pngs += , $ms.ToArray()
}

# ICO: заголовок, каталог, затем PNG подряд
$fs = [System.IO.File]::Create($Out)
$w = New-Object System.IO.BinaryWriter($fs)
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $d = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $w.Write([byte]$d); $w.Write([byte]$d); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$pngs[$i].Length); $w.Write([uint32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $w.Write($p) }
$w.Close()
"icon.ico: $((Get-Item $Out).Length) байт"
