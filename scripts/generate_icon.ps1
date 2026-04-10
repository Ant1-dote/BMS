param(
    [string]$OutputIco = (Join-Path $PSScriptRoot "..\assets\icons\bms_app.ico"),
    [int[]]$Sizes = @(16, 24, 32, 48, 64, 128, 256),
    [switch]$ExportPng
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

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

function New-PngBytesFromBitmap {
    param([System.Drawing.Bitmap]$Bitmap)

    $ms = New-Object System.IO.MemoryStream
    try {
        $Bitmap.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        return $ms.ToArray()
    }
    finally {
        $ms.Dispose()
    }
}

function New-IconPngBytes {
    param([int]$Size)

    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)

    try {
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)

        if ($Size -le 48) {
            $margin = [float]([Math]::Max(1.0, $Size * 0.06))
            $bgRect = New-Object System.Drawing.RectangleF($margin, $margin, [float]($Size - 2 * $margin), [float]($Size - 2 * $margin))
            $bgRadius = [float]($Size * 0.22)
            $bgPath = New-RoundedRectPath -X $bgRect.X -Y $bgRect.Y -Width $bgRect.Width -Height $bgRect.Height -Radius $bgRadius
            try {
                $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
                    $bgRect,
                    ([System.Drawing.ColorTranslator]::FromHtml("#1B5076")),
                    ([System.Drawing.ColorTranslator]::FromHtml("#0E3A5B")),
                    45.0
                )
                try {
                    $g.FillPath($bgBrush, $bgPath)
                }
                finally {
                    $bgBrush.Dispose()
                }

                $bgPen = New-Object System.Drawing.Pen(
                    ([System.Drawing.Color]::FromArgb(170, 157, 192, 218)),
                    [float]([Math]::Max(1.0, $Size * 0.04))
                )
                try {
                    $g.DrawPath($bgPen, $bgPath)
                }
                finally {
                    $bgPen.Dispose()
                }
            }
            finally {
                $bgPath.Dispose()
            }

            $bodyX = [float]($Size * 0.20)
            $bodyY = [float]($Size * 0.32)
            $bodyW = [float]($Size * 0.56)
            $bodyH = [float]($Size * 0.36)
            $bodyR = [float]([Math]::Max(2.0, $Size * 0.10))

            $bodyPath = New-RoundedRectPath -X $bodyX -Y $bodyY -Width $bodyW -Height $bodyH -Radius $bodyR
            try {
                $bodyBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#EAF4FF"))
                try {
                    $g.FillPath($bodyBrush, $bodyPath)
                }
                finally {
                    $bodyBrush.Dispose()
                }
            }
            finally {
                $bodyPath.Dispose()
            }

            $terminalBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#EAF4FF"))
            try {
                $terminalRect = New-Object System.Drawing.RectangleF(
                    [float]($bodyX + $bodyW),
                    [float]($bodyY + $bodyH * 0.28),
                    [float]($Size * 0.08),
                    [float]($bodyH * 0.44)
                )
                $g.FillRectangle($terminalBrush, $terminalRect)
            }
            finally {
                $terminalBrush.Dispose()
            }

            $wavePen = New-Object System.Drawing.Pen(
                ([System.Drawing.ColorTranslator]::FromHtml("#2BC18C")),
                [float]([Math]::Max(1.2, $Size * 0.10))
            )
            try {
                $wavePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
                $wavePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
                $wavePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

                $midY = $bodyY + $bodyH * 0.54
                $wavePoints = @(
                    (New-Object System.Drawing.PointF([float]($bodyX + $bodyW * 0.08), [float]$midY)),
                    (New-Object System.Drawing.PointF([float]($bodyX + $bodyW * 0.36), [float]$midY)),
                    (New-Object System.Drawing.PointF([float]($bodyX + $bodyW * 0.52), [float]($midY - $bodyH * 0.18))),
                    (New-Object System.Drawing.PointF([float]($bodyX + $bodyW * 0.72), [float]($midY + $bodyH * 0.20))),
                    (New-Object System.Drawing.PointF([float]($bodyX + $bodyW * 0.92), [float]($midY - $bodyH * 0.10)))
                )
                $g.DrawLines($wavePen, $wavePoints)
            }
            finally {
                $wavePen.Dispose()
            }

            return New-PngBytesFromBitmap -Bitmap $bmp
        }

        $scale = [double]$Size / 1024.0
        $strokeScale = [Math]::Max(1.0, $scale)

        $bgX = 80.0 * $scale
        $bgY = 80.0 * $scale
        $bgW = 864.0 * $scale
        $bgH = 864.0 * $scale
        $bgR = 220.0 * $scale

        $bgRect = New-Object System.Drawing.RectangleF([float]$bgX, [float]$bgY, [float]$bgW, [float]$bgH)
        $bgPath = New-RoundedRectPath -X ([float]$bgX) -Y ([float]$bgY) -Width ([float]$bgW) -Height ([float]$bgH) -Radius ([float]$bgR)

        try {
            $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
                $bgRect,
                ([System.Drawing.ColorTranslator]::FromHtml("#1B5076")),
                ([System.Drawing.ColorTranslator]::FromHtml("#0E3A5B")),
                45.0
            )
            try {
                $g.FillPath($bgBrush, $bgPath)
            }
            finally {
                $bgBrush.Dispose()
            }

            $borderPen = New-Object System.Drawing.Pen(
                ([System.Drawing.Color]::FromArgb(115, 157, 192, 218)),
                [float]([Math]::Max(1.0, 6.0 * $strokeScale))
            )
            try {
                $g.DrawPath($borderPen, $bgPath)
            }
            finally {
                $borderPen.Dispose()
            }
        }
        finally {
            $bgPath.Dispose()
        }

        $batteryX = 230.0 * $scale
        $batteryY = 330.0 * $scale
        $batteryW = 490.0 * $scale
        $batteryH = 340.0 * $scale
        $batteryR = 56.0 * $scale

        $batteryPath = New-RoundedRectPath -X ([float]$batteryX) -Y ([float]$batteryY) -Width ([float]$batteryW) -Height ([float]$batteryH) -Radius ([float]$batteryR)
        try {
            $batteryPen = New-Object System.Drawing.Pen(
                ([System.Drawing.ColorTranslator]::FromHtml("#EAF4FF")),
                [float]([Math]::Max(1.4, 36.0 * $strokeScale))
            )
            try {
                $batteryPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
                $g.DrawPath($batteryPen, $batteryPath)
            }
            finally {
                $batteryPen.Dispose()
            }
        }
        finally {
            $batteryPath.Dispose()
        }

        $terminalBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#EAF4FF"))
        try {
            $terminalRect = New-Object System.Drawing.RectangleF(
                [float](720.0 * $scale),
                [float](430.0 * $scale),
                [float](70.0 * $scale),
                [float](140.0 * $scale)
            )
            $g.FillRectangle($terminalBrush, $terminalRect)
        }
        finally {
            $terminalBrush.Dispose()
        }

        $wavePen = New-Object System.Drawing.Pen(
            ([System.Drawing.ColorTranslator]::FromHtml("#2BC18C")),
            [float]([Math]::Max(1.0, 46.0 * $strokeScale))
        )
        try {
            $wavePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
            $wavePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
            $wavePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

            $wavePoints = @(
                (New-Object System.Drawing.PointF([float](300.0 * $scale), [float](520.0 * $scale))),
                (New-Object System.Drawing.PointF([float](390.0 * $scale), [float](520.0 * $scale))),
                (New-Object System.Drawing.PointF([float](445.0 * $scale), [float](460.0 * $scale))),
                (New-Object System.Drawing.PointF([float](535.0 * $scale), [float](590.0 * $scale))),
                (New-Object System.Drawing.PointF([float](605.0 * $scale), [float](470.0 * $scale))),
                (New-Object System.Drawing.PointF([float](670.0 * $scale), [float](470.0 * $scale)))
            )
            $g.DrawLines($wavePen, $wavePoints)
        }
        finally {
            $wavePen.Dispose()
        }

        if ($Size -ge 32) {
            $shieldBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#E39A2D"))
            try {
                $shieldPoints = @(
                    (New-Object System.Drawing.PointF([float](690.0 * $scale), [float](250.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](795.0 * $scale), [float](300.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](795.0 * $scale), [float](430.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](690.0 * $scale), [float](602.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](585.0 * $scale), [float](430.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](585.0 * $scale), [float](300.0 * $scale)))
                )
                $g.FillPolygon($shieldBrush, $shieldPoints)
            }
            finally {
                $shieldBrush.Dispose()
            }

            $checkPen = New-Object System.Drawing.Pen(
                ([System.Drawing.ColorTranslator]::FromHtml("#1C3C57")),
                [float]([Math]::Max(1.0, 30.0 * $strokeScale))
            )
            try {
                $checkPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
                $checkPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
                $checkPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

                $checkPoints = @(
                    (New-Object System.Drawing.PointF([float](640.0 * $scale), [float](430.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](677.0 * $scale), [float](468.0 * $scale))),
                    (New-Object System.Drawing.PointF([float](746.0 * $scale), [float](378.0 * $scale)))
                )
                $g.DrawLines($checkPen, $checkPoints)
            }
            finally {
                $checkPen.Dispose()
            }
        }

        return New-PngBytesFromBitmap -Bitmap $bmp
    }
    finally {
        $g.Dispose()
        $bmp.Dispose()
    }
}

function Convert-BitmapToIcoImageData {
    param([System.Drawing.Bitmap]$Bitmap)

    if ($Bitmap.Width -ne $Bitmap.Height) {
        throw "Bitmap for icon must be square."
    }

    $size = $Bitmap.Width
    $maskStride = [int]([Math]::Ceiling($size / 32.0) * 4)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    try {
        # BITMAPINFOHEADER for ICO image payload
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

        # XOR bitmap pixels, BGRA, bottom-up
        for ($y = $size - 1; $y -ge 0; --$y) {
            for ($x = 0; $x -lt $size; ++$x) {
                $pixel = $Bitmap.GetPixel($x, $y)
                $bw.Write([byte]$pixel.B)
                $bw.Write([byte]$pixel.G)
                $bw.Write([byte]$pixel.R)
                $bw.Write([byte]$pixel.A)
            }
        }

        # AND mask: all zero keeps alpha channel behavior
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

$uniqueSizes = $Sizes | Sort-Object -Unique
if (-not $uniqueSizes -or $uniqueSizes.Count -eq 0) {
    throw "Sizes cannot be empty."
}

$images = New-Object 'System.Collections.Generic.List[object]'
$previewDir = Join-Path (Split-Path -Parent $OutputIco) "generated"
if ($ExportPng -and -not (Test-Path $previewDir)) {
    New-Item -Path $previewDir -ItemType Directory -Force | Out-Null
}

foreach ($s in $uniqueSizes) {
    if ($s -lt 16 -or $s -gt 256) {
        throw "Unsupported size: $s. Valid range is 16..256."
    }

    $pngBytes = New-IconPngBytes -Size $s

    $bitmapStream = New-Object System.IO.MemoryStream(,$pngBytes)
    $bitmap = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::FromStream($bitmapStream)
        $icoBytes = Convert-BitmapToIcoImageData -Bitmap $bitmap
    }
    finally {
        if ($bitmap -ne $null) {
            $bitmap.Dispose()
        }
        $bitmapStream.Dispose()
    }

    $images.Add([pscustomobject]@{
        Size = $s
        IcoBytes = $icoBytes
        PngBytes = $pngBytes
    })

    if ($ExportPng) {
        $pngPath = Join-Path $previewDir ("bms_{0}.png" -f $s)
        [System.IO.File]::WriteAllBytes($pngPath, $pngBytes)
    }
}

Write-IcoFile -Path $OutputIco -Images $images
Write-Host "ICO generated:" $OutputIco
if ($ExportPng) {
    Write-Host "PNG previews:" $previewDir
}
