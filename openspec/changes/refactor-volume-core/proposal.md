## Why

`docs/refactoring.md` found duplicated control flow throughout the core and
both script bindings:

- render defaults written twice, field by field;
- six identical record-header stamps;
- copy-pasted input/audio/gamepad guards;
- a duplicated player/REPL run-and-teardown sequence;
- a 118-line class-probe chain in the desktop `destroy()`, plus 22 per-class
  release/finalizer shims;
- twin Body/Character wrappers;
- 27 return-code ladders and hand-freed heap pointers in the web binding;
- 31 pass-through wasm exports;
- three copies of the post-chain blur.

The same analysis found a **suspected bug**. Desktop `destroy()`
(`js_destroy_resource`) identifies the object's class by calling
`JS_GetOpaque2` once per class, and that function *throws* on a mismatch. So
destroying any resource other than a Texture leaves pending TypeErrors behind
while returning `undefined`. Script-visible behavior looks normal, but the
binding misuses the quickjs-ng API.

This change executes the plan's **Phase C–F (R8–R16)** and fixes that bug. It
reaches **Checkpoint 2**. It is post-F14 maintenance, not a roadmap milestone,
and depends on `refactor-volume-tests` having landed: the strengthened test net
and the registered R1 cases guard these edits.

## What Changes

- **R8 — render core:**
  - `static const` designated-initializer defaults for camera2d, camera3d and
    material;
  - one shared default-state routine;
  - `record_push` stamps `target`/`sort_key` itself;
  - one `color_or_white` helper;
  - the two single-use map-bind forwarding wrappers are inlined.
- **R9 — input/audio/gamepad:**
  - input feeders push compound literals through one edge-update helper;
  - `live_voice()`/`live_slot()` guards shrink the audio setters and gamepad
    accessors;
  - one name↔id lookup helper.
  - The ADR 0036 injection seam stays.
- **R10 — runtime/player:** hook lists are freed in a loop. Player root mode
  and the REPL share `run_entry` and exit-code helpers. The teardown order is
  unchanged.
- **R11 — desktop `destroy()`/finalize (+ bug fix):**
  - Per-class `destroy`/`release` hooks move into `CLASS_SPECS`.
  - One generic finalizer and one `destroy()` dispatch through
    `JS_GetAnyOpaque`, which does not throw.
  - A new regression test asserts that no exception is pending after
    `destroy()` on every resource class. It fails on today's code and passes
    after the change.
  - Error messages are unchanged.
- **R12 — desktop physics/options:**
  - One `efxjs_collider` wrapper (kind + handle) replaces the twin
    Body/Character structs, with a single pin list. ADR 0046 pinning semantics
    are unchanged.
  - `efx_api_opt_*` gains explicit policies, so the `audio_opt_*`,
    `phys_opt_*` and `get_opt_number` wrappers are deleted.
- **R13 — web binding:**
  - one `__efxRc` return-code helper;
  - validate-then-marshal (or `try/finally`) instead of hand-freed heap
    pointers;
  - one `__efxSourceRect` validator.
- **R14 — wasm exports:** the 31 pass-through `efx_bridge_*` exports are
  deleted. The core functions are exported directly through an
  `EFX_WEB_CORE_EXPORTS` CMake list, and `tools/check_exports.mjs` learns to
  read it.
- **R15 — batched input reads:** `efx_bridge_input_event` and
  `efx_bridge_input_state` replace 20 per-field getters.
- **R16 — pipeline:**
  - the billboard pipeline is derived from the quad descriptor;
  - `post_draw` takes one sampler, and a NULL `fs` replaces the `has_fs` flag;
  - one `blur_two_pass` helper;
  - one `emit_quad` with a bridge flag.
  - The ADR 0025 clip-depth fold is untouched.
- Update the `docs/refactoring.md` status for R8–R16.

Estimated effect: about −880 non-blank lines.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- None. Every pass is behavior-preserving at the script and spec level. The
  R11 fix removes an internal API misuse (pending exceptions after a normal
  return). Its only observable effect is through the C embedding (a
  `JS_HasException` check), not through scripts. The change sets
  `skip_specs: true`.

## Non-goals

- Any change to error messages or error kinds. `s_error_catalog.expected.txt`
  stays byte-identical, including the desktop numeric-coercion behavior
  recorded as a known divergence (that is `shared-option-validation`'s job).
- Changing `destroy()`/finalizer semantics. ImageData still frees its pixels
  only in the finalizer, Audio still stops its voice only in `destroy()`, and
  the white texture is still permanent.
- Touching `play_mesh_record` or the clip-depth remap (ADR 0025), the
  display-list record/replay semantics (ADR 0019), or the physics core
  (ADR 0040/0045).
- Moving validation between the bindings (`shared-option-validation`).
- Build/CMake, tools and comment passes (`refactor-volume-build`).

## Impact

- **Code:**
  - `src/render/render_records.c`, `render_texture.c`;
  - `src/input/input.c`, `gamepad.c`; `src/audio/audio.c`;
  - `src/runtime/runtime.c`, `runtime_internal.h`;
  - `src/player/player.c`, `repl.c`, `player.h`;
  - `src/api/api.c`, `api_internal.h`, `api_physics.c`, `api_audio.c`,
    `api_text.c`;
  - `src/web/js/*.js`, `src/web/bridge_*.c`;
  - `src/platform/pipeline.c`.
- **Build/tools:** the `EFX_WEB_COMMON` link options in `CMakeLists.txt`
  (export list); `tools/check_exports.mjs`.
- **Tests:** one new `efx_api_tests` case (`destroy_no_pending_exception`).
  Everything else runs unchanged.
- **Docs:** **No ADR.** No new architectural decision: R14 keeps the core
  free of Emscripten macros (ADR 0003), and R11/R12 preserve ADR 0011/0012/0046.
  `docs/refactoring.md` status changes. `docs/js-api.md`, `docs/api/`,
  `efx.d.ts` and `AGENTS.md` are unchanged.
- **Verification:**
  - V1/V2 per pass and V4 on the server.
  - R16 additionally needs **V5 through macOS** (Metal/D3D11 pipeline state).
  - The catalog stays byte-identical (E1), and the ctest inventory equals the
    post-`refactor-volume-tests` inventory plus the one new R11 case.
