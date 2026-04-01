#!/usr/bin/env bash
# build.sh — Build OVMS3 firmware for the current git branch.
#
# Prerequisites: run scripts/setup-toolchain.sh once first, then either
# add the env exports to your shell profile or run:
#
#   source <(bash scripts/setup-toolchain.sh --env)
#
# Usage:
#   bash scripts/build.sh            # build only
#   bash scripts/build.sh --deploy   # build then serve binary for OTA flash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OVMS_DIR="$REPO_ROOT/vehicle/OVMS.V3"
BINARY="$OVMS_DIR/build/ovms3.bin"

# --- Sanity checks ---
if [[ -z "${IDF_PATH:-}" ]]; then
    echo "ERROR: IDF_PATH is not set."
    echo "Run:  source <(bash scripts/setup-toolchain.sh --env)"
    exit 1
fi

if ! command -v xtensa-esp32-elf-gcc &>/dev/null; then
    echo "ERROR: xtensa-esp32-elf-gcc not in PATH."
    echo "Run:  source <(bash scripts/setup-toolchain.sh --env)"
    exit 1
fi

BRANCH=$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)
COMMIT=$(git -C "$REPO_ROOT" rev-parse --short HEAD)

echo "=== OVMS3 build ==="
echo "Branch: $BRANCH  ($COMMIT)"
echo "IDF:    $IDF_PATH"
echo ""

# --- Tests must pass before we build anything ---
TEST_DIR="$REPO_ROOT/vehicle/OVMS.V3/components/vehicle_vwegolf/tests"
echo "[test] Running native test suite..."
if ! make -C "$TEST_DIR" --no-print-directory 2>&1; then
    echo ""
    echo "BUILD ABORTED: fix test failures before building firmware."
    exit 1
fi
echo ""

# --- sdkconfig: create from default if missing ---
if [[ ! -f "$OVMS_DIR/sdkconfig" ]]; then
    echo "[setup] No sdkconfig found, copying sdkconfig.default.hw31..."
    cp "$OVMS_DIR/support/sdkconfig.default.hw31" "$OVMS_DIR/sdkconfig"
fi

# --- Build ---
echo "[build] Running make -j$(nproc)..."
make -C "$OVMS_DIR" -j"$(nproc)"

echo ""
echo "Build complete: $BINARY"
echo ""

if [[ "${1:-}" != "--deploy" ]]; then
    echo "To deploy:"
    echo "  1. Connect laptop to OVMS hotspot (192.168.4.1)"
    echo "  2. Run:  bash scripts/build.sh --deploy"
    echo "  3. On OVMS shell: ota flash http http://<your-ip>:8080/ovms3.bin"
    exit 0
fi

# --- Deploy: serve the binary for OTA ---
echo "=== OTA deploy ==="
echo ""
echo "Connect your laptop to the OVMS WiFi hotspot first."
echo ""

# Find our IP on the OVMS hotspot network (192.168.4.x)
LAPTOP_IP=$(ip -4 addr show | grep -oP '192\.168\.4\.\d+' | head -1 || true)
if [[ -z "$LAPTOP_IP" ]]; then
    echo "WARNING: Could not detect 192.168.4.x address."
    echo "         Check your IP with: ip addr"
    echo "         Then use: ota flash http http://<your-ip>:8080/ovms3.bin"
    LAPTOP_IP="<your-ip>"
fi

echo "Serving $BINARY on port 8080..."
echo ""
echo "Run this on the OVMS shell (http://192.168.4.1 or SSH):"
echo ""
echo "  ota flash http http://$LAPTOP_IP:8080/ovms3.bin"
echo ""
echo "Press Ctrl-C when the flash is complete."
echo ""

cd "$OVMS_DIR/build"
python -m http.server 8080
