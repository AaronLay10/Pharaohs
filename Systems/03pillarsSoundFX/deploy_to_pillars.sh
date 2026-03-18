#!/usr/bin/env bash
set -euo pipefail

# Deploy 03pillars serial audio service to all pillar Raspberry Pis.
# Usage:
#   ./deploy_to_pillars.sh
#   ./deploy_to_pillars.sh pi

PI_USER="${1:-pi}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE_DIR="/home/pi"

HOSTS=(
  "192.168.3.114"
  "192.168.3.234"
  "192.168.3.76"
  "192.168.3.216"
)

if [[ ! -f "${SCRIPT_DIR}/bowl.wav" && -f "${SCRIPT_DIR}/burning.wav" ]]; then
  cp "${SCRIPT_DIR}/burning.wav" "${SCRIPT_DIR}/bowl.wav"
fi

for host in "${HOSTS[@]}"; do
  echo "============================================================"
  echo "Deploying to ${PI_USER}@${host}"

  ssh "${PI_USER}@${host}" "mkdir -p ${REMOTE_DIR}"

  scp \
    "${SCRIPT_DIR}/pillar_audio_serial.py" \
    "${SCRIPT_DIR}/install_service.sh" \
    "${SCRIPT_DIR}/top.wav" \
    "${SCRIPT_DIR}/middle.wav" \
    "${SCRIPT_DIR}/bottom.wav" \
    "${SCRIPT_DIR}/bowl.wav" \
    "${SCRIPT_DIR}/burning.wav" \
    "${PI_USER}@${host}:${REMOTE_DIR}/"

  ssh "${PI_USER}@${host}" "chmod +x ${REMOTE_DIR}/install_service.sh && cd ${REMOTE_DIR} && ./install_service.sh"

  echo "Done: ${host}"
  echo
 done

echo "All pillar Pi deployments complete."
