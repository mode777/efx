# Tasks

Implements F4b (per-channel maps + alpha masks). Decisions D1–D10 refer to
`design.md`; behavior requirements live in the delta specs
(`specs/lighting/`, `specs/3d-core/`, `specs/js-api/`).

## 1. Material maps and validation (D5, D7)

- [x] 1.1 Extend `efx_material` (`src/render/render.h`/`render.c`) with a handle + presence flag per channel map and for the alpha mask, keep the color/shininess snapshot fields, and update `efx_material_default`/copy helpers; verify with a C unit test in `tests/unit/render_tests.c` that the default has no maps, an explicit map records its handle, and a copied material preserves it
- [x] 1.2 Extend the desktop material parser `read_material` (`src/api/api.c`) to accept `map` on each channel and material-level `alphaMask`, accepting only a live `Texture` (`TypeError` for a non-Texture, a destroyed texture, or an unknown field; `RangeError` for non-positive/non-finite `shininess`); verify with `--script` smoke cases for every throw and valid case (new `tests/scripts/s_4b_validation.js`, exit 0)
- [x] 1.3 Mirror the parser and validation in the web binding: extend the material wire layout in `src/web/bridge.c` so map handles cross as doubles plus presence flags, and update `__efxMaterial` in `src/web/entry.js` to accept `map`/`alphaMask` as live `EfxTexture` with the same errors; verify `tools/run_web_compare.mjs` reports desktop vs web agreement over `s_4b_validation.js`
- [x] 1.4 Confirm the snapshot rule for maps on both the `setMeshSurfaceMaterial` and `createMeshData` `materials` paths: mutating (including swapping the `map` of) the script material object after binding MUST NOT change the bound material; verify with a unit test and a `--script` smoke case

## 2. Bound-map texture retention (D6)

- [x] 2.1 Add a bind-reference count and a release-pending bit to `tex_slot`, plus a retained-native accessor used by playback, in `src/render/render.c`: `Texture.destroy()` marks the script handle dead but defers the native release while the count is non-zero; verify with a C unit test driving create → bind → destroy → draw still resolves the native → rebind → native release fires
- [x] 2.2 Retain/release symmetrically wherever an `efx_material` with maps is stored or replaced (MeshData surfaces, Mesh surfaces at upload, `setMeshSurfaceMaterial`, `createMesh`, `destroy`) so the count returns to zero; verify with a unit test that binds, rebinds, and destroys meshes and asserts the count drains and nothing leaks at `efx_render_shutdown`
- [x] 2.3 Verify the script-facing contract: a texture destroyed while bound keeps shading until unbound, and passing a destroyed texture as a fresh map throws `TypeError`; verify with `--script` smoke cases in `s_4b_validation.js`

## 3. Map/alpha shader and playback (D1, D3, D4)

- [x] 3.1 Extend `shaders/mesh.glsl` from the F4a Phong shader: pass the interpolated `uv` to the fragment stage, add four channel `texture2D`/`sampler` bindings and one alpha-mask binding, multiply each channel by its sample (neutral when absent), and add the uniform-gated `discard` for a mask sample `< 0.5`; regenerate `shaders/mesh.h` with the pinned sokol-shdc flow (ADR 0021) and verify the generated header compiles on the GL, D3D11, Metal, and GLES3 paths with the texture bindings within each backend's limits
- [x] 3.2 Wire playback in `src/platform/pipeline.c`: bind the five map views (engine `whiteTexture` for absent color maps, mask flag off when absent) with the shared sampler, apply `D1`/`D2`/`D4` exactly, and confirm the existing `mvp`/`model`/normal-matrix/light/material uniforms are unchanged; verify on the host with a scratch scene (mapped cube + sphere, alpha-masked surface) before any golden exists
- [x] 3.3 Confirm neutral maps leave F4a output untouched: render the committed scenes with no maps and verify the seven F2 2D goldens and every F3/F4a golden are byte-identical on the host; verify via host ctest

## 4. CPU lighting reference (D8)

- [x] 4.1 Extend `efx_lighting_shade` (`src/render/render.c`) to take a `const efx_map_samples *` (four RGB samples + mask alpha + presence) and return whether the fragment is discarded; `NULL` reproduces the F4a neutral result, and existing F4a tests/callers pass `NULL`; verify the existing CPU-reference unit tests still pass in ctest
- [x] 4.2 Add headless unit tests over analytic configurations: a channel map scaling exactly its channel, neutral maps equal to `NULL`, the mask `0.5` boundary (`< 0.5` discarded; `>= 0.5` shaded with the albedo alpha); verify they pass in ctest on the host
- [x] 4.3 Add a golden scene that renders a known map/mask configuration and assert the captured pixels match the CPU reference within the golden tolerance; verify the comparison passes on the host

## 5. Golden scenes (D9)

- [x] 5.1 Author the F4b scene resource roots (diffuse map, ambient map, specular map, emissive map, alpha-mask cutout, multi-map, CPU-reference agreement) at the standard 640×480 using only the current API; verify each runs in the host player without error
- [x] 5.2 Capture and commit the new goldens via the documented regeneration invocation, run the manual server-side llvmpipe capture step for the new scenes on the SSH verification server (`docs/verification-server.md`), and verify the golden suite passes on the host with every F2/F3/F4a golden unchanged
- [x] 5.3 Add the F4b scenes to the `examples/browser/main.js` gallery (the gallery cycles every golden scene) and verify the web player builds and cycles them

## 6. Docs and ADR

- [x] 6.1 Write ADR `docs/decisions/0027-mesh-material-maps-and-alpha-mask.md` per TEMPLATE.md (single uniform-driven shader with four channel samplers + a mask sampler and white-texture fallback, no permutations — upholding ADR 0026; binary `alpha < 0.5` cutout; bound-map texture retention) and add it to the `docs/decisions/README.md` index; verify the file exists and is indexed
- [x] 6.2 Move the `docs/js-api.md` F4b entries (`map` per channel, `alphaMask`, uv consumption, retained map textures) from provisional to current, state the neutral-absent-map and binary-mask behavior, and keep the `uv`-transform/wrap notes as explicit non-goals; verify every signature matches the implementation
- [x] 6.3 Update AGENTS.md (current-state section and the roadmap status table for F4b, including the shader/lifetime decision and the new ADR); verify the table matches reality at the time of the update

## 7. F4b gate (verification order per AGENTS.md)

- [x] 7.1 Host verification: full `ctest` green (smoke + headless display-list/material tests + extended CPU lighting reference + golden suite incl. the new F4b scenes) with the F2/F3/F4a goldens unchanged; verify via host ctest output
- [x] 7.2 Commit → push branch → `python3 tools/verify_remote.py all <branch>` (Linux golden-bearing jobs as the pre-filter); fix and re-verify until green before any GitHub Actions run
- [x] 7.3 Dispatch `gh workflow run ci.yml --ref <branch>`; the full four-target gate passes in Linux → Windows → macOS order (native suites incl. goldens on each, Emscripten suite + web goldens), per the rendering-milestone gate; verify via the Actions run summary
