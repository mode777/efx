#!/usr/bin/env python3
"""Regenerate the shared radial glow/shadow PNG shipped by curated samples.

One canonical texture — white RGB with an alpha falloff over a normalized
disc — is generated deterministically here (stdlib `zlib` + `struct`, no
Pillow, no RNG) and written as `radial.png` into every curated sample
directory that draws a radial glow or shadow. Each sample loads it with
`loadImage` + `createTexture` and tints it at draw time, so no sample
hand-rolls the falloff loop (see the `Shared radial glow texture asset`
requirement in the web-gallery spec).

Regenerate after editing:
    python3 gallery/scripts/gen-radial-assets.py
Verify the committed PNGs are current (CI/humans):
    python3 gallery/scripts/gen-radial-assets.py --check
"""
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CURATED = ROOT / "samples" / "curated"

SIZE = 64

# Sample directories that ship the shared radial image.
CONSUMERS = [
    "blob-shadow-showcase",
    "game-bloom-breakout",
    "game-bot-arena",
    "game-glow-gauntlet",
    "game-mini-golf",
    "game-sky-steps",
    "particle-plane-showcase",
    "particles-showcase",
]


def _chunk(tag, body):
    return (
        struct.pack(">I", len(body))
        + tag
        + body
        + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
    )


def radial_png(size=SIZE):
    """White RGBA, alpha (1 - d)^2 over a normalized disc (d clamped to 1).

    The falloff matches the byte loop the samples used to hand-roll, so the
    generated image is a drop-in replacement for the non-shadow cases.
    """
    raw = bytearray()
    for y in range(size):
        raw.append(0)  # filter type 0 (None) for this scanline
        dy = (y + 0.5) / size * 2.0 - 1.0
        for x in range(size):
            dx = (x + 0.5) / size * 2.0 - 1.0
            d = min(1.0, (dx * dx + dy * dy) ** 0.5)
            a = int(round((1.0 - d) * (1.0 - d) * 255.0))
            raw += bytes((255, 255, 255, a))
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + _chunk(b"IEND", b"")
    )


def main():
    data = radial_png()
    if "--check" in sys.argv[1:]:
        stale = []
        for name in CONSUMERS:
            path = CURATED / name / "radial.png"
            if not path.exists() or path.read_bytes() != data:
                stale.append(str(path.relative_to(ROOT)))
        if stale:
            print(
                "stale radial.png (" + ", ".join(stale) + "): "
                "run python3 gallery/scripts/gen-radial-assets.py",
                file=sys.stderr,
            )
            return 1
        print(f"all {len(CONSUMERS)} radial.png copies are current")
        return 0
    for name in CONSUMERS:
        out_dir = CURATED / name
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "radial.png").write_bytes(data)
    print(f"wrote radial.png into {len(CONSUMERS)} sample directories")


if __name__ == "__main__":
    sys.exit(main())
