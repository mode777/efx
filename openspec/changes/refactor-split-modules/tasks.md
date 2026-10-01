# Tasks

## 1. Desktop-binding helper consolidation (P3–P6)

- [x] 1.1 (P3) Add `opt_number`/`opt_bool`/`opt_vec3`/`opt_u32` near the other
  helpers; rewrite `phys_opt_*` and `pcfg_num`/`pcfg_vec`/`pcfg_range` as thin
  wrappers passing their existing messages, then inline them; verify the
  `error_catalog` case is byte-identical and `ctest --test-dir build-h` passes
- [x] 1.2 (P4) Extract `read_elements(ctx, v, len, policy, sink)` and rewrite
  `get_float_array`, `read_number_array`, `read_index_array`, `read_vec3`,
  `vec_from_value` on it (no `read_vec3` malloc); verify the catalog is
  byte-identical and record the old-reader→policy mapping table in the PR
- [x] 1.3 (P5) Add `live_opaque(...)` and a `JS_CGETSET_MAGIC_DEF` getter table;
  migrate the seven `get_live_*` and the read-only getters; verify the catalog
  (including destroyed-resource cases) is byte-identical
- [x] 1.4 (P6) Add a static `class_spec[]` looped in `efx_api_init` plus
  `finalize_common`; verify `api_tests` GC/finalizer cases and
  `s_resource_lifecycle.js` pass and the catalog is byte-identical

## 2. Split the desktop binding (P7, move-only)

- [x] 2.1 (P7) Add `src/api/api_internal.h` (class IDs, wrapper structs, error
  helpers, P3–P5 readers) and move `efx_api_init`/lifecycle/`efx` assembly into
  `api.c`; verify `nm -g --defined-only` of `efx_core` is unchanged
- [x] 2.2 (P7) Move the 2D, 3D, lighting and target/post domains into
  `api_2d.c`, `api_3d.c`, `api_lighting.c`, `api_target_post.c` (one commit
  each); verify each is move-only under `--color-moved` and `nm` is unchanged
- [x] 2.3 (P7) Move the resource, text, particles, input (keyboard/mouse/
  window/gamepad), physics and audio domains into `api_resource.c`,
  `api_text.c`, `api_particles.c`, `api_input.c`, `api_physics.c`,
  `api_audio.c`, merging each split half and removing the forward declarations;
  verify `--color-moved` shows only moves and `nm` is unchanged
- [x] 2.4 (P7) Update `CMakeLists.txt` for the new sources; verify the full
  native build links and `ctest --test-dir build` is green

## 3. Web-binding helper consolidation (P8–P9)

- [ ] 3.1 (P8) Add `__efxCheckKnown(obj, knownList, where, enumerate)` and
  replace the 25 inline loops and `__physKeys`, preserving each message format
  and the enumeration difference; verify the catalog compare is identical on
  web and `run_web_harness.mjs` passes
- [ ] 3.2 (P9) Add `__efxResourceClass(name, { destroy, getters, methods })`
  and rebuild the 13 wrapper classes on it, preserving `instanceof`, method
  names and class `name`; verify `run_web_harness.mjs`, the gallery smoke and
  the catalog pass

## 4. Split the web binding (P10, move-only)

- [ ] 4.1 (P10) Move `entry.js` into ordered `src/web/js/*.js` fragments and
  wire them as ordered `--post-js` flags (update `LINK_DEPENDS`); verify the
  concatenated output differs from the pre-split `entry.js` only by whitespace
  at seams (`diff`)
- [ ] 4.2 (P10) Split `bridge.c` into `bridge_*.c` with a private
  `bridge_internal.h`; verify the Emscripten build, web goldens,
  `run_web_harness.mjs` and `run_web_compare.mjs` pass
- [ ] 4.3 (P10) Update `CMakeLists.txt` for the new bridge sources; verify the
  Emscripten job configures and builds

## 5. Render-core helper consolidation (P11–P13)

- [ ] 5.1 (P11) Add `pool_grow(...)` and `handle_decode(...)`; rewrite the four
  `*_get` decoders and nine growth blocks, keeping each OOM branch; verify
  render lifecycle/generation unit tests pass
- [ ] 5.2 (P12) Add `tex_slot_init(...)` called by both texture paths,
  preserving append-vs-reuse; add a unit test pinning current queued-creation
  handle sequencing *before* the change; verify it passes after
- [ ] 5.3 (P13) Add `MAP_OFFSETS[]` and loop the per-channel material-map
  retain/release; verify the F4b retention tests and map goldens pass

## 6. Split the render core (P14, move-only)

- [ ] 6.1 (P14) Add `src/render/render_internal.h` (the `R` state, pool helpers,
  slot types) and split into `render_texture.c`, `render_target.c`,
  `render_mesh.c`, `render_post.c`, `render_records.c`, `render_particles.c`
  (one commit per file); verify each is move-only under `--color-moved` and
  `nm -g --defined-only` of `efx_core` is unchanged
- [ ] 6.2 (P14) Update `CMakeLists.txt`; verify the full native build and
  `render_tests` pass

## 7. Docs and verification

- [ ] 7.1 Update `docs/refactoring.md` status: mark P3–P14 done and note
  Checkpoint 2 reached; verify no pass in Phase C–F is still listed as pending
- [ ] 7.2 Run the local unit suites
  (`cmake -B build-h -DEFX_HEADLESS=ON && ctest --test-dir build-h`) and the
  generated-file checks (`python tools/gen_prelude.py --check`,
  `npm --prefix gallery run docs:check`); verify all green
- [ ] 7.3 Run `python3 tools/verify_remote.py all <branch>` (native ctest incl.
  all goldens, Emscripten ctest, web goldens, `run_web_compare.mjs`); verify
  green before dispatching CI
- [ ] 7.4 Dispatch `gh workflow run ci.yml --ref <branch>` and confirm the
  Linux → Windows → macOS gate is green; then merge to `main` and push per
  `AGENTS.md`
