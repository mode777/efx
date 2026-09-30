# 0047 — F14 audio source model: static/streamed sources over one playback bank

Status: Accepted (2026-09, change `audio-source-model`)

Supports: ADR 0004 (single `efx` namespace), ADR 0011/0012 (dynamic resources
are GC-finalized opaque classes), ADR 0042 (the push-mode, vendored
`sokol_audio` + `dr_libs` stack — its backend, mixing, and decoded-PCM-only
decisions still stand).
Relates to: ADR 0019 (value-snapshot small state, handle-reference resources).

## Context

The original F14 script API split audio into a "background music" role
(`playBackgroundMusic`, `Music`, one singleton streamed source) and a
"sound-effect" role (`playSound`/`playAudioEffect`, `Sound`, a 32-voice bank).
That split fused two orthogonal axes: **decode strategy** (fully-decoded PCM
vs incremental streaming) and **playback role**. It also broke the engine's
load-then-use paradigm — `playBackgroundMusic(path)` took a path while effects
loaded data first — and imposed a hard "only one music stream" limit that made
crossfades impossible. LÖVE, the project's reference model, has no
music/effect distinction: one `Source` with a `static`/`stream` type and
uniform per-source controls. See `openspec/changes/audio-source-model/` for
the full record.

## Decision

- **Two source kinds, loaded separately from playback.** `AudioData` is
  fully-decoded, immutable PCM (`loadAudioData`) and can back many overlapping
  playheads; `AudioStream` is a compressed resource decoded incrementally by
  each playhead (`loadAudioStream`), with a per-playhead decoder and ~1 s ring.
- **One playback verb and one handle class.** `efx.audio.playAudio(source,
  opts?)` accepts either kind and returns a native-backed `Audio` handle with
  read-only `playing`/`paused`, read-write `volume`/`pan`/`pitch`/`loop`, and
  `stop`/`pause`/`resume`/`destroy`. Start-time options are **initial values
  only**; the handle is the control surface, so fades are ordinary handle
  writes in `update`.
- **One fixed playback bank.** `src/audio/` mixes 32 voices, each referencing
  either a static buffer or a streaming playhead; the number of simultaneous
  streaming voices is capped at **4**. The deterministic steal policy
  (quietest non-looping, oldest first; deterministic reject when all looping)
  applies bank-wide, and the stream cap steals a stream voice rather than
  exceeding itself.
- **Master gain only.** `efx.audio.volume` is the single read-write master
  output gain. There are **no** script-visible channels, buses, sends, or
  mixing graph, and no per-source buses.
- **Resource exposure.** `AudioData`, `AudioStream`, and `Audio` are
  native-backed opaque classes with idempotent `destroy()` and a GC-finalizer
  backstop (ADR 0011/0012); the fixed bank stays engine-owned (not a
  script-visible slot bank). A playing handle retains its source.
- **Removed.** `playBackgroundMusic`, `stopBackgroundMusic`,
  `playAudioEffect`, `playSound`, and the `Music`/`Sound` classes are gone;
  no deprecated aliases are kept.

## Consequences

- Scripts choose static vs stream explicitly at load time and control every
  playback the same way; "music" and "effect" are no longer engine concepts.
- Multiple streams may play at once (up to the cap), enabling crossfades and
  overlapping long tracks; each streamed playhead costs a decoder + ~1 s ring.
- The `audio` and `js-api` specs and the committed reference encode the new
  classes, the one-verb rule, and the 4-stream/32-voice limits; a future change
  to those numbers updates both.
- The `SoundData`/`Sound`/`Music` names and the music/effect entry points are
  gone, so ADR 0042's script-surface description is superseded by this record;
  the vendored stack, push-mode mixing, decoded-PCM-only wall, no-device
  soft-fail, and web unlock are unchanged.
- Fades and ducking remain script-side: the engine deliberately ships no
  envelope/tween API, keeping the core a deterministic function of its state.

## Rejected alternatives

- **Keep the music/effect split** — it is the conflation this record exists to
  remove; the only real difference is decode strategy.
- **LÖVE-style `source:play()` (the loaded object is the playhead)** — makes a
  static source a singleton playhead and forces `clone()` for overlapping; the
  data/handle split already gives overlapping static playback for free.
- **Auto-select static vs stream by duration/size** — implicit policy at the
  border is surprising and hard to test; an explicit loader is predictable.
- **Script-visible channels or buses** (the original request's first sketch) —
  a mixing graph the spec forbids; the fade/crossfade motivation is satisfied
  by handle writes, and grouping beyond the master gain is left to the game.
- **A singleton music stream retained alongside the bank** — blocks crossfade
  and keeps the role split alive.
- **Engine fade helpers (`fadeIn`/`fadeOut` options)** — an envelope system for
  something three lines of `update` does; the old spec prose mentioned these
  but they were never implemented and are not carried forward.
