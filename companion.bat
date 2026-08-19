@echo off
setlocal EnableDelayedExpansion
title HomeMonitor Companion (Windows)

:: GitHub raw URL for downloading companion.py if missing
set "RAW_COMPANION_URL=https://raw.githubusercontent.com/kmaba/ardmonitor/master/companion.py"
set "SCRIPT_DIR=%~dp0"
set "COMPANION_PY=%SCRIPT_DIR%companion.py"

echo ===================================================
echo        HomeMonitor Companion Launcher
echo ===================================================
echo.

:: -----------------------------------------------------------------
:: Step 1: Ensure companion.py exists (download from GitHub if not)
:: -----------------------------------------------------------------
if not exist "%COMPANION_PY%" (
    echo [*] companion.py not found locally. Downloading from GitHub...
    curl --version >nul 2>nul
    if !errorlevel! equ 0 (
        curl -sSL "%RAW_COMPANION_URL%" -o "%COMPANION_PY%"
    ) else (
        powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object System.Net.WebClient).DownloadFile('%RAW_COMPANION_URL%', '%COMPANION_PY%')"
    )
    if exist "%COMPANION_PY%" (
        echo [+] Downloaded companion.py successfully.
    ) else (
        echo [!] Error: Failed to download companion.py. Please check your internet connection.
        pause
        exit /b 1
    )
)

:: -----------------------------------------------------------------
:: Step 2: Check for Python
:: -----------------------------------------------------------------
set "PYCMD="

:: Check standard python in PATH
where python >nul 2>nul
if !errorlevel! equ 0 (
    python -c "import sys; sys.exit(0 if sys.version_info[0]>=3 else 1)" >nul 2>nul
    if !errorlevel! equ 0 set "PYCMD=python"
)

:: Check py launcher
if not defined PYCMD (
    where py >nul 2>nul
    if !errorlevel! equ 0 (
        py -3 -c "import sys" >nul 2>nul
        if !errorlevel! equ 0 set "PYCMD=py -3"
    )
)

:: Check common default Windows install paths
if not defined PYCMD (
    if exist "%LocalAppData%\Programs\Python\Python312\python.exe" (
        set "PYCMD=%LocalAppData%\Programs\Python\Python312\python.exe"
    ) else if exist "%LocalAppData%\Programs\Python\Python311\python.exe" (
        set "PYCMD=%LocalAppData%\Programs\Python\Python311\python.exe"
    ) else if exist "%ProgramFiles%\Python312\python.exe" (
        set "PYCMD=%ProgramFiles%\Python312\python.exe"
    ) else if exist "%ProgramFiles%\Python311\python.exe" (
        set "PYCMD=%ProgramFiles%\Python311\python.exe"
    )
)

:: -----------------------------------------------------------------
:: Step 3: Install Python if missing (Request UAC if needed)
:: -----------------------------------------------------------------
if not defined PYCMD (
    echo [!] Python 3 is not installed on this system.
    echo [*] Preparing to download and install Python automatically...
    echo.

    :: Check for Administrator permissions
    net session >nul 2>nul
    if !errorlevel! neq 0 (
        echo [*] Requesting Administrator privileges for installation (UAC prompt)...
        powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
        exit /b 0
    )

    :: Try installing via winget first
    where winget >nul 2>nul
    if !errorlevel! equ 0 (
        echo [*] Installing Python via Windows Package Manager (winget)...
        winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements
    ) else (
        :: Download and run official Python installer
        echo [*] Downloading official Python installer...
        set "INSTALLER=%TEMP%\python_installer.exe"
        powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object System.Net.WebClient).DownloadFile('https://www.python.org/ftp/python/3.12.5/python-3.12.5-amd64.exe', '$env:TEMP\python_installer.exe')"
        
        echo [*] Installing Python quietly (adding to PATH)...
        "%INSTALLER%" /quiet InstallAllUsers=1 PrependPath=1 Include_pip=1
        del /f /q "%INSTALLER%" >nul 2>nul
    )

    :: Refresh environment PATH
    set "PATH=%ProgramFiles%\Python312;%ProgramFiles%\Python312\Scripts;%LocalAppData%\Programs\Python\Python312;%LocalAppData%\Programs\Python\Python312\Scripts;%PATH%"
    
    where python >nul 2>nul
    if !errorlevel! equ 0 (
        set "PYCMD=python"
    ) else if exist "%ProgramFiles%\Python312\python.exe" (
        set "PYCMD=%ProgramFiles%\Python312\python.exe"
    ) else if exist "%LocalAppData%\Programs\Python\Python312\python.exe" (
        set "PYCMD=%LocalAppData%\Programs\Python\Python312\python.exe"
    ) else (
        echo [!] Python installation finished. Please restart this script or your PC if PATH has not updated.
        pause
        exit /b 1
    )
    echo [+] Python installed successfully.
)

:: -----------------------------------------------------------------
:: Step 4: Ensure pyserial dependency is installed
:: -----------------------------------------------------------------
%PYCMD% -c "import serial" >nul 2>nul
if !errorlevel! neq 0 (
    echo [*] Installing required 'pyserial' library...
    %PYCMD% -m pip install pyserial
    if !errorlevel! neq 0 (
        echo [!] Failed to install pyserial automatically.
        echo [!] Trying user-level pip install...
        %PYCMD% -m pip install --user pyserial
    )
)

:: -----------------------------------------------------------------
:: Step 5: Launch companion.py
:: -----------------------------------------------------------------
echo [*] Starting HomeMonitor Companion...
echo.
%PYCMD% "%COMPANION_PY%"

if !errorlevel! neq 0 (
    echo.
    echo [!] Program exited with an error.
    pause
)
