param(
    [Parameter(Mandatory = $true)]
    [string]$SourceImage,
    [string]$OutputIco = "",
    [int[]]$Sizes = @(16, 24, 32, 48, 64, 128, 256),
    [switch]$ExportPng,
    [switch]$StripBlueBase
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($OutputIco)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $OutputIco = Join-Path $scriptDir "..\assets\icons\bms_app.ico"
}

function Convert-RgbToHsv {
    param(
        [byte]$R,
        [byte]$G,
        [byte]$B
    )

    $rf = $R / 255.0
    $gf = $G / 255.0
    $bf = $B / 255.0

    $max = [Math]::Max($rf, [Math]::Max($gf, $bf))
    $min = [Math]::Min($rf, [Math]::Min($gf, $bf))
    $delta = $max - $min

    $h = 0.0
    if ($delta -gt 0.00001) {
        if ($max -eq $rf) {
            $h = 60.0 * ((($gf - $bf) / $delta) % 6.0)
        }
        elseif ($max -eq $gf) {
            $h = 60.0 * ((($bf - $rf) / $delta) + 2.0)
        }
        else {
            $h = 60.0 * ((($rf - $gf) / $delta) + 4.0)
        }
    }
    if ($h -lt 0.0) {
        $h += 360.0
    }

    $s = if ($max -le 0.00001) { 0.0 } else { $delta / $max }
    [pscustomobject]@{
        Hue = $h
        Sat = $s
        Val = $max
    }
}

function Remove-DeepBlueBase {
    param([System.Drawing.Bitmap]$Bitmap)

    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $p = $Bitmap.GetPixel($x, $y)
            if ($p.A -eq 0) {
                continue
            }

            $hsv = Convert-RgbToHsv -R $p.R -G $p.G -B $p.B
            $h = [double]$hsv.Hue
            $s = [double]$hsv.Sat
            $v = [double]$hsv.Val

            $blueCyanLift = ([int]$p.G + [int]$p.B) - (2 * [int]$p.R)
            $brightAccent = ($h -ge 170.0 -and $h -le 225.0 -and $s -ge 0.30 -and $v -ge 0.42)
            $highlightAccent = ($h -ge 165.0 -and $h -le 235.0 -and $blueCyanLift -ge 58 -and $v -ge 0.36)

            if ($brightAccent -or $highlightAccent) {
                $alphaScale = [Math]::Min(1.0, [Math]::Max(0.0, ($v - 0.34) / 0.22))
                $newA = [int][Math]::Round($p.A * $alphaScale)
                if ($newA -lt 0) { $newA = 0 }
                if ($newA -gt 255) { $newA = 255 }
                $Bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($newA, $p.R, $p.G, $p.B))
            }
            else {
                $Bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, $p.R, $p.G, $p.B))
            }
        }
    }
}

function Crop-ToVisibleBounds {
    param([System.Drawing.Bitmap]$Source)

    $minX = $Source.Width
    $minY = $Source.Height
    $maxX = -1
    $maxY = -1

    for ($y = 0; $y -lt $Source.Height; ++$y) {
        for ($x = 0; $x -lt $Source.Width; ++$x) {
            if ($Source.GetPixel($x, $y).A -gt 0) {
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }

    if ($maxX -lt 0 -or $maxY -lt 0) {
        return [System.Drawing.Bitmap]::new($Source)
    }

    $cropRect = [System.Drawing.Rectangle]::new(
        [int]$minX,
        [int]$minY,
        [int]($maxX - $minX + 1),
        [int]($maxY - $minY + 1))

    return $Source.Clone($cropRect, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function Resize-ImageToBitmap {
    param(
        [System.Drawing.Bitmap]$Source,
        [int]$Size,
        [bool]$StripBase
    )

    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $inset = [int][Math]::Round($Size * 0.04)
        $target = [int]($Size - $inset * 2)
        if ($target -lt 4) {
            $target = $Size
            $inset = 0
        }

        $srcW = [double]$Source.Width
        $srcH = [double]$Source.Height
        # Icon conversion uses cover mode in rounded rectangle area.
        $scale = [Math]::Max($target / $srcW, $target / $srcH)
        $drawW = [Math]::Round($srcW * $scale)
        $drawH = [Math]::Round($srcH * $scale)
        $drawX = [Math]::Floor(($target - $drawW) / 2.0) + $inset
        $drawY = [Math]::Floor(($target - $drawH) / 2.0) + $inset

        $radius = [int][Math]::Round($target * 0.24)
        if ($radius -lt 2) {
            $radius = 2
        }

        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        try {
            $diameter = $radius * 2
            $path.AddArc($inset, $inset, $diameter, $diameter, 180, 90)
            $path.AddArc($inset + $target - $diameter, $inset, $diameter, $diameter, 270, 90)
            $path.AddArc($inset + $target - $diameter, $inset + $target - $diameter, $diameter, $diameter, 0, 90)
            $path.AddArc($inset, $inset + $target - $diameter, $diameter, $diameter, 90, 90)
            $path.CloseFigure()
            $graphics.SetClip($path)

            $graphics.DrawImage(
                $Source,
                [System.Drawing.Rectangle]::new([int]$drawX, [int]$drawY, [int]$drawW, [int]$drawH),
                [System.Drawing.Rectangle]::new(0, 0, $Source.Width, $Source.Height),
                [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $path.Dispose()
            $graphics.ResetClip()
        }

        if ($StripBase) {
            Remove-DeepBlueBase -Bitmap $bitmap
        }

        return $bitmap
    }
    finally {
        $graphics.Dispose()
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
$sourceCropped = $null
try {
    $sourceCropped = Crop-ToVisibleBounds -Source $source

    $uniqueSizes = $Sizes | Sort-Object -Unique
    $images = New-Object 'System.Collections.Generic.List[object]'

    $previewDir = Join-Path (Split-Path -Parent $OutputIco) "generated"
    if ($ExportPng -and -not (Test-Path $previewDir)) {
        New-Item -Path $previewDir -ItemType Directory -Force | Out-Null
    }

    foreach ($s in $uniqueSizes) {
        if ($s -lt 16 -or $s -gt 256) {
            throw "Unsupported size: $s. Valid range is 16..256."
        }

        $bitmap = Resize-ImageToBitmap -Source $sourceCropped -Size $s -StripBase:$StripBlueBase
        try {
            $icoBytes = Convert-BitmapToIcoImageData -Bitmap $bitmap
            $images.Add([pscustomobject]@{
                Size = $s
                IcoBytes = $icoBytes
            })

            if ($ExportPng) {
                $pngPath = Join-Path $previewDir ("source_icon_{0}.png" -f $s)
                $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }

    Write-IcoFile -Path $OutputIco -Images $images
    Write-Host "ICO generated:" $OutputIco
    if ($ExportPng) {
        Write-Host "PNG previews:" $previewDir
    }
}
finally {
    if ($null -ne $sourceCropped) {
        $sourceCropped.Dispose()
    }
    $source.Dispose()
}
