# Tasks

## 1. Prerequisites and vendoring

- [x] 1.1 Confirm F1–F2 and F6a gates are green (F14's only predecessors) and that the `feature-roadmap` delta declaring F14 orthogonal (preserving F13) is coherent; verify by reading the archived F9/F6a changes and the current roadmap spec
- [x] 1.2 Vendor `sokol_audio.h` from the already-pinned Sokol commit under `vendor/sokol/` and record/confirm its row in `vendor/README.md` (same commit, Zlib); verify a configure/build with the header present and no network succeeds
- [x] 1.3 Vendor a pinned `dr_libs` snapshot under `vendor/dr_libs/` (`dr_wav.h`, `dr_mp3.h` + upstream license text) and record its row in `vendor/README.md` (project, revision, source, Unlicense-or-MIT-0, chosen-over alternatives); verify a configure/build with the headers present and no network succeeds
- [x] 1.4 Add the decoder implementation TU (`src/audio/dr_impl.c`) that defines the dr_wav/dr_mp3 implementations exactly once with relaxed warnings, matching the cgltf/stb treatment; verify it compiles into `efx_core` on a local build and the headers' warnings do not trip `-Werror`

## 2. Pure-C mixing and decoding core

- [x] 2.1 Implement the float32-stereo mixer and the deterministic `efx_audio_mix(out, frames)` entry point in `src/audio/`, plus a device-free lifecycle; verify a headless unit test mixes a known source and asserts exact output samples and that repeated mixes are bit-identical
- [x] 2.2 Implement the decoder abstraction over `dr_wav`/`dr_mp3` (open from a provider buffer, report frame count/rate/channels, read N frames, seek to start) and the internal sample conversion; verify a headless test decodes a committed WAV and MP3 fixture and asserts frame counts and sample values within tolerance
- [x] 2.3 Implement linear-interpolation resampling from file rate to device rate and the `pitch` playback-rate multiplier over it; verify a headless test plays a fixture at a different device rate and at pitch 1.0/2.0 and asserts expected output length and a known sample
- [x] 2.4 Implement error handling for malformed/truncated/unsupported resources so a bad load fails with an error code and never crashes; verify a headless test feeds a truncated and a non-audio buffer and asserts clean failure
- [x] 2.5 Add the deterministic control/injection seam (start effect/music, stop, set volume/pan/pitch, advance the pump) used by tests and not script-visible; verify a headless test drives a full start → mix → stop sequence through the production path

## 3. Voice bank and streaming music

- [x] 3.1 Implement the fixed 32-voice SFX bank with per-voice volume/pan/pitch/loop and the documented deterministic steal policy (quietest non-looping, then oldest; reject when all looping); verify headless unit tests for overlap up to the limit, steal order, and the all-looping rejection
- [x] 3.2 Implement the single streaming background-music source with a ~1-second device-rate ring buffer, incremental decode, loop (on/off), pause/resume/stop/volume, and start-replaces-current; verify headless tests for streaming without full residency, loop continuity at the ring boundary, and the control matrix
- [x] 3.3 Implement the bounded voice bank and deterministic control path (push model, single-threaded): control calls mutate engine state directly and the mixer validates/steals at mix time with no allocation; verify headless tests for allocation order, voice-bank overflow, and allocation-free mixing (the push model makes a request queue unnecessary — recorded in design D2)

## 4. Platform backend and web behavior

- [x] 4.1 Add `src/platform/audio_backend.c` compiled into `efx_platform` only: `saudio_setup` in push mode (no stream callback, enlarged `buffer_frames`), and a per-frame tick called from the frame loop that pumps music, mixes `saudio_expect()` frames via `efx_audio_mix`, and calls `saudio_push`; verify the pure-C core and headless test targets do not link the backend and a native windowed run on the verification server plays a fixture
- [x] 4.2 Implement no-device soft-fail: when `saudio_isvalid()` is false (or setup fails), the player runs silently and audio calls remain safe; verify a headless/CI run of the player starts, runs its script, and exits 0 with no audio device
- [x] 4.3 Implement web autoplay unlock: resume the audio context on the first keyboard/mouse input via sokol_audio's DOM listeners plus an explicit `efx.audio.resume()` path, with pre-unlock requests deferred or reported consistently; verify in the browser harness that music starts after a synthetic first input and that behavior matches the documented contract
- [x] 4.4 Confirm the Emscripten build needs no `-sASYNCIFY` and that decoding uses the in-memory resource buffer (no async fetch); verify a web build with and without the flag and record the result in the design doc

## 5. Script bindings

- [x] 5.1 Register `efx.audio` in `src/api/api.c`/`src/runtime/runtime.c`: `playBackgroundMusic`, `stopBackgroundMusic`, `resume`, `loadSoundData`, `playSound`, and the `SoundData`/`Sound`/`Music` classes with their `destroy()`/methods/properties; verify headless unit tests for the query/control/validation/destroy matrix
- [x] 5.2 Add the pure-JS `playAudioEffect` path cache and the high-level sugar over `loadSoundData`/`playSound` in `src/prelude/prelude.js` (regenerating `src/prelude/prelude.h` and passing `gen_prelude.py --check`); verify a headless script starts the same effect repeatedly and observes one decode
- [x] 5.3 Mirror `efx.audio` in `src/web/bridge.c` and `src/web/entry.js` with identical names, semantics, and errors; verify `tools/run_web_compare.mjs` diffs desktop vs web at zero for an audio script

## 6. Tests and integration harness

- [x] 6.1 Add committed tiny WAV and MP3 fixtures under `tests/fixtures/` and headless unit tests for the `audio` capability (mix determinism, decode, resampling/pitch, error handling, voice overlap/steal, music loop/controls, no-device); verify they pass in an `EFX_HEADLESS=ON` local build
- [x] 6.2 Add a portable script-level harness that loads a fixture, starts music and effects, drives controls, and asserts script-observable results end to end; verify it runs green in ctest on the desktop build and on Emscripten via the bridge
- [x] 6.3 Add a cross-runtime compare case for the audio script; verify desktop and web outputs are identical

## 7. Docs and ADR

- [x] 7.1 Write ADR `docs/decisions/0042-audio-mixing-and-vendoring.md` (next free number — re-check 0039–0041 at apply time) covering the vendored stack, push-mode mixing, the fixed voice bank and steal policy, decoded-PCM-only, and no-device/web-unlock behavior, per `TEMPLATE.md`; add it to `docs/decisions/README.md` and verify the index row links the new file
- [x] 7.2 Add the F14 audio model to the API design guidelines `docs/js-api.md` (resource table + fixed limits + audio model + traceability) and regenerate the committed per-symbol reference `docs/api/` from `gallery/src/api/efx.d.ts`; verify every signature matches the implementation and `npm --prefix gallery run docs:check` passes
- [x] 7.3 Add the `efx.audio` types to `gallery/src/api/efx.d.ts`; verify the gallery build type-checks (`npm --prefix gallery run check`)
- [x] 7.4 Update `AGENTS.md` (roadmap table gains F14 as orthogonal with its non-visual gate; current-state and script-API sections mention audio) and add the audio rows to `vendor/README.md`; verify the roadmap spec's "documented in AGENTS.md" requirement is satisfied

## 8. Verification gate

- [x] 8.1 Run the four-target gate in order per AGENTS.md: `python3 tools/verify_remote.py all <branch>` on the SSH verification server, then `gh workflow run ci.yml --ref <branch>`, confirming Linux, then Windows, then macOS green; note F14 has no golden-image gate — the gate is the headless pure-C audio unit tests plus the portable script harness on all four targets, the cross-runtime compare, and the no-device soft-fail check
