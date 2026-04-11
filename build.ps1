# AutoDOS Build Script
# Run this from the AutoDOS folder:
#   cd C:\path\to\AutoDOS
#   .\build.ps1

param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot

Write-Host ""
Write-Host "=== AutoDOS Build Script ===" -ForegroundColor Cyan
Write-Host ""

# ── Check prerequisites ───────────────────────────────────────────────────────

# Check CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found." -ForegroundColor Red
    Write-Host "Download from: https://cmake.org/download/" -ForegroundColor Yellow
    Write-Host "Make sure to check 'Add to PATH' during install." -ForegroundColor Yellow
    exit 1
}
Write-Host "✓ CMake found: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green

# Check Git (needed for FetchContent)
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Git not found." -ForegroundColor Red
    Write-Host "Download from: https://git-scm.com/" -ForegroundColor Yellow
    exit 1
}
Write-Host "✓ Git found" -ForegroundColor Green

# Check miniz
if (-not (Test-Path "$ProjectDir\src\miniz.h") -or -not (Test-Path "$ProjectDir\src\miniz.c")) {
    Write-Host ""
    Write-Host "ERROR: miniz not found in src\" -ForegroundColor Red
    Write-Host ""
    Write-Host "Download miniz from:" -ForegroundColor Yellow
    Write-Host "  https://github.com/richgel999/miniz/releases/latest" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Download miniz.h and miniz.c and place both in:" -ForegroundColor Yellow
    Write-Host "  $ProjectDir\src\" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Then run this script again." -ForegroundColor Yellow
    exit 1
}
Write-Host "✓ miniz found" -ForegroundColor Green

# Check games.json
if (-not (Test-Path "$ProjectDir\src\games.json")) {
    Write-Host ""
    Write-Host "WARNING: games.json not found in src\" -ForegroundColor Yellow
    Write-Host "Copy it from your CARTRIDGE install:" -ForegroundColor Yellow
    Write-Host "  CARTRIDGE-v0.4.1 (39)\cartridge\rom\games.json" -ForegroundColor Cyan
    Write-Host "  → $ProjectDir\src\games.json" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Continuing without it (AutoDOS will use scorer only)..." -ForegroundColor Yellow
}

# Check DOSBox
if (-not (Test-Path "$ProjectDir\dosbox\dosbox.exe")) {
    Write-Host ""
    Write-Host "WARNING: dosbox\dosbox.exe not found." -ForegroundColor Yellow
    Write-Host "Copy DOSBox Staging from your CARTRIDGE install:" -ForegroundColor Yellow
    Write-Host "  CARTRIDGE-v0.4.1 (39)\cartridge\native\dosbox-staging-win\" -ForegroundColor Cyan
    Write-Host "  → $ProjectDir\dosbox\" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Continuing build (you can add DOSBox later)..." -ForegroundColor Yellow
}

# ── Build ─────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "Building AutoDOS ($Config)..." -ForegroundColor Cyan

$BuildDir = "$ProjectDir\build"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# Configure
Write-Host ""
Write-Host "Configuring with CMake..." -ForegroundColor White
Push-Location $BuildDir
try {
    cmake .. -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        # Try VS 2019
        cmake .. -G "Visual Studio 16 2019" -A x64
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: CMake configuration failed." -ForegroundColor Red
        Write-Host "Make sure Visual Studio is installed with C++ workload." -ForegroundColor Yellow
        exit 1
    }
} finally {
    Pop-Location
}

# Build
Write-Host ""
Write-Host "Compiling..." -ForegroundColor White
Push-Location $BuildDir
try {
    cmake --build . --config $Config --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed." -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# ── Package output ────────────────────────────────────────────────────────────

$ExePath = "$BuildDir\bin\$Config\AutoDOS.exe"
if (-not (Test-Path $ExePath)) {
    # Try alternate output location
    $ExePath = "$BuildDir\$Config\AutoDOS.exe"
}

if (Test-Path $ExePath) {
    $OutDir = "$ProjectDir\dist"
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

    Copy-Item $ExePath "$OutDir\AutoDOS.exe" -Force

    # Copy games.json if available
    if (Test-Path "$ProjectDir\src\games.json") {
        Copy-Item "$ProjectDir\src\games.json" "$OutDir\games.json" -Force
    }

    # Copy DOSBox if available
    if (Test-Path "$ProjectDir\dosbox\dosbox.exe") {
        $dosboxDist = "$OutDir\dosbox"
        New-Item -ItemType Directory -Force -Path $dosboxDist | Out-Null
        Copy-Item "$ProjectDir\dosbox\*" $dosboxDist -Recurse -Force
    }

    Write-Host ""
    Write-Host "=== Build successful! ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Output: $OutDir\AutoDOS.exe" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "To run:" -ForegroundColor White
    Write-Host "  cd $OutDir" -ForegroundColor Cyan
    Write-Host "  .\AutoDOS.exe" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "ERROR: AutoDOS.exe not found after build." -ForegroundColor Red
    Write-Host "Check build output above for errors." -ForegroundColor Yellow
    exit 1
}
