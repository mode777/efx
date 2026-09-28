#!/usr/bin/env python3
"""Pack the authored F10 showcase modules into a deterministic zip.

The gallery mounts at most one zip as a sample's resource root (F6a, ADR 0030),
so the CommonJS modules the `modules-showcase` sample requires must live inside
`modules-showcase.zip`. The readable sources are committed under
`gallery/samples/curated/modules/`; this script packs them at the archive root
with fixed entry timestamps, stored (uncompressed) entries and no directory
records, so the committed zip is a pure function of the source tree.

Regenerate after editing a module:
    python3 gallery/scripts/pack-curated-modules.py
Verify the committed pack is current (CI/humans):
    python3 gallery/scripts/pack-curated-modules.py --check
"""
import io
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "samples" / "curated" / "modules"
OUT = ROOT / "samples" / "curated" / "modules-showcase.zip"

# fixed date (1980-01-01) so the archive is byte-identical across runs/hosts
FIXED_DATE = (1980, 1, 1, 0, 0, 0)


def build() -> bytes:
    files = sorted(p for p in SRC.rglob("*") if p.is_file())
    if not files:
        raise SystemExit(f"no module sources under {SRC}")
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_STORED) as z:
        for path in files:
            rel = path.relative_to(SRC).as_posix()
            info = zipfile.ZipInfo(rel, date_time=FIXED_DATE)
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o644 << 16
            z.writestr(info, path.read_bytes())
    return buf.getvalue()


def main() -> int:
    data = build()
    if "--check" in sys.argv[1:]:
        current = OUT.read_bytes() if OUT.exists() else b""
        if current != data:
            print(f"{OUT} is stale: run python3 gallery/scripts/pack-curated-modules.py",
                  file=sys.stderr)
            return 1
        print(f"{OUT} is current")
        return 0
    OUT.write_bytes(data)
    print(f"wrote {OUT} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
