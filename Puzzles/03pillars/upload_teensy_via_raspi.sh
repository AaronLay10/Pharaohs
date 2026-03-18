#!/usr/bin/env bash
set -euo pipefail

# Upload pillar-specific HEX files to each Raspberry Pi and flash Teensy boards.
# Run this script from anywhere; it resolves paths relative to this script location.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEX_DIR="${SCRIPT_DIR}/build/hex_artifacts"
PI_USER="${PI_USER:-pi}"
REMOTE_HEX_PATH="/home/${PI_USER}/03pillars.hex"
MCU="${MCU:-imxrt1062}"
ATTEMPTS="${ATTEMPTS:-3}"
COPY_ONLY=0
FLASH_ONLY=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [--copy-only] [--flash-only] [--help]

Options:
  --copy-only   Copy HEX files to Pis, do not flash.
  --flash-only  Flash using existing /home/pi/03pillars.hex on each Pi, do not copy.
  --help        Show this help.

Environment overrides:
  PI_USER   SSH user (default: pi)
  MCU       teensy_loader_cli target MCU (default: imxrt1062)
  ATTEMPTS  Number of flash retries per pillar (default: 3)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --copy-only)
      COPY_ONLY=1
      ;;
    --flash-only)
      FLASH_ONLY=1
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1"
      usage
      exit 1
      ;;
  esac
  shift
done

if [[ "$COPY_ONLY" -eq 1 && "$FLASH_ONLY" -eq 1 ]]; then
  echo "ERROR: --copy-only and --flash-only cannot be used together"
  exit 1
fi

pillars=(1 2 3 4)
hosts=(
  "192.168.3.114"
  "192.168.3.234"
  "192.168.3.76"
  "192.168.3.216"
)

check_hex_files() {
  for pillar in "${pillars[@]}"; do
    local hex_file="${HEX_DIR}/03pillars_pillar${pillar}.hex"
    if [[ ! -f "$hex_file" ]]; then
      echo "ERROR: Missing HEX file: $hex_file"
      echo "Build first with: ./build_all_pillars_hex.sh"
      exit 1
    fi
  done
}

copy_hex_files() {
  echo "== Copying HEX files to Raspberry Pis =="
  for i in "${!pillars[@]}"; do
    local pillar="${pillars[$i]}"
    local host="${hosts[$i]}"
    local hex_file="${HEX_DIR}/03pillars_pillar${pillar}.hex"

    echo "Pillar ${pillar} -> ${PI_USER}@${host}"
    scp "$hex_file" "${PI_USER}@${host}:${REMOTE_HEX_PATH}"
  done
}

flash_one_host() {
  local host="$1"

  ssh -o BatchMode=yes -o ConnectTimeout=8 "${PI_USER}@${host}" "
set +e
ok=0

if ! command -v teensy_loader_cli >/dev/null 2>&1; then
  sudo apt-get update -y >/dev/null 2>&1
  sudo apt-get install -y teensy-loader-cli >/dev/null 2>&1 || true
fi

for i in \\$(seq 1 ${ATTEMPTS}); do
  out=\\$(sudo teensy_loader_cli --mcu=${MCU} -v ${REMOTE_HEX_PATH} 2>&1 || true)
  if echo \\\"\\$out\\\" | grep -q 'Booting'; then
    ok=1
    break
  fi

  out=\\$(sudo teensy_loader_cli --mcu=${MCU} -s -v ${REMOTE_HEX_PATH} 2>&1 || true)
  if echo \\\"\\$out\\\" | grep -q 'Booting'; then
    ok=1
    break
  fi

  sleep 1
done

if [[ \\$ok -eq 1 ]]; then
  echo SUCCESS
  exit 0
fi

echo FAILED
exit 1
"
}

flash_all_hosts() {
  local failures=0

  echo "== Flashing Teensys via Raspberry Pis =="
  for i in "${!pillars[@]}"; do
    local pillar="${pillars[$i]}"
    local host="${hosts[$i]}"

    echo "Pillar ${pillar} (${host})"
    if flash_one_host "$host"; then
      echo "Result: SUCCESS"
    else
      echo "Result: FAILED"
      failures=$((failures + 1))
    fi
    echo
  done

  if [[ "$failures" -gt 0 ]]; then
    echo "Completed with ${failures} failure(s)."
    return 1
  fi

  echo "Completed successfully: all pillars flashed."
  return 0
}

main() {
  if [[ "$FLASH_ONLY" -eq 0 ]]; then
    check_hex_files
    copy_hex_files
  fi

  if [[ "$COPY_ONLY" -eq 0 ]]; then
    flash_all_hosts
  else
    echo "Copy step complete. Flash skipped due to --copy-only."
  fi
}

main
