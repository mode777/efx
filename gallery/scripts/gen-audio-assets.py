#!/usr/bin/env python3
"""Regenerate the synthesized F14 audio-showcase effect WAVs.

The audio showcase's three effect sounds are synthesized deterministically
here (stdlib `wave`, no RNG) so there is one source of truth for them. They
are committed loose in the sample directory
(`gallery/samples/curated/audio-showcase/`), which is also the player's
resource root; `music.mp3` and `font.ttf` are committed sources beside them.
The sample directory is packed as-is (no separate packer).

Regenerate after editing:
    python3 gallery/scripts/gen-audio-assets.py
Verify the committed WAVs are current (CI/humans):
    python3 gallery/scripts/gen-audio-assets.py --check
"""
import io
import math
import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "samples" / "curated" / "audio-showcase"

RATE = 22050


def wav_bytes(samples):
    frames = bytearray()
    for v in samples:
        s = int(max(-1.0, min(1.0, v)) * 32767)
        frames += struct.pack("<h", s)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))
    return buf.getvalue()


def synth_blip():
    """Short bright effect: a fast 660->1320 Hz chirp with a quick decay."""
    n = int(0.11 * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 660.0 + (1320.0 - 660.0) * (t / 0.11)
        phase += f / RATE
        env = math.exp(-t * 34.0)
        out[i] = math.sin(2.0 * math.pi * phase) * env * 0.7
    return wav_bytes(out)


def synth_chime():
    """Bell-ish effect: three decaying harmonics."""
    n = int(0.6 * RATE)
    out = [0.0] * n
    partials = [(523.25, 0.55, 5.0), (1046.50, 0.30, 7.5), (1567.98, 0.15, 10.0)]
    for i in range(n):
        t = i / RATE
        v = 0.0
        for f, a, d in partials:
            v += a * math.sin(2.0 * math.pi * f * t) * math.exp(-t * d)
        out[i] = v * 0.8
    return wav_bytes(out)


def synth_thud():
    """Low percussive effect: two decaying low sines."""
    n = int(0.28 * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / 0.004) * math.exp(-t * 16.0)
        v = 0.7 * math.sin(2.0 * math.pi * 82.0 * t) + \
            0.3 * math.sin(2.0 * math.pi * 123.0 * t)
        out[i] = v * env * 0.9
    return wav_bytes(out)


def build():
    return {
        "blip.wav": synth_blip(),
        "chime.wav": synth_chime(),
        "thud.wav": synth_thud(),
    }


def main():
    files = build()
    if "--check" in sys.argv[1:]:
        stale = []
        for name, data in files.items():
            path = OUT_DIR / name
            if not path.exists() or path.read_bytes() != data:
                stale.append(name)
        if stale:
            print(
                f"{OUT_DIR} is stale ({', '.join(stale)}): "
                "run python3 gallery/scripts/gen-audio-assets.py",
                file=sys.stderr,
            )
            return 1
        print(f"{OUT_DIR} WAVs are current")
        return 0
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (OUT_DIR / name).write_bytes(data)
    print(f"wrote {len(files)} WAVs to {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
