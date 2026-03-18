#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_NAME="pillar-audio.service"
SERVICE_PATH="/etc/systemd/system/${SERVICE_NAME}"

echo "Installing dependencies (python3-serial, alsa-utils, ALSA equalizer plugin)..."
sudo apt-get update -y
sudo apt-get install -y python3-serial alsa-utils
if ! sudo apt-get install -y alsa-equal; then
	sudo apt-get install -y libasound2-plugin-equal
fi

cat > /tmp/pharaohs-asoundrc <<'EOF'
ctl.equal {
	type equal
}

pcm.plugequal {
	type equal
	slave.pcm "plughw:CARD=Device,DEV=0"
}

pcm.equal {
	type plug
	slave.pcm plugequal
}

pcm.!default {
	type plug
	slave.pcm equal
}

ctl.!default {
	type hw
	card Device
}
EOF

echo "Installing ALSA profile to /home/pi/.asoundrc"
sudo install -o pi -g pi -m 0644 /tmp/pharaohs-asoundrc /home/pi/.asoundrc

cat > /tmp/${SERVICE_NAME} <<EOF
[Unit]
Description=Pharaohs Pillar Serial Audio Service
After=network-online.target sound.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${SCRIPT_DIR}
ExecStart=/usr/bin/python3 ${SCRIPT_DIR}/pillar_audio_serial.py --port /dev/ttyS0 --baud 115200
Restart=always
RestartSec=2
User=pi
Group=audio

[Install]
WantedBy=multi-user.target
EOF

echo "Installing ${SERVICE_NAME} to ${SERVICE_PATH}"
sudo cp /tmp/${SERVICE_NAME} ${SERVICE_PATH}
sudo systemctl daemon-reload
sudo systemctl enable --now ${SERVICE_NAME}

echo
echo "Service status:"
sudo systemctl --no-pager --full status ${SERVICE_NAME}
