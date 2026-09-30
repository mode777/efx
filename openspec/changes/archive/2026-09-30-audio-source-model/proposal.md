# Proposal

## Why

F14 shipped a split API — `playBackgroundMusic(path)` vs
`playAudioEffect(path)`/`playSound(data)` with separate `Music` and `Sound`
classes — that fuses two orthogonal axes into one vocabulary:

- **decode strategy**: a fully-decoded sample buffer (`SoundData`) vs an
  incrementally-decoded stream (the singleton `efx_music`);
- **playback role**: one exclusive "music" track vs a 32-voice effect bank.

A script does not care whether a sound is "music" or an "effect" — only
whether it should be held in memory or streamed. The role labels also leak an
inconsistency: `playBackgroundMusic` takes a path, breaking the engine's
load-then-use paradigm (`loadImage` → `createTexture`, `loadMeshData` →
`createMesh`), while effects already load data first. LÖVE, this engine's
reference model, has no music/effect distinction at all: it has one
`Source` with a `"static" | "stream"` decode strategy, uniform playback
controls, and per-source volume (no channels). This change re-aligns the API
with that model.

## What Changes

- **Rename and generalize the resource kinds.** `loadAudioData(path)` returns
  a fully-decoded `AudioData` (today's `SoundData`); new
  `loadAudioStream(path)` returns a streamed `AudioStream` (the compressed
  bytes plus incremental decoder, today's singleton music). **BREAKING**:
  `loadSoundData`/`SoundData` are renamed.
- **One playback verb.** `playAudio(source, opts?)` accepts **either** an
  `AudioData` or an `AudioStream` and returns a unified **`Audio`** playback
  handle. The `opts` bag sets initial `volume`/`pitch`/`pan`/`loop` only;
  the handle is the control surface.
- **One handle shape.** `Audio` exposes read-only `playing`/`paused` and
  read-write `volume`/`pitch`/`pan`/`loop`, plus `stop`/`pause`/`resume`/
  `destroy`. Fades and group control are ordinary JS over the handle —
  volume is no longer a property of the play call.
- **Streams are first class.** A streamed source is no longer a singleton:
  up to a documented cap of concurrent streamed playbacks (≥2) may mix,
  enabling crossfade and overlapping long tracks.
- **Master gain.** `efx.audio.volume` is a read-write overall gain (LÖVE's
  `love.audio.setVolume`), the only grouping control added.
- **BREAKING removals.** `playBackgroundMusic`, `stopBackgroundMusic`,
  `playAudioEffect`, `playSound`, and the `Music`/`Sound` classes are removed
  (subsumed by `loadAudioStream`/`playAudio` and `Audio`).
- The pure-C core is generalized: one player bank whose voices reference
  either a static buffer or a streaming decoder+ring, retiring the singleton
  `efx_music`.
- **Explicit non-goal:** no script-visible channels, buses, sends, or mixing
  graph. The "no channels" wall and per-handle + master gain stand.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `audio`: replace the "Streaming background music" and "Polyphonic
  sound-effect voice bank" requirements with a static/stream source model over
  a unified playback bank; amend "Engine-owned mixing with no script-visible
  channels" to name per-handle and master gain as the only volume controls;
  state the concurrent-stream cap.
- `js-api`: update the native-backed class list (`AudioData`, `AudioStream`,
  `Audio` replace `SoundData`, `Sound`, `Music`), the documented query
  properties per class, and the fixed-limits table (streams cap).
- `feature-roadmap`: update the F14 description and its verification-gate
  wording to the static/stream source model.

## Impact

- **Core:** `src/audio/` (`audio.h`/`audio.c`/`decode.*`) — generalize
  `efx_sound_data`/`efx_music` into one source abstraction and one voice bank;
  retire the singleton music path.
- **Bindings:** `src/api/api.c` and `src/web/bridge.c` — replace the
  `SoundData`/`Sound`/`Music` classes and audio namespace entries.
- **API docs:** `gallery/src/api/efx.d.ts` (rename/add classes and
  functions), then regenerate `docs/api/` via `npm --prefix gallery run
  docs:markdown`; update the Audio model section of `docs/js-api.md`; update
  the F14 current-state text in `AGENTS.md`.
- **ADR:** needed — a new `docs/decisions/0047-audio-source-model.md`
  records the static/stream + unified-handle decision and the deliberate
  "no channels" stance, and notes which parts of ADR 0042 (backend, mixing,
  vendoring) still stand.
- **Tests/samples:** `tests/unit/api_tests.c`, `tests/scripts/s_14_audio.js`,
  `tests/fixtures/audio/`, and `gallery/samples/curated/audio-showcase` (plus
  its mountable pack) migrate to the new API.
- **Dependencies:** none added; the vendored `sokol_audio`/`dr_libs` stack and
  the decode-only PCM wall are unchanged.

## Non-goals

- Script-visible channels, buses, sends, effects routing, or any mixing graph.
- Volume/pitch fade helpers built into the engine — fades stay pure JS over
  the handle.
- 3D/positional audio, attenuation, Doppler, or listener state.
- DSP effects (reverb, filters), recording, and network/URL streaming.
- Timed sequence playback, MIDI, tracker modules, or PSF formats.
- Seeking and `.duration`/position queries (may be a later change).
