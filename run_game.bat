@echo off
set PATH=C:\msys64\clang64\bin;C:\msys64\usr\bin;%PATH%
cd /d "%~dp0"
where minecraft.exe >nul 2>nul
if errorlevel 1 (
    echo minecraft.exe was not found in the project directory.
    echo This project must be built before launch.
    pause
    exit /b 1
)

minecraft.exe
if errorlevel 1 (
    echo.
    echo Launch failed. This game needs a desktop Windows session and OpenGL support.
    echo If you are running from a remote/headless environment, the window cannot open.
    pause
)
