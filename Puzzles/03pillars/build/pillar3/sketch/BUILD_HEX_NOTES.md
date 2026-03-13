#line 1 "/Users/aaron/GitRepos/Pharaohs/Puzzles/03pillars/BUILD_HEX_NOTES.md"
# 03pillars Hex Build Notes

Use this script to generate all four pillar hex files with identity verification:

```bash
cd /Users/aaron/Git\ Repos/Pharaohs/Puzzles/03pillars
./build_all_pillars_hex.sh
```

What it does:
- Builds pillar 1, 2, 3, and 4 deterministically.
- Restores `03pillars.ino` to its original state after building.
- Verifies each ELF contains the expected identity string (`PillarOne`..`PillarFour`).
- Writes final artifacts to `build/hex_artifacts/`:
  - `03pillars_pillar1.hex`
  - `03pillars_pillar2.hex`
  - `03pillars_pillar3.hex`
  - `03pillars_pillar4.hex`

Optional overrides:
- `FQBN` (default: `teensy:avr:teensy41`)
- `LIBS_ARDUINO` (default: `/Users/aaron/Documents/Arduino/libraries`)
- `LIBS_REPO` (default: `<repo>/Libraries`)

Example with overrides:

```bash
FQBN=teensy:avr:teensy41 \
LIBS_ARDUINO=/Users/aaron/Documents/Arduino/libraries \
LIBS_REPO=/Users/aaron/Git\ Repos/Pharaohs/Libraries \
./build_all_pillars_hex.sh
```
