#!/usr/bin/env bash
# capture.sh — Capture CAN frames from the OVMS to SD card, retrieve on Ctrl-C.
#
# Usage (from anywhere in the repo, or directly from tests/):
#   bash capture.sh [--with-metrics] [--duration SECONDS] [bus]
#
# Arguments:
#   bus                 CAN bus to capture (default: can3).
#   --with-metrics      Sample OVMS metrics over SSH every 5 s into a sidecar
#                       ${BASE}.metrics.tsv. Uses SSH ControlMaster so the
#                       repeated polls reuse one connection.
#   --duration SECONDS  Auto-stop after SECONDS instead of waiting for Ctrl-C.
#                       Handy for timed calibration captures.
#
# Requires:
#   - Laptop on the OVMS WiFi hotspot, with `ovms.local` resolving to the module
#     (typically via /etc/hosts or mDNS)
#   - SSH key for ovms@ovms.local
#   - SD card mounted in OVMS module (/sd)
#   - scp (openssh-client)

set -euo pipefail

OVMS="ovms.local"
SSH_USER="ovms"
SD_DIR="/sd"

WITH_METRICS=false
DURATION=""
BUS_ARG=""
METRICS_INTERVAL=5

while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-metrics) WITH_METRICS=true; shift ;;
    --duration)     DURATION="${2:-}"; shift 2 ;;
    --duration=*)   DURATION="${1#*=}"; shift ;;
    -h|--help)
      awk 'NR>1 && /^#/ { sub(/^# ?/,""); print; next } NR>1 { exit }' "$0"
      exit 0 ;;
    -*)
      echo "Error: unknown flag '$1'" >&2; exit 1 ;;
    *)
      BUS_ARG="$1"; shift ;;
  esac
done

if [[ -n "$DURATION" && ! "$DURATION" =~ ^[0-9]+$ ]]; then
  echo "Error: --duration must be a positive integer number of seconds" >&2
  exit 1
fi

BUS="${BUS_ARG:-can3}"

case "$BUS" in
  can1|1) BUS_FILTER=1 ;;
  can2|2) BUS_FILTER=2 ;;
  can3|3) BUS_FILTER=3 ;;
  *)
    echo "Error: unknown bus '$BUS' — expected can1/can2/can3" >&2
    exit 1
    ;;
esac
BUS="can${BUS_FILTER}"

cd "$(dirname "${BASH_SOURCE[0]}")"
OUTDIR="candumps"
mkdir -p "$OUTDIR"

# ---------------------------------------------------------------------------
# 1. SSH helper.
# ---------------------------------------------------------------------------
SSH_CTL_SOCK="/tmp/ovms-capture-${USER:-unknown}-$$"
SSH_DEBUG="${SSH_DEBUG:-false}"

ssh_base_opts=(
  -o StrictHostKeyChecking=no
  -o ConnectTimeout=5
  -o BatchMode=yes
  -o HostKeyAlgorithms=+ssh-rsa
  -o PubkeyAcceptedKeyTypes=+ssh-rsa
  -o ControlPath="$SSH_CTL_SOCK"
)

ssh_open_master() {
  ssh "${ssh_base_opts[@]}" \
    -o ControlMaster=yes \
    -o ControlPersist=10m \
    -o ServerAliveInterval=15 \
    -o ServerAliveCountMax=4 \
    -fN "${SSH_USER}@${OVMS}" 2>/dev/null
}

ssh_master_alive() {
  ssh -o ControlPath="$SSH_CTL_SOCK" -O check "${SSH_USER}@${OVMS}" >/dev/null 2>&1
}

ssh_cmd() {
  local t0 dt rc=0
  $SSH_DEBUG && t0=$(date +%s.%N)
  if $SSH_DEBUG; then
    ssh "${ssh_base_opts[@]}" "${SSH_USER}@${OVMS}" "$1" || rc=$?
  else
    ssh "${ssh_base_opts[@]}" "${SSH_USER}@${OVMS}" "$1" 2>/dev/null || rc=$?
  fi
  if $SSH_DEBUG; then
    dt=$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.2f", b-a}')
    echo "[ssh ${dt}s rc=$rc] $1" >&2
  fi
  return $rc
}

ssh_close_master() {
  ssh -o ControlPath="$SSH_CTL_SOCK" -O exit "${SSH_USER}@${OVMS}" 2>/dev/null || true
}

# ---------------------------------------------------------------------------
# 2. Open SSH master and fetch firmware version.
# ---------------------------------------------------------------------------
echo "Connecting to OVMS (one-time SSH handshake)..."
if ssh_open_master; then
  VERSION=$(ssh_cmd "metrics list m.version" | awk '{print $2}') || VERSION=""
else
  VERSION=""
fi
VERSION="${VERSION:-unknown}"

if [[ "$VERSION" == "unknown" ]]; then
  echo "Warning: SSH to $OVMS failed — is the laptop on the OVMS hotspot?"
  echo "         Continuing with 'unknown' in the filename."
fi

VERSION_SLUG=$(echo "$VERSION" | tr '/' '_' | tr ' ' '_' | sed 's/[^a-zA-Z0-9._-]/_/g')

# ---------------------------------------------------------------------------
# 2b. Verify SD card is mounted.
# ---------------------------------------------------------------------------
if ssh_master_alive && ! ssh_cmd "ls ${SD_DIR}" >/dev/null; then
  echo "Error: SD card not mounted at ${SD_DIR} on OVMS." >&2
  echo "       Insert card and retry." >&2
  ssh_close_master
  exit 1
fi

# ---------------------------------------------------------------------------
# 3. Ask for a description of the sequence about to be performed.
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
# 4. Build filenames.
# ---------------------------------------------------------------------------
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
BASE="${BUS}-${VERSION_SLUG}-${TIMESTAMP}"
CRTD_FILE="$OUTDIR/${BASE}.crtd"
MD_FILE="$OUTDIR/${BASE}.md"
METRICS_FILE="$OUTDIR/${BASE}.metrics.tsv"
SD_CRTD="${SD_DIR}/${BASE}.crtd"

# ---------------------------------------------------------------------------
# 5. Start the log on SD card via SSH.
# ---------------------------------------------------------------------------
LOG_CMD="can log start vfs crtd ${SD_CRTD} ${BUS_FILTER}"
STATUS_CMD="can log status"
STOP_CMD="can log stop"

SSH_OK=false
echo "Starting log on OVMS ($BUS → SD card)..."
if ! ssh_master_alive; then
  echo "  (SSH master dropped — reopening)"
  ssh_open_master || true
fi
if ssh_cmd "$LOG_CMD"; then
  SSH_OK=true
  echo "Log started: ${SD_CRTD}"
else
  echo "SSH unavailable — start the log manually on the OVMS shell:"
  echo "  $LOG_CMD"
  echo ""
  read -r -p "Press Enter when the log is running..."
fi

echo ""
echo "Output : $CRTD_FILE (retrieved from SD on stop)"
if $WITH_METRICS; then
  echo "Metrics: $METRICS_FILE (every ${METRICS_INTERVAL}s via SSH)"
fi
if [[ -n "$DURATION" ]]; then
  echo "Auto-stop after ${DURATION}s."
else
  echo "Capturing — press Ctrl-C to stop."
fi
echo ""

# ---------------------------------------------------------------------------
# 6. On Ctrl-C / auto-stop: stop log, retrieve file from SD, write markdown.
# ---------------------------------------------------------------------------
START_TIME=$(date +%s)
METRICS_PID=""

start_metrics_sampler() {
  if ! $SSH_OK; then
    echo "Skipping metrics sampler: SSH unavailable."
    return
  fi

  {
    echo "# capture_start_unix: ${START_TIME}"
    echo "# interval_s: ${METRICS_INTERVAL}"
    echo "# metric prefixes: v.b v.c"
    printf "t_rel\tname\tvalue\n"
  } > "$METRICS_FILE"

  (
    while true; do
      now=$(date +%s)
      t_rel=$((now - START_TIME))
      {
        ssh_cmd "metrics list v.b" || true
        ssh_cmd "metrics list v.c" || true
      } | awk -v t="$t_rel" '
        /^v\./ {
          name=$1
          $1=""
          sub(/^[ \t]+/,"")
          gsub(/\t/," ")
          print t"\t"name"\t"$0
        }
      ' >> "$METRICS_FILE"
      sleep "$METRICS_INTERVAL"
    done
  ) &
  METRICS_PID=$!
}

cleanup() {
  trap '' INT TERM

  echo ""

  for pid in "${TIMER_PID:-}" "${SLEEP_PID:-}" "${METRICS_PID:-}"; do
    if [[ -n "$pid" ]]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done

  # Get logger stats BEFORE stopping.
  STATUS_LINE=""
  LOGGER_MESSAGES=""
  LOGGER_FILTERED=""
  LOGGER_DROPPED=""
  if $SSH_OK; then
    STATUS_LINE=$(ssh_cmd "$STATUS_CMD" | grep -E '^#[0-9]+:' | head -1 || true)
    if [[ -n "$STATUS_LINE" ]]; then
      LOGGER_MESSAGES=$(echo "$STATUS_LINE" | grep -oE 'Messages:[0-9]+' | head -1 | cut -d: -f2)
      LOGGER_FILTERED=$(echo "$STATUS_LINE" | grep -oE 'Filtered:[0-9]+' | head -1 | cut -d: -f2)
      LOGGER_DROPPED=$(echo "$STATUS_LINE" | grep -oE 'Dropped:[0-9]+' | head -1 | cut -d: -f2)
    fi
    ssh_cmd "$STOP_CMD" && echo "Log stopped on OVMS." || true
  else
    echo "Stop the log on the OVMS shell:"
    echo "  $STOP_CMD"
  fi

  # Retrieve CRTD from SD card.
  echo "Retrieving ${SD_CRTD}..."
  scp_opts=(
    -o StrictHostKeyChecking=no
    -o HostKeyAlgorithms=+ssh-rsa
    -o PubkeyAcceptedKeyTypes=+ssh-rsa
    -o ControlPath="$SSH_CTL_SOCK"
    -o BatchMode=yes
  )
  SCP_OK=false
  if scp -q "${scp_opts[@]}" "${SSH_USER}@${OVMS}:${SD_CRTD}" "$CRTD_FILE" 2>/dev/null; then
    SCP_OK=true
    echo "Retrieved: $CRTD_FILE"
  else
    echo "SCP failed — retrieve manually:"
    echo "  scp ${SSH_USER}@${OVMS}:${SD_CRTD} $CRTD_FILE"
  fi

  ssh_close_master

  FRAMES=0
  RAW_FRAMES=0
  if $SCP_OK; then
    FRAMES=$(grep -c "^[0-9]" "$CRTD_FILE" 2>/dev/null || true)
    FRAMES="${FRAMES:-0}"
    RAW_FRAMES=$(grep -cE ' [0-9](R11|R29|T11|T29) ' "$CRTD_FILE" 2>/dev/null || true)
    RAW_FRAMES="${RAW_FRAMES:-0}"
  fi
  END_TIME=$(date +%s)
  ELAPSED=$((END_TIME - START_TIME))

  METRICS_LINE=""
  if $WITH_METRICS && [[ -f "$METRICS_FILE" ]]; then
    METRIC_ROWS=$(grep -c $'^[0-9]' "$METRICS_FILE" 2>/dev/null || true)
    METRIC_ROWS="${METRIC_ROWS:-0}"
    METRICS_LINE="| Metrics | \`$(basename "$METRICS_FILE")\` (${METRIC_ROWS} rows, ${METRICS_INTERVAL}s interval) |"
  fi

  STATS_LINE=""
  if [[ -n "$LOGGER_MESSAGES" ]]; then
    STATS_LINE="| Logger stats | Messages:${LOGGER_MESSAGES} Dropped:${LOGGER_DROPPED} Filtered:${LOGGER_FILTERED} |"
  fi

  cat >"$MD_FILE" <<MDEOF
# ${BASE}.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | \`${VERSION}\` |
| Bus | ${BUS} |
| Captured | ${TIMESTAMP} |
| Duration | ~${ELAPSED}s |
| Frames | ${RAW_FRAMES} raw (${FRAMES} lines incl. events) |
${STATS_LINE}
${METRICS_LINE}

## Sequence

${DESCRIPTION}

---

## Notes

<!-- Add analysis notes here -->
MDEOF

  echo ""
  if $SCP_OK; then
    echo "Saved $CRTD_FILE (${RAW_FRAMES} raw frames, ${ELAPSED}s)"
    echo "      $MD_FILE"
  else
    echo "Saved $MD_FILE"
  fi
  if $WITH_METRICS && [[ -f "$METRICS_FILE" ]]; then
    echo "      $METRICS_FILE"
  fi

  if [[ -n "$LOGGER_FILTERED" && "$LOGGER_FILTERED" -gt 1000 && "$RAW_FRAMES" -lt 100 ]]; then
    echo ""
    echo "WARNING: wrong bus likely."
    echo "   Captured $RAW_FRAMES raw frames on $BUS, but the OVMS logger"
    echo "   rejected $LOGGER_FILTERED frames from other bus(es)."
    echo "   The action was happening on a bus you weren't listening to."
    echo "   Re-run with the other bus, e.g.:"
    if [[ "$BUS" == "can3" ]]; then
      echo "     bash capture.sh can2"
    else
      echo "     bash capture.sh can3"
    fi
  fi
  exit 0
}

trap cleanup INT TERM

if $WITH_METRICS; then
  start_metrics_sampler
fi

# ---------------------------------------------------------------------------
# 7. Wait. Background sleep so Ctrl-C trap fires immediately.
#    --duration kills the sleep after N seconds, triggering cleanup.
# ---------------------------------------------------------------------------
SLEEP_PID=""
TIMER_PID=""

sleep 86400 &
SLEEP_PID=$!

if [[ -n "$DURATION" ]]; then
  ( sleep "$DURATION"; kill "$SLEEP_PID" 2>/dev/null || true ) &
  TIMER_PID=$!
fi

wait "$SLEEP_PID" 2>/dev/null || true
SLEEP_PID=""

cleanup
