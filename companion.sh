#!/usr/bin/env bash
# HomeMonitor Companion Launcher (Linux / macOS)
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================="
echo "    HomeMonitor Companion (Linux/macOS)  "
echo "========================================="
echo ""

# Find Python 3
if command -v python3 >/dev/null 2>&1; then
    PY="python3"
elif command -v python >/dev/null 2>&1; then
    PY="python"
else
    echo "[!] Error: Python 3 is not installed or not in PATH." >&2
    exit 1
fi

# Check for pyserial dependency
if ! $PY -c "import serial" >/dev/null 2>&1; then
    echo "[*] Installing 'pyserial'..."
    $PY -m pip install --user pyserial 2>/dev/null || pip install pyserial 2>/dev/null || {
        echo "[!] Failed to install pyserial automatically."
        echo "[!] Please run: pip install pyserial (or pacman/apt install python-pyserial)"
        exit 1
    }
fi

# Launch companion.py
if [ -f "$DIR/companion.py" ]; then
    exec $PY "$DIR/companion.py" "$@"
else
    echo "[!] Error: companion.py not found in $DIR" >&2
    exit 1
fi
