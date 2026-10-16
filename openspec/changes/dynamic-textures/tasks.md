# Tasks

## 1. Render core

- [x] 1.1 Add `dynamic` bit to `tex_slot` and thread it through
      `efx_render_texture_create` (create signature or flags param); reject
      `mipmaps && dynamic` at core level (`EFX_RENDER_ERR_ARG`). Verify:
      unit test in `tests/` render suite — create dynamic + mipmaps returns
      error, plain create unchanged.
- [x] 1.2 Add `update_texture` hook to `efx_render_sink` (`src/render/render.h`)
      and implement `efx_render_texture_update(handle, rgba)`: validate
      handle, liveness, dynamic flag, sink presence; no-op success when
      sink-less (ADR 0052). Verify: unit test with mock sink counting calls
      and checking error codes for dead handle / non-dynamic / destroyed.
- [x] 1.3 Confirm deferred-release interplay: update during `bind_refs > 0`
      or `release_pending` touches only bytes, not slot state. Verify: unit
      test updating a texture retained by a bound material.

## 2. Platform sink

- [x] 2.1 Implement `pipe_update_texture` in `src/platform/pipeline.c` via
      `sg_update_image` on the streaming image and register it in the sink
      vtable. Verify: native build compiles on all four target toolchains.

## 3. Script API

- [x] 3.1 Prelude: extend the `createTexture` option parser with `dynamic`
      (boolean, default false, mutual exclusion with `mipmaps` → TypeError);
      thread it to the native wire call; run `tools/gen_prelude.py` and
      commit regenerated `src/prelude/prelude.h` (gate fails on drift).
      Verify: `gen_prelude.py --check` clean; prelude unit coverage for the
      new option errors.
- [x] 3.2 Desktop (`src/api/api_2d.c`): pass `dynamic` through create; add
      `update` method to the Texture class with native validation
      (ImageData instanceof, live, dynamic, exact size → else `TypeError`).
      Verify: headless unit test — update on CPU-only run succeeds; error
      cases throw `TypeError`.
- [x] 3.3 Web (`src/web/bridge_render2d.c` family + `src/web/js/core.js`
      `EfxTexture` + graphics fragment `createTexture` in
      `src/web/js/audio.js`): same surface and validation split. Verify:
      web build; bridge smoke test in browser harness.
- [x] 3.4 Types + docs: `gallery/src/api/efx.d.ts` (`dynamic` option,
      `update` method), `gallery/src/api/efx.type-test.ts` coverage,
      regenerate `docs/api/` (`npm --prefix gallery run docs:markdown`),
      `npm --prefix gallery run check` and `docs:check` green.

## 4. Tests — behavior

- [x] 4.1 Error catalog cases in `tests/scripts/s_error_catalog.js`:
      `update` non-ImageData, size mismatch, non-dynamic texture, destroyed
      texture, `dynamic` + `mipmaps` combination, non-boolean `dynamic`.
      Verify: headless `--script` suite passes.
- [ ] 4.2 Golden scene `tests/goldens/texture_update/`: dynamic texture
      updated per frame with a frame-index-driven pattern, drawn full-frame;
      capture pins a deterministic frame. Verify: golden passes in the
      Linux llvmpipe job first, then all four targets via
      `tools/verify_remote.py all <branch>` before dispatching the gate.

## 5. ADR + wrap-up

- [x] 5.1 Write `docs/decisions/0061-*.md` (streaming-usage opt-in,
      full-texture scope, live binding semantics) and index it in
      `docs/decisions/README.md`.
- [x] 5.2 Full local gate: `cmake -B build` unit tests + ctest (golden
      excluded locally, ADR 0020), gallery build. Verify: all green before
      committing the change scope.
