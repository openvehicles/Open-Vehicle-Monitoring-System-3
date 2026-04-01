#!/usr/bin/env bash
# setup-toolchain.sh — Install the ESP-IDF toolchain for building OVMS3 firmware.
#
# Run once on a fresh machine.  Safe to re-run — skips steps already done.
#
# What this installs (everything under ~/ovms-toolchain, no root required after
# the pacman step):
#   - Xtensa ESP32 toolchain (GCC 5.2.0, Espressif binary)
#   - OVMS fork of ESP-IDF v3.3 (with submodules)
#   - Python dependencies for the ESP-IDF build tools
#
# Usage:
#   sudo pacman -S --needed base-devel git python python-pip gperf dos2unix
#   bash scripts/setup-toolchain.sh
#   source scripts/setup-toolchain.sh --env   # add to shell session

set -euo pipefail

TOOLCHAIN_DIR="$HOME/ovms-toolchain"
TOOLCHAIN_URL="https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-97-gc752ad5-5.2.0.tar.gz"
TOOLCHAIN_TAR="xtensa-esp32-elf-linux64-1.22.0-97-gc752ad5-5.2.0.tar.gz"
IDF_REPO="https://github.com/openvehicles/esp-idf.git"
IDF_DIR="$TOOLCHAIN_DIR/esp-idf"
VENV_DIR="$TOOLCHAIN_DIR/venv"

# --- Env-only mode: print export lines for sourcing into a shell session ---
if [[ "${1:-}" == "--env" ]]; then
    echo "export IDF_PATH=$IDF_DIR"
    echo "export PATH=$TOOLCHAIN_DIR/xtensa-esp32-elf/bin:\$PATH"
    echo "source $VENV_DIR/bin/activate"
    exit 0
fi

echo "=== OVMS3 toolchain setup ==="
echo "Installing into: $TOOLCHAIN_DIR"
mkdir -p "$TOOLCHAIN_DIR"

# --- Xtensa toolchain ---
TOOLCHAIN_BIN="$TOOLCHAIN_DIR/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc"
if [[ -x "$TOOLCHAIN_BIN" ]]; then
    echo "[skip] Xtensa toolchain already installed"
else
    echo "[1/3] Downloading Xtensa ESP32 toolchain (~175MB)..."
    curl -L "$TOOLCHAIN_URL" -o "/tmp/$TOOLCHAIN_TAR"
    echo "[1/3] Extracting..."
    tar -xzf "/tmp/$TOOLCHAIN_TAR" -C "$TOOLCHAIN_DIR"
    rm "/tmp/$TOOLCHAIN_TAR"
    echo "[1/3] Toolchain ready: $TOOLCHAIN_BIN"
fi

# --- ESP-IDF (OVMS fork) ---
if [[ -d "$IDF_DIR/.git" ]]; then
    echo "[skip] ESP-IDF already cloned at $IDF_DIR"
else
    echo "[2/3] Cloning OVMS ESP-IDF fork (this may take a few minutes)..."
    git clone --recurse-submodules "$IDF_REPO" "$IDF_DIR"
    echo "[2/3] ESP-IDF ready"
fi

# --- Python venv + dependencies ---
if [[ -d "$VENV_DIR" ]]; then
    echo "[skip] Python venv already exists at $VENV_DIR"
else
    echo "[3/3] Creating Python venv..."
    python -m venv "$VENV_DIR"
fi
echo "[3/3] Installing Python dependencies into venv..."
"$VENV_DIR/bin/pip" install -r "$IDF_DIR/requirements.txt"
echo "[3/3] Python deps ready"

echo ""
echo "=== Setup complete ==="
echo ""
echo "Add the following to your shell profile (~/.zshrc or ~/.bashrc):"
echo ""
echo "  export IDF_PATH=$IDF_DIR"
echo "  export PATH=$TOOLCHAIN_DIR/xtensa-esp32-elf/bin:\$PATH"
echo "  source $VENV_DIR/bin/activate"
echo ""
echo "Or activate for this session only:"
echo ""
echo "  source <(bash scripts/setup-toolchain.sh --env)"
