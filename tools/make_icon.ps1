# Renders res/app.ico from a square logo image using System.Drawing.
# Run after changing the logo; the resulting .ico is committed.
#   .\tools\make_icon.ps1 -Source D:\logo.png
param(
    [Parameter(Mandatory)][string]$Source,
    [string]$Out = "$PSScriptRoot\..\res\app.ico"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sizes = 16, 20, 24, 32, 40, 48, 64, 96, 128, 256

$src = [System.Drawing.Image]::FromFile((Resolve-Path $Source).Path)
$side = [Math]::Min($src.Width, $src.Height)
$crop = New-Object System.Drawing.RectangleF (($src.Width - $side) / 2), (($src.Height - $side) / 2), $side, $side

function New-IconPng([int]$size, [System.Drawing.RectangleF]$crop) {
    # Scale the crop first, then fill a rounded square with it: FillPath is anti-aliased, SetClip is not.
    $scaled = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($scaled)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.CompositingQuality = 'HighQuality'
    $attr = New-Object System.Drawing.Imaging.ImageAttributes
    $attr.SetWrapMode('TileFlipXY')   # no dark fringe along the edges
    $dest = New-Object System.Drawing.Rectangle 0, 0, $size, $size
    $g.DrawImage($src, $dest, $crop.X, $crop.Y, $crop.Width, $crop.Height, 'Pixel', $attr)
    $g.Dispose()

    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)
    $d = [single]($size * 0.44)   # corner radius 22%
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc(0, 0, $d, $d, 180, 90)
    $path.AddArc($size - $d, 0, $d, $d, 270, 90)
    $path.AddArc($size - $d, $size - $d, $d, $d, 0, 90)
    $path.AddArc(0, $size - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $brush = New-Object System.Drawing.TextureBrush $scaled
    $g.FillPath($brush, $path)
    $brush.Dispose(); $g.Dispose(); $scaled.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return ,$ms.ToArray()
}

$pngs = @($sizes | ForEach-Object { ,(New-IconPng $_ $crop) })
$src.Dispose()

$fs = [System.IO.File]::Create([System.IO.Path]::GetFullPath($Out))
$w = New-Object System.IO.BinaryWriter $fs
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $len = $pngs[$i].Length
    $w.Write([byte]($s % 256)); $w.Write([byte]($s % 256))
    $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]32)
    $w.Write([UInt32]$len); $w.Write([UInt32]$offset)
    $offset += $len
}
foreach ($p in $pngs) { $w.Write($p) }
$w.Close()
Write-Host "Wrote $Out"
