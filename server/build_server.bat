@echo off
setlocal
chcp 65001 >nul
gcc "%~dp0server.c" -O2 -std=c11 -Wall -Wextra -lws2_32 -lm -o "%~dp0server.exe"
if errorlevel 1 exit /b 1
echo Built server.exe
