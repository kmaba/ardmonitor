#!/usr/bin/env bash
# HomeMonitor Companion Launcher (Linux / macOS)
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPANION_PY="$DIR/companion.py"
RAW_URL="https://raw.githubusercontent.com/kmaba/ardmonitor/master/companion.py"

echo "==================================================="
echo "        HomeMonitor Companion Launcher"
echo "==================================================="
echo ""

# -----------------------------------------------------------------
# Step 1: Ensure companion.py exists (download from GitHub if missing)
# -----------------------------------------------------------------
if [ ! -f "$COMPANION_PY" ]; then
    echo "[*] companion.py not found locally. Downloading from GitHub..."
    if command -v curl >/dev/null 2>&1; then
        curl -sSL "$RAW_URL" -o "$COMPANION_PY"
    elif command -v wget >/dev/null 2>&1; then
        wget -q "$RAW_URL" -O "$COMPANION_PY"
    else
        echo "[!] Error: curl or wget is required to download companion.py." >&2
        exit 1
    fi
    echo "[+] Downloaded companion.py successfully."
fi

# -----------------------------------------------------------------
# Step 2: Ensure Python 3 is installed
# -----------------------------------------------------------------
if ! command -v python3 >/dev/null 2>&1 && ! command -v python >/dev/null 2>&1; then
    echo "[!] Python 3 is not installed. Attempting installation..."
    
    # Detect package manager
    if command -v apt-get >/dev/null 2>&1; then
        echo "[*] Running: sudo apt-get update && sudo apt-get install -y python3 python3-pip python3-serial"
        sudo apt-get update && sudo apt-get install -y python3 python3-pip python3-serial
    elif command -v pacman >/dev/null 2>&1; then
        echo "[*] Running: sudo pacman -S --noconfirm python python-pip python-pyserial"
        sudo pacman -S --noconfirm python python-pip python-pyserial
    elif command -v dnf >/dev/null 2>&1; then
        echo "[*] Running: sudo dnf install -y python3 python3-pip python3-pyserial"
        sudo dnf install -y python3 python3-pip python3-pyserial
    elif command -v brew >/dev/null 2>&1; then
        echo "[*] Running: brew install python"
        brew install python
    else
        echo "[!] Unsupported package manager. Please install Python 3 manually." >&2
        exit 1
    fi
fi

# Pick python binary
if command -v python3 >/dev/null 2>&1; then
    PY="python3"
else
    PY="python"
fi

# -----------------------------------------------------------------
# Step 3: Ensure pyserial dependency is installed
# -----------------------------------------------------------------
if ! $PY -c "import serial" >/dev/null 2>&1; then
    echo "[*] Installing 'pyserial' package..."
    $PY -m pip install --user pyserial 2>/dev/null || \
    pip install --user pyserial 2>/dev/null || \
    $PY -m pip install pyserial --break-system-packages 2>/dev/null || \
    pip install pyserial 2>/dev/null || {
        echo "[!] Failed to install pyserial automatically. Please run: pip install pyserial"
        exit 1
    }
fi

# -----------------------------------------------------------------
# Step 4: Run companion.py
# -----------------------------------------------------------------
echo "[*] Starting HomeMonitor Companion..."
echo ""
exec $PY "$COMPANION_PY" "$@"
