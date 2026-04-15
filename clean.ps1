# Clean AutoDOS build outputs on Windows.
# Usage:
#   .\clean.ps1

param()

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot

$Paths = @(
    "build",
    "dist",
    "linux\build",
    "linux\build-linux",
    "linux\dist",
    "macos\build",
    "macos\dist",
    "macos\staging",
    "macos\AutoDOS.iconset",
    "macos\AutoDOS.icns"
)

Write-Host "Cleaning AutoDOS build outputs..." -ForegroundColor Cyan

foreach ($RelativePath in $Paths) {
    $Path = Join-Path $ProjectDir $RelativePath
    if (Test-Path $Path) {
        Remove-Item $Path -Recurse -Force
        Write-Host "Removed $RelativePath"
    }
}

Write-Host "Clean complete." -ForegroundColor Green
