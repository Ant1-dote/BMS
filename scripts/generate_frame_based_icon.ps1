param(
    [Parameter(Mandatory = $true)]
    [string]$SourceFrameImage,
    [string]$OutputIco = "",
    [int[]]$Sizes = @(16, 24, 32, 48, 64, 128, 256),
    [switch]$ExportPng
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($OutputIco)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $OutputIco = Join-Path $scriptDir "..\assets\icons\bms_app.ico"
}

function New-RoundedRectPath {
    param(
        [float]$X,
        [float]$Y,
        [float]$Width,
        [float]$Height,
        [float]$Radius
    )

    $diameter = $Radius * 2.0
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($X, $Y, $diameter, $diameter, 180, 90)
    $path.AddArc($X + $Width - $diameter, $Y, $diameter, $diameter, 270, 90)
    $path.AddArc($X + $Width - $diameter, $Y + $Height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($X, $Y + $Height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function Draw-CoverImage {
    param(
        [System.Drawing.Graphics]$Graphics,
        [System.Drawing.Bitmap]$Source,
        [int]$Size
    )

    $srcW = [double]$Source.Width
    $srcH = [double]$Source.Height
    $dst = [double]$Size
    $scale = [Math]::Max($dst / $srcW, $dst / $srcH)

    $drawW = [Math]::Round($srcW * $scale)
    $drawH = [Math]::Round($srcH * $scale)
    $drawX = [Math]::Floor(($dst - $drawW) / 2.0)
    $drawY = [Math]::Floor(($dst - $drawH) / 2.0)

    $Graphics.DrawImage(
        $Source,
        [System.Drawing.Rectangle]::new([int]$drawX, [int]$drawY, [int]$drawW, [int]$drawH),
        [System.Drawing.Rectangle]::new(0, 0, $Source.Width, $Source.Height),
        [System.Drawing.GraphicsUnit]::Pixel)
}

function Render-FrameBasedBitmap {
    param(
        [System.Drawing.Bitmap]$Source,
        [int]$Size
    )

    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)

        $outerRadius = [float]([Math]::Max(2.0, $Size * 0.18))
        $outerPath = New-RoundedRectPath -X 0 -Y 0 -Width $Size -Height $Size -Radius $outerRadius
        try {
            $g.SetClip($outerPath)
            Draw-CoverImage -Graphics $g -Source $Source -Size $Size
            $g.ResetClip()

            $inset = [float]([Math]::Round($Size * 0.16))
            $innerW = [float]($Size - ($inset * 2))
            $innerH = $innerW
            $innerRadius = [float]([Math]::Max(2.0, $Size * 0.14))
            $innerPath = New-RoundedRectPath -X $inset -Y $inset -Width $innerW -Height $innerH -Radius $innerRadius
            try {
                $innerRect = [System.Drawing.RectangleF]::new($inset, $inset, $innerW, $innerH)
                $innerBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
                    $innerRect,
                    ([System.Drawing.ColorTranslator]::FromHtml("#1E244C")),
                    ([System.Drawing.ColorTranslator]::FromHtml("#141C3F")),
                    90.0)
                try {
                    $g.FillPath($innerBrush, $innerPath)
                }
                finally {
                    $innerBrush.Dispose()
                }
            }
            finally {
                $innerPath.Dispose()
            }

            $wavePen = New-Object System.Drawing.Pen(
                ([System.Drawing.ColorTranslator]::FromHtml("#26CCF2")),
                [float]([Math]::Max(1.0, $Size * 0.085)))
            try {
                $wavePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
                $wavePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
                $wavePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

                $pts = @(
                    [System.Drawing.PointF]::new([float]($Size * 0.20), [float]($Size * 0.62)),
                    [System.Drawing.PointF]::new([float]($Size * 0.37), [float]($Size * 0.62)),
                    [System.Drawing.PointF]::new([float]($Size * 0.50), [float]($Size * 0.42)),
                    [System.Drawing.PointF]::new([float]($Size * 0.64), [float]($Size * 0.68)),
                    [System.Drawing.PointF]::new([float]($Size * 0.80), [float]($Size * 0.48))
                )
                $g.DrawCurve($wavePen, $pts)
            }
            finally {
                $wavePen.Dispose()
            }

            if ($Size -ge 32) {
                $text = "BMS"
                $fontSize = [float]([Math]::Max(7.0, $Size * 0.13))
                $font = New-Object System.Drawing.Font("Segoe UI", $fontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
                try {
                    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(190, 198, 224, 255))
                    try {
                        $sf = New-Object System.Drawing.StringFormat
                        try {
                            $sf.Alignment = [System.Drawing.StringAlignment]::Center
                            $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
                            $rect = [System.Drawing.RectangleF]::new([float]($Size * 0.50), [float]($Size * 0.80), 0.0, 0.0)
                            $g.DrawString($text, $font, $brush, $rect, $sf)
                        }
                        finally {
                            $sf.Dispose()
                        }
                    }
                    finally {
                        $brush.Dispose()
                    }
                }
                finally {
                    $font.Dispose()
                }
            }
        }
        finally {
            $outerPath.Dispose()
        }

        return $bmp
    }
    catch {
        $bmp.Dispose()
        throw
    }
    finally {
        $g.Dispose()
    }
}

function Convert-BitmapToIcoImageData {
    param([System.Drawing.Bitmap]$Bitmap)

    $size = $Bitmap.Width
    $maskStride = [int]([Math]::Ceiling($size / 32.0) * 4)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    try {
        $bw.Write([Int32]40)
        $bw.Write([Int32]$size)
        $bw.Write([Int32]($size * 2))
        $bw.Write([Int16]1)
        $bw.Write([Int16]32)
        $bw.Write([Int32]0)
        $bw.Write([Int32]($size * $size * 4))
        $bw.Write([Int32]0)
        $bw.Write([Int32]0)
        $bw.Write([Int32]0)
        $bw.Write([Int32]0)

        for ($y = $size - 1; $y -ge 0; --$y) {
            for ($x = 0; $x -lt $size; ++$x) {
                $pixel = $Bitmap.GetPixel($x, $y)
                $bw.Write([byte]$pixel.B)
                $bw.Write([byte]$pixel.G)
                $bw.Write([byte]$pixel.R)
                $bw.Write([byte]$pixel.A)
            }
        }

        $maskBytes = New-Object byte[] ($maskStride * $size)
        $bw.Write($maskBytes)
        return $ms.ToArray()
    }
    finally {
        $bw.Dispose()
        $ms.Dispose()
    }
}

function Write-IcoFile {
    param(
        [string]$Path,
        [System.Collections.Generic.List[object]]$Images
    )

    $outDir = Split-Path -Parent $Path
    if (-not (Test-Path $outDir)) {
        New-Item -Path $outDir -ItemType Directory -Force | Out-Null
    }

    $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $bw = New-Object System.IO.BinaryWriter($fs)
    try {
        $bw.Write([UInt16]0)
        $bw.Write([UInt16]1)
        $bw.Write([UInt16]$Images.Count)

        $offset = 6 + (16 * $Images.Count)
        foreach ($img in $Images) {
            $size = [int]$img.Size
            $bytes = [byte[]]$img.IcoBytes

            $dimByte = if ($size -ge 256) { 0 } else { [byte]$size }
            $bw.Write([byte]$dimByte)
            $bw.Write([byte]$dimByte)
            $bw.Write([byte]0)
            $bw.Write([byte]0)
            $bw.Write([UInt16]1)
            $bw.Write([UInt16]32)
            $bw.Write([UInt32]$bytes.Length)
            $bw.Write([UInt32]$offset)
            $offset += $bytes.Length
        }

        foreach ($img in $Images) {
            $bw.Write([byte[]]$img.IcoBytes)
        }
    }
    finally {
        $bw.Dispose()
        $fs.Dispose()
    }
}

if (-not (Test-Path $SourceFrameImage)) {
    throw "Source frame image not found: $SourceFrameImage"
}

$source = [System.Drawing.Bitmap]::FromFile((Resolve-Path $SourceFrameImage))
try {
    $images = New-Object 'System.Collections.Generic.List[object]'
    $previewDir = Join-Path (Split-Path -Parent $OutputIco) "generated"

    if ($ExportPng -and -not (Test-Path $previewDir)) {
        New-Item -Path $previewDir -ItemType Directory -Force | Out-Null
    }

    foreach ($s in ($Sizes | Sort-Object -Unique)) {
        if ($s -lt 16 -or $s -gt 256) {
            throw "Unsupported size: $s"
        }

        $bmp = Render-FrameBasedBitmap -Source $source -Size $s
        try {
            $images.Add([pscustomobject]@{
                Size = $s
                IcoBytes = (Convert-BitmapToIcoImageData -Bitmap $bmp)
            })

            if ($ExportPng) {
                $pngPath = Join-Path $previewDir ("frame_based_icon_{0}.png" -f $s)
                $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
        }
        finally {
            $bmp.Dispose()
        }
    }

    Write-IcoFile -Path $OutputIco -Images $images
    Write-Host "ICO generated:" $OutputIco
}
finally {
    $source.Dispose()
}
