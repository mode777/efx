# Proposal

## Why

AGENTS.md has grown to 293 lines, roughly half of which is hand-maintained
history: a per-milestone archive index, a 14-row roadmap table with gate run
IDs, and a "Not yet decided" section whose every item is settled. All
milestones F1–F14 are done, and that history is already recorded in
queryable systems (`openspec/changes/archive/`, `docs/decisions/`,
`openspec/specs/feature-roadmap`). Because the file doubles as a changelog,
it must be hand-synced on every change and has already drifted: it narrates
`collapse-pre-gpu-queue` and `entry-after-gpu-init`, which are complete but
not yet archived. Worse, its process rules are now wrong for the era: it
requires every proposal to "name the milestone it implements", which is
unsatisfiable post-roadmap and actively misleads the agents it instructs.

## What Changes

- **Rewrite AGENTS.md from ~293 lines to ~100**, keeping only what changes
  agent behavior:
  - Delete the milestone index (lines 18–67) and the Roadmap section
    (lines 175–210, incl. the table with gate run IDs and the orthogonal-
    milestone predecessor rules). Replaced by a short pointer block:
    status lives in `openspec/changes/archive/`, `docs/decisions/`, and the
    roadmap spec.
  - Delete the "Not yet decided" section (all items settled).
  - Fuse the six scattered operational bullets into one numbered **change
    lifecycle** (propose → apply → server verification → gate dispatch in
    Linux→Windows→macOS order → merge to main → archive), preserving every
    rule: CI never fires per-push (ADR 0023), SSH-server-first verification
    via `tools/verify_remote.py`, agents may commit/push to dispatch the
    gate, merge-on-green without a separate request, Pages deploys from
    `main` via `pages.yml` only.
  - Add an explicit **maintenance rule**: AGENTS.md records invariants and
    pointers, never per-change history; anything derivable from the archive,
    the ADR index, or the roadmap spec does not belong in the file.
  - Add a short dated **Open risks** section (e.g. the ubuntu-latest →
    Ubuntu 26 llvmpipe/golden re-baseline risk of 2026-10-19), each item
    carrying an expiry date.
  - Deduplicate repeated statements to one mention each: the `docs/api/`
    auto-generated warning (currently four times), the prelude-regen rule,
    the gallery build recipe.
  - Replace the milestone-ladder rule with its post-roadmap successor:
    **every proposal names the capability spec(s) under `openspec/specs/`
    it modifies or creates**.
- **Move load-bearing supersession facts to `docs/decisions/README.md`**
  index rows (the index already has a Status column supporting this, e.g.
  rows 0009 and 0013): F8b retired → superseded by 0024; music/effect
  audio split → superseded by 0047; pre-GPU upload queue → superseded by
  0052. This preserves the one thing the milestone index carried that the
  ADR index did not: which ADRs define current behavior.
- **Refresh the stale `openspec/config.yaml`**: its `context` block still
  presents the F1–F8 ladder as live, and its `rules.proposal`/`rules.tasks`
  entries still demand milestone names and milestone-gate verification —
  the same staleness this change fixes in AGENTS.md, injected into every
  future artifact creation.

## Capabilities

### New Capabilities

None — this change alters no runtime or script-facing behavior.

### Modified Capabilities

None — no spec-level behavior changes (pure docs/process change;
`skip_specs: true`).

## Impact

- **Files**: `AGENTS.md` (rewrite), `docs/decisions/README.md` (Status
  column annotations on three existing rows, no new ADR), `openspec/config.yaml`
  (context block and the two stale milestone rules).
- **Docs rules**: affected docs are AGENTS.md itself, the ADR index, and the
  OpenSpec project context. `docs/js-api.md` and the generated `docs/api/`
  reference are untouched — no script-facing API delta.
- **Milestone**: none — post-roadmap. The proposal rules' requirement to
  name a milestone (F1–F8) is exactly the stale convention this change
  retires; its successor rule is stated above.
- **ADR**: No ADR — no durable product-architecture decision is made; the
  change restructures agent guidance and relocates history to where it is
  maintained. The maintenance rule itself is recorded in AGENTS.md, which is
  the operative home for process invariants.
- **Verification**: docs-only — `npx openspec validate --strict` plus a
  human diff review of AGENTS.md; no ctest harness applies.

## Non-goals

- No changes to product code, build scripts, CI workflows, or tests.
- No edits to `openspec/specs/**` (the roadmap spec stays as-is; it is the
  normative history).
- No rewrite of `CONTRIBUTING.md`, `vision.md`, or `README.md` — AGENTS.md
  will point at CONTRIBUTING for human-facing build/verify detail instead of
  restating it.
- No new ADRs and no renumbering of existing ADRs.
- No history deletion: nothing is removed from the archive, ADR files, or
  the roadmap spec — AGENTS.md stops duplicating it.
