# AutoDOS Windows build script
# Usage:
#   .\build-windows.ps1
#   .\build-windows.ps1 -Config Debug

param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [string]$BuildDir = "build",
    [string]$Generator = "",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot
$BuildPath = Join-Path $ProjectDir $BuildDir
$DistDir = Join-Path $ProjectDir "dist"

function Require-Command {
    param(
        [string]$Name,
        [string]$InstallHint
    )

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: $Name was not found." -ForegroundColor Red
        Write-Host $InstallHint -ForegroundColor Yellow
        exit 1
    }
}

Write-Host ""
Write-Host "=== AutoDOS Windows Build ===" -ForegroundColor Cyan
Write-Host ""

Require-Command "cmake" "Install CMake and add it to PATH: https://cmake.org/download/"
Require-Command "git" "Install Git and add it to PATH: https://git-scm.com/download/win"

if (-not (Test-Path (Join-Path $ProjectDir "src\miniz.h")) -or
    -not (Test-Path (Join-Path $ProjectDir "src\miniz.c"))) {
    Write-Host "ERROR: src\miniz.h and src\miniz.c are required." -ForegroundColor Red
    Write-Host "Download them from https://github.com/richgel999/miniz/releases and place them in src\." -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path (Join-Path $ProjectDir "src\games.json"))) {
    Write-Host "WARNING: src\games.json was not found. The build will continue." -ForegroundColor Yellow
}

New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null

$ConfigureArgs = @("-S", $ProjectDir, "-B", $BuildPath)
if ($Generator -ne "") {
    $ConfigureArgs += @("-G", $Generator)
    if ($Arch -ne "") {
        $ConfigureArgs += @("-A", $Arch)
    }
} else {
    $ConfigureArgs += @("-G", "Visual Studio 17 2022", "-A", $Arch)
}

Write-Host "Configuring CMake..." -ForegroundColor White
& cmake @ConfigureArgs

if ($LASTEXITCODE -ne 0 -and $Generator -eq "") {
    Write-Host "Visual Studio 2022 generator failed; trying Visual Studio 2019..." -ForegroundColor Yellow
    & cmake -S $ProjectDir -B $BuildPath -G "Visual Studio 16 2019" -A $Arch
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configuration failed." -ForegroundColor Red
    Write-Host "Install Visual Studio with the Desktop development with C++ workload." -ForegroundColor Yellow
    exit 1
}

Write-Host "Building $Config..." -ForegroundColor White
& cmake --build $BuildPath --config $Config --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed." -ForegroundColor Red
    exit 1
}

$ExePath = Join-Path $BuildPath "bin\$Config\AutoDOS.exe"
if (-not (Test-Path $ExePath)) {
    $ExePath = Join-Path $BuildPath "$Config\AutoDOS.exe"
}

if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: AutoDOS.exe was not found after building." -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Copy-Item $ExePath (Join-Path $DistDir "AutoDOS.exe") -Force

if (Test-Path (Join-Path $ProjectDir "src\games.json")) {
    Copy-Item (Join-Path $ProjectDir "src\games.json") (Join-Path $DistDir "games.json") -Force
}

if (Test-Path (Join-Path $ProjectDir "dosbox\dosbox.exe")) {
    $DosboxDist = Join-Path $DistDir "dosbox"
    New-Item -ItemType Directory -Force -Path $DosboxDist | Out-Null
    Copy-Item (Join-Path $ProjectDir "dosbox\*") $DosboxDist -Recurse -Force
}

Write-Host ""
Write-Host "Build complete." -ForegroundColor Green
Write-Host "Output: $(Join-Path $DistDir 'AutoDOS.exe')" -ForegroundColor Cyan
