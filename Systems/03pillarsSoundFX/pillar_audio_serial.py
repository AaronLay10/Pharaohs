#!/usr/bin/env python3
"""Serial audio controller for Pharaohs pillars on Raspberry Pi.

Listens for line-delimited commands from Teensy and plays WAV files:
- top -> top.wav (one-shot)
- middle -> middle.wav (one-shot)
- bottom -> bottom.wav (one-shot)
- burning -> burning.wav (continuous loop)
- stop -> stop all current audio
"""

from __future__ import annotations

import argparse
import os
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

try:
    import paho.mqtt.client as mqtt
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "paho-mqtt is required. Install with: pip install paho-mqtt"
    ) from exc

AUDIO_DIR = Path(__file__).resolve().parent
USB_DEVICE = "plughw:CARD=Device,DEV=0"
MQTT_BROKER_HOST = "192.168.20.8"
MQTT_BROKER_PORT = 33002
ONE_SHOTS = {
    "top": AUDIO_DIR / "top.wav",
    "middle": AUDIO_DIR / "middle.wav",
    "bottom": AUDIO_DIR / "bottom.wav",
}
LOOPS = {
    "burning": AUDIO_DIR / "burning.wav",
}
KNOWN_STATES = {"state1", "state2", "state3", "state4"}


class StateTracker:
    def __init__(self, initial_state: str = "state1") -> None:
        self._state = initial_state
        self._lock = threading.Lock()

    def set(self, state: str) -> None:
        with self._lock:
            self._state = state

    def get(self) -> str:
        with self._lock:
            return self._state


class PillarMqttListener:
    def __init__(
        self,
        broker_host: str,
        broker_port: int,
        device_id: str,
        state_tracker: StateTracker,
    ) -> None:
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.device_id = device_id
        self.state_tracker = state_tracker
        self._stop_requested = False
        self.client = mqtt.Client(client_id=f"{device_id}-audio", clean_session=True)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect

    @staticmethod
    def _parse_action(payload: str) -> str:
        payload = payload.strip()
        if not payload:
            return ""
        for delim in (" ", ","):
            if delim in payload:
                return payload.split(delim, 1)[0].strip().lower()
        return payload.lower()

    def _on_connect(self, client: mqtt.Client, userdata, flags, rc):
        if rc != 0:
            print(f"mqtt: connect failed rc={rc}")
            return
        device_topic = f"ToDevice/{self.device_id}"
        client.subscribe(device_topic)
        client.subscribe("ToDevice/All")
        print(f"mqtt: connected {self.broker_host}:{self.broker_port}")
        print(f"mqtt: subscribed {device_topic}, ToDevice/All")

    def _on_disconnect(self, client: mqtt.Client, userdata, rc):
        if self._stop_requested:
            return
        print(f"mqtt: disconnected rc={rc}")

    def _on_message(self, client: mqtt.Client, userdata, msg: mqtt.MQTTMessage):
        payload = msg.payload.decode("utf-8", errors="ignore").strip()
        action = self._parse_action(payload)
        if action in KNOWN_STATES:
            self.state_tracker.set(action)
            print(f"mqtt state: {action} (topic={msg.topic})")
        else:
            print(f"mqtt cmd: {payload} (topic={msg.topic})")

    def start(self) -> None:
        self.client.reconnect_delay_set(min_delay=1, max_delay=15)
        self.client.connect(self.broker_host, self.broker_port, keepalive=30)
        self.client.loop_start()

    def stop(self) -> None:
        self._stop_requested = True
        self.client.loop_stop()
        try:
            self.client.disconnect()
        except Exception:
            pass


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

    def _loop_worker(self, wav_path: Path) -> None:
        while not self._stop_event.is_set():
            self._play_file_blocking(wav_path)
            if not self._stop_event.is_set():
                time.sleep(0.05)

    def start(self, loop_name: str, wav_path: Path) -> bool:
        if self.current_loop == loop_name and self._thread and self._thread.is_alive():
            return False
        self.stop()
        self._stop_event.clear()
        self.current_loop = loop_name
        self._thread = threading.Thread(target=self._loop_worker, args=(wav_path,), daemon=True)
        self._thread.start()
        return True

    def stop(self) -> bool:
        was_running = self.is_active()
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
        return was_running

    def is_active(self) -> bool:
        return self._thread is not None and self._thread.is_alive()


class OneShotPlayer:
    def __init__(self, player_cmd: str, device: str | None = None) -> None:
        self.player_cmd = player_cmd
        self.device = device
        self._lock = threading.Lock()
        self._proc: subprocess.Popen | None = None
        self._last_play: dict[str, float] = {}
        self._last_any_play: float = 0.0

    def _build_cmd(self, wav_path: Path) -> list[str]:
        cmd = [self.player_cmd]
        if self.device:
            cmd.extend(["-D", self.device])
        cmd.extend(["-q", str(wav_path)])
        return cmd

    def play(
        self,
        key: str,
        wav_path: Path,
        min_interval_s: float = 0.18,
        global_interval_s: float = 0.12,
    ) -> bool:
        now = time.monotonic()
        last = self._last_play.get(key, 0.0)
        if now - last < min_interval_s:
            return False

        with self._lock:
            # Ignore overlapping triggers instead of interrupting audio mid-write.
            if self._proc and self._proc.poll() is None:
                return False

            if now - self._last_any_play < global_interval_s:
                return False

            self._proc = subprocess.Popen(self._build_cmd(wav_path))
            self._last_play[key] = now
            self._last_any_play = now
            return True

    def stop(self) -> bool:
        with self._lock:
            proc = self._proc
            is_running = proc is not None and proc.poll() is None
            if is_running:
                proc.terminate()
                try:
                    proc.wait(timeout=0.3)
                except subprocess.TimeoutExpired:
                    proc.kill()
            self._proc = None
        return is_running

def ensure_audio_files() -> None:
    missing = [str(p) for p in [*ONE_SHOTS.values(), *LOOPS.values()] if not p.exists()]
    if missing:
        raise SystemExit("Missing WAV files:\n" + "\n".join(missing))


def resolve_player() -> str:
    player = shutil.which("aplay")
    if not player:
        raise SystemExit("Could not find 'aplay'. Install ALSA utils: sudo apt install alsa-utils")
    return player


def ensure_usb_audio_present(player_cmd: str) -> None:
    """Fail fast unless the expected Plugable USB card is available."""
    res = subprocess.run(
        [player_cmd, "-L"],
        capture_output=True,
        text=True,
        check=False,
    )
    if res.returncode != 0:
        raise SystemExit("Could not query ALSA devices with aplay -L")

    devices = res.stdout.lower()
    if "card=device" not in devices:
        raise SystemExit(
            "Plugable USB audio device is not available as CARD=Device. "
            "Check USB connection and ALSA card naming."
        )


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
        default=USB_DEVICE,
        help="ALSA playback device (default: plughw:CARD=Device,DEV=0)",
    )
    parser.add_argument(
        "--mqtt-host",
        default=os.environ.get("PILLAR_MQTT_HOST", MQTT_BROKER_HOST),
        help="MQTT broker host (default: 192.168.20.8)",
    )
    parser.add_argument(
        "--mqtt-port",
        type=int,
        default=int(os.environ.get("PILLAR_MQTT_PORT", str(MQTT_BROKER_PORT))),
        help="MQTT broker port (default: 33002)",
    )
    parser.add_argument(
        "--mqtt-device-id",
        default=os.environ.get("PILLAR_DEVICE_ID", "PillarUnknown"),
        help="Device id used for ToDevice/<deviceID> subscription",
    )
    args = parser.parse_args()

    ensure_audio_files()
    player_cmd = resolve_player()
    ensure_usb_audio_present(player_cmd)
    configure_audio_output()
    loop_player = LoopPlayer(player_cmd, args.device)
    one_shot_player = OneShotPlayer(player_cmd, args.device)
    state_tracker = StateTracker(initial_state="state1")
    mqtt_listener = PillarMqttListener(
        broker_host=args.mqtt_host,
        broker_port=args.mqtt_port,
        device_id=args.mqtt_device_id,
        state_tracker=state_tracker,
    )

    print(f"audio: playback device '{args.device}'")
    print(f"mqtt: device id '{args.mqtt_device_id}'")
    mqtt_listener.start()

    current_state = "state1"

    def stop_all_audio(reason: str) -> None:
        loop_was_running = loop_player.stop()
        one_shot_was_running = one_shot_player.stop()
        if loop_was_running or one_shot_was_running:
            print(f"audio stopped: {reason}")

    def apply_state(state: str) -> None:
        if state in ("state1", "state2"):
            stop_all_audio(state)
            print(f"state active: {state} (no sound)")
            return

        if state == "state3":
            loop_player.stop()
            print("state active: state3 (sensor one-shots enabled)")
            return

        if state == "state4":
            one_shot_player.stop()
            if loop_player.start("burning", LOOPS["burning"]):
                print("state active: state4 (burning loop)")
            else:
                print("state active: state4 (burning loop already active)")
            return

        print(f"warning: unknown state '{state}'")

    apply_state(current_state)

    print(f"Opening serial: {args.port} @ {args.baud}")
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        try:
            while True:
                desired_state = state_tracker.get()
                if desired_state != current_state:
                    current_state = desired_state
                    apply_state(current_state)

                raw = ser.readline()
                if not raw:
                    continue

                cmd = raw.decode("utf-8", errors="ignore").strip().lower()
                if not cmd:
                    continue

                if cmd == "stop":
                    stop_all_audio("stop command")
                    print("stop: all audio")
                    continue

                if current_state == "state3" and cmd in ONE_SHOTS:
                    if loop_player.is_active():
                        print(f"skip one-shot during loop: {cmd}")
                        continue
                    if one_shot_player.play(cmd, ONE_SHOTS[cmd]):
                        print(f"one-shot: {cmd}")
                    else:
                        print(f"skip: {cmd}")
                    continue

                if current_state == "state4" and cmd == "burning":
                    # Allow explicit reassertion of burning loop command.
                    if loop_player.start("burning", LOOPS["burning"]):
                        print("loop start: burning")
                    else:
                        print("loop already active: burning")
                    continue

                print(f"ignored in {current_state}: {cmd}")
        except KeyboardInterrupt:
            print("Stopping...")
        finally:
            mqtt_listener.stop()
            loop_player.stop()
            one_shot_player.stop()


if __name__ == "__main__":
    main()
