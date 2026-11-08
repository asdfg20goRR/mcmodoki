@echo off
setlocal
cd /d "%~dp0"
if not exist server.exe (
    echo server.exe was not found. Run build_server.bat first.
    exit /b 1
)
if "%~1"=="" (
    server.exe
) else (
    server.exe %1
)
