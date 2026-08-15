@echo off

cd /d "%~dp0"

if "%1"=="clean" (
    rmdir /s /q build
)

cmake -S . -B build
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

build\server.exe