param(
    [Parameter(Mandatory = $true)]
    [string]$SourceImage,
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

function Resize-CoverBitmap {
    param(
        [System.Drawing.Bitmap]$Source,
        [int]$Size
    )

    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $srcW = [double]$Source.Width
        $srcH = [double]$Source.Height
        $dst = [double]$Size
        $scale = [Math]::Max($dst / $srcW, $dst / $srcH)

        $drawW = [Math]::Round($srcW * $scale)
        $drawH = [Math]::Round($srcH * $scale)
        $drawX = [Math]::Floor(($dst - $drawW) / 2.0)
        $drawY = [Math]::Floor(($dst - $drawH) / 2.0)

        $g.DrawImage(
            $Source,
            [System.Drawing.Rectangle]::new([int]$drawX, [int]$drawY, [int]$drawW, [int]$drawH),
            [System.Drawing.Rectangle]::new(0, 0, $Source.Width, $Source.Height),
            [System.Drawing.GraphicsUnit]::Pixel)

        return $bmp
    }
    finally {
        $g.Dispose()
    }
}

function Get-BackgroundColor {
    param([System.Drawing.Bitmap]$Bitmap)

    $w = $Bitmap.Width
    $h = $Bitmap.Height
    $sample = [Math]::Max(2, [int][Math]::Floor([Math]::Min($w, $h) * 0.06))

    $sumR = 0
    $sumG = 0
    $sumB = 0
    $count = 0

    $regions = @(
        @{ X = 0; Y = 0 },
        @{ X = $w - $sample; Y = 0 },
        @{ X = 0; Y = $h - $sample },
        @{ X = $w - $sample; Y = $h - $sample }
    )

    foreach ($r in $regions) {
        for ($y = [int]$r.Y; $y -lt [int]$r.Y + $sample; ++$y) {
            for ($x = [int]$r.X; $x -lt [int]$r.X + $sample; ++$x) {
                $p = $Bitmap.GetPixel($x, $y)
                $sumR += $p.R
                $sumG += $p.G
                $sumB += $p.B
                ++$count
            }
        }
    }

    if ($count -eq 0) {
        return [System.Drawing.Color]::FromArgb(0, 0, 0)
    }

    return [System.Drawing.Color]::FromArgb(
        [int]($sumR / $count),
        [int]($sumG / $count),
        [int]($sumB / $count)
    )
}

function Remove-Background {
    param([System.Drawing.Bitmap]$Bitmap)

    $bg = Get-BackgroundColor -Bitmap $Bitmap
    $w = $Bitmap.Width
    $h = $Bitmap.Height

    function IsBackgroundLike([System.Drawing.Color]$p, [System.Drawing.Color]$ref) {
        $dr = [double]($p.R - $ref.R)
        $dg = [double]($p.G - $ref.G)
        $db = [double]($p.B - $ref.B)
        $dist = [Math]::Sqrt($dr * $dr + $dg * $dg + $db * $db)

        $rf = $p.R / 255.0
        $gf = $p.G / 255.0
        $bf = $p.B / 255.0
        $max = [Math]::Max($rf, [Math]::Max($gf, $bf))
        $min = [Math]::Min($rf, [Math]::Min($gf, $bf))
        $sat = if ($max -le 0.0001) { 0.0 } else { ($max - $min) / $max }
        $val = $max

        if ($dist -le 18.0) {
            return $true
        }
        if ($dist -le 24.0 -and $sat -le 0.28 -and $val -le 0.35) {
            return $true
        }
        return $false
    }

    $visited = New-Object bool[] ($w * $h)
    $queue = New-Object 'System.Collections.Generic.Queue[System.Drawing.Point]'

    function EnqueueIfBackground([int]$x, [int]$y) {
        if ($x -lt 0 -or $x -ge $w -or $y -lt 0 -or $y -ge $h) {
            return
        }
        $idx = $y * $w + $x
        if ($visited[$idx]) {
            return
        }

        $px = $Bitmap.GetPixel($x, $y)
        if (IsBackgroundLike $px $bg) {
            $visited[$idx] = $true
            $queue.Enqueue([System.Drawing.Point]::new($x, $y))
        }
    }

    for ($x = 0; $x -lt $w; ++$x) {
        EnqueueIfBackground $x 0
        EnqueueIfBackground $x ($h - 1)
    }
    for ($y = 0; $y -lt $h; ++$y) {
        EnqueueIfBackground 0 $y
        EnqueueIfBackground ($w - 1) $y
    }

    while ($queue.Count -gt 0) {
        $p = $queue.Dequeue()
        EnqueueIfBackground ($p.X + 1) $p.Y
        EnqueueIfBackground ($p.X - 1) $p.Y
        EnqueueIfBackground $p.X ($p.Y + 1)
        EnqueueIfBackground $p.X ($p.Y - 1)
    }

    for ($y = 0; $y -lt $h; ++$y) {
        for ($x = 0; $x -lt $w; ++$x) {
            $idx = $y * $w + $x
            $p = $Bitmap.GetPixel($x, $y)
            if ($visited[$idx]) {
                $Bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, $p.R, $p.G, $p.B))
            } else {
                $Bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $p.R, $p.G, $p.B))
            }
        }
    }
}

function Fit-AlphaContent {
    param(
        [System.Drawing.Bitmap]$Source,
        [int]$Size
    )

    $minX = $Size
    $minY = $Size
    $maxX = -1
    $maxY = -1

    for ($y = 0; $y -lt $Size; ++$y) {
        for ($x = 0; $x -lt $Size; ++$x) {
            $a = $Source.GetPixel($x, $y).A
            if ($a -gt 8) {
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }

    if ($maxX -lt $minX -or $maxY -lt $minY) {
        return $Source
    }

    $contentW = $maxX - $minX + 1
    $contentH = $maxY - $minY + 1
    $pad = [Math]::Max(1, [int][Math]::Round($Size * 0.08))
    $targetW = $Size - ($pad * 2)
    $targetH = $Size - ($pad * 2)

    $scale = [Math]::Min($targetW / [double]$contentW, $targetH / [double]$contentH)
    $drawW = [int][Math]::Round($contentW * $scale)
    $drawH = [int][Math]::Round($contentH * $scale)
    $drawX = [int][Math]::Floor(($Size - $drawW) / 2.0)
    $drawY = [int][Math]::Floor(($Size - $drawH) / 2.0)

    $out = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($out)
    try {
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $g.DrawImage(
            $Source,
            [System.Drawing.Rectangle]::new($drawX, $drawY, $drawW, $drawH),
            [System.Drawing.Rectangle]::new($minX, $minY, $contentW, $contentH),
            [System.Drawing.GraphicsUnit]::Pixel)

        return $out
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

if (-not (Test-Path $SourceImage)) {
    throw "Source image not found: $SourceImage"
}

$source = [System.Drawing.Bitmap]::FromFile((Resolve-Path $SourceImage))
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

        $cover = Resize-CoverBitmap -Source $source -Size $s
        try {
            Remove-Background -Bitmap $cover
            $fit = Fit-AlphaContent -Source $cover -Size $s
            try {
                $images.Add([pscustomobject]@{
                    Size = $s
                    IcoBytes = (Convert-BitmapToIcoImageData -Bitmap $fit)
                })

                if ($ExportPng) {
                    $pngPath = Join-Path $previewDir ("cutout_icon_{0}.png" -f $s)
                    $fit.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
                }
            }
            finally {
                if (-not [object]::ReferenceEquals($fit, $cover)) {
                    $fit.Dispose()
                }
            }
        }
        finally {
            $cover.Dispose()
        }
    }

    Write-IcoFile -Path $OutputIco -Images $images
    Write-Host "ICO generated:" $OutputIco
    if ($ExportPng) {
        Write-Host "PNG previews:" $previewDir
    }
}
finally {
    $source.Dispose()
}
