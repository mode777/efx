# Tasks

## 1. Desktop binding — positional `drawMesh`

- [x] 1.1 Reshape `efx_js_drawMesh` in `src/api/api.c` to read the mesh from `argv[0]` (require a live Mesh, else `TypeError`) and the option bag from `argv[1]` (optional; must be an object when present); drop `mesh` from the known-fields set and keep `transform`/`color` parsing and validation unchanged. Verify: `tests/unit/api_tests.c` exercises `drawMesh(mesh, opts)`, `drawMesh(mesh)`, a non-Mesh first arg, a destroyed Mesh, and an unknown bag field.
- [x] 1.2 Update the `drawMesh` registration arity hint in `src/runtime/runtime.c` from 1 to 2. Verify: `grep JS_CFUNC_DEF("drawMesh"` reports 2.

## 2. Web binding — positional `drawMesh`

- [x] 2.1 Rewrite the `drawMesh` wrapper in `src/web/entry.js` to `function (mesh, opts)` with the same validation order and error classes as the desktop binding (`arguments.length` for presence, object check on `opts`, unknown-field check over `{ transform, color }`, live-Mesh check first); keep the existing `_efx_bridge_draw_mesh` call unchanged. Verify: the portable validation script passes under the web bridge and desktop runtime with identical results.
- [x] 2.2 Confirm `src/web/bridge.c`'s `efx_bridge_draw_mesh(handle, transformPtr, colorPtr)` needs no change. Verify: no bridge signature change is required by the wrapper rewrite (record in the apply notes).

## 3. Primitive `material` option

- [x] 3.1 In `src/prelude/prelude.js`, add `material` to the allowed-field list of `__efxPrimOpts` and read it in `__efxMakeCube`/`__efxMakePlane`/`__efxMakeSphere`, passing `materials: material === undefined ? undefined : [material]` into each `createMeshData` call (so `null` selects the default). Verify: `tests/scripts/s_3d_math.js` (or a new primitive-material case) shows a bound material carried to `createMesh`, and a non-material `material` value throws `TypeError`.
- [x] 3.2 Regenerate `src/prelude/prelude.h` with `python3 tools/gen_prelude.py` and confirm `python3 tools/gen_prelude.py --check` is clean. Verify: the drift check exits 0.

## 4. Gallery type document

- [x] 4.1 In `gallery/src/api/efx.d.ts`, replace `CreateMeshDataOptions extends MeshSurfaceData` with a union of `MeshDataBatch` (`{ surfaces: MeshSurfaceData[]; materials?: (Material | null)[] }`) and `MeshDataShorthand` (`MeshSurfaceData & { materials?: … }`); add a `DrawMeshCallOptions` holding only `transform?`/`color?` and change the member to `drawMesh(mesh: EfxMesh, opts?: DrawMeshCallOptions): void`; add `material?: Material | null` to `MakeCubeOptions`, `MakePlaneOptions`, and `MakeSphereOptions`. Verify: `npm --prefix gallery run build` type-checks.
- [x] 4.2 Extend `gallery/src/api/efx.type-test.ts` with the new shapes and the rejected ones: a batch `createMeshData({ surfaces: [...] })` without `positions`; an `@ts-expect-error` for `createMeshData({ surfaces, positions })`; `drawMesh(mesh)` and `drawMesh(mesh, { transform })`; an `@ts-expect-error` for `drawMesh({ mesh })`; `makeCube({ size: 1, material })`. Verify: `svelte-check`/`tsc` reports exactly the expected `@ts-expect-error` sites and no others.

## 5. Documentation

- [x] 5.1 Update `docs/js-api.md`: the F3 `drawMesh` entry, primitive entries, and every sample (F3, F4a, F4b, F6b, provisional F7/F8) to `drawMesh(mesh, opts?)` and the primitive `material` option; state that `mesh` is a required positional argument. Verify: no current/provisional `drawMesh({` sample remains, and every signature matches the implementation.
- [x] 5.2 Update the AGENTS.md current-state paragraph to record the positional `drawMesh` signature, the primitive `material` option, and the type-document union fix. Verify: no statement contradicts `docs/js-api.md`. (No ADR — this is a signature refinement of ADR 0024's already-recorded conventions; state this in the apply notes.)

## 6. Call-site rewrites

- [x] 6.1 Rewrite `examples/browser/main.js` and the curated gallery samples (`gallery/samples/curated/hello-cube.js`, `light-show.js`, `texture-showcase.js`, `gltf-showcase.js`) to the positional signature. Verify: `rg 'drawMesh\(\{'` reports no matches in `examples/` or `gallery/samples/`.
- [x] 6.2 Rewrite all 24 3D golden scenes under `tests/goldens/*/main.js` (`light_ambient`, `depth3d`, `light_emissive`, `map_diffuse`, `plane3d`, `light_specular`, `rt_materialmap`, `transform3d`, `light_attenuation`, `light_reference`, `light_multimaterial`, `cube3d`, `surfaces3d`, `light_directional`, `map_reference`, `vertcolor3d`, `rt_scene3d`, `map_specular`, `gltf_import`, `map_multimap`, `map_ambient`, `map_emissive`, `skin_import`, `map_alpha_mask`). Verify: `rg 'drawMesh\(\{'` reports no matches under `tests/goldens/`.
- [x] 6.3 Rewrite the script suites `tests/scripts/s_3d_validation.js`, `s_3d_math.js`, `s_4b_validation.js`, and `s_5a_validation.js`, and the `tests/unit/api_tests.c` draw cases, to the positional signature; add cases for the new throws (missing/non-Mesh/destroyed first arg; `mesh` as an unknown bag field) and the primitive `material` binding. Verify: the smoke and unit suites pass locally (`cmake -B build -DEFX_HEADLESS=ON && ctest --test-dir build`).

## 7. Verification

- [ ] 7.1 Run the Linux native suite including all goldens (`ctest`) and confirm the committed PNGs are reproduced unchanged — the reshape is call-site only. Verify: no golden diff.
- [ ] 7.2 Run the Emscripten ctest suite, web goldens, browser harness, and cross-runtime compare; confirm desktop and web bindings reject the same inputs. Verify: all green.
- [ ] 7.3 Pre-filter on the verification server: `python3 tools/verify_remote.py all <branch>`. Verify: native + web golden jobs green.
- [ ] 7.4 Dispatch the four-target gate `gh workflow run ci.yml --ref <branch>` and confirm all four targets pass before archive. Verify: the run is green on Linux, Windows, macOS, and Emscripten.
