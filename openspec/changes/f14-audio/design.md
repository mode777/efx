# Design

## Context

See `proposal.md` — Why. The engine currently has no audio path at all. The
relevant existing structure:

- `src/platform/` owns Sokol and the frame loop; `sokol_app`/`sokol_gfx` are
  vendored from one pinned commit (`vendor/README.md`). The platform layer
  already translates backend callbacks into a pure-C core for input
  (`src/platform/platform.c` → `src/input/`), which is the model to follow.
- `src/` is one core static library plus thin per-target mains (ADR 0003);
  pure-C subsystems (`input/`, `physics/`) are device-free and unit-tested
  headlessly under `-DEFX_HEADLESS=ON`.
- F6a gives scripts synchronous `load*` from a dir/zip root; decoded audio
  reads its bytes through `efx_resource_read`.
- Fixed limits and GC discipline: dynamic resources are GC-finalized
  opaque classes with explicit `destroy()` (ADR 0011/0012); fixed banks get
  slots (lights). Scripts are pure ES6 with no host dependencies (ADR 0008).
- CI is headless (xvfb + llvmpipe) and has **no audio device**; the
  Emscripten golden/browser harness runs in headless Chrome.

## Goals / Non-Goals

**Goals:**

- One streamed background-music source and 32 overlapping sound-effect
  voices, mixed by the engine, with no channel/voice management in scripts.
- WAV + MP3 decoding from the resource root, on all four targets.
- A device-free, deterministic pure-C core that headless tests can drive and
  assert sample-exactly.
- Player survives a missing audio device (headless CI) and browser autoplay
  blocking.

**Non-Goals (design-level, beyond the proposal's):**

- No 3D listener/positional model, no DSP effects, no recording.
- No general-purpose mixing graph or script-visible buses.
- No async/`Promise` audio API — the resource provider is synchronous and the
  audio API stays synchronous (consistent with `loadImage`/`loadMeshData`).
- No streaming from a network URL.

## Decisions

### D1 — Vendor `sokol_audio` (pinned Sokol) + `dr_libs` for decoding

Add `vendor/sokol/sokol_audio.h` from the already-pinned Sokol commit, and a
pinned `dr_libs` snapshot (`vendor/dr_libs/dr_wav.h`, `dr_mp3.h` + license).
The decoder implementation compiles in one dedicated TU with relaxed warnings
(`src/audio/dr_impl.c`), matching `cgltf_impl.c`/`stb_image_impl.c`.

Dependency evaluation (per the roadmap's third-party-dependency requirement):

| Candidate | License | Four-target | C11 fit | Verdict |
|---|---|---|---|---|
| **sokol_audio** | Zlib | yes (WASAPI/CoreAudio/ALSA-Pulse/WebAudio) | already vendored family, behind platform wall | **chosen** — one pinned commit, matches ADR 0003 |
| **dr_libs** (`dr_wav`+`dr_mp3`) | Unlicense **or** MIT-0 | yes | single-header C99, memory + incremental decode | **chosen** — one dep covers both formats, streaming-capable |
| minimp3 | CC0-1.0 | yes | single header | rejected — decode-only, still needs a WAV lib |
| stb_vorbis | public domain | yes | single header | rejected — wrong format (Ogg), user asked WAV/MP3 |
| libmpg123 | LGPL | yes | system lib | rejected — static single-binary + relink obligations |
| miniaudio | public domain / MIT-0 | yes | single header | rejected — bundles its own device layer, a second platform abstraction alongside Sokol; decoders are dr_libs-derived anyway |
| SDL_mixer / SoLoud | Zlib / various | yes | system/heavy | rejected — large surface, brings a second mixer/device model |

### D2 — Push-mode mixing from the frame loop

`saudio` is configured in **push mode** (no `stream_cb`). Each frame the main
thread (a) pumps the music decoder into a ring buffer (~1 second) and (b)
mixes `saudio_expect()` frames with `efx_audio_mix` and calls `saudio_push`.
All audio state lives on the main thread, so there are **no threads, no
atomics, and no cross-thread lifetimes** — the core stays a deterministic pure
function of its state. This is the PS2-authentic model (games mixed into the
SPU2 ring each frame) and is trivially unit-testable headless.

```
main/script thread (one thread, per frame)
──────────────────────────────────────────
efx.audio.*  ──enqueue request──▶  efx_audio_mix(out, frames):
efx_audio_music_pump()                  drain queued requests
  decode → music ring (≈1s)             mix 32 SFX voices (RAM)
saudio_push(out, frames)                read music ring
```

Alternatives:
- **Callback mode** (`stream_cb` on a separate audio thread): best timing and
  frame-rate independence, but requires C11 atomics for the lock-free command
  ring and per-voice state — and **MSVC does not support C11 atomics**, so the
  four-target Windows build would need an `Interlocked*` shim plus careful
  cross-thread lifetime handling (refcounts, deferred frees, completion flags)
  for no v1 benefit. Rejected.
- **Callback draining a ring the main thread mixed into**: keeps the callback
  trivial (a memcpy), but still needs a portable cross-thread SPSC ring and
  memory barriers. Rejected as unnecessary complexity.
- **Dedicated decoder thread**: best isolation, most machinery; dr_mp3 decodes
  far faster than realtime, so a 1-second ring absorbs ordinary hitches.
  Rejected.

### D3 — Module split and walls

- `src/audio/` (pure C, in `efx_core`): mixer, fixed voice bank, music stream
  + ring, decoder abstraction, command ring, name/error helpers. No Sokol, no
  quickjs. Exposes `efx_audio_mix`, a control/injection seam for tests, and a
  device-free lifecycle. This is what headless tests exercise.
- `src/platform/audio_backend.c` (in `efx_platform` only): `saudio_setup`
  (push mode, enlarged `buffer_frames`), the per-frame music-pump + mix + push
  tick called from the frame loop, web unlock-on-input, and soft-fail when
  `saudio_isvalid()` is false.

This mirrors input/physics and keeps the vendored library out of the test
core. The Emscripten build needs no `-sASYNCIFY`: `sokol_audio`'s WebAudio
path and the in-memory decode are synchronous (confirmed by the server
Emscripten build).

### D4 — Resource and API shape

Native-backed classes (ADR 0011/0012):

- `SoundData` — fully-decoded PCM, `destroy()`, no query props. From
  `loadSoundData(path)`.
- `Sound` — one playing SFX voice: `destroy()`/`stop()`, read-only `playing`,
  read-write `volume`/`pan`/`pitch`. From `playSound`/`playAudioEffect`.
- `Music` — the streaming source: `destroy()`/`stop()`, `pause()`, `resume()`,
  `setVolume()`, read-only `playing`. From `playBackgroundMusic`.

`playBackgroundMusic` and `playAudioEffect` are **pure-JS sugar** over the C
mid-level (`loadSoundData`/`playSound`) plus a module-level path→`SoundData`
cache for effects; this honors the two-layer rule and keeps the C surface
explicit. The fixed voice pool is engine-owned: scripts hold per-voice handles,
never indices, so it is not a script-visible slot bank.

Alternatives:
- One `Sound` class for both SFX and music: rejected — streaming has a decoder
  ring and a long lifecycle that an in-memory voice does not.
- Slot indices (`playAudioEffect` returns a number, `stopAudioEffect(id)`):
  rejected — less ergonomic, and generation-tracking to detect reuse is more
  complex than a handle; the repo already prefers opaque classes.
- Fire-and-forget (return nothing): rejected — you cannot stop a looping
  ambience or adjust a channel; a returned handle is ignorable, so it costs
  nothing.

### D5 — Fixed voice bank and steal policy

32 SFX voices (documented limit), matching the "fixed bank" idiom. When all
voices are busy, steal the **quietest non-looping** voice, then the **oldest**;
if all are looping, reject the new request deterministically. This is pure,
testable, and avoids dynamic native allocation on the audio thread.

### D6 — Internal format, resampling, and limits

Mix in float32 stereo. Each source is resampled from its file rate to the
device rate with linear interpolation; `pitch` is a playback-rate multiplier
over that ratio (so pitch and rate conversion share one code path, and a
44.1 kHz file on a 48 kHz device plays at correct speed). Pan is a documented
linear L/R gain. Music ring ≈ 1 second at the device rate.

### D7 — No-device soft-fail and web unlock

`saudio_setup` failure or invalid device leaves audio in a silent, safe state:
`efx_audio_mix` still runs (or is skipped) without crashing, calls report not
playing, and the player continues. On Emscripten the engine resumes the audio
context on the first keyboard/mouse input (hooking the existing F9 input
path) and also exposes `efx.audio.resume()`; requests before unlock are
deferred/reported consistently.

### D8 — Decoded PCM only (PSF2 and friends are a non-goal)

The engine plays decoded PCM. It will not implement sequencer/score +
instrument-program formats (PSF2/PSF, tracker modules, MIDI); unsupported
resources fail with an error. This is a deliberate scope wall, recorded in the
`audio` spec and the ADR.

## Risks / Trade-offs

- **[Frame-rate coupling]** → Push mode is frame-coupled: a long hitch can
  underrun. Mitigate with an enlarged `buffer_frames` (~93ms) and by pushing
  `saudio_expect()` every frame; degradation is a silent gap, never a crash.
- **[Headless CI has no device]** → Platform backend soft-fails on
  `!saudio_isvalid()`; the unit-tested core never touches `saudio`, so the
  gate is device-free.
- **[Browser autoplay policy]** → Resume on first input + explicit `resume()`;
  document pre-unlock behavior; web harness asserts state, not sound.
- **[MP3 gapless looping]** → Encoder delay/padding can leave a small seam on
  loop; WAV loops clean. Mitigate by honoring dr_mp3's reported delay/padding
  where available and documenting the residual; exactness is not a gate.
- **[Main-thread decode hitch]** → ~1 s ring; decode is much faster than
  realtime; a pathological frame could still underrun, which degrades to a
  silent gap, never a crash.
- **[SFX cache memory growth]** → Cache decoded effects by path with a size
  cap and simple eviction (LRU); `SoundData` is also explicitly loadable and
  destroyable.
- **[Cross-target decode determinism]** → One pinned decoder snapshot; tests
  assert frame counts and sample values with tolerance, plus identical
  cross-runtime script output.
- **[Linux backend availability on the verification server]** → Soft-fail
  keeps the suite green; audio output itself is not part of the gate.

## Migration Plan

Additive: no existing behavior changes. New code lands behind the new
`src/audio/` module and platform backend; `EFX_HEADLESS=ON` still builds the
pure-C tests without Sokol. Rollback is reverting the change branch — no data
or format migration is involved.

## Open Questions

- Exact pan law and fade curves (linear vs equal-power): deferrable, does not
  change the specs or the task breakdown; pin in `docs/js-api.md` at apply.
- Whether to add 3D positional audio or DSP effects later: explicitly a future
  change; not decided here.
- Whether the SFX cache should be configurable: default cap is fine for v1.
