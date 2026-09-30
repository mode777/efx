# Proposal

## Why

`docs/refactoring.md` records that the largest translation units have become
unreviewable: `src/api/api.c` (~5 970 non-blank lines) holds the entire quickjs
binding with feature sections interleaved and bridged by forward declarations;
`src/web/entry.js` (~3 790) holds the whole web binding in one `--post-js`;
`src/render/render.c` (~2 820) owns textures, targets, meshes, materials, the
post chain, records and particles in one TU sharing the global `R`; and
`src/web/bridge.c` (~1 900) exports every domain from one file. Inside those
files, the same helper shapes are copy-pasted (two optional-field reader
families, five array readers, seven live-opaque resolvers, ~12 read-only
getters, 13 finalizer/class-registration triples, 25 inline known-field loops,
13 web resource classes, four handle decoders, nine pool-growth blocks). This
change executes the plan's **Phase C–F** — consolidate the duplicated helpers
in place, then split the four large files by domain — the plan's **Checkpoint
2**.

This is post-F14 maintenance, not a roadmap milestone: it implements no F1–F14
milestone and changes no spec-level behavior. It assumes `refactor-safety-net`
(Checkpoint 1) has landed, so the error catalog and dead-export guard are
available to prove the refactor is behavior-preserving.

## What Changes

- **P3**: one optional-field reader family (`opt_number` / `opt_bool` /
  `opt_vec3` / `opt_u32`) replacing `phys_opt_*` and `pcfg_*`, with each
  call site passing its existing message string (messages are not merged).
- **P4**: one internal `read_elements(ctx, v, len, policy, sink)` shared by
  `get_float_array`, `read_number_array`, `read_index_array`, `read_vec3` and
  `vec_from_value`; the existing per-reader rules (coerce vs strict, integer
  range, error class) become explicit policy, `read_vec3` stops mallocing.
- **P5**: a generic `live_opaque(...)` resolver and a `JS_CGETSET_MAGIC_DEF`
  getter table replacing the seven `get_live_*` and ~12 read-only getters;
  message strings stay exactly as-is per class.
- **P6**: a static `class_spec[]` looped in `efx_api_init` plus a shared
  `finalize_common`, replacing 13 spelled-out registrations and 13
  same-shaped finalizers.
- **P7**: split `src/api/api.c` by domain behind a new
  `src/api/api_internal.h` (class IDs, wrapper structs, error helpers,
  readers): `api.c` (init/lifecycle/`efx` assembly), `api_2d.c`, `api_3d.c`,
  `api_lighting.c`, `api_target_post.c`, `api_resource.c`, `api_text.c`,
  `api_particles.c`, `api_input.c`, `api_physics.c`, `api_audio.c`; forward
  declarations removed. Move-only, one domain per commit.
- **P8**: one `__efxCheckKnown(obj, knownList, where, enumerate)` helper
  replacing the 25 inline known-field loops and `__physKeys`, preserving each
  existing message format and the `getOwnPropertyNames` vs `Object.keys`
  difference.
- **P9**: an `__efxResourceClass(name, { destroy, getters, methods })` factory
  replacing the 13 hand-written web wrapper classes, preserving `instanceof`
  identity, prototype method names and class `name`.
- **P10**: split `entry.js` into ordered `src/web/js/*.js` fragments and
  `bridge.c` into `bridge_*.c` along the same domains, with a private
  `bridge_internal.h`. Move-only; concatenated output must be byte-identical
  apart from seam whitespace.
- **P11**: `pool_grow(...)` and `handle_decode(...)` replacing the four handle
  decoders and nine growth blocks, keeping each call site's OOM branch.
- **P12**: `tex_slot_init(...)` shared by the queued and live texture paths,
  preserving the append-vs-reuse difference.
- **P13**: a `MAP_OFFSETS[]` loop replacing the hand-written per-channel
  material-map retain/release.
- **P14**: split `src/render/render.c` behind `src/render/render_internal.h`
  into `render_texture.c`, `render_target.c`, `render_mesh.c`,
  `render_post.c`, `render_records.c`, `render_particles.c`; `render.h` stays
  the unchanged public API. Move-only.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. Every pass is behavior-preserving (including error messages), so no
  spec-level behavior changes. The change sets `skip_specs: true`.

## Impact

- **Code**: `src/api/api.c` → `src/api/api*.c` + `src/api/api_internal.h`
  (P3–P7); `src/web/entry.js` → `src/web/js/*.js`, `src/web/bridge.c` →
  `src/web/bridge_*.c` + `src/web/bridge_internal.h`, and the CMake
  `--post-js`/`LINK_DEPENDS` wiring (P8–P10); `src/render/render.c` →
  `src/render/render_*.c` + `src/render/render_internal.h` (P11–P14).
- **Build**: `CMakeLists.txt` source lists and Emscripten `--post-js` flags
  change; the `efx_core` exported symbol surface must be unchanged (verified by
  an empty `nm -g --defined-only` diff for the move-only passes).
- **Tests**: no new golden images; the F4b retention cases, render lifecycle /
  generation cases, `api_tests` GC/finalizer cases, `s_resource_lifecycle.js`,
  the module scripts and `run_web_harness.mjs` are the regression net.
- **Docs**: update `docs/refactoring.md` status (P3–P14 done, Checkpoint 2
  reached). No ADR — behavior-preserving restructuring with no new cross-cutting
  invariant. No `js-api` delta, no `efx.d.ts` change, no `docs/api/`
  regeneration, no golden re-baseline.

## Non-goals

- **Any observable behavior change**, including error messages. The error
  catalog from `refactor-safety-net` must stay byte-identical through every
  pass.
- **Changing handle-allocation order or texture-slot reuse** — P11/P12 preserve
  current sequencing; making queued textures reuse freed slots is a deferred
  behavior change (`docs/refactoring.md` §4.3/§4.4).
- **Decomposing long functions** (P15/P16) and the tools/naming/hygiene passes
  (P17–P21) — separate change.
- **Collapsing validation to a single source of truth** (deferred, needs an
  ADR — `docs/refactoring.md` §4.2).
