#!/usr/bin/env bash
# capture.sh — Capture CAN frames from the OVMS, named after the running firmware.
#
# Usage (from anywhere in the repo, or directly from tests/):
#   bash capture.sh [bus]
#
# Arguments:
#   bus   CAN bus to capture (default: can3).
#
# Requires:
#   - Laptop on the OVMS WiFi hotspot (192.168.4.1)
#   - nc (netcat)
#   - SSH key for ovms@192.168.4.1 (used for version query and log start/stop)

set -euo pipefail

OVMS="192.168.4.1"
BUS="${1:-can3}"
SSH_USER="ovms"

cd "$(dirname "${BASH_SOURCE[0]}")"
OUTDIR="candumps"

# ---------------------------------------------------------------------------
# 1. SSH helper (defined early — also used for version query).
# ---------------------------------------------------------------------------
ssh_cmd() {
    # OVMS uses an RSA host key. Newer OpenSSH clients disable ssh-rsa by default
    # (SHA-1 deprecation), so we re-enable it explicitly for this host.
    ssh -o StrictHostKeyChecking=no \
        -o ConnectTimeout=5 \
        -o BatchMode=yes \
        -o HostKeyAlgorithms=+ssh-rsa \
        -o PubkeyAcceptedKeyTypes=+ssh-rsa \
        "${SSH_USER}@${OVMS}" "$1" 2>/dev/null
}

# ---------------------------------------------------------------------------
# 2. Fetch firmware version via SSH (same channel used for log control).
# ---------------------------------------------------------------------------
echo "Querying OVMS firmware version..."
VERSION=$(ssh_cmd "metrics list m.version" \
    | awk '{print $2}') || VERSION=""
VERSION="${VERSION:-unknown}"

if [[ "$VERSION" == "unknown" ]]; then
    echo "Warning: SSH to $OVMS failed — is the laptop on the OVMS hotspot?"
    echo "         Continuing with 'unknown' in the filename."
fi

VERSION_SLUG=$(echo "$VERSION" | tr '/' '_' | tr ' ' '_' | sed 's/[^a-zA-Z0-9._-]/_/g')

# ---------------------------------------------------------------------------
# 2. Ask for a description of the sequence about to be performed.
# ---------------------------------------------------------------------------
echo ""
echo "Firmware : $VERSION"
echo "Bus      : $BUS"
echo ""
echo "Describe the sequence you are about to perform in the car."
echo "(One line summary — you can expand the .md file afterwards.)"
echo ""
read -r -p "> " DESCRIPTION
echo ""

# ---------------------------------------------------------------------------
# 3. Build filenames.
# ---------------------------------------------------------------------------
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
BASE="${BUS}-${VERSION_SLUG}-${TIMESTAMP}"
CRTD_FILE="$OUTDIR/${BASE}.crtd"
MD_FILE="$OUTDIR/${BASE}.md"

# ---------------------------------------------------------------------------
# 4. Try to start the log via SSH; fall back to manual instructions.
# ---------------------------------------------------------------------------
LOG_CMD="can log start tcpserver transmit crtd :3000 $BUS"
STOP_CMD="can log stop"

SSH_OK=false
if ssh_cmd "$LOG_CMD"; then
    SSH_OK=true
    echo "Log started on OVMS ($BUS)."
else
    echo "SSH unavailable — start the log manually on the OVMS shell:"
    echo "  $LOG_CMD"
    echo ""
    read -r -p "Press Enter when the log is running..."
fi

echo ""
echo "Output : $CRTD_FILE"
echo "Capturing — press Ctrl-C to stop."
echo ""

# ---------------------------------------------------------------------------
# 5. On Ctrl-C: stop the log, write the markdown, print summary.
# ---------------------------------------------------------------------------
START_TIME=$(date +%s)

cleanup() {
    echo ""

    if $SSH_OK; then
        ssh_cmd "$STOP_CMD" && echo "Log stopped on OVMS." || true
    else
        echo "Stop the log on the OVMS shell:"
        echo "  $STOP_CMD"
    fi

    FRAMES=$(grep -c "^[0-9]" "$CRTD_FILE" 2>/dev/null || echo "0")
    END_TIME=$(date +%s)
    DURATION=$(( END_TIME - START_TIME ))

    cat > "$MD_FILE" <<MDEOF
# ${BASE}.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | \`${VERSION}\` |
| Bus | ${BUS} |
| Captured | ${TIMESTAMP} |
| Duration | ~${DURATION}s |
| Frames | ${FRAMES} |

## Sequence

${DESCRIPTION}

---

## Notes

<!-- Add analysis notes here -->
MDEOF

    echo ""
    echo "Saved $CRTD_FILE ($FRAMES frames, ${DURATION}s)"
    echo "      $MD_FILE"
    exit 0
}

trap cleanup INT TERM

# ---------------------------------------------------------------------------
# 6. Stream frames.
# ---------------------------------------------------------------------------
nc "$OVMS" 3000 > "$CRTD_FILE" || true

# nc exited cleanly (OVMS closed the connection) — run cleanup manually.
trap - INT TERM
cleanup
