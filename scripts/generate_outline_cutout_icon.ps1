param(
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

function Render-OutlineCutoutBitmap {
    param([int]$Size)

    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

        $margin = [float]($Size * 0.11)
        $width = [float]($Size - $margin * 2)
        $radius = [float]($Size * 0.22)
        $stroke = [float]([Math]::Max(1.0, $Size * 0.11))

        $outlinePath = New-RoundedRectPath -X $margin -Y $margin -Width $width -Height $width -Radius $radius
        try {
            $outlineRect = [System.Drawing.RectangleF]::new($margin, $margin, $width, $width)
            $outlineBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
                $outlineRect,
                ([System.Drawing.ColorTranslator]::FromHtml("#15B9EB")),
                ([System.Drawing.ColorTranslator]::FromHtml("#1E88E5")),
                45.0)
            try {
                $outlinePen = New-Object System.Drawing.Pen($outlineBrush, $stroke)
                try {
                    $outlinePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
                    $outlinePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
                    $outlinePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
                    $g.DrawPath($outlinePen, $outlinePath)
                }
                finally {
                    $outlinePen.Dispose()
                }
            }
            finally {
                $outlineBrush.Dispose()
            }

            $outerGlowPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(110, 120, 230, 255), [float]([Math]::Max(1.0, $Size * 0.02)))
            try {
                $outerGlowPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
                $g.DrawPath($outerGlowPen, $outlinePath)
            }
            finally {
                $outerGlowPen.Dispose()
            }
        }
        finally {
            $outlinePath.Dispose()
        }

        # Center wave ribbon.
        $wavePath = New-Object System.Drawing.Drawing2D.GraphicsPath
        try {
            $wavePath.AddBezier(
                [System.Drawing.PointF]::new([float]($Size * 0.14), [float]($Size * 0.60)),
                [System.Drawing.PointF]::new([float]($Size * 0.33), [float]($Size * 0.76)),
                [System.Drawing.PointF]::new([float]($Size * 0.58), [float]($Size * 0.30)),
                [System.Drawing.PointF]::new([float]($Size * 0.86), [float]($Size * 0.50))
            )

            $waveRect = [System.Drawing.RectangleF]::new(0, [float]($Size * 0.24), $Size, [float]($Size * 0.58))
            $waveBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
                $waveRect,
                ([System.Drawing.ColorTranslator]::FromHtml("#159EF2")),
                ([System.Drawing.ColorTranslator]::FromHtml("#37D7FF")),
                10.0)
            try {
                $wavePen = New-Object System.Drawing.Pen($waveBrush, [float]([Math]::Max(1.0, $Size * 0.13)))
                try {
                    $wavePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
                    $wavePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
                    $wavePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
                    $g.DrawPath($wavePen, $wavePath)
                }
                finally {
                    $wavePen.Dispose()
                }
            }
            finally {
                $waveBrush.Dispose()
            }

            $highlightPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(140, 120, 247, 255), [float]([Math]::Max(1.0, $Size * 0.045)))
            try {
                $highlightPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
                $highlightPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
                $g.DrawPath($highlightPen, $wavePath)
            }
            finally {
                $highlightPen.Dispose()
            }
        }
        finally {
            $wavePath.Dispose()
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

$images = New-Object 'System.Collections.Generic.List[object]'
$previewDir = Join-Path (Split-Path -Parent $OutputIco) "generated"
if ($ExportPng -and -not (Test-Path $previewDir)) {
    New-Item -Path $previewDir -ItemType Directory -Force | Out-Null
}

foreach ($s in ($Sizes | Sort-Object -Unique)) {
    if ($s -lt 16 -or $s -gt 256) {
        throw "Unsupported size: $s"
    }

    $bmp = Render-OutlineCutoutBitmap -Size $s
    try {
        $images.Add([pscustomobject]@{
            Size = $s
            IcoBytes = (Convert-BitmapToIcoImageData -Bitmap $bmp)
        })

        if ($ExportPng) {
            $pngPath = Join-Path $previewDir ("outline_cutout_icon_{0}.png" -f $s)
            $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
    }
    finally {
        $bmp.Dispose()
    }
}

Write-IcoFile -Path $OutputIco -Images $images
Write-Host "ICO generated:" $OutputIco
if ($ExportPng) {
    Write-Host "PNG previews:" $previewDir
}
