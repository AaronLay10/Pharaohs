# 03pillars: Upload To Teensy Via Raspberry Pi

This runbook documents how to upload pillar firmware to each Teensy through its attached Raspberry Pi.

## Pillar To Pi Mapping

1. Pillar 1 -> 192.168.3.114
2. Pillar 2 -> 192.168.3.234
3. Pillar 3 -> 192.168.3.76
4. Pillar 4 -> 192.168.3.216

## Files

1. Build script: [build_all_pillars_hex.sh](build_all_pillars_hex.sh)
2. Upload script: [upload_teensy_via_raspi.sh](upload_teensy_via_raspi.sh)
3. HEX artifacts folder: [build/hex_artifacts](build/hex_artifacts)

Expected build outputs:

1. [build/hex_artifacts/03pillars_pillar1.hex](build/hex_artifacts/03pillars_pillar1.hex)
2. [build/hex_artifacts/03pillars_pillar2.hex](build/hex_artifacts/03pillars_pillar2.hex)
3. [build/hex_artifacts/03pillars_pillar3.hex](build/hex_artifacts/03pillars_pillar3.hex)
4. [build/hex_artifacts/03pillars_pillar4.hex](build/hex_artifacts/03pillars_pillar4.hex)

## Standard Workflow

1. Build all pillar HEX files:

```bash
cd /Users/aaron/GitRepos/Pharaohs/Puzzles/03pillars
./build_all_pillars_hex.sh
```

2. Upload and flash all pillars:

```bash
cd /Users/aaron/GitRepos/Pharaohs/Puzzles/03pillars
./upload_teensy_via_raspi.sh
```

The script will:

1. Copy each pillar-specific HEX to `/home/pi/03pillars.hex` on its matching Pi.
2. Flash via `teensy_loader_cli` using MCU target `imxrt1062`.
3. Retry with direct and soft-reboot modes for reliability.

## Optional Modes

1. Copy only (no flash):

```bash
./upload_teensy_via_raspi.sh --copy-only
```

2. Flash only (no copy):

```bash
./upload_teensy_via_raspi.sh --flash-only
```

3. Custom retries or user:

```bash
ATTEMPTS=5 PI_USER=pi ./upload_teensy_via_raspi.sh
```

## Troubleshooting

1. If a pillar reports `FAILED`, run the script again. Intermittent USB bootloader timing issues are handled by retries and often pass on a second run.
2. If all pillars fail, verify SSH connectivity to all Pi IPs.
3. If HEX files are missing, rebuild with `./build_all_pillars_hex.sh`.
4. If Teensy tool is missing, the script auto-installs `teensy-loader-cli` on the Pi.

## Notes

1. Teensy 4.1 uploads here are most reliable with `--mcu=imxrt1062`.
2. Script output prints `SUCCESS` or `FAILED` per pillar for quick verification.
