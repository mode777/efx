# 0042 — F14 audio: push-mode mixing over a vendored sokol_audio + dr_libs stack

Status: Accepted (2026-09, change `f14-audio`)

## Context

F14 adds audio to the engine: one streamed background-music source and a fixed
bank of sound-effect voices, with WAV and MP3 loading, all mixed by the engine
so scripts never see channels. The platform layer already uses Sokol for the
window and GPU; `sokol_audio` is the natural device layer, but it is only a
device + callback (it neither mixes nor decodes). The four-target gate includes
Windows, and **MSVC does not support C11 atomics**, which rules out the
lock-free callback-mixing design that was initially considered. The engine's
established pattern is a pure-C, device-free, headless-testable core behind a
thin platform backend (input, physics).

## Decision

Audio is a **push-model** subsystem. A pure-C core in `src/audio/` owns one
fixed 32-voice sound-effect bank, a single streamed background-music source
with a ~1 s device-rate ring buffer, a decoder abstraction over vendored
`dr_wav`/`dr_mp3`, linear-interpolation resampling (which doubles as pitch),
and a deterministic `efx_audio_mix(out, frames)` entry point. All state lives
on the main thread: there are no threads, no atomics, and no cross-thread
lifetimes. Each frame the platform backend (`src/platform/audio_backend.c`,
compiled into `efx_platform` only) pumps the music decoder, mixes
`saudio_expect()` frames, and `saudio_push`es them.

`vendor/sokol/sokol_audio.h` is taken from the already-pinned Sokol commit
(push mode, enlarged `buffer_frames`); `vendor/dr_libs/` supplies WAV and MP3
decoding (Unlicense **or** MIT-0). Both are confined behind the platform wall.
The engine plays **decoded PCM only** — no sequenced/modular formats
(PSF2/PSF, tracker modules, MIDI). When no device is present the backend
soft-fails to silence; on web the browser suspends the AudioContext until a
gesture and sokol_audio resumes it on the first input, with
`efx.audio.resume()` as an explicit path.

## Consequences

- The audio core is deterministic and unit-testable headlessly with no device
  (`efx_audio_tests` embeds tiny WAV/MP3 fixtures), matching the input/physics
  precedent. There is no golden-image gate.
- Audio is coupled to frame rate: a long hitch can underrun (mitigated by the
  ~93 ms device buffer). Degradation is a silent gap, never a crash.
- Scripts get a small author-facing surface (`efx.audio.playBackgroundMusic`,
  `playAudioEffect`) plus native-backed `SoundData`/`Sound`/`Music` classes;
  the fixed voice pool is engine-owned and never a script-visible slot bank.
- The Linux build links ALSA (`libasound2-dev`), macOS links `AudioToolbox`,
  Windows uses WASAPI's pragma-comment libs; CI installs `libasound2-dev`.
- Future audio work (3D positioning, DSP effects, multiple music streams)
  builds on the same pure-C core; the spec's decoded-PCM-only wall stands.

## Rejected alternatives

- **Callback mode with a lock-free SPSC command ring** (the original design
  D2): best timing, but requires C11 atomics, which MSVC lacks — it would need
  an `Interlocked*` shim plus cross-thread refcount/deferred-free handling for
  no v1 benefit. Rejected during apply.
- **Callback draining a ring the main thread mixed into**: keeps the callback
  trivial, but still needs a portable cross-thread ring and barriers.
- **A dedicated decoder thread**: best isolation, most machinery; dr_mp3
  decodes far faster than realtime, so a 1 s ring absorbs ordinary hitches.
- **miniaudio**: bundles its own device layer, a second platform abstraction
  alongside Sokol; its decoders are dr_libs-derived anyway.
- **minimp3** (decode-only, still needs a WAV lib), **stb_vorbis** (Ogg, not
  the requested formats), **libmpg123** (LGPL — static single-binary + relink
  obligations), **SDL_mixer/SoLoud** (large surface, second mixer model).
