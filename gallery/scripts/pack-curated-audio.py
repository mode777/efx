#!/usr/bin/env python3
"""Pack the authored F14 audio showcase into a deterministic zip.

The gallery mounts at most one zip as a sample's resource root (F6a, ADR 0030),
so the music, effects, and font the `audio-showcase` sample uses must live
inside `audio-showcase.zip`. The WAV effects are synthesized here (stdlib
`wave`, no RNG) so there is one source of truth for them; `music.mp3` and
`font.ttf` are committed sources under `samples/curated/audio/` (MP3 is not
byte-reproducible across encoders, so it is not generated). The zip uses fixed
entry timestamps, stored (uncompressed) entries and no directory records, so
the committed archive is a pure function of this script and the sources.

Regenerate after editing:
    python3 gallery/scripts/pack-curated-audio.py
Verify the committed pack is current (CI/humans):
    python3 gallery/scripts/pack-curated-audio.py --check
"""
import io
import math
import struct
import sys
import wave
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "samples" / "curated" / "audio"
OUT = ROOT / "samples" / "curated" / "audio-showcase.zip"

FIXED_DATE = (1980, 1, 1, 0, 0, 0)
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
    files = {
        "blip.wav": synth_blip(),
        "chime.wav": synth_chime(),
        "thud.wav": synth_thud(),
        "music.mp3": (SRC / "music.mp3").read_bytes(),
        "font.ttf": (SRC / "font.ttf").read_bytes(),
    }
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_STORED) as z:
        for name in sorted(files):
            info = zipfile.ZipInfo(name, date_time=FIXED_DATE)
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o644 << 16
            z.writestr(info, files[name])
    return buf.getvalue()


def main():
    data = build()
    if "--check" in sys.argv[1:]:
        current = OUT.read_bytes() if OUT.exists() else b""
        if current != data:
            print(
                f"{OUT} is stale: run python3 gallery/scripts/pack-curated-audio.py",
                file=sys.stderr,
            )
            return 1
        print(f"{OUT} is current")
        return 0
    OUT.write_bytes(data)
    print(f"wrote {OUT} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
