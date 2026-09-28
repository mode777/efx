# Tasks

## 1. Core skinning math

- [x] 1.1 Add a skinning module (e.g. `src/render/skin.c` + header) exposing the rig → joint bind-local derivation (`world_bind = inverse(inverse_bind)`, `local_bind` from `joint_parents`, identity fallback for singular matrices) and verify with a unit test on a hand-built two-joint rig comparing derived transform values.
- [x] 1.2 Implement clip sampling for LINEAR and STEP channels with `time` wrapped modulo clip length, and verify with a unit test at keyframes, between keyframes, past the clip end, and for STEP hold.
- [x] 1.3 Implement FK in joint space (node→joint resolution from `joint_nodes`, hierarchy walk, ignored non-joint-ancestor channels) and the skin-matrix palette `M[j] = world[j] * inverse_bind[j]`; verify against an independently computed CPU reference on the `skin.gltf` fixture.
- [x] 1.4 Implement CPU linear-blend skinning of positions and normals with per-vertex weight normalization (zero-sum vertices stay at bind, normals renormalized) and verify posed vertices match the CPU reference within epsilon.
- [x] 1.5 Implement the stateless `pose` evaluation for a single sample and a weighted sample array (normalize blend weights; negative weight errors) and verify blend output equals the reference for two clips.

## 2. Renderer integration

- [x] 2.1 Extend the mesh/render layer with a per-surface posed CPU array and a second GPU vertex buffer for skinned meshes only; verify static meshes allocate no second buffer and existing render unit tests still pass.
- [x] 2.2 Add an engine entry point that applies a pose to a live `Mesh` in place (writing the posed arrays, leaving bind-pose data untouched) and verify a pose-then-repose sequence changes only the posed data.
- [x] 2.3 Thread a `skinned` flag into the mesh display-list record and the playback path: `true` updates/draws the posed buffer, absent/`false` draws bind pose, and `skinned: true` on a rig-less mesh errors; verify with a render unit test and an unposed-vs-posed golden comparison.
- [x] 2.4 Verify skinned-mesh destruction releases both buffers and later use throws, adding a case to the render/mesh lifecycle unit tests.

## 3. Bindings

- [x] 3.1 Add desktop quickjs `efx.poseMesh(mesh, pose)` in `src/api/api.c` with the D5 error taxonomy (TypeError for mesh/type/unknown field, RangeError for index/negative weight, Error for unknown clip name) and verify with API unit tests for valid, blended, wrapped, and each error case.
- [x] 3.2 Add `"skinned"` to `efx_js_drawMesh`'s known option fields and value-snapshot it; verify signature/validation unit tests.
- [x] 3.3 Add the web bridge pose entry and thread `skinned` through `src/web/entry.js` `drawMesh`, keeping `createMeshData` meshes rig-less; verify with the web unit/compare harness that a script-built mesh rejects `skinned: true` and `poseMesh` while an imported rig poses.

## 4. Verification harness

- [x] 4.1 Add a committed golden scene under `tests/goldens/` that loads a skinned fixture, poses it deterministically, and draws `skinned: true`; capture the golden on the verification server (llvmpipe) per `docs/verification-server.md` and confirm the pixel diff passes.
- [x] 4.2 Add a script smoke test exercising `poseMesh` + `skinned` (and the rig-less rejection) through the `--script` run mode, and register it with ctest.
- [x] 4.3 Run the native unit suites (including the CPU-reference skinning tests) and the Emscripten ctest/web-compare suites locally, fixing anything that fails before remote verification.

## 5. Gallery sample and docs

- [x] 5.1 Fetch a fully-CC0 rigged model (Quaternius `Fox` from *Ultimate Animated Animals*), optimize it to the importer's supported feature set (base-color factors, PBR→Phong factors), and build a deterministic root-level `fox-walk.zip`; add the `gallery/samples/curated/` manifest entry with `assets`, a `CREDITS.md` provenance row + recipe, and a `fox-walk.js` sample that poses `Walk` from a script clock and draws `skinned: true`.
- [x] 5.2 Add an analogous `smoke_showcase_fox` player test to `tests/CMakeLists.txt` and verify the sample runs clean under the gallery smoke harness.
- [x] 5.3 Update `docs/js-api.md`: turn the F7 entry from provisional to current (`poseMesh`, `drawMesh({ skinned })`), document signatures, errors, time-wrap/weight rules, and that no playback helper exists; update the resource/limits notes if needed.
- [x] 5.4 Update `gallery/src/api/efx.d.ts` and `gallery/src/api/efx.type-test.ts` for `poseMesh` and the `skinned` draw option, and verify `npm --prefix gallery run check` passes.
- [x] 5.5 Write ADR `docs/decisions/0035-cpu-skinning-pipeline.md` (bind-local derivation, FK/palette, weight normalization, non-joint-ancestor limitation, dual-buffer semantics) and add it to `docs/decisions/README.md`.
- [x] 5.6 Update `AGENTS.md` (roadmap/current-state rows for F7) and confirm `openspec validate --strict` passes for the change.

## 6. Gate

- [x] 6.1 Run `python3 tools/verify_remote.py all <branch>` (native goldens + web suites + gallery) and fix any failures.
- [x] 6.2 Dispatch the four-target gate with `gh workflow run ci.yml --ref <branch>` following Linux → Windows → macOS, and confirm the run is green.
- [x] 6.3 Merge the branch to `main`, push, and archive the change (syncing specs).
