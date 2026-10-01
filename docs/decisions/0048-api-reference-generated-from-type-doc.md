# 0048 — The API reference is generated from the type declaration; `js-api.md` is design guidelines

Status: Accepted (2026-09, change `api-reference-docs`)

Supports: ADR 0004 (single `efx` namespace), ADR 0023 (Pages deploys
separately from the four-target gate).
Relates to: ADR 0030 (gallery host/engine embedding — the gallery shell gains
an `/api` link).

## Context

The script API was documented in two hand-maintained places: the TypeScript
declaration `gallery/src/api/efx.d.ts` (loaded into the gallery editor and
type-checked) and the per-symbol catalog `docs/js-api.md`. The catalog
duplicated the declaration and had drifted from it, and the declaration now
carries full TSDoc (summaries, `@param`, `@returns`, `@example`). TypeDoc can
render that single source into both a committed Markdown reference and a
publishable HTML site. See `openspec/changes/api-reference-docs/` for the
full process record.

## Decision

- **`gallery/src/api/efx.d.ts` is the single source of truth for the
  per-symbol API reference.** Its TSDoc comments define each symbol's summary,
  parameters, return value, defaults, constraints, and examples.
- **The reference is generated, never hand-written.** Pinned TypeDoc
  (`typedoc` + `typedoc-plugin-markdown`) renders it two ways: the committed
  Markdown tree `docs/api/` (one file per symbol plus an index) and an HTML
  site built into the Pages artifact under `/api`.
- **`docs/js-api.md` is the API design guidelines.** It states the rules
  future additions follow — namespace, two-layer structure, conventions,
  units/colors, error model, resource and memory model, fixed limits,
  lifecycle, module model, gamepad model, and the process for adding API —
  and points to the generated reference instead of cataloging functions.
- **Regeneration is part of every API change.** `docs/api/` is regenerated
  from the declaration (`npm --prefix gallery run docs:markdown`) and
  committed; a drift guard (`npm --prefix gallery run docs:check`) fails the
  Pages build when the committed reference is stale.
- **The reference carries no internal tags.** No roadmap-milestone tags and
  no per-entry layer tags: the API is end-user facing, and both are
  implementation concerns.

## Consequences

- Future API changes MUST update `gallery/src/api/efx.d.ts` and regenerate
  `docs/api/`; hand-editing `docs/api/` is forbidden and caught by
  `docs:check`.
- The declaration is now load-bearing for published documentation, so TSDoc
  quality (params, returns, examples, defaults, constraints) is part of the
  API change, not optional.
- `docs/js-api.md` no longer duplicates the declaration; it must be updated
  only when a design rule changes, not per function.
- The four-target verification gate is unaffected (no engine/runtime code);
  the Pages workflow gains a docs build and a drift check.
- TypeDoc and the Markdown plugin are pinned; upgrading them may change
  generated output and requires regenerating and reviewing `docs/api/`.
- The reference and the editor hovers share one source, so they cannot
  disagree.

## Rejected alternatives

- **Keep the hand-written catalog** — it duplicates the declaration and had
  already drifted; two sources to reconcile per API change.
- **Generate from the C registration code** — describes bindings, not the
  script contract (option bags, defaults, error semantics live in prose), and
  has no type model.
- **VitePress / Starlight (+ typedoc markdown)** — a second site toolchain
  for a reference the default TypeDoc theme already renders well.
- **API Extractor + api-documenter** — aimed at npm-package APIs; awkward for
  a single ambient global.
- **`typedoc --json` + a custom renderer** — own a renderer against an
  unstable schema for no requirement the default theme misses.
- **Layer and milestone tags in the reference** — implementation/internal
  detail; the reference is end-user facing.
- **Include narrative docs and ADRs in the reference** — deferred; they stay
  separate Markdown under `docs/`.
