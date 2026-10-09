#!/usr/bin/env python3
"""Regenerate the synthesized F14 effect WAVs shipped by curated samples.

Each bank is synthesized deterministically here (stdlib `wave`, no RNG) so
there is one source of truth for the loose WAVs committed in the sample
directories (which are also the player's resource roots). `music.mp3` and
`font.ttf` are committed sources beside them; sample directories are packed
as-is (no separate packer).

Banks:
  * `audio-showcase/`  — the three F14 tour effects (blip, chime, thud)
  * `game-bloom-breakout/` — paddle, brick, wall, life

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
CURATED = ROOT / "samples" / "curated"

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


# ---- audio-showcase -------------------------------------------------------
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


# ---- game-bloom-breakout --------------------------------------------------
def synth_paddle():
    """Mid wooden knock: 300 Hz sine with a fast decay and a click edge."""
    n = int(0.08 * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / 0.002) * math.exp(-t * 55.0)
        v = 0.8 * math.sin(2.0 * math.pi * 300.0 * t) + \
            0.2 * math.sin(2.0 * math.pi * 600.0 * t)
        out[i] = v * env * 0.8
    return wav_bytes(out)


def synth_brick():
    """Bright glassy tick: 900->1500 Hz chirp, very short."""
    n = int(0.07 * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 900.0 + (1500.0 - 900.0) * (t / 0.07)
        phase += f / RATE
        env = math.exp(-t * 60.0)
        out[i] = math.sin(2.0 * math.pi * phase) * env * 0.7
    return wav_bytes(out)


def synth_wall():
    """Dull low thunk for a wall bounce: 180 Hz, short."""
    n = int(0.06 * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / 0.002) * math.exp(-t * 60.0)
        out[i] = math.sin(2.0 * math.pi * 180.0 * t) * env * 0.7
    return wav_bytes(out)


def synth_life():
    """Descending failure tone: 520->130 Hz over 0.4 s."""
    n = int(0.4 * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 520.0 + (130.0 - 520.0) * (t / 0.4)
        phase += f / RATE
        env = min(1.0, t / 0.01) * math.exp(-t * 6.0)
        out[i] = math.sin(2.0 * math.pi * phase) * env * 0.7
    return wav_bytes(out)


# ---- game-glow-gauntlet ---------------------------------------------------
def synth_gauntlet_music():
    """~8 s seamless looping synth arpeggio over a four-chord progression.

    Deterministic; each eighth-note pluck and each beat bass note decays to
    near-silence before the loop point, so the WAV loops cleanly with
    `loadAudioStream`.
    """
    beat = 0.5  # 120 bpm
    bar = 4.0 * beat
    total = 4.0 * bar  # 8 s
    n = int(total * RATE)
    roots = [220.00, 174.61, 130.81, 196.00]  # A3, F3, C3, G3
    arp = [0, 4, 7, 12, 7, 4]
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        bar_i = int(t / bar) % 4
        root = roots[bar_i]
        step = int((t % bar) / (beat / 2.0))
        semi = arp[step % len(arp)]
        f = root * (2.0 ** (semi / 12.0))
        tn = t % (beat / 2.0)
        lead = 0.34 * (math.sin(2.0 * math.pi * f * t) +
                       0.3 * math.sin(2.0 * math.pi * 2.0 * f * t)) * math.exp(-tn * 9.0)
        bt = t % beat
        bass = 0.24 * math.sin(2.0 * math.pi * (root / 2.0) * t) * math.exp(-bt * 5.0)
        out[i] = (lead + bass) * 0.75
    return wav_bytes(out)


def synth_gauntlet_sting():
    """A short descending hit for death: 440->70 Hz with a fast decay."""
    n = int(0.5 * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 440.0 * math.exp(-t * 4.5) + 70.0
        phase += f / RATE
        env = min(1.0, t / 0.003) * math.exp(-t * 7.0)
        out[i] = (math.sin(2.0 * math.pi * phase) + 0.35 * math.sin(4.0 * math.pi * phase)) * env * 0.7
    return wav_bytes(out)


# ---- game-bot-arena -------------------------------------------------------
def _lcg(seed):
    s = seed & 0x7FFFFFFF
    while True:
        s = (s * 1103515245 + 12345) & 0x7FFFFFFF
        yield s / 0x7FFFFFFF


def synth_arena_shot():
    """Laser zap: 900->220 Hz with a fast decay."""
    n = int(0.09 * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 900.0 + (220.0 - 900.0) * (t / 0.09)
        phase += f / RATE
        env = math.exp(-t * 40.0)
        out[i] = math.sin(2.0 * math.pi * phase) * env * 0.6
    return wav_bytes(out)


def synth_arena_hit():
    """Bright tick: 1200 Hz, very short."""
    n = int(0.05 * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        env = math.exp(-t * 70.0)
        out[i] = math.sin(2.0 * math.pi * 1200.0 * t) * env * 0.55
    return wav_bytes(out)


def synth_arena_explosion():
    """Noise burst over a low thump."""
    n = int(0.5 * RATE)
    out = [0.0] * n
    noise = _lcg(0x9E3779B9)
    prev = 0.0
    for i in range(n):
        t = i / RATE
        env = math.exp(-t * 8.0)
        white = next(noise) * 2.0 - 1.0
        prev = prev * 0.6 + white * 0.4  # cheap low-pass
        thump = 0.6 * math.sin(2.0 * math.pi * 60.0 * t) * math.exp(-t * 10.0)
        out[i] = (prev * 0.5 + thump) * env * 0.9
    return wav_bytes(out)


def synth_arena_wave():
    """Rising two-tone alert for a new wave."""
    n = int(0.5 * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        f = 330.0 if t < 0.25 else 495.0
        env = min(1.0, t / 0.005) * math.exp(-(t % 0.25) * 6.0)
        out[i] = (math.sin(2.0 * math.pi * f * t) + 0.3 * math.sin(4.0 * math.pi * f * t)) * env * 0.55
    return wav_bytes(out)


def synth_arena_music():
    """~8 s seamless looping synth bassline over a minor progression."""
    beat = 0.5
    bar = 4.0 * beat
    n = int(4.0 * bar * RATE)
    roots = [110.00, 98.00, 87.31, 82.41]  # A2, G2, F2, E2
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        bar_i = int(t / bar) % 4
        root = roots[bar_i]
        step = int((t % bar) / (beat / 2.0))
        # eighth-note bass with an accent on the offbeat
        f = root * (2.0 if step % 3 == 2 else 1.0)
        tn = t % (beat / 2.0)
        bass = 0.34 * math.sin(2.0 * math.pi * f * t) * math.exp(-tn * 7.0)
        pad = 0.12 * math.sin(2.0 * math.pi * (root * 2.0) * t)
        out[i] = (bass + pad) * 0.75
    return wav_bytes(out)


def build_banks():
    return {
        CURATED / "audio-showcase": {
            "blip.wav": synth_blip(),
            "chime.wav": synth_chime(),
            "thud.wav": synth_thud(),
        },
        CURATED / "game-bloom-breakout": {
            "paddle.wav": synth_paddle(),
            "brick.wav": synth_brick(),
            "wall.wav": synth_wall(),
            "life.wav": synth_life(),
        },
        CURATED / "game-glow-gauntlet": {
            "music.wav": synth_gauntlet_music(),
            "sting.wav": synth_gauntlet_sting(),
        },
        CURATED / "game-bot-arena": {
            "shot.wav": synth_arena_shot(),
            "hit.wav": synth_arena_hit(),
            "explosion.wav": synth_arena_explosion(),
            "wave.wav": synth_arena_wave(),
            "music.wav": synth_arena_music(),
        },
    }


def main():
    banks = build_banks()
    if "--check" in sys.argv[1:]:
        stale = []
        for out_dir, files in banks.items():
            for name, data in files.items():
                path = out_dir / name
                if not path.exists() or path.read_bytes() != data:
                    stale.append(str(path.relative_to(ROOT)))
        if stale:
            print(
                "stale synthesized WAVs (" + ", ".join(stale) + "): "
                "run python3 gallery/scripts/gen-audio-assets.py",
                file=sys.stderr,
            )
            return 1
        total = sum(len(f) for f in banks.values())
        print(f"all {total} synthesized WAVs are current")
        return 0
    written = 0
    for out_dir, files in banks.items():
        out_dir.mkdir(parents=True, exist_ok=True)
        for name, data in files.items():
            (out_dir / name).write_bytes(data)
            written += 1
    print(f"wrote {written} WAVs across {len(banks)} banks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
