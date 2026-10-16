# Design

## Context

pl_mpeg (phoboslab) is a single-header C99 MPEG-1 video + MP2 audio decoder
with its own container parser, incremental decode over a memory buffer, and
interleaved RGB output — the same shape as the vendored dr_libs audio
decoders (F14). The engine's audio core is a fixed voice bank with a push
model (ADR 0042): all state on the main thread, one mix per frame, so video
frame decode and video audio decode share one clock with no device-thread
sync. The `dynamic-textures` change provides the frame→texture seam.

## Goals / Non-Goals

- Goals: MPEG-1 playback into any texture slot, implicit audio, four-target
  parity, deterministic decode, minimal binary growth (~0.1 MB wasm est.).
- Non-Goals: seeking/scrubbing, reverse playback, codec choice per game,
  script-accessible raw frames, hardware decode, subtitles/menus, MPEG-2.

## Decisions

- **D1 — MPEG-1 via pl_mpeg.** Evaluated against FFmpeg (LGPL static-link
  obligations + multi-MB bloat — fails the F14 precedent and single-binary
  vision), platform hardware decode (MediaFoundation/VideoToolbox/VAAPI +
  `<video>` on web — divergent APIs, Linux has no unified surface, violates
  platform parity), libvpx VP8/WebM (BSD but real build integration + EBML
  parser + Vorbis/Opus audio deps to halve file sizes), dav1d/AV1 (meson +
  nasm build, decode-heavy in wasm, overkill for the aesthetic), and
  MJPEG-in-AVI (zero decode deps but ~10× file sizes, no in-band audio).
  pl_mpeg wins on vendoring pattern fit, license (MIT), patent status
  (MPEG-1 pool expired), and wasm size. VP8/WebM remains the escalation path
  if author-side file sizes prove painful; the script surface would not
  change.
- **D2 — Pure-C `src/video/` module mirroring `src/audio/decode.c`.**
  `efx_video_open(bytes, size)` → decoder; `efx_video_advance(dt)` decodes
  video frames due in the elapsed window into an RGBA scratch buffer and
  pulls audio PCM into the stream ring; getters for size/duration/fps/time.
  No Sokol/quickjs includes (ADR 0003); impl TU with relaxed warnings like
  miniz/cgltf/dr_libs.
- **D3 — Engine-clock master, small decode-ahead.** Video time advances by
  the frame delta while playing; `advance` decodes exactly the frames whose
  timestamps fall into the elapsed window (bounded work per frame — no
  unbounded catch-up spiral: if the script stalls longer than the clip,
  decode jumps to the latest frame boundary and reports the skip via
  `currentTime`, never looping decode internally). Two-frame RGBA scratch
  ring; texture update happens once per produced frame
  (`efx_render_texture_update`).
- **D4 — Texture owned by the video.** The engine creates the dynamic
  Texture at open (core-level, not through the JS `createTexture` path),
  exposes it read-only, and destroys it with the video. Script-side destroy
  of the video's texture directly is rejected by the existing ownership
  rules (TypeError). Audio-free videos work identically.
- **D5 — Implicit audio as a streaming-voice attachment.** The audio core
  gains one internal attachment point: a voice whose PCM source is the
  video's decode ring rather than an `efx_audio_stream` (the MP2 stream is
  not a standalone WAV/MP3 resource, so the existing stream loader cannot
  open it). `play()` allocates under the existing stream-cap/steal policy;
  failure → silent video (documented in the spec delta). `muted` zeroes the
  voice gain without detaching. `efx_audio_stop_all` (ADR 0057) already
  sweeps every voice, so game-swap coupling is free.
- **D6 — `efx.video` sub-namespace.** Mirrors `efx.audio`/`efx.noise`
  (ADR 0050); drawing stays with the existing draw functions via the
  `texture` property — no `drawVideo` helper in v1 (a trivial prelude
  wrapper can be added later without breaking anything). Option-bag
  validation for `openVideo` is cold-path in the shared prelude (ADR 0049);
  `play`/`pause`/getters are thin native-validated calls.
- **D7 — Fixtures at authoring time.** The unit/golden fixture is a tiny
  procedural `.mpg` (e.g. 64×64, ~1 s, indexed color pattern + short tone)
  produced once with ffmpeg (`-c:v mpeg1video -c:a mp2`) and committed;
  ffmpeg is a generation-time tool like sokol-shdc (ADR 0021 precedent),
  never a build dependency. The showcase uses a short Big Buck Bunny excerpt
  (Blender Foundation, CC-BY; attribution in the sample page + README;
  re-encoded 512×384@24, `-q:v` tuned to stay in the low MBs) — per user
  direction.

## Risks / Trade-offs

- [MPEG-1 file sizes are 2–5× H.264] → acceptable for intro/attract-length
  clips; documented asset guidelines pin resolution/bitrate; VP8/WebM is the
  pre-planned escalation (D1) behind an unchanged script surface.
- [Main-thread decode cost in wasm at higher resolutions] → asset
  guidelines target ≤480p-class clips; the bounded decode-ahead (D3) keeps
  worst-case frame cost predictable; decode runs only while playing.
- [Video stalls hold the frame budget] → D3 caps per-frame decode work;
  long stalls degrade to a time skip, never a spiral.
- [Stream-slot contention silences intro videos] → follows the existing
  deterministic policy; a game that wants guaranteed video audio frees
  slots before `play()` (documented).
- [CC-BY attribution forgotten downstream] → attribution lives in the
  sample's README and rendered page header, and the catalog entry carries
  it.

## Migration Plan

Additive; depends on `dynamic-textures` landing first (texture seam). Video
is inert unless a script opens one — zero cost to existing games. Rollback =
revert; no persisted formats.

## Open Questions

None blocking. Exact fixture parameters (dimensions, duration) are settled
during implementation against golden-capture sizes.
