#!/usr/bin/env bash
set -euo pipefail

# Build all 4 pillar firmware variants deterministically and place .hex artifacts
# into Puzzles/03pillars/build/hex_artifacts.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SKETCH_REL="Puzzles/03pillars"
SKETCH_FILE="$REPO_ROOT/$SKETCH_REL/03pillars.ino"
BACKUP_FILE="$SKETCH_FILE.bak_build"

LIBS_ARDUINO="${LIBS_ARDUINO:-/Users/aaron/Documents/Arduino/libraries}"
LIBS_REPO="${LIBS_REPO:-$REPO_ROOT/Libraries}"
FQBN="${FQBN:-teensy:avr:teensy41}"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "ERROR: arduino-cli not found in PATH"
  exit 1
fi

if [[ ! -f "$SKETCH_FILE" ]]; then
  echo "ERROR: Sketch file not found: $SKETCH_FILE"
  exit 1
fi

restore_source() {
  if [[ -f "$BACKUP_FILE" ]]; then
    mv "$BACKUP_FILE" "$SKETCH_FILE"
  fi
}
trap restore_source EXIT

cp "$SKETCH_FILE" "$BACKUP_FILE"

for pillar in 1 2 3 4; do
  echo "== Building pillar ${pillar} =="

  perl -0pi -e "s/#define PILLAR\s+\d+/#define PILLAR                    ${pillar}/" "$SKETCH_FILE"

  arduino-cli compile \
    -b "$FQBN" \
    --clean \
    --build-path "$REPO_ROOT/$SKETCH_REL/build/pillar${pillar}" \
    --libraries "$LIBS_ARDUINO" \
    --libraries "$LIBS_REPO" \
    "$REPO_ROOT/$SKETCH_REL"

  cp \
    "$REPO_ROOT/$SKETCH_REL/build/pillar${pillar}/03pillars.ino.hex" \
    "$REPO_ROOT/$SKETCH_REL/build/hex_artifacts/03pillars_pillar${pillar}.hex"
done

# Verify identity strings in each ELF so we catch wrong-PILLAR builds immediately.
declare -a expected=("PillarOne" "PillarTwo" "PillarThree" "PillarFour")
for idx in 0 1 2 3; do
  pillar=$((idx + 1))
  want="${expected[$idx]}"
  elf="$REPO_ROOT/$SKETCH_REL/build/pillar${pillar}/03pillars.ino.elf"

  got="$(strings "$elf" | egrep -m1 '^PillarOne$|^PillarTwo$|^PillarThree$|^PillarFour$' || true)"
  echo "pillar${pillar}: ${got:-<missing>}"

  if [[ "$got" != "$want" ]]; then
    echo "ERROR: pillar${pillar} identity mismatch. Expected ${want}, got ${got:-<missing>}"
    exit 1
  fi
done

echo "== Build complete =="
ls -lh "$REPO_ROOT/$SKETCH_REL/build/hex_artifacts/03pillars_pillar"*.hex
