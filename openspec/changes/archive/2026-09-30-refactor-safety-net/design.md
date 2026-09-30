# Design

## Context

See `proposal.md — Why` and `docs/refactoring.md` (§1.2, §2, Phase A–B) for the
motivation and the full inventory. Current-state facts that shape the approach:

- The desktop binding (`src/api/api.c`) and the web binding
  (`src/web/entry.js`) validate option bags independently, and for particles /
  post effects the C core re-validates too. Parity is checked by
  `tools/run_web_compare.mjs`, which diffs the stdout of portable scripts
  (`tests/scripts/s_*.js`). Those scripts assert only
  `instanceof TypeError/RangeError`; they do not print messages, so the
  compare sees identical (near-empty) output even when the two bindings throw
  different text.
- There is no mechanical check for unused exports. `bridge.c` exposes
  `EMSCRIPTEN_KEEPALIVE` functions consumed from `entry.js` as
  `Module['_name']` / `_name`; some exports are dead. Public `efx_*` C
  functions declared in headers may be defined but never referenced.
- The `add_player_test` / `run_test.cmake` harness compares exit codes (and,
  for goldens, images); it has no mode for comparing captured stdout against a
  committed expected file.
- `docs/refactoring.md` §1.2 is the authoritative dead-symbol inventory; the
  re-verified revision is `main` at `9435f8c`.

## Goals / Non-Goals

**Goals:**

- Pin the exact error-message text of both bindings so that a refactor which
  accidentally changes a message is caught by the normal gate.
- Make the dead-export inventory mechanically reproducible, so P1/P2 (and any
  future cleanup) can be checked instead of re-derived by hand.
- Delete only dead code, with no change to any observable behavior.

**Non-Goals:**

- Fixing the coercion divergence the catalog exposes (behavior change).
- Making the guard script part of the gate.
- Any restructuring (P3+).

## Decisions

### D1 — Characterize messages with a script, not by unit tests

Add `tests/scripts/s_error_catalog.js` that constructs each bad option bag /
liveness case and prints a stable `Kind: message` line, with the expected text
committed to `tests/scripts/s_error_catalog.expected.txt`.

- **Why**: the same script runs through both runtimes (desktop quickjs and the
  web bridge) and through `run_web_compare.mjs`, so one artifact pins both
  bindings and their parity. A C unit test could not cover the web binding.
- **Alternatives**: assert full messages inside the existing `s_*_validation.js`
  scripts (rejected: they are intentionally class-only and shared with the
  compare; changing them would alter the compare baseline); golden-style image
  comparison (irrelevant — this is text).

### D2 — `EXPECT_OUT_FILE` mode in the harness

Extend `tests/run_test.cmake` / `add_player_test` so a case may declare an
expected-output file; the harness runs the player and byte-compares stdout.
Wire the case through `tools/run_web_compare.mjs` so the web runtime is held to
the same file.

- **Why**: reuses the existing player-invocation plumbing and keeps the
  expected text reviewable in-tree. A byte compare is the strongest possible
  pin and the plan's §2 lists it as the "error catalog" evidence.
- **Alternatives**: a bespoke runner script (duplicates the harness);
  normalizing whitespace (weakens the pin for no benefit).

### D3 — Record known divergences in-script, do not fix them

Where the two bindings are known or suspected to differ (the
`phys_opt_number` / `pcfg_num` coercion on desktop vs `__physNumber` on web),
the catalog emits a line marked `// KNOWN-DIVERGENCE` and the compare stays
green.

- **Why**: this change is behavior-preserving; the divergence is a finding to
  fix under its own change (`docs/refactoring.md` §4.1). Recording it here
  means the net is honest about what it does not pin.
- **Alternatives**: fail the catalog until the divergence is fixed (couples two
  changes and blocks the safety net on a behavior decision).

### D4 — Guard script is advisory and read-only

`tools/check_exports.mjs` scans `src/web/bridge.c` for `EMSCRIPTEN_KEEPALIVE`
definitions and cross-references `src/web/**` and `tools/**` for `_name` uses,
then scans headers for `efx_*` declarations cross-referenced against `src/` and
`tests/`. It prints candidates and exits zero.

- **Why**: the plan explicitly keeps it out of the gate (it would need
  false-positive handling and is a maintenance aid). Read-only means it cannot
  regress the build.
- **Alternatives**: a CMake/ctest check (rejected: gate coupling); a Python
  script (rejected: the web tooling is already Node/ESM).

### D5 — Deletions are per-module commits, symbol-surface checked

P2 deletes one module's dead symbols per commit, each with its header
declaration, after grepping `openspec/specs/` and `docs/decisions/` for the
symbol. Before/after `nm -g --defined-only` of `efx_core` must differ only by
the removed symbols.

- **Why**: isolates any unexpected reference to a single commit and gives the
  reviewer a precise diff. It also honors the plan's "keep it if a spec names
  it" rule.
- **Alternatives**: one bulk deletion commit (harder to bisect if a symbol
  turns out to be a test seam).

### D6 — Test-seam symbols gain a test instead of deletion

`efx_input_gamepad_inject_clear` and `efx_input_gamepad_load_mappings` are
kept if no spec contract exists but a headless test can exercise them; the
task adds that test so they stop being dead. Otherwise they are deleted like
the rest.

- **Why**: they exist to inject synthetic gamepad state, which the F13 gate
  already uses conceptually; keeping a tested seam is cheaper than re-adding
  one later.
- **Alternatives**: delete unconditionally (loses a useful injection point).

## Risks / Trade-offs

- **The catalog pins messages that may legitimately differ by platform** (e.g.
  libc/JS-engine number formatting) → Keep all catalog inputs and outputs
  engine-independent (plain strings and integers); mark any genuinely
  platform-specific case `// KNOWN-DIVERGENCE`.
- **A dead symbol turns out to be referenced from generated or vendored
  code** → The per-module `nm` diff and the grep step catch it before commit;
  revert that symbol only.
- **Deleting an export shrinks the wasm export table** → Intended (P1's stated
  benefit); the web harness, goldens and compare confirm nothing depended on
  it.
- **The guard script false-positives on macro-constructed references** →
  Advisory only; the task instructs manual confirmation before acting.

## Migration Plan

Land in the plan's order: P0 (catalog) → P0b (guard) → P1 (bridge exports) →
P2 (C exports). Each is its own commit. Rollback is reverting the commit(s);
no data or API migration. Verification follows the plan's ladder: V1/V2 locally
where a display exists, V3 for generated-file checks, V4 on the SSH
verification server, then the Linux → Windows → macOS V5 gate. Checkpoint 1 is
reached when P2 is in and the full gate is green once.

## Open Questions

None. The one genuine unknown (the coercion divergence) is deliberately
recorded, not resolved, by this change.
