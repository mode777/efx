# Proposal

## Why

EmotionFX has no audio at all: `vision.md` targets "ps2-era graphics" and
game feel, but the engine cannot play a single sound. Games need a streamed
background music track and a handful of overlapping sound effects, and the
engine's resource provider (F6a) already gives scripts synchronous access to
files inside the dir/zip root, so adding audio now is a small, self-contained
step. Sokol — the engine's rendering platform — ships `sokol_audio`, but it is
only a device + callback: it mixes nothing and decodes nothing, so the engine
must own the mixer, the voice bank, and the format decoders.

This change adds audio as a new **orthogonal milestone F14** (predecessors:
F1–F2 for the runtime/frame loop and dual bindings, and F6a for the dir/zip
resource provider; independent of F3–F13). It vendors `sokol_audio.h` from the
already-pinned Sokol snapshot plus `dr_libs` (`dr_wav.h`, `dr_mp3.h`) for WAV
and MP3 decoding, builds a dependency-free pure-C mixing core, and exposes one
small author-facing surface: `efx.audio.playBackgroundMusic` and
`efx.audio.playAudioEffect`.

## What Changes

- **New orthogonal milestone F14 (audio playback)** in the roadmap
  (predecessors F1–F2 + F6a; independent of F3–F13), verified by headless
  unit tests over the pure-C mixing/decoding core plus a portable script
  harness on all four targets — **no golden image**.
- **Vendored audio stack**: `vendor/sokol/sokol_audio.h` taken from the
  existing pinned Sokol commit, and a pinned `dr_libs` snapshot
  (`vendor/dr_libs/`) supplying `dr_wav` + `dr_mp3`. Both permissive
  (sokol: Zlib; dr_libs: Unlicense/public-domain **or** MIT-0), single-header,
  no external dependencies, four-target. The decoder implementation lives in
  one dedicated TU with relaxed warnings, matching the cgltf/stb/miniz
  treatment. Recorded in `vendor/README.md`.
- **New pure-C audio core in `src/audio/`** (no Sokol, no quickjs, ADR 0003):
  a deterministic mixer (`efx_audio_mix(out, frames)`), a fixed polyphonic
  sound-effect voice bank, a streaming music source with a decode ring buffer,
  a decoder abstraction over `dr_wav`/`dr_mp3`, linear-interpolation
  resampling (which doubles as pitch), and a lock-free command queue that
  carries script-thread requests to the audio thread. The whole core is
  drivable headlessly with no device, so it is unit-testable.
- **Platform audio backend** in `src/platform/audio_backend.c`: initializes
  `saudio` in callback mode, pumps music decoding from the main thread, and
  **soft-fails to silence when no audio device exists** (headless CI) rather
  than aborting the player.
- **Engine-owned mixing**: scripts never see channels, buses, sample rates,
  or buffers. Voice allocation, pan/pitch, envelopes, and streaming are the
  engine's job.
- **Script surface `efx.audio`**:
  - `playBackgroundMusic(path, opts?)` → a `Music` handle (`stop`, `pause`,
    `resume`, `setVolume`, read-only `playing`); plus
    `stopBackgroundMusic(opts?)` for the current track.
  - `playAudioEffect(path, opts?)` → a `Sound` handle (`stop`, read-only
    `playing`, read-write `volume`/`pan`/`pitch`); loading is cached by path.
  - Mid-level `loadSoundData(path)` → `SoundData` and `playSound(soundData,
    opts?)` → `Sound` for preloading/reuse.
  - Format WAV (PCM/float) and MP3; errors throw `TypeError`/`Error` with the
    engine's existing conventions.
- **New native-backed resource classes** `SoundData`, `Sound`, and `Music`
  (ADR 0011/0012: explicit `destroy()` plus a GC-finalizer backstop), with
  fixed limits (documented): one background-music stream and a fixed
  sound-effect voice bank (32 voices) with a deterministic steal policy.
- **Web behavior**: `sokol_audio` on Emscripten uses Web Audio; because of the
  browser autoplay policy the engine SHALL resume/unlock audio on the first
  user input or via an explicit `efx.audio.resume()`, and audio decoding uses
  the in-memory resource buffer (no async fetch).
- **Docs**: `docs/js-api.md` gains the `efx.audio` section; `gallery/src/api/
  efx.d.ts` gains the types; `AGENTS.md` roadmap/current-state/script-API
  sections gain F14; a **new ADR** records the durable decisions (vendored
  stack, callback-mixing + main-thread decode pump, command-queue boundary,
  fixed voice bank, decoded-PCM-only).

### Explicit non-goals

- **No modular / sequenced audio formats (PSF2/PSF, tracker modules, MIDI).**
  The PS2's hardware and the PSF2 format split audio into a sequencer program
  plus instrument samples. Those formats are obsolete and lack modern
  authoring/content; the engine plays **decoded PCM only** — streamed music or
  fully-decoded samples — never a score + instrument program.
- **No 3D positional audio / listener model** in v1 (pan is the only spatial
  control). A later change may add it.
- **No audio DSP effects** (reverb/chorus/echo/filters) in v1.
- **No recording, capture, or input from a microphone.**
- **No script-visible mixing graph** (buses, sends, per-channel routing).
- **No streaming from a URL** — resources come from the dir/zip root only.

## Capabilities

### New Capabilities

- `audio`: the engine audio layer — the vendored `sokol_audio` + `dr_libs`
  stack and its offline/four-target contract, the pure-C deterministic mixer
  and its headless seam, the streaming background-music source, the fixed
  polyphonic sound-effect voice bank and steal policy, the WAV/MP3 decoder and
  resampling contract, the engine-owned mixing guarantee (no script-visible
  channels), the no-device soft-fail behavior, web autoplay unlock, the fixed
  limits, and the decoded-PCM-only (no sequenced formats) rule.

### Modified Capabilities

- `js-api`: adds the `efx.audio` namespace, the `SoundData`/`Sound`/`Music`
  native-backed resource classes, and the new fixed limits (one music stream,
  32 sound-effect voices); extends the resource-class list and the reference
  catalog's milestone range to F14.
- `feature-roadmap`: declares **F14 (audio playback)** as a new orthogonal
  milestone (predecessors F1–F2 + F6a) with its scope and non-visual
  verification gate, preserving F13.

## Impact

- **Core**: new `src/audio/` module (mixer, voice bank, music stream, decoder
  wrappers, command queue) compiled into `efx_core`; pure C, no Sokol/quickjs,
  unit-testable headlessly.
- **Platform**: new `src/platform/audio_backend.c` (sokol_audio setup +
  callback + main-thread decode pump + no-device soft-fail), compiled into
  `efx_platform` only.
- **Vendored**: `vendor/sokol/sokol_audio.h` (same pinned commit) and
  `vendor/dr_libs/` (`dr_wav.h`, `dr_mp3.h` + license); new rows in
  `vendor/README.md`.
- **Bindings**: `src/api/api.c` + `src/runtime/runtime.c` (desktop quickjs)
  and `src/web/bridge.c` + `src/web/entry.js` (web) expose `efx.audio` with
  identical semantics/errors; new runtime host-state entries for the audio
  cache and live handles.
- **Docs**: `docs/js-api.md`, `gallery/src/api/efx.d.ts`, `AGENTS.md`, and a
  **new ADR `docs/decisions/0042-audio-mixing-and-vendoring.md`** (next free
  number — re-check at apply time; 0039–0041 are claimed by F11/F12/F13).
- **Tests**: headless unit tests over the pure-C mixer/decoder/voice bank
  (deterministic, no device) with tiny committed WAV + MP3 fixtures, a
  portable script-level harness on all four targets, and a cross-runtime
  compare; no golden image.
- **Dependencies**: two new third-party dependencies (sokol_audio — same pin
  as existing Sokol; dr_libs — new pin). The dependency evaluation is recorded
  in `design.md` per the roadmap's third-party-dependency requirement.

## Roadmap position

This change implements **F14**, a new **orthogonal** milestone whose only
predecessors are **F1–F2** (the runtime/frame loop and the dual desktop/web
script bindings) and **F6a** (the dir/zip resource provider that makes
synchronous `load*` possible). It does not depend on F3–F13 and may land
independently of them. The `feature-roadmap` delta is written as a
**superset** that preserves F13 so archiving this change does not drop it.
An ADR is required (new `docs/decisions/0042-audio-mixing-and-vendoring.md`):
the vendored stack, the callback-mixing + main-thread decode-pump model, the
lock-free command-queue boundary, the fixed voice bank, and the decoded-PCM-only
rule are durable decisions future changes must respect.
