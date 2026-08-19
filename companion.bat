@echo off
setlocal
title HomeMonitor Companion
echo =========================================
echo    HomeMonitor Companion (Windows)
echo =========================================
echo.

:: Check for Python
where python >nul 2>nul
if %errorlevel% neq 0 (
    where py >nul 2>nul
    if %errorlevel% neq 0 (
        echo [!] Python is not installed or not found in PATH.
        echo [!] Download and install Python from https://www.python.org/
        echo [!] Make sure to check "Add Python to PATH" during installation.
        echo.
        pause
        exit /b 1
    )
    set PYCMD=py -3
) else (
    set PYCMD=python
)

:: Check for pyserial
%PYCMD% -c "import serial" >nul 2>nul
if %errorlevel% neq 0 (
    echo [*] Installing required 'pyserial' package...
    %PYCMD% -m pip install pyserial
    if %errorlevel% neq 0 (
        echo [!] Failed to install pyserial.
        echo [!] Try opening Command Prompt as Administrator and running:
        echo     pip install pyserial
        echo.
        pause
        exit /b 1
    )
)

:: Launch companion.py
if exist "%~dp0companion.py" (
    %PYCMD% "%~dp0companion.py"
) else (
    echo [!] Error: companion.py was not found in "%~dp0"
    pause
    exit /b 1
)
