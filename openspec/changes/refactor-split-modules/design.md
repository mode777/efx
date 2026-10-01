# Design

## Context

See `proposal.md — Why` and `docs/refactoring.md` (§1.1, §1.3, §2, Phases
C–F) for the full inventory and pass list. Facts that shape the approach:

- The desktop binding is one TU; `efx_api_init` registers 13 classes and
  references domain functions defined far below, which is why forward
  declarations exist (`api.c` L85, L90, L671).
- The web binding is embedded as a single `--post-js=src/web/entry.js`;
  `bridge.c` exports ~179 `EMSCRIPTEN_KEEPALIVE` functions. `entry.js` tracks
  liveness itself and mirrors the desktop contract.
- The render core owns one global `R` state struct and public `render.h`; the
  pipeline reads the recorded display list, so nothing here changes the
  record/replay boundary.
- The plan's validation ladder (§2): V1 headless unit suites, V2 full native
  build with goldens, V3 generated-file checks, V4 SSH server pre-check, V5 the
  four-target CI gate. Move-only passes are reviewed with
  `git diff -M --color-moved=dimmed-zebra`; symbol-stability passes compare
  `nm -g --defined-only` of `efx_core` before/after.
- `refactor-safety-net` (assumed landed) provides the byte-pinned error catalog
  and the dead-export guard.

## Goals / Non-Goals

**Goals:**

- Consolidate duplicated helpers so the rule differences live in one named
  place, then split the four large files into domain-sized TUs without changing
  any behavior, error message, handle-allocation order, or the `efx_core`
  symbol surface.
- Keep each pass independently reviewable (in-place diffs first, then move-only
  diffs) and independently revertable.

**Non-Goals:**

- Behavior changes (including the coercion divergence) and handle-order changes.
- Long-function decomposition (P15/P16).
- Touching the fixed-function consumer contract, the display-list semantics,
  or the clip-depth remap (ADR 0025).

## Decisions

### D1 — Consolidate in place before moving files

Run P3–P6 (api helpers), P8–P9 (web helpers), P11–P13 (render helpers) as
in-place edits before P7/P10/P14 move code between files.

- **Why**: reviewers see real diffs (behavior-preserving rewrites) rather than
  a move that hides a semantic change; and the later move-only passes start
  from already-smaller, already-deduplicated code.
- **Alternatives**: split first then dedupe (each split would carry the
  duplicated code into the new files, then require a second churn).

### D2 — Internal headers, public headers untouched

Introduce `src/api/api_internal.h`, `src/web/bridge_internal.h` and
`src/render/render_internal.h` holding the shared non-public surface (class IDs,
wrapper structs, error/reader helpers, slot tables, the `R` state struct).
Public `api.h`, `web.h`, `render.h` are unchanged.

- **Why**: the split needs a private contract between fragments; keeping the
  public headers byte-stable preserves the `nm` symbol-stability check and
  avoids any script-facing implication.
- **Alternatives**: make helpers non-static and declare them ad hoc in each TU
  (scatters the contract); one giant internal header for all modules (defeats
  the domain split).

### D3 — Web fragments via ordered `--post-js`, no generator

Split `entry.js` into `src/web/js/{core,render2d,render3d,resource,text,
particles,input,physics,audio,boot}.js` and pass them as ordered `--post-js`
flags; update `LINK_DEPENDS`.

- **Why**: no build-time concatenation step to maintain, and the seam order is
  explicit in CMake. The plan's stated preference.
- **Alternatives**: generate a concatenated `entry.js` at build time (extra
  generator and a generated file to keep in sync — the plan's other option);
  ES modules in the post-js (the glue relies on shared top-level scope, not
  module imports).

### D4 — Move-only passes prove equivalence mechanically

For P7/P10/P14, review with `--color-moved` (only includes, `static` →
internal-header declarations, and section headers should be non-dimmed) and
compare `nm -g --defined-only` of `efx_core` before/after; the diff must be
empty. For P10 also `diff` the concatenated web output against the pre-split
`entry.js`, allowing only whitespace at seams.

- **Why**: these are the strongest available evidence that a large move changed
  nothing, and the plan mandates them.
- **Alternatives**: rely on tests alone (tests cannot prove the absence of an
  unintended export or a reordered initializer).

### D5 — Policy objects, not merged behavior, for the readers

P3/P4 introduce generic helpers whose call sites pass the *existing* message
string and an explicit policy (coerce vs strict, integer range, error class);
no two rules are merged.

- **Why**: the plan requires the catalog stay byte-identical; naming the
  differences removes the copy-paste without changing what any reader accepts
  or throws.
- **Alternatives**: unify on one coercion rule (a behavior change — deferred to
  `docs/refactoring.md` §4.1).

### D6 — Preserve handle sequencing and slot reuse exactly

P11's `handle_decode` only decodes `gen<<32 | idx`; `pool_grow` only grows.
P12's `tex_slot_init` is called by both texture paths but the queued path keeps
appending and the live path keeps scanning for a free slot.

- **Why**: handle values are observable to the core tests and a reuse change
  would alter them; the asymmetry is a known deferred behavior change.
- **Alternatives**: unify reuse now (changes observable handles; out of scope).

### D7 — Shared resolvers, not a magic getter table, for P5

P5 adds `live_opaque(...)` plus per-class `*_alive` predicates and rewrites the
seven `get_live_*` (plus new `get_live_texture`/`get_live_font`) as one-line
resolvers. The ~12 read-only getters use those resolvers and a shared
`font_metric` helper. The design originally suggested a
`JS_CGETSET_MAGIC_DEF` table dispatching on `magic`; that was **not** adopted.

- **Why**: the getters differ not only in field but in class id, type/dead
  message and value extraction, so a magic table needs a large switch and
  casts for no behavioral benefit while raising the risk of a message change
  the byte-pinned catalog would (correctly) reject. The shared resolvers
  remove the same duplication with a smaller, reviewable diff.
- **Alternatives**: the magic table (rejected as above); leaving the getters
  untouched (keeps six copies of the unwrap/alive preamble).

## Risks / Trade-offs

- **A "move" silently changes initialization order or an include** → D4's
  `--color-moved` review plus empty `nm` diff; V1/V2/V4/V5 catch runtime
  differences.
- **`--post-js` ordering/global-scope assumptions** → Keep the current order
  (core → domains → boot); the byte-compare in D4 catches a reordering.
- **MSVC/Clang/GCC differ on new internal headers / `static` visibility** →
  V5 compiles all three; move a helper from `static` to an internal declaration
  only when a second TU needs it.
- **Consolidating helpers accidentally normalizes an error message** → The
  `error_catalog` case is byte-compared after every pass.
- **Large review burden** → One commit per pass/domain; move-only diffs are
  reviewed with color-moved.

## Migration Plan

Land in the plan's dependency order: P3 → P4 → P5 → P6 → P7; P8 → P9 → P10;
P11 → P12 → P13 → P14. P3–P6 precede P7; P8–P9 precede P10; P11–P13 precede
P14. Each pass is its own commit/branch; Checkpoint 2 is reached when P7, P10
and P14 are in and the V5 gate is green once. Rollback is reverting the pass's
commit(s); no data or API migration. Verification uses the plan's V1–V5 ladder,
with V5 through all four targets (Metal/D3D11 catch clip-depth/attachment
regressions per ADR 0025).

## Open Questions

None. The deferred behavior changes are explicitly out of scope and tracked in
`docs/refactoring.md` §4.
