# Tasks

Implements F4a (Phong lighting, no maps). Decisions D1–D12 refer to
`design.md`; behavior requirements live in the delta specs
(`specs/lighting/`, `specs/3d-core/`, `specs/js-api/`).

## 1. Light bank and bindings (D4)

- [x] 1.1 Add the fixed light bank to the render core: four point slots + one directional slot, all disabled at startup, with the point `{pos, color, range}` and directional `{dir, color}` values; verify with a C unit test that defaults are disabled and setting a slot replaces only that slot
- [x] 1.2 Implement `setLight(slot, opts)` in the shared binding layer: integer slot `0..3` (`RangeError`), bag `{pos, color, range?}` with documented defaults and `null` disable, type/unknown-field errors (`TypeError`), range `>= 0` finite (`RangeError`); verify with `--script` smoke tests per throw case and per valid case (exit 0)
- [x] 1.3 Implement `setDirectionalLight(opts)`: `{dir, color}` with nonzero `dir`, `null` disable, malformed bag (`TypeError`); verify with `--script` smoke tests (valid, disabled-by-default, malformed)
- [x] 1.4 Snapshot the full light set into each mesh record at record time (ADR 0019) and extend the headless display-list test to assert the recorded light values are unaffected by later `setLight` calls; verify via the ctest display-list suite

## 2. Material model and per-surface binding (D5, D6)

- [x] 2.1 Implement the material snapshot model in the shared core: channels `ambient`/`diffuse`/`specular`/`emissive` with documented defaults (ambient/emissive/specular black, diffuse white, shininess 32), channel-alpha ignored, map/`alphaMask` rejected as unknown; verify with a C unit test on defaults, channel overrides, and snapshots not tracking later mutation
- [x] 2.2 Implement per-surface material storage on MeshData and Mesh, and accept the parallel `materials` array on both `createMeshData` forms (entry count must equal surface count → `RangeError`; invalid entry → `TypeError`), carrying bindings over at `createMesh`; verify with `--script` smoke tests for valid/null/mismatched arrays
- [x] 2.3 Implement `setMeshSurfaceMaterial(mesh, surfaceIndex, mat)`: live-Mesh check (`TypeError`), integer in-range index (`RangeError`), object-or-`null` mat (`TypeError`), snapshot rebind of one surface; verify with `--script` smoke tests (rebind, `null` reset, out-of-range, destroyed mesh)
- [ ] 2.4 Register the F4a material/light entries with identical semantics on both bindings (`src/api/api.c`, `src/web/bridge.c`, ADR 0022); verify `tools/run_web_compare.mjs` reports desktop vs web agreement for the F4a smoke scripts

## 3. Lit shader and playback (D1, D2, D3, D7)

- [x] 3.1 Extend `shaders/mesh.glsl` from the unlit fill into the Phong shader (world-space ambient/diffuse/specular/emissive, Blinn-Phong, light/material uniforms) and regenerate `shaders/mesh.h` via the pinned sokol-shdc flow (ADR 0021); verify the generated header compiles on the GL, D3D11, Metal, and GLES3 backend paths and that the uniform blocks stay under the size limit
- [ ] 3.2 Wire playback per D3: pass `mvp` (as F3), `model`, and the CPU-computed normal matrix; pass world position/normal to the fragment stage, plus camera position, the recorded light snapshot, and the surface's material snapshot; verify manually on the host with a scratch scene (lit cube + sphere, one point light, one directional light) before any golden exists
- [ ] 3.3 Apply D7 exactly (including point attenuation at `range`, `L = normalize(-dir)` for the directional light, emissive unmodulated, output clamped to `[0,1]`, interpolated normal normalized) and confirm the seven committed F2 2D goldens are byte-identical on the host; verify via host ctest

## 4. CPU lighting reference (D8)

- [x] 4.1 Implement the pure-C CPU reference for the D7 equation in `src/render/` (no sokol, no quickjs) and a headless unit test over analytic cases: ambient-only, diffuse maximum head-on, zero diffuse facing away, specular peak, attenuation at exactly `range`, directional-only, emissive-only; verify it passes in ctest on the host
- [ ] 4.2 Add a golden scene that renders a known light/material configuration and assert the captured pixels match the CPU reference within the golden tolerance; verify the comparison passes on the host

## 5. Golden scenes (D9)

- [ ] 5.1 Author the F4a scene resource roots (ambient+diffuse, directional, specular, point attenuation, emissive, multi-material/default-material) at the standard 640×480 using only the current API; verify each runs in the host player without error
- [ ] 5.2 Update the six F3 mesh scene setups to include explicit lights/materials and re-baseline their goldens; verify the seven F2 goldens are unchanged and every 3D golden renders a meaningful lit image
- [ ] 5.3 Capture and commit the goldens via the documented regeneration invocation, run the manual server-side capture step on the SSH verification server (llvmpipe-only quirk, `docs/verification-server.md`), and verify `ctest` golden tests pass on the host

## 6. Docs and ADR

- [x] 6.1 Write ADR `docs/decisions/0026-lighting-and-canned-shader-strategy.md` per TEMPLATE.md (single uniform-driven lit shader, no F4a permutations; world-space Phong equation and light semantics; default material) and add it to the `docs/decisions/README.md` index; verify the file exists and is indexed
- [x] 6.2 Move the `docs/js-api.md` F4a entries (`setLight`, `setDirectionalLight`, `setMeshSurfaceMaterial`, `materials` array, default material and light semantics) from provisional to current, keep the F4b map/alpha-mask entries provisional as an extension of the same material object, and update the limits/classification wording; verify every F4a signature matches the implementation
- [ ] 6.3 Update AGENTS.md (current-state section and roadmap status table for F4a once verified); verify the table matches reality at the time of the update
- [ ] 6.4 Add the F4a scenes to the `examples/browser/main.js` gallery (the gallery cycles every golden scene) and verify the web player builds and cycles them

## 7. F4a gate (verification order per AGENTS.md)

- [ ] 7.1 Host verification: full `ctest` green (smoke + headless display-list/material tests + CPU lighting reference + golden suite incl. new/re-baselined 3D scenes) with the F2 suites and goldens unchanged; verify via host ctest output
- [ ] 7.2 Commit → push branch → `python3 tools/verify_remote.py all <branch>` (Linux golden-bearing jobs as the pre-filter); fix and re-verify until green before any GitHub Actions run
- [ ] 7.3 Dispatch `gh workflow run ci.yml --ref <branch>`; the full four-target gate passes in Linux → Windows → macOS order (native suites incl. goldens on each, Emscripten suite + web goldens), per the rendering-milestone gate; verify via the Actions run summary
