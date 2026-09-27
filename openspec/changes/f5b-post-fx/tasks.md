# Tasks

## 1. Prerequisite

- [x] 1.1 Confirm `f5a-render-targets` has passed its four-target gate (roadmap ladder rule) before starting f5b implementation; verify via the archived change and the green CI run reference in AGENTS.md's current state

## 2. Effect registry and canned shaders

- [x] 2.1 Author the post-pass GLSL (single-source per ADR 0021): color-ops pass, blur tap passes, bright-pass, upsample/composite pass — uniform-driven, no permutations (ADR 0026/0027 discipline); verify the pinned sokol-shdc compile step regenerates and the engine builds
- [x] 2.2 Implement the native effect registry (name → option validation, pass plan from options, uniform block) with the v1 set — `colorFilter`, `blur` (separable below the internal threshold, downsample-chain above), `bloom` (threshold + chain + additive up) — and per-entry `mix` as a uniform lerp; verify a headless unit test drives each descriptor's validation matrix (unknown fields → `TypeError`, bounds → `RangeError`)

## 3. Resolve pipeline and render scale

- [x] 3.1 Implement the implicit engine-owned scene target + two ping-pong temporaries over the f5a RT machinery (never script-visible: no handles, no class entries), engaged only when a chain is set or scale ≠ 1; verify a unit test asserts the fast path takes the direct branch (no scene-target allocation) when nothing is set
- [x] 3.2 Implement chain execution at the default target's resolve: entries in array order, engine-owned temporaries, final blit; user RT segments render raw; verify a unit test samples a user RT through a chained frame and asserts its contents are unfiltered
- [x] 3.3 Implement `efx.setRenderScale(scale, { filter })` — scene size `ceil(surface × scale)`, nearest/linear blit filter, validation per the spec; verify a unit test covers the validation matrix and a half-res nearest golden shows crisp 2×2 blocks

## 4. Script API bindings

- [x] 4.1 Bind `efx.setPostEffects(list | null)` in `src/api` and the `src/web` bridge with eager atomic validation (≤ 8 entries, snapshot at call, throw leaves the previous chain); verify headless unit tests for atomicity, snapshot semantics, null-clear, and persistence across frames
- [x] 4.2 Keep bridge parity: verify `tools/run_web_compare.mjs` diffs desktop vs web at zero for a chained scene and the Emscripten ctest smoke suite runs the same portable scripts green

## 5. Golden scenes and unit tests

- [x] 5.1 Author the F5b golden scenes (colorFilter on a known scene, blur radius small and large, bloom, a `mix: 0.5` blend, render-scale nearest and linear, chain-order determinism pair) and capture baselines server-side via the llvmpipe recipe in `docs/verification-server.md`; verify the scenes appear under `tests/goldens/` and `examples/browser/main.js` cycles them
- [x] 5.2 Add the headless unit tests for the `post-fx` capability (validation matrix, defaults-are-neutral, mix-zero identity, chain persistence, render-scale validation) to the ctest suite; verify they run green in an `EFX_HEADLESS=ON` local build
- [x] 5.3 Verify the fast path: run the full committed golden suite with no chain set and confirm every pre-F5b golden is byte-identical (no re-baselining)

## 6. Docs and ADR

- [x] 6.1 Write ADR `docs/decisions/0029` (post-chain architecture: declarative chain over imperative apply and per-effect globals, mix over blend modes, implicit scene target + fast path over always-on resolve, engine-owned registry with pass structure invisible; the rejected alternatives with reasons) and add it to `docs/decisions/README.md`; verify the index row links the new file
- [x] 6.2 Complete the `docs/js-api.md` F5 section: deliver `setPostEffects` (with the v1 effect table, defaults, bounds, `mix`) and `setRenderScale`, retire the provisional `setColorFilter`/`setBlur` entries, add the 8-entry chain limit to the fixed-limits section, and classify chain entries as JS-managed; verify every F5b signature matches the implementation
- [ ] 6.3 Update AGENTS.md's current-state section with F5b/F5 completion at archive time; verify the roadmap table and narrative stay consistent

## 7. Verification gate

- [ ] 7.1 Run the four-target gate in order: `python3 tools/verify_remote.py all <branch>` on the SSH verification server (native ctest incl. goldens, Emscripten golden suite), then dispatch `gh workflow run ci.yml --ref <branch>` and confirm the Linux, Windows, and macOS pipelines green in that order per AGENTS.md
