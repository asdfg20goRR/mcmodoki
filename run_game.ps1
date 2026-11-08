$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:Path = 'C:\msys64\clang64\bin;C:\msys64\usr\bin;' + $env:Path
Set-Location $root

$exe = Join-Path $root 'minecraft.exe'
if (-not (Test-Path $exe)) {
    Write-Host 'minecraft.exe not found in project directory.' -ForegroundColor Red
    Read-Host 'Press Enter to exit'
    exit 1
}

Write-Host 'Launching Minecraft-like Demo...'
& $exe
$code = $LASTEXITCODE
if ($code -ne 0) {
    Write-Host "Game exited with code: $code" -ForegroundColor Yellow
    Write-Host 'This usually means the game could not create a desktop OpenGL window.' -ForegroundColor Yellow
    Write-Host 'Please run it on a real Windows desktop session (not a headless/remote terminal).' -ForegroundColor Yellow
    Read-Host 'Press Enter to exit'
}
exit $code
