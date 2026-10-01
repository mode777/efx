## Context

See `proposal.md` (Why) and `docs/refactoring.md` §2.2–§2.4 and R8–R16. The
current facts that shape the approach:

- **Desktop class registration.** It is already table-driven: `CLASS_SPECS`
  in `src/api/api.c` (L1004) has one row per script class, looped by
  `efx_api_init`.
- **Destroy and finalize.** `js_destroy_resource` (L421) and the per-class
  finalizers predate that table:
  - `destroy()` probes `JS_GetOpaque2` once per class. In quickjs-ng
    (`vendor/quickjs-ng/quickjs.c` L12125) that call throws on a class
    mismatch, and `JS_Throw` stores the error in `rt->current_exception`.
  - The binding then returns `undefined`, so the error stays pending until the
    next throw or runtime teardown (`JS_FreeRuntime` frees it).
  - Scripts cannot observe this; `JS_HasException(ctx)` can.
- **Physics wrappers.** Body and Character wrappers (`efxjs_body`,
  `efxjs_character`) have identical layouts. Both handles are `uint32_t`. They
  sit on two host lists (`physics_bodies`, `physics_characters` in
  `runtime_internal.h`).
  - Under ADR 0046 the world pins each wrapper (an owned `self` reference)
    until `destroy()`, `physics.clear()` or teardown.
  - `find_body_wrapper` maps a contact's handle back to its wrapper.
- **Option-reader wrappers.** `phys_opt_*` coerce through `JS_ToFloat64`;
  the catalog records this as `coercion.phys-number-string`. `audio_opt_*`
  treat `null` as absent, require a number type, and format messages with the
  key. `get_opt_number` (text) also reports whether the field was present.
- **Web bridge.** The bridge links its `bridge_*.c` fragments into each web
  executable. `EXPORTED_FUNCTIONS` is set once in `EFX_WEB_COMMON` and today
  lists only `_main,_malloc,_free`; everything else is `EMSCRIPTEN_KEEPALIVE`.
  `src/web/js/core.js` already has a reusable draw scratch buffer
  (`drawScratchPtr`).
- **Pipeline.** `src/platform/pipeline.c` keeps the clip-depth fold in
  `play_mesh_record` (ADR 0025). The quad, billboard and post pipelines are
  created in `efx_pipeline_install`.
- **Resource exposure to scripts** stays as it is:
  - dynamic-count resources are GC-finalized opaque classes with an explicit
    `destroy()` (ADR 0011, memory discipline ADR 0012);
  - lights are a fixed pre-allocated bank;
  - Body/Character are world-held until released (ADR 0046).

  R11/R12 change how this is *implemented*, not what scripts see.

## Goals / Non-Goals

**Goals:**

- Remove the duplicated control flow listed in the proposal, keeping every
  check in its current order.
- Fix the pending-exception misuse in `destroy()`, proven by a test that fails
  before the fix and passes after it.

**Non-Goals:**

- Merging validation rules that differ on purpose (policies stay explicit).
- Changing handle values, slot reuse, or allocation order in any pool.
- Any change visible in the error catalog.

## Decisions

### D1 — Render defaults as `static const` initializers

Add `DEFAULT_CAMERA2D`, `DEFAULT_CAMERA3D` and `DEFAULT_MATERIAL` as
designated-initializer constants, plus one `apply_default_state()` (camera,
camera3d, lights zeroed, clear color, blend) called by `ensure_state` and
`efx_render_reset_state`. The reset path still calls `post_reset()`.

- **Why:** the defaults become data, written once and readable at a glance.
- **Rejected — keep the field-by-field functions and only de-duplicate the
  callers:** about half the saving, and the defaults stay spread over 40
  lines.

### D2 — `record_push` owns the record header

`record_push(efx_record *rec)` sets `rec->target = R.active_target` and
`rec->sort_key = (uint32_t)R.record_count` before the budget check. All 6
producers stop setting those fields.

- **Why:** every producer stamps them identically. Doing it at the single
  choke point makes that a guarantee.
- **Rejected — a `record_header(type)` helper called by each producer:** the
  stamping stays repeated at every call site.

### D3 — Accessor guards return a pointer or NULL

- `live_voice(int)` returns the voice if it is in range and active, otherwise
  NULL.
- `live_slot(int)` does the same for gamepad slots.
- `set_level(down, pressed, released, i, is_down)` is the shared edge update.
- Feeders call `queue_push(&(efx_input_event){ .type = …, … })`. Compound
  literals are already used throughout `pipeline.c` under MSVC.
- `efx_input_inject_*` are kept as named functions (ADR 0036).

**Why:** one guard per module instead of one per function. The behavior of an
out-of-range or inactive id (silently ignored) is unchanged.

### D4 — Player and REPL share entry and exit-code helpers

`player.h` declares:

- `int efx_player_run_entry(efx_runtime *rt, efx_resource *res)` — returns −1
  to continue into the frame loop, or the exit code;
- `int efx_player_exit_code(const efx_runtime *rt)`.

The REPL keeps its "no `main.js` is fine" branch. The teardown order is
unchanged: runtime destroy → render end/shutdown → platform shutdown →
resource close.

**Rejected — one `efx_player_run(…, frame_cb)` for both modes:** the REPL's
stdin pump and the capture mode would need flags inside a shared loop, which
reads worse than two short callers.

### D5 — `destroy()` and finalizers dispatch through `CLASS_SPECS` with `JS_GetAnyOpaque`

Each row gains:

- `destroy(JSContext *, void *)` — script `destroy()` semantics: idempotent,
  marks dead, releases native; the permanent white texture throws its existing
  TypeError;
- `release(void *)` — finalizer semantics.

One `class_finalizer(rt, val)` and one `js_destroy_resource` call
`JS_GetAnyOpaque(val, &id)`, look up the row by class id, and dispatch. An
unknown class keeps the TypeError `"not a resource object"`.

`destroy` and `release` stay separate because their semantics differ by
design:

- ImageData's `destroy()` only marks the object dead, and the finalizer frees
  the pixels.
- Audio's `destroy()` stops the voice, and its finalizer does not (a
  fire-and-forget sound keeps playing).

**Regression test first.** Add an `efx_api_tests` case,
`destroy_no_pending_exception`:

- create one instance of each non-Texture resource class, call `destroy()` on
  it, and assert `JS_HasException(ctx)` is false;
- reach the context through a new internal accessor, `efx_runtime_context()`,
  declared in `runtime_internal.h` (the public `runtime.h` stays unchanged).

The test is expected to fail on the old code. Record the observed failure in
"Findings during apply", then land the refactor so it passes.

- **Rejected — keep the probe chain and clear the exception after each miss:**
  that adds lines and keeps a known API misuse.
- **Rejected — `JS_GetOpaque` (non-throwing) per class:** it is correct, but
  still a 13-step chain the table already encodes.
- **Rejected — test through script behavior only:** the leak is invisible to
  scripts, so a script test would pass on both the old and the new code.

### D6 — One collider wrapper type; per-kind lists keep their order

`efxjs_collider { kind; w; handle; alive; pinned; self; next; host; }`
replaces the two structs.

- `efx_host_state` keeps **two** list heads, indexed by kind:
  `physics_colliders[2]`.
- Wrap, pin/unpin, finalize, live-resolve, `find_wrapper(kind, handle)` and
  the teardown release are written once and parameterized by kind.
- Kind-specific accessors (`onFloor`, `moveAndSlide`, `applyImpulse`,
  `contacts`, …) stay per class. Shared accessors (position, velocity) dispatch
  on kind.
- Script classes stay distinct (`Body`, `Character`, separate class ids and
  prototypes). `instanceof` and the catalog messages
  (`"expected a Body"`, …) are unchanged.
- **Why per-kind lists:** teardown and lookup order stay byte-for-byte
  identical, so ADR 0046 behavior cannot shift.
- **Rejected — a single merged list:** the release order at teardown would
  interleave bodies and characters, an untested ordering change.

### D7 — Option readers: one family with explicit policy flags

`efx_api_opt_number(ctx, obj, key, out, policy, msg)` takes a policy bitmask:

- `NULL_IS_ABSENT`;
- `STRICT_NUMBER` (no `JS_ToFloat64` coercion);
- `MSG_WITH_KEY` (format `"%s must be …"` with the key);
- `REPORT_PRESENT`.

`opt_bool` and `opt_vec3` follow the same pattern. Each former wrapper becomes
a call with its current policy and message.

- **Why:** it extends the refactor-split-modules D5 approach (policies, not
  merged rules). Physics keeps coercion, so the recorded divergence is
  unchanged.
- **Rejected — unify on one rule:** that is a behavior change, owned by
  `shared-option-validation`.

### D8 — Web: return codes, heap lifetime and `sourceRect` helpers

- `__efxRc(rc, where, extra)`:
  - returns for `0`;
  - codes `1`/`4`/`9` map to the existing shared messages;
  - `extra` supplies per-site codes (e.g. `{2: [TypeError, 'expected a Texture or RenderTarget']}`);
  - anything else throws `Error("<where> failed")`.
- Heap lifetime:
  - prefer **validate-then-marshal**: move each `__efxAllocCStr`/`mallocCopy*`
    after the last throwing check;
  - where a call needs several buffers or can throw after allocation, use
    `try { … } finally { _free(…) }`.
  - Allocation never throws, so check order — and therefore which error a bad
    input produces — is unchanged.
- `__efxSourceRect(tex, v)` replaces the three `sourceRect` validators with
  identical messages.
- **Rejected — a heap arena freed per API call:** a new mechanism for a
  problem `finally` already solves.

### D9 — Export core functions directly from CMake

- Add `set(EFX_WEB_CORE_EXPORTS _efx_input_key_id … )` and join it into
  `-sEXPORTED_FUNCTIONS` in `EFX_WEB_COMMON`.
- Delete the 31 single-statement passthroughs. A function qualifies when its
  body is one `return efx_*(args);` or `efx_*(args);` with an identical
  parameter list.
- JS call sites move from `_efx_bridge_*` to the core name.
- `tools/check_exports.mjs` parses the CMake list as an export source.
- **Why:** the core stays free of Emscripten macros (ADR 0003 module walls).
  Emscripten treats exported functions as required symbols, so they are pulled
  out of the `efx_core` archive.
- **Rejected — an `EFX_EXPORT` macro in core headers:** it leaks a platform
  concern into the core.
- **Rejected — keep the wrappers:** about 100 lines that only rename.

### D10 — Batched input reads use one `double` record

- `efx_bridge_input_event(int i, double *out)` writes 10 doubles:
  type, key, button, repeat, mods, codepoint, x, y, dx, dy.
- `efx_bridge_input_state(double *out)` writes 9:
  x, y, dx, dy, wheel dx, wheel dy, width, height, dpi.
- JS reads them from the existing scratch buffer via `HEAPF64`.
- **Why `double`:** it represents every int and float field exactly
  (codepoints up to 0x10FFFF, 32-bit mods) in one array type.
- **Rejected — JS reads `efx_input_event` struct offsets directly:** that
  couples JS to the C struct layout.

### D11 — Pipeline helpers stay outside the mesh path

- Build the quad `sg_pipeline_desc` once and derive the billboard pipeline by
  changing `depth.compare` only.
- `post_draw` takes one sampler for both slots and `fs == NULL` instead of
  `has_fs`.
- `blur_two_pass(src, t0, t1, dx, dy)` serves the three separable-blur
  copies.
- `emit_quad(v, r, flip, bridge)` merges the two emitters.
- **`play_mesh_record` and the clip-depth fold are not touched** (ADR 0025).
- **Rejected — also fold the mesh pipelines into the shared builder:** it
  would route the D3D11/Metal-sensitive mesh state through a helper for about
  5 lines of gain.

## Risks / Trade-offs

- **[Risk] The `destroy()` table loses a per-class nuance** (permanent
  texture, ImageData pixels, Audio voice). → D5 keeps `destroy` and `release`
  separate per row. Covered by the catalog's destroyed/permanent cases,
  `texture_lifecycle`, `font_js`, `audio_js`, `particles_js`, `mesh_js`,
  `f5a_js` and `smoke_resource_lifecycle`.
- **[Risk] The collider merge changes pinning or teardown.** → D6 keeps
  per-kind lists. Covered by `physics_js`, `smoke_12_physics`, and
  `smoke_showcase_physics` (the ADR 0046 unreferenced static-collider case).
- **[Risk] Moving allocations changes which error is thrown first.** → D8:
  allocation never throws; `web_error_catalog` must be byte-identical.
- **[Risk] Directly exported core symbols are dropped by the linker or
  renamed.** → V4 web ctest, web goldens, `run_web_harness.mjs` and gallery
  smoke. E6: the export list differs only by the intended renames.
- **[Risk] Pipeline state differs on Metal/D3D11.** → R16 requires V5 through
  macOS before Checkpoint 2.
- **[Trade-off] A policy bitmask is less self-describing than named
  wrappers.** → Each call site passes named flags. The flag set is closed and
  documented in `api_internal.h`.

## Migration Plan

1. Branch `refactor-volume-core` from `main` once `refactor-volume-tests` is
   merged. One commit per pass, in `tasks.md` order.
2. Per pass: V1, plus V2 where noted. Web passes (R13–R15) go through V4.
3. Checkpoint 2:
   - V4 (`python3 tools/verify_remote.py all refactor-volume-core`);
   - then V5 in the order Linux → Windows → **macOS** (required by R16).
4. Merge to `main`, push, and archive (`skip_specs`).
5. Rollback: revert per pass. R11's regression test stays, so a revert of
   R11 re-exposes the bug visibly.

## Findings during apply

- **D4 signature.** `efx_player_run_entry` is
  `int (efx_runtime *rt, char *code, int *exit_code)`: it returns 1 (stop,
  `*exit_code` set) or 0 (continue). A "−1 means continue" return would
  collide with `efx.quit(-1)`, which is a legal exit code. The caller still
  reads `main.js`, because root mode reports a missing entry as an error
  naming the root while the REPL treats it as fine.
- **R11 confirmed.** On the old code, `destroy_no_pending_exception` failed
  for **all ten** classes: ImageData, MeshData, Mesh, RenderTarget, FontData,
  Font, ParticleSystem, AudioData, AudioStream and Audio. Each one reported
  "exception pending after destroy". Creation was clean in every row, and no
  `destroy()` threw, so the leak comes only from the `JS_GetOpaque2` probe
  chain. A Texture is the first probe, so it never leaked.
- **D7 flags.** Three flags were enough: `EFX_OPT_NULL_ABSENT`,
  `EFX_OPT_STRICT` and `EFX_OPT_KEY_MSG` (the message is "<key> <msg>").
  `REPORT_PRESENT` was dropped, because the readers' 1/0/−1 return already
  reports presence. Text's two "requires a numeric …" sites throw their own
  message when the field is absent (return 0). `efx_api_opt_vec3` now writes
  an `efx_vec3`, since physics is its only caller.
- **D8 opt-in shared codes.** `__efxRc(rc, where, codes)` applies the shared
  1/4/9 messages only where a site lists them (`{1: true, …}`). Applying them
  everywhere would have changed one path: `drawSprites` can get
  `EFX_RENDER_ERR_FEEDBACK` (9) from `efx_render_quad`, which today throws
  `Error('drawSprites failed')`. The text helper `__efxTextError` folded into
  `__efxRc(rc, 'text operation', {4: …})`. The real ladders were 10 call sites
  (plus the text helper), not 27: the plan counted `if (rc === N)` blocks.
  Heap: `createFont` and `loadMeshData` now validate before they marshal.
  `createImageData` and `moveAndSlide` use one catch/finally free each.

<!-- Record whether destroy_no_pending_exception failed on the old code (per class). -->
