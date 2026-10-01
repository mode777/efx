## Why

The desktop binding (`src/api/*.c`, quickjs) and the web binding
(`src/web/js/*.js`, the page's JS engine, ADR 0022) each implement **all**
option-bag validation for the same `efx` API:

- 391 throw sites on desktop and 323 on the web (246 unique web messages);
- every new option is written, reviewed and tested twice;
- the copies have already drifted. The error catalog
  (`tests/scripts/s_error_catalog.js`) records **18 message divergences**
  (light slots, particle `max`/`facing`, billboard `pos`, physics
  `createBody`/`raycast`/`overlap`/`shapeCast`/`step`, `createStaticMesh`, and
  the image/glTF/audio load failures).
- It also records one **semantic divergence**: on desktop, physics number
  fields coerce `'0.5'` to 0.5. That contradicts the `docs/js-api.md` rule that
  a wrongly-typed field throws `TypeError`, and the web binding enforces that
  rule.

Part 1 of `docs/refactoring.md` (the `refactor-volume-*` changes) shrinks each
binding but cannot remove the duplication between them. This change does.
Cold-path validation moves **once** into the shared engine prelude
(`src/prelude/prelude.js`), which both runtimes already evaluate, and both
bindings shrink to marshalling a normalized form. It is the plan's **Part 2
(R22–R29)**. It is post-F14 maintenance, not a roadmap milestone, and it
starts after `refactor-volume-core` (which lands the web `__efxRc` and the
desktop option-policy groundwork).

## What Changes

- **Spike and decision (R22).**
  - Prototype the particles domain: `__efxParticleWire` moves into the prelude,
    and the desktop `createParticleSystem` accepts the normalized wire.
  - Measure the quickjs cost on desktop for a cold path
    (`createParticleSystem` ×1000) and a hot path (`drawQuad` with options
    ×10 000).
  - Record the outcome as **ADR 0049**, including the cold/hot boundary, the
    performance budget and the canonical-message rule.
  - The change continues only if the ADR is accepted (go/no-go task).
- **Shared validation infrastructure.**
  - The prelude receives a private `natives` object (never on `efx`, never a
    global), so the public namespace and `efx.d.ts` are unchanged.
  - The validation helpers live once in the prelude: known-field check, number
    and array readers, `sourceRect`, and the return-code → error table.
  - Natives report failures as codes; one prelude table turns codes into
    errors.
- **Domain migrations (R23–R29), in this order:**
  1. particles config;
  2. post effects;
  3. fonts;
  4. physics creation and queries;
  5. audio;
  6. resource construction (ImageData, Texture, RenderTarget, MeshData +
     materials, `loadMeshData`, loaders);
  7. lights and cameras.

  For each domain, the desktop C reader and the web JS reader are deleted, and
  each native unpacks the normalized form. Core re-validation at the C ABI
  boundary stays.
- **Hot paths stay native** unless ADR 0049's measurements show the cost is
  within budget: `drawQuad`, `drawSprites`, `drawBillboard`, `drawMesh`,
  `drawText`/`measureText`, and the input queries.
- **Message convergence.** Each of the 18 divergent cases gets one canonical
  message, used identically by both runtimes. The catalog's `DIVERGENT` map
  ends empty.
- **BREAKING (desktop only): no numeric coercion.** Number-typed option fields
  accept only `typeof === 'number'` on every runtime. Desktop scripts passing
  numeric strings or booleans to physics options now get `TypeError`, as the
  web build already does.

Estimated effect: about −1 000 to −1 800 net non-blank lines (R22 confirms the
number). Every future option is then validated in one place.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `js-api`:
  - **"Two-layer API with strict layering"** is modified so that a
    C-implemented function may validate and normalize its arguments in the
    engine prelude, through a binding object scripts cannot reach. It stays
    tagged C-implemented.
  - **A new requirement** says both runtimes throw the same error class and
    message for the same invalid call or failed load, and that number-typed
    fields reject non-number values.

## Non-goals

- Any new script-facing function, option, or resource type. `efx.d.ts` and
  `docs/api/` are unchanged.
- Moving hot-path validation unless ADR 0049's budget allows it.
- Removing the core's own re-validation (`ps_config_valid`,
  `post_entry_valid`, …) — defense in depth at the C ABI boundary.
- Merging the two bindings or adding quickjs to the web build (ADR 0022
  holds).
- Changing liveness tracking. Each binding keeps its own mechanism
  (ADR 0011/0012); only the check *order* and *messages* are shared.

## Impact

- **Code:**
  - `src/prelude/prelude.js` (+ regenerated `prelude.h`) gains the validators;
  - `src/runtime/runtime.c` and `src/web/js/core.js`/`boot.js` pass the
    `natives` object;
  - `src/api/api_*.c` and `src/web/js/*.js` lose their per-domain readers;
  - `src/web/bridge_*.c` exports accept the normalized forms;
  - `tools/gen_prelude.py` changes if the prelude wrapper signature changes.
- **Tests:**
  - `tests/scripts/s_error_catalog.js` and its `.expected.txt` (divergences
    resolved, a coercion case added for both runtimes);
  - the `api_tests` cases that assert desktop coercion, if any;
  - a timing harness for the R22 measurements (spike only, not part of the
    gate).
- **Docs:**
  - **new ADR `docs/decisions/0049-shared-option-validation.md`** + an index
    row;
  - `docs/js-api.md` (layering and the "adding API" process: cold-path
    validation is written once, in the prelude);
  - the `AGENTS.md` "JS API layering" bullet;
  - the `docs/refactoring.md` Part 2 status.
  - `docs/api/` and `efx.d.ts` are unchanged; `docs:markdown` must produce no
    diff.
- **Verification:**
  - V1/V2 and V3 (`prelude.h` drift);
  - V4: the catalog through both runtimes is byte-identical to the updated
    expected file, and `run_web_compare.mjs` is green;
  - V5 four-target gate;
  - each domain's desktop timing stays within the ADR 0049 budget.
