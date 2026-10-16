# Tasks

## 1. Vendor + build

- [ ] 1.1 Vendor pl_mpeg into `vendor/pl_mpeg/` (single header + LICENSE,
      pinned version recorded), add the `vendor/README.md` pin row.
      Verify: row matches table style; pinned files present.
- [ ] 1.2 Wire `src/video/` into CMake on all four toolchains (impl TU with
      relaxed warnings, `efx_video.h` declarations-only elsewhere), plus the
      new `src/web/js/video.js` fragment in the `CMakeLists.txt` concat
      order. Verify: configure + build desktop headless and Emscripten.

## 2. Video core

- [x] 2.1 Generate and commit the procedural MPEG-1 test fixture (authoring
      time, external encoder; record the recipe in
      `docs/verification-server.md` or the fixture directory).
      Verify: fixture decodes with the vendored decoder in a scratch test.
- [x] 2.2 Implement `efx_video` open/advance/getters over the resource
      bytes: RGBA frame output, stereo float32 MP2 PCM, engine-clock
      advance with bounded decode-ahead (design D3), loop/ended/pause
      state. Verify: unit suite — fixture decode determinism (two decodes
      byte-identical), time advance/wrap/hold, ended restart, non-MPEG-1
      rejection, no-audio-track path.

## 3. Texture + audio integration

- [x] 3.1 Create the video's dynamic Texture at open through the core
      dynamic-texture path and upload each decoded frame via
      `efx_render_texture_update`; destroy with the video. Verify: render
      unit test with mock sink counts one update per decoded frame;
      destroy releases texture. **Requires `dynamic-textures` landed.**
- [x] 3.2 Add the video-attached streaming voice to the audio core
      (PCM source = video ring, existing cap/steal policy, `muted` gain,
      pause/loop/destroy coupling, `efx_audio_stop_all` sweep).
      Verify: audio unit tests — attachment consumes a stream slot,
      contention fallback silent, muted gain, swap releases.

## 4. Script API

- [x] 4.1 Prelude: `openVideo` option-bag validation (`loop`, `muted`,
      unknown-field/TypeError rules, ADR 0049) + `tools/gen_prelude.py`
      regen committed. Verify: `gen_prelude.py --check` clean.
- [x] 4.2 Desktop `src/api/api_video.c`: `efx.video` namespace, Video class
      (GC finalizer + `destroy()`, ADR 0011/0012), `texture` property,
      playback members with native validation. Verify: headless `--script`
      smoke test opens/plays/pauses/destroys; error cases throw `TypeError`.
- [ ] 4.3 Web: `src/web/bridge_video.c` + `src/web/js/video.js` (class +
      namespace fragment), bridge registration. Verify: web build; browser
      harness smoke test.
- [ ] 4.4 Types + docs: `gallery/src/api/efx.d.ts`, type-test coverage,
      regenerate `docs/api/`; `npm --prefix gallery run check` and
      `docs:check` green.

## 5. Tests — behavior

- [x] 5.1 Error-catalog cases: non-MPEG-1 open, bad options, use-after-
      destroy members, destroy of the video-owned texture. Verify: headless
      suite passes.
- [ ] 5.2 Golden scene `tests/goldens/video_frame/`: open fixture, play,
      pin a deterministic frame, draw full-frame. Verify: Linux llvmpipe
      job first, then `tools/verify_remote.py all <branch>` before the gate
      (ADR 0020).
- [ ] 5.3 Record Emscripten player wasm size before/after in this change.
      Verify: growth within the estimated ~0.1 MB band; investigate if not.

## 6. Showcase + ADR

- [ ] 6.1 Gallery sample `gallery/samples/curated/video-showcase/` (ADR 0044
      layout): BBB MPEG-1 excerpt (small, with audio), attribution in sample
      README + page header; regenerate the catalog
      (`gallery/scripts/gen-catalog.mjs`). Verify: gallery build + catalog
      updated.
- [ ] 6.2 Write `docs/decisions/0063-*.md` (MPEG-1/pl_mpeg choice, implicit
      audio, engine-clock master) and index it in
      `docs/decisions/README.md`.
- [ ] 6.3 Full local gate: unit tests + ctest (golden excluded locally),
      gallery build; then remote verification and four-target gate per
      AGENTS.md lifecycle. Verify: all green.
