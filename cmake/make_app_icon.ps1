# cmake/make_app_icon.ps1
#
# Generates resources/app.ico: a blue rounded square with a white letter "P".
#
# The drawing parameters are identical to CUINavBarItem::makeLetterIcon()
# in NavBar/UINavBarItem.cpp, so the .ico looks exactly like the letter icon
# drawn in code for the sidebar InfoIcon:
#   canvas      : size x size, transparent background
#   rounded rect: origin (0.04*size, 0.04*size), side 0.92*size,
#                 corner radius 0.22*size, no outline, filled with the brand blue
#   letter      : white, bold, pixel height 0.58*size, centered on the canvas
#   brand blue  : #2563EB (same kAppletIconColor used by the sidebar/applet page)
#
# Output format: a multi-size ICO whose entries are PNG compressed
# (256 / 128 / 64 / 48 / 32 / 16). Layout:
#   ICONDIR (6 bytes) + one 16-byte ICONDIRENTRY per image + the PNG blobs.
#   The native ICO decoder on Windows Vista and later handles PNG entries,
#   so Explorer, the taskbar and the window icon all render correctly.
#
# NOTE: comments in this file are intentionally ASCII-only. Windows PowerShell
# 5.1 reads .ps1 files as ANSI unless they carry a BOM, so non-ASCII comments
# could be mangled. Keeping the script pure ASCII removes that risk entirely.

param(
    [Parameter(Mandatory = $true)][string]$OutPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

# --- drawing parameters (mirror makeLetterIcon) ---
$Letter      = 'P'
$BgR = 0x25
$BgG = 0x63
$BgB = 0xEB
$MarginRatio = 0.04
$SideRatio   = 0.92
$RadiusRatio = 0.22
$FontRatio   = 0.58

function New-LetterBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size,
                                            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $m    = [single]($size * $MarginRatio)
    $side = [single]($size * $SideRatio)
    $r    = [single]($size * $RadiusRatio)
    $d    = [single]($r * 2)

    # rounded square background
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($m,              $m,              $d, $d, 180, 90)
    $path.AddArc($m + $side - $d, $m,              $d, $d, 270, 90)
    $path.AddArc($m + $side - $d, $m + $side - $d, $d, $d,   0, 90)
    $path.AddArc($m,              $m + $side - $d, $d, $d,  90, 90)
    $path.CloseFigure()

    $bg = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(255, $BgR, $BgG, $BgB))
    $g.FillPath($bg, $path)

    # centered white bold letter
    $font = New-Object System.Drawing.Font('Segoe UI', [single]($size * $FontRatio),
                                           [System.Drawing.FontStyle]::Bold,
                                           [System.Drawing.GraphicsUnit]::Pixel)
    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment     = [System.Drawing.StringAlignment]::Center
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center
    $rect = New-Object System.Drawing.RectangleF(0, 0, [single]$size, [single]$size)
    $g.DrawString($Letter, $font, $white, $rect, $fmt)

    $bg.Dispose()
    $white.Dispose()
    $font.Dispose()
    $fmt.Dispose()
    $path.Dispose()
    $g.Dispose()
    return $bmp
}

$sizes = @(256, 128, 64, 48, 32, 16)
$pngs = New-Object 'System.Collections.Generic.List[byte[]]'

foreach ($s in $sizes) {
    $bmp = New-LetterBitmap $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs.Add($ms.ToArray())
    $ms.Dispose()
    $bmp.Dispose()
}

$outDir = Split-Path -Parent $OutPath
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

# --- write the ICO container ---
$fs = New-Object System.IO.FileStream($OutPath, [System.IO.FileMode]::Create)
$bw = New-Object System.IO.BinaryWriter($fs)

$bw.Write([UInt16]0)                # reserved, must be 0
$bw.Write([UInt16]1)                # resource type: 1 = icon
$bw.Write([UInt16]($sizes.Count))   # number of images

$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    # a dimension of 256 is stored as 0 in the ICO directory entry
    if ($s -ge 256) { $dim = [Byte]0 } else { $dim = [Byte]$s }

    $bw.Write($dim)                                # width
    $bw.Write($dim)                                # height
    $bw.Write([Byte]0)                             # palette colour count
    $bw.Write([Byte]0)                             # reserved
    $bw.Write([UInt16]1)                           # colour planes
    $bw.Write([UInt16]32)                          # bits per pixel
    $bw.Write([UInt32]($pngs[$i].Length))          # size of the image data
    $bw.Write([UInt32]$offset)                     # offset of the image data
    $offset += $pngs[$i].Length
}

foreach ($blob in $pngs) {
    $bw.Write($blob)
}

$bw.Flush()
$bw.Dispose()
$fs.Dispose()

$len = (Get-Item -LiteralPath $OutPath).Length
Write-Host ("app.ico written: {0} ({1} bytes, {2} sizes)" -f $OutPath, $len, $sizes.Count)
exit 0
