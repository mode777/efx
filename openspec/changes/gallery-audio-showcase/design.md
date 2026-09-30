# Design

## Context

See `proposal.md` — Why. The gallery mounts at most one zip as a sample's
resource root (F6a, ADR 0030); curated samples ship an optional committed
pack beside `manifest.json` and `gen-catalog.mjs` copies it into the site.
`pack-curated-modules.py` is the precedent for a deterministic, authored
pack. The showcase must run identically on desktop (device present) and in
the browser (audio locked until a gesture), and must not throw in either.

## Goals / Non-Goals

**Goals:** a visitor-audible, interactive F14 tour that needs no engine
change; a deterministic authored pack; free coverage from the existing
gallery smoke plus one explicit player smoke.

**Non-goals:** any `efx` API change, a golden scene, third-party audio.

## Decisions

### D1 — Authored, deterministic pack

`gallery/scripts/pack-curated-audio.py` writes `audio-showcase.zip` from:
- three WAV effects **synthesized in-memory** (Python `wave`, fixed sample
  rate/length/amplitude; no RNG) — `blip.wav`, `chime.wav`, `thud.wav`;
- `audio/music.mp3`, a committed ~6 s looping arpeggio (encoded once with a
  pinned ffmpeg; MP3 is not byte-reproducible across encoders, so it is a
  source, not generated);
- `audio/font.ttf`, the CC0 Kenney font already used by the text showcase.

The zip uses fixed entry timestamps and stored entries (same recipe as the
modules pack), so `--check` is a byte comparison. WAVs are synthesized by
the script so there is one source of truth for them.

### D2 — Showcase interaction

The sample is a sound board drawn into the fixed 640×480 2D frame:

- Three labeled pads; a click (or `1`/`2`/`3`) starts the matching effect
  with `pan` from the click x and `pitch` from the click y, so the 32-voice
  bank is audibly polyphonic. Each start pulses the panel.
- Background music starts at load (`loop: true`). On the first pointer/key
  input the sample calls `efx.audio.resume()`, covering the browser
  autoplay rule; the panel shows `Music: playing|paused|stopped` and volume.
- `Space` toggles pause/resume, `M` toggles mute, `[`/`]` change volume.
- `efx.window.size` maps surface pixels into the frame, exactly like the
  input playground.

The script never assumes a device: `playAudioEffect` returns `null` when no
voice/device is available, and `Music.playing` is a boolean, so the panel
degrades to a "no device / click to unlock" state without throwing.

### D3 — Verification

- `smoke_showcase_audio` runs `player --script gallery/samples/curated/
  audio-showcase.js --root gallery/samples/curated/audio-showcase.zip` on
  the desktop player (headless `--script` mode is device-free, so it proves
  the pack loads and the script's API calls do not throw).
- The catalog id `curated:audio-showcase` sorts into the first samples
  `tools/run_gallery_smoke.mjs` drives in headless Chrome, so the browser
  path is exercised for free; the smoke fails on any console/page error.
- `python3 gallery/scripts/pack-curated-audio.py --check` guards pack drift.

## Risks / Trade-offs

- **[Browser autoplay]** → the sample calls `resume()` on first input and
  treats a locked context as "not playing"; the smoke asserts no console
  error rather than sound.
- **[MP3 source is not regenerable byte-for-byte]** → it is committed and
  documented; the drift check only zips it.
- **[Gallery smoke runs the first N samples]** → adding an early-sorting id
  shifts which samples are smoke-run; acceptable (all still boot without
  error).
