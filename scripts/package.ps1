param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$BuildDir = "",
    [string]$QtDir = "C:\Qt\6.11.0\mingw_64",
    [string]$InnoCompiler = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    [string]$AppVersion = "2.2.1"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-AppExe {
    param(
        [string]$Root,
        [string]$UserBuildDir
    )

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($UserBuildDir -ne "") {
        $candidates.Add((Join-Path $UserBuildDir "appBMS.exe"))
    }

    $candidates.Add((Join-Path $Root "build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\appBMS.exe"))
    $candidates.Add((Join-Path $Root "build\appBMS.exe"))

    $existing = @()
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $existing += (Get-Item $candidate)
        }
    }

    if ($existing.Count -eq 0) {
        throw "appBMS.exe not found. Build the project first."
    }

    $latest = $existing | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    return $latest.FullName
}

$resolvedRoot = (Resolve-Path $RepoRoot).Path
$resolvedBuildDir = ""
if ($BuildDir -ne "") {
    $resolvedBuildDir = (Resolve-Path $BuildDir).Path
}

$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt"
}
if (-not (Test-Path $InnoCompiler)) {
    throw "Inno Setup compiler not found: $InnoCompiler"
}

$appExe = Resolve-AppExe -Root $resolvedRoot -UserBuildDir $resolvedBuildDir
$issFile = Join-Path $resolvedRoot "installer\BMS.iss"
if (-not (Test-Path $issFile)) {
    throw "Inno script not found: $issFile"
}

$distRoot = Join-Path $resolvedRoot "dist"
$distApp = Join-Path $distRoot "app"
$distInstaller = Join-Path $distRoot "installer"

if (Test-Path $distApp) {
    Remove-Item -Path $distApp -Recurse -Force
}
New-Item -ItemType Directory -Path $distApp -Force | Out-Null
New-Item -ItemType Directory -Path $distInstaller -Force | Out-Null

$targetExe = Join-Path $distApp "appBMS.exe"
Copy-Item -Path $appExe -Destination $targetExe -Force

$iconSource = Join-Path $resolvedRoot "assets\icons\bms_app.ico"
if (Test-Path $iconSource) {
    Copy-Item -Path $iconSource -Destination (Join-Path $distApp "bms_app.ico") -Force
}

$rawIconImage = Join-Path $resolvedRoot "assets\icons\source_icon.jpg"
if (Test-Path $rawIconImage) {
    Copy-Item -Path $rawIconImage -Destination (Join-Path $distApp "source_icon.jpg") -Force
}

Write-Host "[1/3] 部署 Qt 运行时..."
& $windeployqt --force --qmldir $resolvedRoot --dir $distApp $targetExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code: $LASTEXITCODE"
}

Write-Host "[2/3] 编译 Inno Setup 安装包..."
& $InnoCompiler "/DSourceDir=$distApp" "/DOutputDir=$distInstaller" "/DMyAppVersion=$AppVersion" $issFile
if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code: $LASTEXITCODE"
}

Write-Host "[3/3] Done"
Write-Host "App deploy dir: $distApp"
Write-Host "Installer output dir: $distInstaller"
