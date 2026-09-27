# Tasks

## 1. Spike and dependency setup

- [x] 1.1 Spike Emscripten async boot: from a synchronous `postRun`, start an
  async fetch→`FS.writeFile`→mount step and confirm the sokol frame loop still
  starts after the promise resolves. Verify by loading a throwaway fetched blob
  in the pinned headless Chrome harness and observing the loop run; record the
  result (and any fallback needed) in the design doc.
- [x] 1.2 Vendor miniz at a pinned revision (`vendor/miniz/`, single-file C,
  MIT) and update `vendor/README.md`. Verify the license and pin are recorded
  and the tree contains no generated binaries.
- [x] 1.3 Promote the already-vendored `stb_image` into `efx_core` with one
  implementation TU. Verify a headless build (`cmake -B build
  -DEFX_HEADLESS=ON`) compiles clean under `-Wall -Wextra -Werror` and a
  parent `efx_core` change is not required.
- [x] 1.4 Add the `src/resource/` module with its public header (opaque root,
  `read`, `size`, `exists`, `open`, `close`). Verify it is pure C with no
  quickjs/sokol includes and compiles on the headless build.

## 2. Directory provider and load API

- [x] 2.1 Implement the directory backend: join relative paths, read bytes,
  and reject `..`/absolute escapes. Verify with a headless unit test that reads
  a fixture and asserts an escape attempt returns an error.
- [x] 2.2 Implement image decoding to `rgba8` over the provider (PNG alpha
  preserved, JPEG forced opaque, corrupt input errors). Verify with a headless
  unit test over committed PNG/JPEG fixtures, including a corrupt-file case.
- [x] 2.3 Expose `loadText` and `loadImage` through the quickjs binding
  (`src/api/`), returning a string and a native-backed `ImageData`
  respectively. Verify a ctest `--script` test that loads text and an image
  next to the script, and asserts missing/malformed paths throw.
- [x] 2.4 Add the pure-JS `loadTexture(path)` to the engine prelude and
  `efx` object. Verify a script test asserts a live Texture with the decoded
  dimensions and `destroy()` lifecycle.
- [x] 2.5 Thread the desktop resource root from `player.c` into runtime/api
  state and read `main.js` through the provider. Verify existing smoke tests
  (`smoke_root_ok`, `smoke_root_no_entry`) still pass.

## 3. Zip backend

- [x] 3.1 Implement the zip backend using miniz behind the same provider
  interface. Verify with a headless unit test over a committed zip fixture
  (entry read, missing entry error, corrupt archive error).
- [x] 3.2 Accept a zip archive as the player resource root and read `main.js`
  from it. Verify a new smoke test launches with a zip fixture and exits 0 with
  the expected log output.
- [x] 3.3 Resolve the `--script` resource root to the script file's directory,
  with an optional `--root <directory|archive>` override. Verify one script
  test loads an adjacent resource with no flags and another loads from an
  explicit zip via `--root`.

## 4. Web binding and async boot

- [x] 4.1 Mirror `loadText`/`loadImage` through the web bridge and expose them
  from `entry.js`. Verify the web compare harness runs the new loader script
  with output matching the desktop run.
- [x] 4.2 Make `entry.js` boot async: consume `globalThis.__efx_assets` (or
  `?assets=`), fetch the single zip, write it into MEMFS, set the provider
  root, then read/evaluate `main.js`, starting the frame loop only after the
  mount completes (no script-visible loading hook). Verify a browser harness
  case that preloads a fixture zip, serves it, and asserts the entry script
  loads a resource from it.
- [x] 4.3 Surface fetch/mount failures through the console error channel and
  the non-zero exit-code contract before evaluating `main.js`. Verify a harness
  case with an unreachable asset URL reports the error and the failure code.
- [x] 4.4 Consume and delete the asset channel before evaluation. Verify a test
  in the style of `tools/test_web_override.mjs` asserts the channel is gone and
  the script cannot observe it.
- [x] 4.5 Keep the Node/no-URL path unchanged. Verify `run_web_compare.mjs`,
  `test_web_override.mjs`, and the web golden driver still pass with no fetch.

## 5. Docs and ADR

- [x] 5.1 Write `docs/decisions/0031-*` for the provider + fetch-once-at-boot
  decision (per `TEMPLATE.md`) and add it to `docs/decisions/README.md`.
- [x] 5.2 Amend ADR 0016 (pre-boot mount) and ADR 0030 (host asset channel).
- [x] 5.3 Update `docs/js-api.md` (move F6a entries to current behavior with
  layer tags, errors, and lifecycle) and `gallery/src/api/efx.d.ts` in the same
  change.
- [x] 5.4 Update the AGENTS.md current-state and roadmap rows to note the F6a
  slice.

## 6. Verification

- [x] 6.1 Add a golden scene whose `main.js` loads and draws a decoded PNG;
  capture the golden server-side (llvmpipe, per
  `docs/verification-server.md`) and commit it. Verify `ctest` golden suite
  passes locally with a display and in the Emscripten golden job.
- [x] 6.2 Run the Linux pipeline first (ctest smoke + goldens) and fix
  findings; only then Windows, then macOS. Verify via
  `python3 tools/verify_remote.py all <branch>` before dispatching
  `gh workflow run ci.yml --ref <branch>`.
- [ ] 6.3 Confirm the four-target gate is green (native suites incl. all
  goldens on Linux/Windows/macOS, Emscripten ctest + web goldens) before
  archiving.
