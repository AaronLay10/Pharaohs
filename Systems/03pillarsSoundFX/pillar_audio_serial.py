#!/usr/bin/env python3
"""Serial audio controller for Pharaohs pillars on Raspberry Pi.

Listens for line-delimited commands from Teensy and plays WAV files:
- top -> top.wav (one-shot)
- middle -> middle.wav (one-shot)
- bottom -> bottom.wav (one-shot)
- bowl -> bowl.wav (loop until state change)
- burning -> burning.wav (loop until state change)

Optional state commands are also supported:
- state1/state2: stop looping audio
- state3: loop bowl.wav
- state4: loop burning.wav
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import threading
import time
from pathlib import Path

try:
    import serial
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "pyserial is required. Install with: pip install pyserial"
    ) from exc

AUDIO_DIR = Path(__file__).resolve().parent
ONE_SHOTS = {
    "top": AUDIO_DIR / "top.wav",
    "middle": AUDIO_DIR / "middle.wav",
    "bottom": AUDIO_DIR / "bottom.wav",
}
LOOPS = {
    "bowl": AUDIO_DIR / "bowl.wav",
    "burning": AUDIO_DIR / "burning.wav",
}


class LoopPlayer:
    def __init__(self, player_cmd: str, device: str | None = None) -> None:
        self.player_cmd = player_cmd
        self.device = device
        self._thread: threading.Thread | None = None
        self._stop_event = threading.Event()
        self._lock = threading.Lock()
        self._proc: subprocess.Popen | None = None
        self.current_loop: str | None = None

    def _build_cmd(self, wav_path: Path) -> list[str]:
        cmd = [self.player_cmd]
        if self.device:
            cmd.extend(["-D", self.device])
        cmd.extend(["-q", str(wav_path)])
        return cmd

    def _play_file_blocking(self, wav_path: Path) -> None:
        proc = subprocess.Popen(self._build_cmd(wav_path))
        with self._lock:
            self._proc = proc
        try:
            while proc.poll() is None:
                if self._stop_event.wait(0.1):
                    proc.terminate()
                    try:
                        proc.wait(timeout=1.0)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                    break
        finally:
            with self._lock:
                self._proc = None

    def _loop_worker(self, loop_name: str, wav_path: Path) -> None:
        while not self._stop_event.is_set():
            self._play_file_blocking(wav_path)

    def start(self, loop_name: str, wav_path: Path) -> None:
        if self.current_loop == loop_name and self._thread and self._thread.is_alive():
            return
        self.stop()
        self._stop_event.clear()
        self.current_loop = loop_name
        self._thread = threading.Thread(
            target=self._loop_worker,
            args=(loop_name, wav_path),
            daemon=True,
        )
        self._thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        with self._lock:
            proc = self._proc
        if proc and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                proc.kill()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1.5)
        self._thread = None
        self.current_loop = None


class OneShotPlayer:
    def __init__(self, player_cmd: str, device: str | None = None) -> None:
        self.player_cmd = player_cmd
        self.device = device
        self._lock = threading.Lock()
        self._proc: subprocess.Popen | None = None
        self._last_play: dict[str, float] = {}

    def _build_cmd(self, wav_path: Path) -> list[str]:
        cmd = [self.player_cmd]
        if self.device:
            cmd.extend(["-D", self.device])
        cmd.extend(["-q", str(wav_path)])
        return cmd

    def play(self, key: str, wav_path: Path, min_interval_s: float = 0.18) -> None:
        now = time.monotonic()
        last = self._last_play.get(key, 0.0)
        if now - last < min_interval_s:
            return

        with self._lock:
            # Keep one-shot playback single-instance to avoid aplay stampede.
            if self._proc and self._proc.poll() is None:
                self._proc.terminate()
                try:
                    self._proc.wait(timeout=0.2)
                except subprocess.TimeoutExpired:
                    self._proc.kill()

            self._proc = subprocess.Popen(self._build_cmd(wav_path))
            self._last_play[key] = now

    def is_playing(self) -> bool:
        with self._lock:
            return self._proc is not None and self._proc.poll() is None


def ensure_audio_files() -> None:
    missing = [str(p) for p in [*ONE_SHOTS.values(), *LOOPS.values()] if not p.exists()]
    if missing:
        raise SystemExit("Missing WAV files:\n" + "\n".join(missing))


def resolve_player() -> str:
    player = shutil.which("aplay")
    if not player:
        raise SystemExit("Could not find 'aplay'. Install ALSA utils: sudo apt install alsa-utils")
    return player


def configure_audio_output() -> None:
    """Best-effort: set volume to 100% and apply a smile EQ curve."""
    amixer = shutil.which("amixer")
    if not amixer:
        print("warning: amixer not found; skipping volume/EQ setup")
        return

    # Set common output controls to 100% and unmute.
    controls = ("Speaker", "PCM", "Master", "Digital")
    volume_set = False
    for ctl in controls:
        res = subprocess.run(
            [amixer, "-c", "Device", "sset", ctl, "100%", "unmute"],
            capture_output=True,
            text=True,
            check=False,
        )
        if res.returncode == 0:
            volume_set = True
    if volume_set:
        print("audio: USB card volume set to 100%")
    else:
        print("warning: could not set USB card volume controls")

    # Smile EQ using deterministic percent values (66 is roughly flat for this plugin).
    eq_bands = [
        ("00. 31 Hz", "100%"),
        ("01. 63 Hz", "82%"),
        ("02. 125 Hz", "76%"),
        ("03. 250 Hz", "70%"),
        ("04. 500 Hz", "66%"),
        ("05. 1 kHz", "75%"),
        ("06. 2 kHz", "58%"),
        ("07. 4 kHz", "66%"),
        ("08. 8 kHz", "74%"),
        ("09. 16 kHz", "100%"),
    ]

    eq_set_count = 0
    for band_name, gain in eq_bands:
        res = subprocess.run(
            [amixer, "-D", "equal", "sset", band_name, gain],
            capture_output=True,
            text=True,
            check=False,
        )
        if res.returncode == 0:
            eq_set_count += 1

    if eq_set_count == len(eq_bands):
        print("audio: smile EQ applied")
    elif eq_set_count > 0:
        print(f"warning: partial EQ applied ({eq_set_count}/{len(eq_bands)} bands)")
    else:
        print("warning: could not apply EQ (is alsaequal configured?)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Pillar serial audio player")
    parser.add_argument("--port", default="/dev/ttyS0", help="Serial port (default: /dev/ttyS0)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument(
        "--device",
        default="plughw:CARD=Device,DEV=0",
        help="ALSA playback device (default: plughw:CARD=Device,DEV=0)",
    )
    args = parser.parse_args()

    ensure_audio_files()
    player_cmd = resolve_player()
    configure_audio_output()
    loop_player = LoopPlayer(player_cmd, args.device)
    one_shot_player = OneShotPlayer(player_cmd, args.device)
    print(f"audio: playback device '{args.device}'")

    print(f"Opening serial: {args.port} @ {args.baud}")
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        try:
            while True:
                raw = ser.readline()
                if not raw:
                    continue

                cmd = raw.decode("utf-8", errors="ignore").strip().lower()
                if not cmd:
                    continue

                if cmd in ONE_SHOTS:
                    # Sensor-trigger sounds. If we were in solved loop, stop it.
                    if loop_player.current_loop == "burning":
                        loop_player.stop()
                    if loop_player.current_loop is not None:
                        print(f"skip one-shot during loop: {cmd}")
                        continue
                    one_shot_player.play(cmd, ONE_SHOTS[cmd])
                    print(f"one-shot: {cmd}")
                    continue

                if cmd in LOOPS:
                    if one_shot_player.is_playing():
                        print(f"defer loop while one-shot active: {cmd}")
                        continue
                    loop_player.start(cmd, LOOPS[cmd])
                    print(f"loop: {cmd}")
                    continue

                if cmd in ("state1", "state2"):
                    loop_player.stop()
                    print(f"stop loops: {cmd}")
                    continue

                if cmd == "state3":
                    if one_shot_player.is_playing():
                        print("defer state3 loop while one-shot active")
                        continue
                    loop_player.start("bowl", LOOPS["bowl"])
                    print("loop: bowl (state3)")
                    continue

                if cmd == "state4":
                    if one_shot_player.is_playing():
                        print("defer state4 loop while one-shot active")
                        continue
                    loop_player.start("burning", LOOPS["burning"])
                    print("loop: burning (state4)")
                    continue

                print(f"ignored: {cmd}")
        except KeyboardInterrupt:
            print("Stopping...")
        finally:
            loop_player.stop()


if __name__ == "__main__":
    main()
