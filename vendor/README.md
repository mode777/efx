# Vendored dependencies

All third-party dependencies are vendored in-repo at pinned versions; the
build never touches the network (see `build-system` spec). Update pins by
replacing the snapshot and editing the table below.

| Path | Project | Pinned version | Source |
|---|---|---|---|
| `sokol/` | floooh/sokol | master @ `2e75443dbd4940b5aa8d76a8e479f8e4b270b9a3` | https://github.com/floooh/sokol (only `sokol_app.h`, `sokol_gfx.h`, `sokol_glue.h`, `sokol_audio.h`; Zlib) |
| `quickjs-ng/` | quickjs-ng/quickjs | v0.17.0 (QJS 0.17.0) | https://github.com/quickjs-ng/quickjs, release tarball `v0.17.0.tar.gz` |
| `stb/` | nothings/stb | master @ `2c980bb59875b0d32144a71867fbdebb2f77cd20` (`stb_image` v2.30, `stb_image_write` v1.16) | https://github.com/nothings/stb (only `stb_image.h`, `stb_image_write.h`) |
| `stb/` | nothings/stb | master @ `2c980bb59875b0d32144a71867fbdebb2f77cd20` (`stb_truetype` v1.26, `stb_rect_pack` v1.01) | https://github.com/nothings/stb (only `stb_truetype.h`, `stb_rect_pack.h`) |
| `miniz/` | richgel999/miniz | 3.1.2 @ `77d0dce8627735138c51770d1799a1ef48f2117d` | https://github.com/richgel999/miniz (`miniz-3.1.2.zip` release amalgamation: `miniz.c`, `miniz.h`, `LICENSE`) |
| `glm/` | g-truc/glm | 1.0.3 @ `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed` | https://github.com/g-truc/glm (core headers + `detail/` + `simd/` + `ext/` + `gtc/`; excludes `gtx/`, the C++20 module `glm.cppm`, `CMakeLists.txt`, umbrella `ext.hpp`) |
| `cgltf/` | jkuhlmann/cgltf | v1.15 @ `360db1a95480fe102ae9c69b27c5d101167ff5ba` | https://github.com/jkuhlmann/cgltf (`cgltf.h` single header; MIT, in-header notice) |
| `minigamepad/` | ColleagueRiley/minigamepad | main @ `a7f8fde128a3732053dd17ce9ced440ed9926125` (2026-06-13) | https://github.com/ColleagueRiley/minigamepad (`minigamepad.h` + `LICENSE`; Zlib, in-header notice mislabels it "libpng license") |
| `dr_libs/` | mackron/dr_libs | master @ `dfe8377631000664666519fdb83da193fd8037f4` (2026-08-31) | https://github.com/mackron/dr_libs (`dr_wav.h`, `dr_mp3.h` + `LICENSE`; choice of public domain (Unlicense) **or** MIT-0) |
| — (tool, not vendored) | floooh/sokol-tools-bin | master @ `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` (2026-08-29) | https://github.com/floooh/sokol-tools-bin (`bin/linux/sokol-shdc`) — generation-time tool for `shaders/quad.h`; never linked into the player |

Notes:

- sokol is a rolling project without release tags; the pin is a master commit
  SHA. Re-pin by downloading the new commit's `sokol_app.h` / `sokol_gfx.h`.
- Only the sokol headers the engine needs are vendored (`sokol_app.h` for the
  window / frame loop, `sokol_gfx.h` for rendering, `sokol_audio.h` for the
  F14 audio device). Add further sokol headers from the same pinned commit when
  a milestone needs them. `sokol_audio.h` is compiled into the platform layer
  only (`src/platform/audio_backend.c`), never into the pure-C core.
- quickjs-ng is consumed via its own CMake target (built as a static library,
  tests/examples/CLI/install disabled). The engine does not compile
  quickjs-libc into the runtime — scripts get only the engine's `efx` API plus
  the ES6 standard library, keeping them free of host (browser/Node) APIs.
- GLM (F3, ADR 0005): header-only C++; consumed only from C++-compiled
  translation units (`src/math/`) that expose a plain C API — GLM types never
  enter C11 translation units. The wrapper calls the convention-explicit
  `glm::perspectiveRH_NO` / `glm::orthoRH_NO` / `glm::lookAtRH` (right-handed,
  OpenGL depth range −1..+1); the `GLM_FORCE_*` defines are not used. The
  projection conventions for D3D11/Metal (0..1 depth) are handled at playback
  by the platform layer, not by flipping GLM conventions. Evaluation record:
  `openspec/changes/f3-3d-core/proposal.md`.
- sokol-shdc (ADR 0021) compiles `shaders/quad.glsl` into the committed
  `shaders/quad.h`. The tool is used at author time only; the player
  build never needs it (offline policy intact). Regeneration: download
  the pinned sokol-tools-bin revision, run
  `sokol-shdc -i shaders/quad.glsl -o shaders/quad.h --slang glsl410:glsl300es:hlsl4:metal_macos -f sokol_impl`,
  and commit the diff. Source-snapshot vendoring was evaluated and
  rejected: sokol-tools has no CMake build and pulls an 8-submodule
  dependency graph (glslang, SPIRV-Tools/Cross, tint, ...) for a
  generation-time-only tool.
- stb is vendored for golden-image PNG I/O (F2 verification harness):
  `stb_image_write` encodes captured frames, `stb_image` decodes committed
  goldens for comparison. Both are single-header public-domain/MIT; the
  implementation TUs live in the tools that need them (`tests/imgdiff.c`,
  `src/platform/capture.c`). Evaluation record:
  `openspec/changes/f2-2d-layer/proposal.md`.
- stb_truetype/stb_rect_pack are vendored for the F8a font/text module
  (`stb_truetype` parses fonts, extracts metrics/kerning and rasterizes
  glyphs; `stb_rect_pack` packs the fixed atlas). Both are single-header
  public-domain/MIT, same family as the already-vendored stb_image; their
  implementation TUs live in `src/render/stb_truetype_impl.c` and
  `src/render/stb_rect_pack_impl.c`. Evaluation record:
  `openspec/changes/f8a-font-typesetting/`.
- miniz is vendored for the F6a resource provider's zip backend
  (`mz_zip_reader`): the release is a single-file amalgamation (`miniz.c` +
  `miniz.h`, no external zlib), MIT. It is linked into `efx_core` on all four
  targets. Evaluation record: `openspec/changes/f6a-resource-loading/`.
- cgltf is vendored for the F6b glTF 2.0 importer: a single-header C99 parser
  (carries its own jsmn JSON parser, no external deps), MIT. The implementation
  is compiled once in `src/resource/cgltf_impl.c` with warnings relaxed (same
  treatment as the vendored stb/miniz TUs); the engine includes only the
  declarations. Evaluation record: `openspec/changes/f6b-gltf-import/`.
- minigamepad is vendored for the F13 gamepad poll backend (design D1): a
  single-header C89 library with per-platform backends (evdev/inotify on
  Linux, XInput/DirectInput on Windows, IOKit on macOS, the Emscripten
  gamepad API on web), GLFW-compatible SDL GUID generation, and an SDL
  game-controller mapping database. Zlib license. Chosen over libstem_gamepad
  (no web backend, no SDL GUID/layout), GLFW's joystick layer (couples to the
  GLFW platform struct), SDL2/3 (a second windowing/input system), and a
  bespoke four-backend shim. Evaluation record:
  `openspec/changes/gamepad-input/proposal.md`.
  - The header is included exactly once, from
    `src/platform/gamepad_backend.c`, with `MG_IMPLEMENTATION`,
    `MG_MAX_GAMEPADS 4`, and `MG_API` left empty so its symbols stay local to
    that translation unit. It is compiled into `efx_platform` only — never
    into the pure-C core (`src/input/`) or the headless test targets.
  - **Local divergence (documented, permitted by design D7).** The pinned
    snapshot carries two web-path defects that affect the normalized surface
    we consume; they are patched in place and marked `LOCAL PATCH (F13)` in
    the header: (1) the web axis map duplicated the left trigger and advanced
    `j += 2`, so axis 5 (right trigger) was never sampled — the loop now maps
    every axis and axis 5 to `MG_AXIS_RIGHT_TRIGGER`; (2) `mg_gamepads_init_platform`
    registered connect/disconnect callbacks but never enumerated pads already
    connected at load — it now synthesizes the connect callback for each live
    pad. It also records `gamepad->src.index` on the web connect path so the
    per-frame update samples the right browser pad. (3) On macOS the IOKit
    backend never stored the HID device handle on the gamepad, so its input
    callback dropped every event (connect worked, input did not); it also
    compared mapping lookups against `0` instead of `MG_*_UNKNOWN` (-1),
    indexing `buttons[-1]`/`axes[-1]`. The patch records the device, resolves
    buttons/axes through the platform tables only (Microsoft pads use the
    Xbox HID button order), decodes the hat switch into the D-pad and
    `SystemMainMenu` into guide, and rests triggers at -1. (4) The SDL
    mapping parser matches no fields (its field lengths include the NUL), so
    every mapping is empty and the index-0 element spuriously matched
    button/axis 0; unparsed elements are now skipped, and the platform tables
    are the sole resolver. Linux seeds axis values from the device's current
    state (resting triggers) and clears `axes` instead of clearing `buttons`
    twice; the Windows DirectInput path (XInput pads are unaffected) gets an
    Xbox-style button/axis fallback table, which was previously empty. Re-pin
    by replacing the header and re-applying the `LOCAL PATCH (F13)` hunks (or
    upstreaming them).
  - minigamepad's own SDL-mapping evaluator is **not** the engine's
    normalization boundary: the backend re-encodes its platform-mapped
    semantic state into the engine's canonical standard descriptor and the
    pure-C evaluator in `src/input/efx_gamepad.c` owns the semantic surface
    (ADR 0041).
- dr_libs is vendored for the F14 audio decoders: `dr_wav` (WAV integer PCM /
  float) and `dr_mp3` (MP3). Both are single-header C99 with memory **and**
  incremental/streaming decode and no external dependencies, dual-licensed
  public domain (Unlicense) or MIT-0. Chosen over minimp3 (decode-only, still
  needs a WAV library), stb_vorbis (Ogg, not the requested formats),
  libmpg123 (LGPL — static single-binary + relink obligations), and miniaudio
  (bundles its own device layer alongside Sokol). The two implementation TUs
  live in `src/audio/dr_impl.c`, compiled with warnings relaxed exactly like
  the miniz/cgltf/stb TUs; the engine includes only the declarations.
  Evaluation record: `openspec/changes/f14-audio/design.md` (D1).
