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

DEVICE_IDS=(
  "PillarOne"
  "PillarTwo"
  "PillarThree"
  "PillarFour"
)

for i in "${!HOSTS[@]}"; do
  host="${HOSTS[$i]}"
  device_id="${DEVICE_IDS[$i]}"
  echo "============================================================"
  echo "Deploying to ${PI_USER}@${host} (${device_id})"

  ssh "${PI_USER}@${host}" "mkdir -p ${REMOTE_DIR}"
  ssh "${PI_USER}@${host}" "rm -f ${REMOTE_DIR}/bowl.wav"

  ssh "${PI_USER}@${host}" "cat > ${REMOTE_DIR}/pillar_audio.env <<'EOF'
PILLAR_DEVICE_ID=${device_id}
PILLAR_MQTT_HOST=192.168.20.8
PILLAR_MQTT_PORT=33002
EOF"

  scp \
    "${SCRIPT_DIR}/pillar_audio_serial.py" \
    "${SCRIPT_DIR}/install_service.sh" \
    "${SCRIPT_DIR}/top.wav" \
    "${SCRIPT_DIR}/middle.wav" \
    "${SCRIPT_DIR}/bottom.wav" \
    "${SCRIPT_DIR}/burning.wav" \
    "${PI_USER}@${host}:${REMOTE_DIR}/"

  ssh "${PI_USER}@${host}" "chmod +x ${REMOTE_DIR}/install_service.sh && cd ${REMOTE_DIR} && ./install_service.sh"

  echo "Done: ${host} (${device_id})"
  echo
 done

echo "All pillar Pi deployments complete."
