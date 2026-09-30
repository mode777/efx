# Tasks

## 1. Asset pack

- [x] 1.1 Add the committed pack sources under `gallery/samples/curated/audio/`: `music.mp3` (a ~6 s looping arpeggio encoded once with the pinned server ffmpeg) and `font.ttf` (the CC0 Kenney font reused from `text-showcase.zip`); verify both files are present and non-empty
- [x] 1.2 Write `gallery/scripts/pack-curated-audio.py` (deterministic zip: fixed timestamps, stored entries, synthesized `blip.wav`/`chime.wav`/`thud.wav`, plus the committed `music.mp3` and `font.ttf`); verify `python3 gallery/scripts/pack-curated-audio.py` writes `gallery/samples/curated/audio-showcase.zip` and `--check` is clean on a re-run

## 2. Showcase sample

- [x] 2.1 Write `gallery/samples/curated/audio-showcase.js`: load the pack, start looping background music, draw a mixer panel + three labeled pads with a baked font, and wire mouse clicks / keys `1`-`3` to `efx.audio.playAudioEffect` (pan from x, pitch from y), `Space` pause/resume, `M` mute, `[`/`]` volume, and `efx.audio.resume()` on first input; verify it runs headless against the pack without throwing
- [ ] 2.2 Verify the sample degrades safely with no device/voice (effect starts returning `null`) and never throws; verify with a headless run

## 3. Catalog and credits

- [x] 3.1 Add the `manifest.json` entry (`curated:audio-showcase`, category `Showcase`, description, `assets: "audio-showcase.zip"`); verify `node gallery/scripts/gen-catalog.mjs` includes it and `npm --prefix gallery run check` passes
- [x] 3.2 Add the `audio-showcase.zip` row and the generation recipe to `gallery/samples/curated/CREDITS.md` (font provenance; WAV/MP3 authored in-repo); verify the pack and row agree

## 4. Tests

- [ ] 4.1 Add `smoke_showcase_audio` to `tests/CMakeLists.txt` (desktop player, `--script` + `--root` the pack); verify it passes in the native suite
- [ ] 4.2 Confirm the sample is driven by the gallery smoke (its id sorts into the run set) and that the smoke reports no console/page error; verify on the verification server

## 5. Docs

- [x] 5.1 Update the `AGENTS.md` gallery bullet to mention the audio showcase; verify it names the sample

## 6. Verification gate

- [ ] 6.1 Run the native suite on the SSH verification server (`python3 tools/verify_remote.py native <branch>`) confirming `smoke_showcase_audio`; run the gallery suite (`python3 tools/verify_remote.py gallery <branch>`) confirming the sample runs in headless Chrome; then dispatch `gh workflow run ci.yml --ref <branch>` and confirm green
