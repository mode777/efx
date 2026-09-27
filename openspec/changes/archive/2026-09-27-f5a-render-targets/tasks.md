# Tasks

## 1. Render-target resource core

- [x] 1.1 Implement the RenderTarget native-backed class in `src/render` (create/destroy, idempotent destroy, GC-finalizer backstop, teardown finalization, GPU size into GC-pressure accounting) with 4096-per-side validation; verify a headless unit test creates, queries `width`/`height`, destroys, and observes `TypeError` on post-destroy use
- [x] 1.2 Extend deferred release to RenderTarget for display-list records and bound material maps (the Texture retention set); verify a unit test that destroys a target mid-frame while a record and a bound map still reference it, pinning the exact release points

## 2. Display-list segmentation and redirection

- [x] 2.1 Add the target tag to display-list records plus `beginRenderTarget`/`endRenderTarget` control records; constrain the stable-sort reordering to within a segment and play segments in first-record order; verify a headless unit test drives begin/draw/end interleavings and asserts segment ordering and painter's order within segments
- [x] 2.2 Implement render redirection in playback: entering a segment binds the target's attachments (env-default color + depth formats per ADR 0025), clears to the value-snapshotted clear color, and the existing clip-depth fold applies unchanged; verify a golden scene renders a known 2D scene into a target and samples it full-screen (upright, clear color visible)
- [x] 2.3 Make the default 2D camera frame and the 3D projection aspect derive from the active rendering surface's extent; verify a golden scene renders 3D + lighting into a non-window-extent target and a unit test checks the default-frame value switches with the active target
- [x] 2.4 Implement the feedback-loop guard (a draw sampling the currently active target throws `TypeError` at record time); verify a unit test records such a draw inside an active segment and asserts the throw and empty record list

## 3. Script API bindings and coercion

- [x] 3.1 Extend the texture-argument validation in `drawQuad` (desktop `src/api` and `src/web` bridge) to accept a live RenderTarget with identical error behavior, evaluating `sourceRect`/size derivation against the target's extent; verify unit tests covering valid draws, destroyed targets, out-of-bounds sourceRect, and wrong types
- [x] 3.2 Extend material `map` channels and `alphaMask` (F4b bindings) to accept a live RenderTarget with retention; verify a unit test binds and unbinds a target map and asserts retention/release, and a golden scene shades a mesh with a RenderTarget as `diffuse.map`
- [x] 3.3 Keep bridge parity: verify `tools/run_web_compare.mjs` diffs desktop vs web output at zero for an RT scene, and the Emscripten ctest smoke suite passes the same portable scripts

## 4. Golden scenes and unit tests

- [x] 4.1 Author the F5a golden scenes (2D sampled from a target, 3D + lighting into a target, default camera frame into a target, RT as material map) and capture baselines server-side via the llvmpipe recipe in `docs/verification-server.md`; verify the scenes appear under `tests/goldens/` and `examples/browser/main.js` cycles them
- [x] 4.2 Add the headless unit tests for the `render-targets` capability (validation matrix, segmentation ordering, clear-per-begin, nesting/unbalanced throws, deferred destroy) to the ctest suite; verify they run green in an `EFX_HEADLESS=ON` local build
- [x] 4.3 Verify the committed F2/F3/F4 goldens are byte-identical after the change (no-RT fast path untouched) by running the full golden suite

## 5. Docs and ADR

- [x] 5.1 Write ADR `docs/decisions/0028` (render targets sampled directly: coercion at the binding layer, segmentation, deferred release; rejected `rt.texture` alias and `drawRenderTarget` with reasons) and add it to `docs/decisions/README.md`; verify the index row links the new file
- [x] 5.2 Rewrite the `docs/js-api.md` F5 section for the delivered F5a half: `createRenderTarget`/`beginRenderTarget`/`endRenderTarget`, RenderTarget class entry with `width`/`height` query properties, the texture-coercion rule, the provisional `drawRenderTarget` entry removed, and the F5b half still marked provisional; verify every F5a signature matches the implementation
- [x] 5.3 Update AGENTS.md's current-state section with F5a status at archive time; verify the roadmap table and narrative stay consistent

## 6. Verification gate

- [x] 6.1 Run the four-target gate in order: `python3 tools/verify_remote.py all <branch>` on the SSH verification server (native ctest incl. goldens, Emscripten golden suite), then dispatch `gh workflow run ci.yml --ref <branch>` and confirm the Linux, Windows, and macOS pipelines green in that order per AGENTS.md
