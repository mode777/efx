# Tasks

## 1. Core source model (`src/audio/`)

- [x] 1.1 Generalize the source representation into a union of a fully-decoded static source and a streamed source (compressed bytes + incremental decoder + ring), and a playhead abstraction over it; retire the singleton `efx_music`. Verify the headless `efx_audio_tests` build compiles and the existing mix/decode/resample assertions still pass after the refactor.
- [x] 1.2 Replace `efx_audio_play_effect`/`efx_audio_play_music` with one `efx_audio_play(source, volume, pan, pitch, loop)` over the fixed 32-voice bank that carries the documented steal policy (quietest non-looping, then oldest; deterministic reject when all looping) and enforces a documented cap of 4 concurrent streaming voices. Verify headless tests for: a static source backing overlapping voices, two streams mixing concurrently, the stream cap enforced deterministically, and the all-looping reject.
- [x] 1.3 Add the master output gain to the core (set/get) applied after the voice sum in `efx_audio_mix`. Verify headless tests that the gain scales the output and that zero is silent.
- [x] 1.4 Implement source lifetime for playheads: a playing voice retains its source, destroying a source with live playheads defers the native release, and a destroyed source cannot start a new playback. Verify a headless test covering retain/destroy-while-playing/reject-after-destroy.
- [x] 1.5 Confirm the core stays deterministic and device-free (no backend linkage). Verify an `EFX_HEADLESS=ON` build and a repeated-mix bit-identical test.

## 2. Script bindings

- [x] 2.1 Replace the audio classes in `src/api/api.c` with native-backed `AudioData`, `AudioStream`, and `Audio` (read-only `playing`/`paused`; read-write `volume`/`pan`/`pitch`/`loop`; `stop`/`pause`/`resume`/`destroy`) and register `efx.audio` = `loadAudioData`, `loadAudioStream`, `playAudio`, the `volume` accessor, and `resume`; remove `SoundData`/`Sound`/`Music` and the old functions. Verify `efx_api_tests audio_js` covers the class/method/property matrix, source-kind errors, `RangeError` on negative volume/master gain, and `destroy()` idempotence.
- [x] 2.2 Update `src/prelude/prelude.js` (remove the obsolete `playAudioEffect` path-cache sugar and any `Music`/`Sound` references), regenerate `src/prelude/prelude.h`, and pass `tools/gen_prelude.py --check`. Verify a headless smoke run exercises the prelude.
- [ ] 2.3 Mirror the namespace and classes in `src/web/bridge.c` (and `src/web/entry.js` if it references audio) with identical names, semantics, and errors. Verify `tools/run_web_compare.mjs` diffs desktop vs web at zero for the audio script.

## 3. Tests, fixtures, samples

- [x] 3.1 Rework the audio cases in `tests/unit/api_tests.c` and `tests/scripts/s_14_audio.js` to the new API. Verify `EFX_HEADLESS=ON` ctest passes.
- [ ] 3.2 Migrate the audio script fixture (`tests/fixtures/audio/`) and confirm the portable `smoke_14_audio` and `web_14_audio` cases pass on desktop and Emscripten.
- [ ] 3.3 Update `gallery/samples/curated/audio-showcase/main.js` to a fade/crossfade demo built on the handle API (and refresh its mountable pack if resources change). Verify `npm --prefix gallery run check` and that the sample runs.
- [ ] 3.4 Update the cross-runtime compare case `14_audio` and verify desktop and web outputs are identical.

## 4. Docs, type document, and ADR

- [x] 4.1 Update `gallery/src/api/efx.d.ts` (add `AudioData`/`AudioStream`/`Audio`, `loadAudioData`/`loadAudioStream`/`playAudio`, and the `volume` master gain; remove the old classes/functions) and `gallery/src/api/efx.type-test.ts`. Verify `npm --prefix gallery run check` type-checks.
- [x] 4.2 Regenerate the committed reference `docs/api/` from the declaration. Verify `npm --prefix gallery run docs:check` passes.
- [x] 4.3 Update the Audio model, resource table, fixed-limits table, and vision traceability row in `docs/js-api.md`. Verify the document reflects only the new API (no `Music`/`playBackgroundMusic`/`playAudioEffect`).
- [x] 4.4 Update `AGENTS.md` (current-state F14 text, script-API list, and roadmap table wording) to the static/stream source model. Verify the roadmap spec's "documented in AGENTS.md" requirement is satisfied.
- [x] 4.5 Write ADR `docs/decisions/0047-audio-source-model.md` (next free number — re-check 0042–0046 at apply time) covering the static/stream source kinds, the unified handle, options-as-initial-values, the master gain, and the deliberate "no channels" stance, and note which ADR 0042 decisions still stand; add it to `docs/decisions/README.md` and verify the index row links the new file.

## 5. Verification gate

- [ ] 5.1 Run the SSH-server pre-filter `python3 tools/verify_remote.py all <branch>` (native ctest incl. the audio cases + Emscripten ctest incl. `web_14_audio`/web goldens + cross-runtime compare) and fix anything it finds before dispatching CI.
- [ ] 5.2 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm Linux, then Windows, then macOS green; no golden-image gate — the gate is the headless pure-C audio unit tests, the portable script harness through both runtimes, and the cross-runtime compare.
