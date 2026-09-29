# Tasks

## 1. Architecture record and docs scaffolding

- [x] 1.1 Write `docs/decisions/0039-cpu-particle-billboard-pipeline.md` per `docs/decisions/TEMPLATE.md`, recording the durable decisions (CPU simulation ownership, the shared billboard/oriented-quad basis, the depth-test/no-write policy, within-batch particle sorting, `facing` on the system vs `drawBillboard` camera-only), and add its row to `docs/decisions/README.md`; verify the index links resolve.
- [ ] 1.2 Update `AGENTS.md` (roadmap table + current-state notes) and `openspec/specs/feature-roadmap` current text with milestone F11 once the change lands; verify `npx openspec validate --strict` still passes.

## 2. Billboard render path

- [x] 2.1 Add the billboard/oriented-quad basis builder (`view`/`y`/`plane`) in `src/render/` and expose it for unit tests; verify with `ctest -R` on a new basis-math test covering camera-facing, world-up, and fixed-normal cases.
- [ ] 2.2 Add the billboard pipeline variant with `compare = LESS_EQUAL, write = false` (normal and RT-flipped winding) in `src/platform/pipeline.c`; verify the build links and a smoke frame with one billboard renders without errors.
- [x] 2.3 Add the billboard display-list record + `efx_render_billboard(...)` recording (camera snapshot, `facing`, `depthTest`, atlas rect, rotation, tint) in `src/render/render.{c,h}`; verify the record contents and validation via a unit test.
- [ ] 2.4 Extend playback in `src/platform/pipeline.c` to draw billboard records with the recorded camera and depth flag; verify with a headless display-list test and a manual capture.

## 3. Batched 2D sprites

- [ ] 3.1 Implement `drawSprites` as an atomic validate-then-record loop over `efx_quad_record` in `src/render/` + the binding layer; verify a unit test asserting entry-order, per-sprite options, and that a bad entry records nothing.

## 4. Particle simulation core

- [x] 4.1 Add the `ParticleSystem` pool + configuration struct and engine-side create/destroy/lifecycle, exposed as a native handle with deterministic `destroy()`; verify create/destroy/leak behavior in a unit test.
- [x] 4.2 Add the engine-owned deterministic RNG seeded at creation; verify two identically configured systems produce identical sequences.
- [x] 4.3 Implement the simulation step: rate + `emitterLifetime` + `emit(n)` emission, emission shapes, 3D/2D velocities, gravity/linear/radial/tangential acceleration, damping, integration, lifetime retirement, slot reuse by `insertMode`, and the `max` cap; verify against a CPU reference over a fixed `dt` sequence.
- [x] 4.4 Implement lifetime-interpolated `sizes`/`colors`, initial `rotation`/`spin`/`spinVariation`, `relativeRotation`, and flipbook `quads` selection; verify interpolation endpoints and midpoint in unit tests.
- [ ] 4.5 Wire live systems into the engine frame step (auto-update by `dt × speedScale`) with `start`/`stop`/`pause`/`reset` and `emit`; verify pause suspends aging and reset clears `count`.
- [x] 4.6 Implement atomic `set(opts)` reconfiguration with full validation and unchanged-on-failure semantics; verify with a unit test that a mixed valid/invalid bag changes nothing.

## 5. Particle rendering

- [ ] 5.1 Add the particle-batch record and `drawParticles` playback that expands live particles into billboard (`view`/`y`), oriented `plane`, or 2D screen quads; verify each mode with a focused render test.
- [ ] 5.2 Apply depth-test/no-write to particle quads and back-to-front ordering within alpha batches (additive/subtractive unsorted); verify ordering with a synthetic two-particle depth test.
- [x] 5.3 Retain a system's `Texture`/`RenderTarget` until `destroy()` and release it on destroy; verify via the texture ref-count introspection used by existing retention tests.

## 6. Bindings and type document

- [ ] 6.1 Register `drawBillboard`, `drawSprites`, `createParticleSystem`, `drawParticles`, and the `ParticleSystem` class (query `count`, `speedScale`, methods) in the desktop quickjs binding (`src/api/`); verify with headless script cases for happy path and each error type.
- [ ] 6.2 Mirror the exact names, signatures, semantics, and errors in the web binding (`src/web/`); verify the cross-runtime comparison (`tools/run_web_compare.mjs`) matches for the F11 script case.
- [ ] 6.3 Update `gallery/src/api/efx.d.ts` and its type test with `drawBillboard`, `drawSprites`, `ParticleSystem`, `createParticleSystem`, and `drawParticles`, including the `facing`/`normal` discriminated forms; verify the gallery type-check passes.

## 7. Developer reference

- [ ] 7.1 Update `docs/js-api.md` with the four F11 entries, the `ParticleSystem` class table row (query property, retention), the `facing` render modes, the 65536-particle limit, and the F11 tags; verify every cataloged symbol has a matching `efx.d.ts` declaration.

## 8. Gallery showcases

- [ ] 8.1 Add `gallery/samples/curated/particles-showcase.js` (world-space effects: `'view'`/`'y'` billboards, additive + alpha/subtractive blends, burst + continuous emission, lifetime-interpolated size/color, procedural `createImageData` textures) plus its `manifest.json` entry; verify it runs under a windowed build on the verification server (`player` against the sample, screenshot non-empty, exit 0 after a few frames).
- [ ] 8.2 Add `gallery/samples/curated/particle-plane-showcase.js` (fixed `facing: 'plane'` oriented planes demonstrating a water-like surface, camera orbit) plus its `manifest.json` entry; verify it runs under a windowed build on the verification server and that the planes stay world-oriented as the camera moves.
- [ ] 8.3 Confirm both showcases use only the public API (no browser/Node dependency, no asset pack) and that the gallery smoke drives at least one of them (name/order so it is covered), failing on any console/page error.

## 9. Verification

- [ ] 9.1 Add a portable script smoke case exercising billboards, sprite batching, and a particle system (deterministic short animation), wired into ctest and the Emscripten suite with a cross-runtime compare entry; verify it passes on the desktop and web runtimes.
- [ ] 9.2 Author a golden scene covering the billboard and particle render paths and capture its baseline on the verification server (llvmpipe, per `docs/verification-server.md`); if the golden still fails after **5 capture/debug attempts, stop and report back** rather than continuing.
- [ ] 9.3 Run `python3 tools/verify_remote.py all <branch>` on the verification server and fix anything it finds before dispatching the gate.
- [ ] 9.4 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the four-target gate is green (native suites incl. the F11 golden on Linux/Windows/macOS, Emscripten ctest + web goldens + cross-runtime compare), and that the gallery smoke passes.

