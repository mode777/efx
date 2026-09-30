# Design

## Context

See `proposal.md` — Why. The current F14 shape (from `2026-09-30-f14-audio`):

- `src/audio/audio.h` exposes two unrelated representations: `efx_sound_data`
  (fully-decoded PCM) and the singleton `efx_music` (compressed bytes +
  incremental `efx_decoder` + ~1 s device-rate ring), with separate entry
  points `efx_audio_play_effect` and `efx_audio_play_music`.
- The binding (`src/api/api.c`) registers `SoundData`/`Sound`/`Music` classes
  and the namespace functions `loadSoundData`, `playSound`,
  `playAudioEffect`, `playBackgroundMusic`, `stopBackgroundMusic`, `resume`.
  `src/web/bridge.c` mirrors these.
- The spec and `docs/js-api.md` encode a "music vs effect" vocabulary and a
  hard "one streamed background-music source; no script-visible channels"
  rule.
- The pure-C core is device-free and headless-testable; `sokol_audio` is
  confined to `src/platform/audio_backend.c` and the per-frame tick calls
  `efx_audio_music_pump()` then `efx_audio_mix()`.
- Tracks are frame-coupling push-mode; there are no threads or atomics.

Constraints that shape the approach: keep the pure-C/headless seam and the
vendored `sokol_audio` + `dr_libs` stack; keep module walls (ADR 0003); keep
the "no script-visible channels/buses" wall; expose dynamic native resources
as GC-finalized opaque classes with `destroy()` (ADR 0011/0012); never add
free globals (ADR 0004).

## Goals / Non-Goals

**Goals:**

- One source abstraction with exactly two kinds (static, streamed) and one
  playback verb/handle class, so "music" and "effect" stop being engine
  concepts.
- Load is separate from playback; the same static source can back many
  independent playbacks.
- Volume/pitch/pan/loop are live handle properties; initial options are only
  initial values, so fade/crossfade/retune are plain JS.
- More than one stream can play at once (bounded), enabling crossfade.
- A single master output gain exists; no other grouping control.
- Preserve the four-target, device-free, deterministic test seam.

**Non-Goals (design-level):**

- No script-visible channels, buses, sends, or mixing graph of any kind.
- No engine fade/envelope API — fading is handle writes in `update`.
- No 3D/positional audio, attenuation, Doppler, or listener state.
- No seek, `duration`, or playback-position queries in this change (the
  streamed playhead is internal for now).
- No DSP, recording, or network/URL streaming.
- No change to decoding, resampling, pan law, or the vendored stack.

## Decisions

### D1 — Two source kinds, one playback verb (drop the role split)

A source is either **static** (`AudioData`, decoded PCM resident) or
**streamed** (`AudioStream`, compressed bytes + incremental decoder). Both are
produced by loaders and consumed by one `playAudio(source, opts?)` that returns
one `Audio` handle class. The "music" and "effect" roles are removed.

- Alternatives:
  - **Keep the role split** (`Music` vs `Sound`): rejected — it is the
    conflation this change exists to remove; the only real difference is
    decode strategy, which belongs to the source, not the play call.
  - **Auto-select static vs stream by file duration/size** (no explicit
    loader): rejected — implicit policy is surprising and untestable at the
    border; LÖVE and this engine both prefer an explicit choice.
  - **One loader with an option bag** (`loadAudio(path, { stream })`):
    viable, but two named loaders read better in the reference and type
    document and match the two distinct classes.

### D2 — The handle is the control surface; options are initial values

`playAudio(source, opts?)` accepts `volume`/`pan`/`pitch`/`loop` as *initial*
values only; the returned `Audio` exposes `playing`/`paused` (read-only),
`volume`/`pan`/`pitch`/`loop` (read-write), and `stop`/`pause`/`resume`/
`destroy`. This directly answers "volume should not be the playback command's
concern": a fade is `h.volume = t` across frames.

- Alternatives:
  - **LÖVE method style** (`source:play()` where the loaded object is the
    handle): rejected for v1 because it makes an `AudioData` a singleton
    playhead and forces `clone()` for overlapping — the data/handle split
    (D3) already gives overlapping static playbacks for free.
  - **No start-time options at all** (set everything after play): rejected —
    an initial `loop`/`volume` is too convenient to drop, and the values are
    snapshotted before the first mix so there is no audible default-then-set
    pop.
  - **Engine fade helpers** (`fadeIn`/`fadeOut` options): rejected — an
    envelope system costs state and API surface for something three lines of
    `update` does. (Note the old spec prose mentioned `fadeIn`/`fadeOut`; it
    was never implemented and is not carried forward.)

### D3 — Data/handle separation for both kinds

Loaders return resources; `playAudio` creates a playhead and returns a handle.
`AudioData` is immutable PCM, so many handles share one decode. `AudioStream`
is a *resource* holding the compressed bytes; each `playAudio(stream)` opens
its own decoder + ring (a distinct playhead), so the same stream resource can
be started twice. A playing handle **retains** its source; destroying a source
while handles are live defers the native release (same discipline as material
map textures).

- Alternatives:
  - **`AudioStream` is its own single playhead** (LÖVE-ish): rejected — it
    makes the resource stateful, muddies `destroy()` semantics when playing,
    and prevents a streamed resource from being restarted without reload.
  - **Copy PCM for every static playback** instead of sharing: rejected —
    wasteful; a refcounted immutable buffer is cheaper and matches
    `efx_audio_sound_data_retain`.

### D4 — One fixed playback bank; a documented stream cap

The existing 32-voice bank is generalized: a voice references either a static
`efx_sound_data` or a streaming playhead (decoder + ring). Static voices are
cheap; a streamed voice costs ~1 s of ring plus decoder state, so the number of
*simultaneously streaming* voices is capped at **4** (documented). If the
stream cap or the voice bank is full, the existing deterministic policy
applies (steal the quietest non-looping voice, then the oldest; reject
deterministically if every voice is looping).

- Alternatives:
  - **Keep the singleton stream**: rejected — blocks crossfade and the user's
    core motivation.
  - **Unbounded streams**: rejected — each stream is a decoder and a ring;
    memory must be bounded on a fixed-function engine.
  - **Separate stream bank from the 32 static voices**: rejected — two pools
    and two steal policies for no benefit; one bank with a stream sub-cap is
    simpler and keeps deterministic ordering.
  - **Cap of 2**: sufficient for a crossfade but tight for layered ambience;
    4 bounds memory (~1.4 MB of ring at 44.1 kHz) while allowing layering.

### D5 — Master gain only; no channels/buses

`efx.audio.volume` is a read-write master output gain applied in `efx_audio_mix`
after the voice sum. Per-source gain is the handle's `volume`. No other
grouping exists.

- Alternatives:
  - **Named/fixed buses** (music/sfx/master tree): rejected — a mixing graph
    the spec forbids, more engine state, and the user's fade need does not
    require it.
  - **`stopAll()` / `muteAll()` helpers**: deferred — `stop()` per handle and
    `volume = 0` cover them; adding verbs can be a later change.

### D6 — Resource exposure and lifecycle

`AudioData`, `AudioStream`, and `Audio` are native-backed opaque classes with
idempotent `destroy()` and a GC-finalizer backstop (ADR 0011/0012). The fixed
playback bank remains engine-owned and is **not** a script-visible slot bank:
scripts hold handles, never indices. This mirrors the existing F14
classification and the config's resource rule.

### D7 — Naming

`loadAudioData` → `AudioData`, `loadAudioStream` → `AudioStream`,
`playAudio(source, opts?)` → `Audio`.

- Alternatives:
  - **Keep `SoundData`/`Sound`, add `SoundStream`**: rejected — `SoundData`
    and `AudioData` both fit the `*Data` family (ImageData/MeshData/FontData),
    but since the rename is breaking anyway, `Audio`/`AudioStream` gives a
    clean, symmetric triad.
  - **`Source`** for the handle (LÖVE): rejected — `AudioStream` is also a
    "source", so reusing the word would collide.

### D8 — Breaking removals and migration shims

`playBackgroundMusic`, `stopBackgroundMusic`, `playAudioEffect`, `playSound`,
and the `Music` class are removed; `Sound` is renamed to `Audio`. No
deprecated aliases are kept — the engine is pre-1.0 and the gallery/tests are
the only consumers; aliases would keep the role vocabulary alive.

## Risks / Trade-offs

- **[Breaking API churn]** → all call sites are in-repo (`src/api`,
  `src/web/bridge`, `gallery/samples/curated/audio-showcase`,
  `tests/`); migrate them in the same change and update the spec, type
  document, generated reference, and `js-api.md`.
- **[Stream voice memory]** → the cap of 4 bounds ring memory; a streamed
  voice that is stolen frees its decoder/ring with the voice.
- **[Streamed voice steal determinism]** → the bank-wide policy already
  orders by volume then start sequence; streams participate identically, so
  tests stay exact.
- **[Source destroyed while playing]** → playing handles retain the source and
  defer release; a destroyed source cannot start a *new* playback (throws).
- **[MP3 gapless looping]** → unchanged from F14; loop seams are a known
  residual, not a gate.
- **[Headless/no-device CI]** → unchanged: the core initializes device-free and
  the backend soft-fails; the gate stays non-visual.
- **[Web autoplay unlock]** → unchanged behavior; `resume()` and first-input
  unlock keep working because they live in the backend, not the class shape.

## Migration Plan

Breaking, in one change. Order:

1. Generalize the core (`src/audio/`): a source union + playhead abstraction
   over the existing voice/ring code; retire the singleton music path; keep
   `efx_audio_mix` deterministic; add a master gain.
2. Rewrite the binding (`src/api/api.c`) and bridge (`src/web/bridge.c`):
   classes `AudioData`/`AudioStream`/`Audio`; namespace
   `loadAudioData`/`loadAudioStream`/`playAudio`/`volume`/`resume`.
3. Update `gallery/src/api/efx.d.ts`, regenerate `docs/api/`, refresh the
   Audio model in `docs/js-api.md` and the F14 state in `AGENTS.md`, and write
   the new ADR.
4. Migrate `tests/fixtures/audio`, `tests/scripts/s_14_audio.js`,
   `tests/unit/api_tests.c`, and the `audio-showcase` sample (including a
   crossfade/fade demonstration in place of the old music/effect UI).
5. Verify on the SSH server (native + web) then run the four-target gate.

Rollback is reverting the branch; no data or on-disk format changes.

## Open Questions

- Whether to add seek/`duration`/position later — does not change the specs,
  approach, or task breakdown.
- Whether the stream cap should be configurable — 4 is fine for now; the
  spec/jst-api pin it, so a change would update them.
- Whether a path-accepting `playAudio` convenience (with a script-side cache)
  is wanted — deferred; the explicit load-then-play pair is the contract.
