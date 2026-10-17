# Tasks

## 1. Mesh bounds: ingest and storage (C core)

- [x] 1.1 Compute min/max bounds in `efx_meshdata_create` (scan all surface `positions`), add the `bounds` override input to its `src`/create path (verbatim box), and mirror the identical computation in the glTF ingest path (`src/resource/gltf.c`); verify with a `tests/unit/resource_tests.c` case asserting the box for known positions (create + override + a glTF fixture)
- [x] 1.2 Store bounds on the mesh slot so `createMesh` carries them from the MeshData; implement the `Mesh.bounds` native query (`min`/`max`/`center`/`radius` derived on read, fresh arrays, destroyed → `TypeError`); verify via a `tests/unit/api_tests.c` case (create → read → destroy → read throws)
- [x] 1.3 Add cross-linking comments pairing the C ingest scan with the web `createMeshData` computation (material-wire-block style), verified by reading both sites in the diff

## 2. Frustum cull test (C core)

- [x] 2.1 Add the shared plane/box helpers: view basis from `efx_camera3d` (same construction as `screenRay`), 6-plane extraction (fov, active-surface aspect, near/far), 8-corner transform → world AABB, p-vertex per-plane test with early-out; verify with `tests/unit/render_tests.c` cases (box clearly inside → keep, fully behind one plane → cull, straddling → keep) including a degenerate-camera and an inverted-box case (both keep)
- [x] 2.2 Grow `efx_render_mesh` with the `cull` flag: run the test only when `cull` is set and the record's `skinned` flag is 0; on cull return `EFX_RENDER_OK` without pushing a record (no budget consumption, no error); verify with unit tests asserting record count/budget behavior for outside, straddling, skinned, and `cull: false` draws

## 3. Desktop script API (quickjs)

- [x] 3.1 `src/api/api_3d.c`: add `cull` to the `drawMesh` known-field list (boolean, default true, non-boolean `TypeError`) and pass it through; verify via `tests/unit/api_tests.c` validation cases (`cull: 1` throws; `cull: false` records)
- [x] 3.2 `createMeshData`: add the third positional `opts` bag (`{ bounds? }`) with the spec's validation (TypeError for non-object/unknown field; RangeError for shape/length/non-finite) following that function's existing validation placement, and thread the override into 1.1; verify with validation unit tests
- [x] 3.3 Expose the `bounds` getter on the Mesh JS class per the `__efxResourceClass` getters mechanism; verify with the 1.2 API test through the JS surface

## 4. Web bridge parity (Emscripten)

- [ ] 4.1 Web `createMeshData` (`src/web/js/audio.js`): compute bounds identically in the fragment and support the `opts.bounds` override with the same error classes/messages; `src/web/js/render3d.js`: accept and forward `cull`; bridge: forward both to `efx_render_mesh`; verify with the portable validation script tests (5.2) on the web runtime
- [ ] 4.2 Cross-runtime check: run the culling script test through `tools/run_web_compare.mjs` (or the established web-compare harness) and confirm desktop and web record identical draws for outside/straddling/`cull: false`/skinned scenes

## 5. Test corpus

- [x] 5.1 Portable script tests: extend the 3D validation script (`tests/scripts/s_3d_validation.js` or a new `s_*` following its pattern) to cover the new error cases and the `bounds` getter on both runtimes; verify via the ctest script-test suite
- [ ] 5.2 Golden scenes: confirm the existing golden suite is unchanged (all goldens draw visible content) by running the golden harness on the verification server; add one golden only if a scene with a partially-straddling mesh is needed to pin the conservative path
- [x] 5.3 Budget-interaction audit: find existing tests that rely on off-screen mesh draws counting against the record budget and update them to the new normative behavior; verify the budget unit tests pass

## 6. Docs and type surface

- [x] 6.1 Update `gallery/src/api/efx.d.ts`: `createMeshData(surfaces, materials?, opts?)` with the `bounds` override, `drawMesh` `cull` option (documented as *frustum* culling), `Mesh.bounds` property; regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`) and pass `npm --prefix gallery run docs:check` + `npm --prefix gallery run check` (type tests)
- [x] 6.2 Write ADR `docs/decisions/0065` (record-time engine-side fail-open frustum culling; alternatives: script-side frustum, sphere test, playback-time cull) and add the index row in `docs/decisions/README.md`

## 7. Verification gate

- [x] 7.1 Local headless: `cmake -B build -DEFX_HEADLESS=ON` + full unit ctest green
- [x] 7.2 Strict validation: `npx openspec validate "add-mesh-frustum-culling" --type change --strict` passes
- [ ] 7.3 Pre-gate: commit, push the branch, and run `python3 tools/verify_remote.py all <branch>` until green (golden-bearing jobs; see `docs/verification-server.md`)
- [ ] 7.4 Dispatch the CI gate (`gh workflow run ci.yml --ref <branch>`) and iterate Linux → Windows → macOS to green per the ADR 0023 policy
