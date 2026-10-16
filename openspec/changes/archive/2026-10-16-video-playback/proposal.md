# Proposal

## Why

Games need full-motion video — intro stings, cutscenes, attract-mode loops,
in-world screens — and the engine has no way to play one. The PS2-era
aesthetic this engine targets makes an old-school FMV channel a natural
completion of the media stack (images F2/F6, audio F14, video now).
Decoding is deliberately scoped to **MPEG-1** via a single-header vendored
decoder: permissive (MIT), patent-expired, ~0.1 MB on wasm, and its soft
VHS-era look is on-brand rather than a defect. This change depends on the
`dynamic-textures` change for the per-frame texture-update seam.

## What Changes

- Vendor **pl_mpeg** (phoboslab, MIT, single-header C99 MPEG-1 video +
  MPEG audio layer 2 decoder with its own container parser; pinned per
  ADR 0006) — `vendor/pl_mpeg/` + `vendor/README.md` pin row.
- New pure-C `src/video/` module (no Sokol, no quickjs — ADR 0003 walls):
  `efx_video` opens over an in-memory buffer from the F6a resource provider,
  decodes incrementally, outputs RGBA frames (engine consumes full-frame
  RGB→RGBA) and interleaved stereo float32 MP2 PCM (the F14 internal sample
  format).
- Script-facing **Video** class via a new `efx.video` sub-namespace
  (ADR 0050 organization): `openVideo(path, opts?)` with `{ loop, muted }`
  (cold-path option-bag validation in the shared prelude, ADR 0049). The
  Video object:
  - exposes a read-only `texture` property — an engine-created dynamic
    Texture the script passes to any existing draw (`drawQuad`, material
    maps, particles); no new drawing concepts,
  - playback surface: `play()`, `pause()`, read-only `currentTime`,
    `duration`, `width`, `height`, read-write `loop` and `muted`,
    read-only `ended`, `destroy()` (GC finalizer backstop + explicit
    destroy, ADR 0011/0012),
  - playback time advances with the engine frame delta while playing.
- **Implicit engine-managed audio**: `play()` attaches the video's decoded
  MP2 PCM as one streaming voice in the existing F14 voice bank; no
  script-visible handle or channel. `pause()`/`loop`/`destroy()` and the
  ADR 0057 game swap all couple automatically; `muted` silences audio
  without touching video; when no stream slot is available the video plays
  silent rather than failing.
- Determinism fixtures + tests: a tiny committed procedural `.mpg` test
  fixture (author-time ffmpeg, generation-time tool like sokol-shdc — never
  linked), unit tests for the decode/state core, a golden scene drawing a
  decoded frame gated on all four targets (ADR 0020), error-catalog cases
  (non-MPEG-1 bytes, use-after-destroy, bad options).
- Gallery showcase sample `video-showcase` (ADR 0044 layout) using a **short
  Big Buck Bunny MPEG-1 excerpt** (Blender Foundation, CC-BY — attribution
  carried in the sample page and README), catalog regeneration, wasm size
  check on the Emscripten player.
- New ADR (`docs/decisions/0063`) for the codec/library choice;
  `gallery/src/api/efx.d.ts` surface, regenerated `docs/api/`, prelude
  regen.

## Capabilities

### New Capabilities

- `video-playback`: the script-facing MPEG-1 video surface — `openVideo`
  loading from the resource root, the Video object's texture property and
  playback state machine, determinism/CPU-only behavior, error surface, and
  the resource/memory model (destroy, game-swap coupling).

### Modified Capabilities

- `audio`: adds a requirement for video-attached audio — engine-managed
  streaming voice, no script-visible handle, mute/pause/loop/swap coupling,
  and stream-slot contention behavior.

## Impact

- **Vendor**: `vendor/pl_mpeg/` (header + LICENSE), `vendor/README.md` row.
- **Code**: new `src/video/` (vendored impl TU with relaxed warnings +
  `efx_video.h` wrapper); `src/api/api_video.c` desktop wire natives;
  `src/web/bridge_video.c` + `src/web/js/video.js` fragment (CMakeLists
  concat order); `src/audio/` gains a video-attached streaming-voice
  attachment point; `src/prelude/prelude.js` option validation +
  `tools/gen_prelude.py` regen; CMake targets on all four toolchains.
- **Depends on**: `dynamic-textures` (video creates its Texture through the
  dynamic texture path and calls the update entry point per decoded frame).
- **Script API**: new `efx.video` members; `gallery/src/api/efx.d.ts`,
  regenerated `docs/api/`, type-test coverage.
- **Gallery**: `gallery/samples/curated/video-showcase/` (+ BBB excerpt
  asset, attribution), `gallery/scripts/gen-catalog.mjs` catalog regen.
- **Tests**: `src/video/` unit suite (fixture-driven decode, state machine,
  loop/pause/ended, audio attachment + contention), golden
  `tests/goldens/video_frame/`, error-catalog cases, headless `--script`
  smoke test, Emscripten size measurement recorded in the change.
